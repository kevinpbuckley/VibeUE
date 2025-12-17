// Copyright 2025 Vibe AI. All Rights Reserved.

#include "Chat/LLMClientBase.h"
#include "Chat/ChatSession.h"
#include "HttpModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY(LogLLMClientBase);

// Helper to check debug mode
static bool IsDebugLoggingEnabled()
{
    return FChatSession::IsDebugModeEnabled();
}

FString FLLMClientBase::SanitizeForLLM(const FString& Input)
{
    // Remove NUL characters and other problematic control characters
    FString Output;
    Output.Reserve(Input.Len());
    
    for (TCHAR Char : Input)
    {
        // Skip NUL (0x00) and other problematic control characters
        // Keep tab (0x09), newline (0x0A), carriage return (0x0D)
        if (Char == 0 || (Char < 32 && Char != 9 && Char != 10 && Char != 13))
        {
            continue;
        }
        Output.AppendChar(Char);
    }
    
    return Output;
}

FString FLLMClientBase::LoadSystemPromptFromFile()
{
    // Try to load instructions from file
    FString InstructionsPath;
    FString InstructionsContent;
    
    // Priority 1: Project plugins (local development)
    InstructionsPath = FPaths::ProjectPluginsDir() / TEXT("VibeUE") / TEXT("Content") / TEXT("vibeue.instructions.md");
    if (FFileHelper::LoadFileToString(InstructionsContent, *InstructionsPath))
    {
        UE_LOG(LogLLMClientBase, Log, TEXT("Loaded system prompt from: %s"), *InstructionsPath);
        return InstructionsContent;
    }
    
    // Priority 2: Engine marketplace (FAB install) - scan for VibeUE folder
    FString EngineMarketplacePath = FPaths::EnginePluginsDir() / TEXT("Marketplace");
    if (FPaths::DirectoryExists(EngineMarketplacePath))
    {
        IFileManager& FileManager = IFileManager::Get();
        TArray<FString> Directories;
        FileManager.FindFiles(Directories, *(EngineMarketplacePath / TEXT("*")), false, true);
        
        for (const FString& DirName : Directories)
        {
            InstructionsPath = EngineMarketplacePath / DirName / TEXT("Content") / TEXT("vibeue.instructions.md");
            if (FFileHelper::LoadFileToString(InstructionsContent, *InstructionsPath))
            {
                UE_LOG(LogLLMClientBase, Log, TEXT("Loaded system prompt from: %s"), *InstructionsPath);
                return InstructionsContent;
            }
        }
    }
    
    // Fallback: Built-in minimal prompt
    UE_LOG(LogLLMClientBase, Warning, TEXT("Could not load vibeue.instructions.md, using fallback prompt"));
    return TEXT(
        "You are an AI assistant integrated into Unreal Engine via the VibeUE plugin. "
        "You help users with Blueprint development, material creation, asset management, "
        "UMG widget design, Enhanced Input setup, and general Unreal Engine questions.\n\n"
        "You have access to MCP tools that can directly manipulate Unreal Engine. "
        "Use get_help(topic=\"overview\") to learn about available tools and workflows.\n\n"
        "Be concise and provide actionable guidance. When suggesting code or Blueprint "
        "logic, be specific about node names and connections."
    );
}

FLLMClientBase::FLLMClientBase()
    : bToolCallsDetectedInStream(false)
    , bInThinkingBlock(false)
{
}

FLLMClientBase::~FLLMClientBase()
{
    CancelRequest();
}

void FLLMClientBase::ResetStreamingState()
{
    StreamBuffer.Empty();
    AccumulatedContent.Empty();
    PendingToolCalls.Empty();
    bToolCallsDetectedInStream = false;
    bInThinkingBlock = false;
    bInToolCallBlock = false;
}

void FLLMClientBase::CancelRequest()
{
    if (CurrentRequest.IsValid())
    {
        CurrentRequest->CancelRequest();
        CurrentRequest.Reset();
    }
    ResetStreamingState();
}

bool FLLMClientBase::IsRequestInProgress() const
{
    return CurrentRequest.IsValid() && 
           CurrentRequest->GetStatus() == EHttpRequestStatus::Processing;
}

void FLLMClientBase::OnPreRequestError(const FString& ErrorMessage)
{
    UE_LOG(LogLLMClientBase, Error, TEXT("%s"), *ErrorMessage);
    if (CurrentOnError.IsBound())
    {
        CurrentOnError.Execute(ErrorMessage);
    }
    if (CurrentOnComplete.IsBound())
    {
        CurrentOnComplete.Execute(false);
    }
}

FString FLLMClientBase::ProcessErrorResponse(int32 ResponseCode, const FString& ResponseBody)
{
    // Default implementation - try to extract error from JSON
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        // Try common error formats
        FString ErrorMessage;
        if (JsonObject->TryGetStringField(TEXT("detail"), ErrorMessage) ||
            JsonObject->TryGetStringField(TEXT("message"), ErrorMessage) ||
            JsonObject->TryGetStringField(TEXT("error"), ErrorMessage))
        {
            return ErrorMessage;
        }
        
        // Nested error object
        const TSharedPtr<FJsonObject>* ErrorObj;
        if (JsonObject->TryGetObjectField(TEXT("error"), ErrorObj))
        {
            if ((*ErrorObj)->TryGetStringField(TEXT("message"), ErrorMessage))
            {
                return ErrorMessage;
            }
        }
    }
    
    return FString::Printf(TEXT("Request failed (HTTP %d)"), ResponseCode);
}

void FLLMClientBase::SendChatRequest(
    const TArray<FChatMessage>& Messages,
    const FString& ModelId,
    const TArray<FMCPTool>& Tools,
    FOnLLMStreamChunk OnChunk,
    FOnLLMStreamComplete OnComplete,
    FOnLLMStreamError OnError,
    FOnLLMToolCall OnToolCall,
    FOnLLMUsageReceived OnUsage)
{
    // Cancel any existing request
    CancelRequest();

    // Store delegates
    CurrentOnChunk = OnChunk;
    CurrentOnComplete = OnComplete;
    CurrentOnError = OnError;
    CurrentOnToolCall = OnToolCall;
    CurrentOnUsage = OnUsage;

    // Let subclass build the request
    CurrentRequest = BuildHttpRequest(Messages, ModelId, Tools);
    if (!CurrentRequest.IsValid())
    {
        // Subclass should have called OnPreRequestError already
        return;
    }

    // Debug log outgoing request
    if (IsDebugLoggingEnabled())
    {
        UE_LOG(LogLLMClientBase, Log, TEXT("========== LLM REQUEST =========="));
        UE_LOG(LogLLMClientBase, Log, TEXT("URL: %s"), *CurrentRequest->GetURL());
        UE_LOG(LogLLMClientBase, Log, TEXT("Messages: %d, Tools: %d"), Messages.Num(), Tools.Num());
        for (int32 i = 0; i < Messages.Num(); i++)
        {
            const FChatMessage& Msg = Messages[i];
            FString ContentPreview = Msg.Content.Left(200);
            if (Msg.Content.Len() > 200) ContentPreview += TEXT("...");
            UE_LOG(LogLLMClientBase, Log, TEXT("  [%d] %s: %s"), i, *Msg.Role, *ContentPreview);
            if (Msg.ToolCalls.Num() > 0)
            {
                UE_LOG(LogLLMClientBase, Log, TEXT("       ToolCalls: %d"), Msg.ToolCalls.Num());
            }
            if (!Msg.ToolCallId.IsEmpty())
            {
                UE_LOG(LogLLMClientBase, Log, TEXT("       ToolCallId: %s"), *Msg.ToolCallId);
            }
        }
        UE_LOG(LogLLMClientBase, Log, TEXT("=================================="));
    }

    // Bind streaming handlers
    CurrentRequest->OnRequestProgress64().BindRaw(this, &FLLMClientBase::HandleRequestProgress);
    CurrentRequest->OnProcessRequestComplete().BindRaw(this, &FLLMClientBase::HandleRequestComplete);

    // Send the request
    if (IsDebugLoggingEnabled())
    {
        UE_LOG(LogLLMClientBase, Log, TEXT("[REQUEST] Sending HTTP request..."));
        UE_LOG(LogLLMClientBase, Log, TEXT("[REQUEST] URL: %s"), *CurrentRequest->GetURL());
    }
    CurrentRequest->ProcessRequest();
}

void FLLMClientBase::HandleRequestProgress(FHttpRequestPtr Request, uint64 BytesSent, uint64 BytesReceived)
{
    // Only process when we've actually received data
    if (BytesReceived == 0)
    {
        // This is just the upload progress, not download - ignore
        return;
    }
    
    // Always log when progress is called (helps debug streaming issues)
    if (IsDebugLoggingEnabled())
    {
        UE_LOG(LogLLMClientBase, Log, TEXT("[STREAM] HandleRequestProgress: sent=%llu, received=%llu"), BytesSent, BytesReceived);
    }
    
    if (!Request.IsValid() || !Request->GetResponse().IsValid())
    {
        if (IsDebugLoggingEnabled())
        {
            UE_LOG(LogLLMClientBase, Warning, TEXT("[STREAM] Invalid request or response in progress callback"));
        }
        return;
    }

    FString ResponseContent = Request->GetResponse()->GetContentAsString();
    
    // Only process new content
    if (ResponseContent.Len() > StreamBuffer.Len())
    {
        FString NewContent = ResponseContent.RightChop(StreamBuffer.Len());
        if (IsDebugLoggingEnabled())
        {
            UE_LOG(LogLLMClientBase, Log, TEXT("[STREAM] New content: %d chars (total buffer: %d)"), NewContent.Len(), ResponseContent.Len());
        }
        else
        {
            UE_LOG(LogLLMClientBase, Verbose, TEXT("New SSE content (%d chars)"), NewContent.Len());
        }
        StreamBuffer = ResponseContent;
        
        // Check if this is SSE data or plain JSON (non-streaming response)
        // SSE data starts with "data: " prefix
        FString TrimmedContent = NewContent.TrimStart();
        if (TrimmedContent.StartsWith(TEXT("data: ")))
        {
            // SSE streaming response
            ProcessSSEData(NewContent);
        }
        else
        {
            // Non-streaming response - will be processed in HandleRequestComplete
            // Just store it in the buffer, don't process here
            if (IsDebugLoggingEnabled())
            {
                UE_LOG(LogLLMClientBase, Log, TEXT("[STREAM] Non-SSE content detected, deferring to HandleRequestComplete"));
            }
        }
    }
}

void FLLMClientBase::ProcessSSEData(const FString& Data)
{
    // Debug log raw SSE data
    if (IsDebugLoggingEnabled() && !Data.IsEmpty())
    {
        UE_LOG(LogLLMClientBase, Log, TEXT("[SSE] Raw data (%d chars): %s"), Data.Len(), *Data.Left(500));
    }

    // Split by newlines and process each SSE event
    TArray<FString> Lines;
    Data.ParseIntoArray(Lines, TEXT("\n"), true);

    for (const FString& Line : Lines)
    {
        FString TrimmedLine = Line.TrimStartAndEnd();
        
        // Skip empty lines and comments
        if (TrimmedLine.IsEmpty() || TrimmedLine.StartsWith(TEXT(":")))
        {
            continue;
        }

        // Handle SSE data lines
        if (TrimmedLine.StartsWith(TEXT("data: ")))
        {
            FString JsonData = TrimmedLine.RightChop(6); // Remove "data: " prefix
            
            // Check for stream end
            if (JsonData == TEXT("[DONE]"))
            {
                FirePendingToolCalls();
                continue;
            }

            ProcessSSEChunk(JsonData);
        }
    }
}

void FLLMClientBase::ProcessSSEChunk(const FString& JsonData)
{
    // Parse JSON
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonData);
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        return;
    }

    // Check for error
    if (JsonObject->HasField(TEXT("error")))
    {
        const TSharedPtr<FJsonObject>* ErrorObj;
        if (JsonObject->TryGetObjectField(TEXT("error"), ErrorObj))
        {
            FString ErrorMessage = (*ErrorObj)->GetStringField(TEXT("message"));
            UE_LOG(LogLLMClientBase, Error, TEXT("Stream error: %s"), *ErrorMessage);
            CurrentOnError.ExecuteIfBound(ErrorMessage);
        }
        return;
    }

    // Check for usage stats
    const TSharedPtr<FJsonObject>* UsageObj;
    if (JsonObject->TryGetObjectField(TEXT("usage"), UsageObj))
    {
        int32 PromptTokens = 0;
        int32 CompletionTokens = 0;
        (*UsageObj)->TryGetNumberField(TEXT("prompt_tokens"), PromptTokens);
        (*UsageObj)->TryGetNumberField(TEXT("completion_tokens"), CompletionTokens);
        
        if ((PromptTokens > 0 || CompletionTokens > 0) && CurrentOnUsage.IsBound())
        {
            CurrentOnUsage.Execute(PromptTokens, CompletionTokens);
        }
    }

    // Process choices array
    const TArray<TSharedPtr<FJsonValue>>* ChoicesArray;
    if (!JsonObject->TryGetArrayField(TEXT("choices"), ChoicesArray) || ChoicesArray->Num() == 0)
    {
        return;
    }

    const TSharedPtr<FJsonObject>* ChoiceObj;
    if (!(*ChoicesArray)[0]->TryGetObject(ChoiceObj))
    {
        return;
    }

    // Get delta object (streaming format)
    const TSharedPtr<FJsonObject>* DeltaObj;
    if (!(*ChoiceObj)->TryGetObjectField(TEXT("delta"), DeltaObj))
    {
        return;
    }

    // Check for tool calls in delta
    const TArray<TSharedPtr<FJsonValue>>* ToolCallsArray;
    if ((*DeltaObj)->TryGetArrayField(TEXT("tool_calls"), ToolCallsArray))
    {
        bToolCallsDetectedInStream = true;
        
        for (const TSharedPtr<FJsonValue>& ToolCallValue : *ToolCallsArray)
        {
            const TSharedPtr<FJsonObject>* ToolCallObj;
            if (!ToolCallValue->TryGetObject(ToolCallObj))
            {
                continue;
            }

            int32 ToolIndex = 0;
            (*ToolCallObj)->TryGetNumberField(TEXT("index"), ToolIndex);
            
            // Check if this is a new tool call (not yet in pending)
            bool bIsNewToolCall = !PendingToolCalls.Contains(ToolIndex);
            
            // Initialize tool call if not exists
            if (bIsNewToolCall)
            {
                PendingToolCalls.Add(ToolIndex, FMCPToolCall());
            }

            FMCPToolCall& ToolCall = PendingToolCalls[ToolIndex];

            // Get ID if present
            FString ToolCallId;
            if ((*ToolCallObj)->TryGetStringField(TEXT("id"), ToolCallId))
            {
                ToolCall.Id = ToolCallId;
            }

            // Get function details
            const TSharedPtr<FJsonObject>* FunctionObj;
            if ((*ToolCallObj)->TryGetObjectField(TEXT("function"), FunctionObj))
            {
                FString FunctionName;
                if ((*FunctionObj)->TryGetStringField(TEXT("name"), FunctionName))
                {
                    // Fire preparing callback when we first get the tool name
                    if (bIsNewToolCall && !FunctionName.IsEmpty() && CurrentOnToolPreparing.IsBound())
                    {
                        CurrentOnToolPreparing.Execute(FunctionName);
                    }
                    ToolCall.ToolName = FunctionName;
                }

                FString FunctionArgs;
                if ((*FunctionObj)->TryGetStringField(TEXT("arguments"), FunctionArgs))
                {
                    ToolCall.ArgumentsJson += FunctionArgs; // Accumulate arguments
                }
            }
        }
    }

    // Get content if present (skip if we have tool calls)
    FString DeltaContent;
    if ((*DeltaObj)->TryGetStringField(TEXT("content"), DeltaContent))
    {
        UE_LOG(LogLLMClientBase, Log, TEXT("Delta content: '%s' (len=%d, bToolCalls=%d, bThinking=%d)"), 
            *DeltaContent.Left(100), DeltaContent.Len(), bToolCallsDetectedInStream, bInThinkingBlock);
        
        if (!bToolCallsDetectedInStream && !DeltaContent.IsEmpty())
        {
            FString CleanContent = FilterThinkingTags(DeltaContent);
            
            if (!CleanContent.IsEmpty() && CurrentOnChunk.IsBound())
            {
                UE_LOG(LogLLMClientBase, Log, TEXT("Sending chunk: '%s'"), *CleanContent.Left(100));
                CurrentOnChunk.Execute(CleanContent);
            }
        }
    }
}

FString FLLMClientBase::FilterThinkingTags(const FString& Content)
{
    // Track previous thinking state to detect transitions
    bool bWasThinking = bInThinkingBlock;
    
    // Handle <thinking>...</thinking> (Claude), <think>...</think> (Qwen), and <tool_call>...</tool_call> (Qwen text-based tool calls)
    FString CleanContent = Content;
    
    // First filter <tool_call> tags (Qwen sometimes outputs these in text instead of using native tool calls)
    CleanContent = FilterTagBlock(CleanContent, TEXT("<tool_call>"), TEXT("</tool_call>"), bInToolCallBlock);
    
    // Then filter thinking tags
    CleanContent = FilterTagBlock(CleanContent, TEXT("<thinking>"), TEXT("</thinking>"), bInThinkingBlock);
    if (CleanContent == Content) // No <thinking> found, try <think>
    {
        CleanContent = FilterTagBlock(CleanContent, TEXT("<think>"), TEXT("</think>"), bInThinkingBlock);
    }
    
    // Fire thinking status callback on state transitions
    if (bInThinkingBlock != bWasThinking && CurrentOnThinkingStatus.IsBound())
    {
        CurrentOnThinkingStatus.Execute(bInThinkingBlock);
    }
    
    return CleanContent;
}

FString FLLMClientBase::FilterTagBlock(const FString& Content, const FString& OpenTag, const FString& CloseTag, bool& bInBlock)
{
    FString CleanContent;
    int32 TagStart = Content.Find(OpenTag, ESearchCase::IgnoreCase);
    int32 TagEnd = Content.Find(CloseTag, ESearchCase::IgnoreCase);
    int32 CloseTagLen = CloseTag.Len();
    
    if (TagStart != INDEX_NONE || bInBlock)
    {
        if (TagStart != INDEX_NONE && TagEnd != INDEX_NONE && TagEnd > TagStart)
        {
            // Complete block in this chunk - remove it
            CleanContent = Content.Left(TagStart) + Content.Mid(TagEnd + CloseTagLen);
            bInBlock = false;
        }
        else if (TagStart != INDEX_NONE)
        {
            // Block starts but doesn't end
            CleanContent = Content.Left(TagStart);
            bInBlock = true;
        }
        else if (TagEnd != INDEX_NONE)
        {
            // Block ends
            CleanContent = Content.Mid(TagEnd + CloseTagLen);
            bInBlock = false;
        }
        else
        {
            // Still inside block - skip content
            CleanContent = TEXT("");
        }
    }
    else
    {
        CleanContent = Content;
    }
    
    return CleanContent;
}

void FLLMClientBase::FirePendingToolCalls()
{
    if (PendingToolCalls.Num() == 0 || !CurrentOnToolCall.IsBound())
    {
        if (IsDebugLoggingEnabled())
        {
            UE_LOG(LogLLMClientBase, Log, TEXT("[SSE] [DONE] received - no pending tool calls"));
        }
        return;
    }

    if (IsDebugLoggingEnabled())
    {
        UE_LOG(LogLLMClientBase, Log, TEXT("========== TOOL CALLS =========="));
        UE_LOG(LogLLMClientBase, Log, TEXT("Firing %d pending tool calls"), PendingToolCalls.Num());
    }

    // Sort by index and execute
    TArray<int32> Indices;
    PendingToolCalls.GetKeys(Indices);
    Indices.Sort();

    for (int32 Index : Indices)
    {
        FMCPToolCall& ToolCall = PendingToolCalls[Index];
        
        // Parse accumulated arguments JSON into the Arguments object
        if (!ToolCall.ArgumentsJson.IsEmpty())
        {
            TSharedRef<TJsonReader<>> ArgReader = TJsonReaderFactory<>::Create(ToolCall.ArgumentsJson);
            FJsonSerializer::Deserialize(ArgReader, ToolCall.Arguments);
        }
        
        if (IsDebugLoggingEnabled())
        {
            UE_LOG(LogLLMClientBase, Log, TEXT("  [%d] %s (id=%s)"), Index, *ToolCall.ToolName, *ToolCall.Id);
            UE_LOG(LogLLMClientBase, Log, TEXT("       Args: %s"), *ToolCall.ArgumentsJson.Left(300));
        }
        UE_LOG(LogLLMClientBase, Log, TEXT("Firing tool call: %s"), *ToolCall.ToolName);
        CurrentOnToolCall.Execute(ToolCall);
    }
    
    if (IsDebugLoggingEnabled())
    {
        UE_LOG(LogLLMClientBase, Log, TEXT("=================================="));
    }
}

void FLLMClientBase::ProcessNonStreamingResponse(const FString& ResponseContent)
{
    UE_LOG(LogLLMClientBase, Log, TEXT("[NON-STREAM] Processing non-streaming response"));
    
    // Parse the JSON response
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        UE_LOG(LogLLMClientBase, Error, TEXT("[NON-STREAM] Failed to parse JSON response"));
        CurrentOnError.ExecuteIfBound(TEXT("Failed to parse LLM response"));
        return;
    }
    
    // Check for error
    if (JsonObject->HasField(TEXT("error")))
    {
        const TSharedPtr<FJsonObject>* ErrorObj;
        if (JsonObject->TryGetObjectField(TEXT("error"), ErrorObj))
        {
            FString ErrorMessage = (*ErrorObj)->GetStringField(TEXT("message"));
            UE_LOG(LogLLMClientBase, Error, TEXT("[NON-STREAM] API error: %s"), *ErrorMessage);
            CurrentOnError.ExecuteIfBound(ErrorMessage);
        }
        return;
    }
    
    // Get usage stats if present
    const TSharedPtr<FJsonObject>* UsageObj;
    if (JsonObject->TryGetObjectField(TEXT("usage"), UsageObj))
    {
        int32 PromptTokens = 0;
        int32 CompletionTokens = 0;
        (*UsageObj)->TryGetNumberField(TEXT("prompt_tokens"), PromptTokens);
        (*UsageObj)->TryGetNumberField(TEXT("completion_tokens"), CompletionTokens);
        
        if ((PromptTokens > 0 || CompletionTokens > 0) && CurrentOnUsage.IsBound())
        {
            UE_LOG(LogLLMClientBase, Log, TEXT("[NON-STREAM] Usage: prompt=%d, completion=%d"), PromptTokens, CompletionTokens);
            CurrentOnUsage.Execute(PromptTokens, CompletionTokens);
        }
    }
    
    // Get choices array
    const TArray<TSharedPtr<FJsonValue>>* ChoicesArray;
    if (!JsonObject->TryGetArrayField(TEXT("choices"), ChoicesArray) || ChoicesArray->Num() == 0)
    {
        UE_LOG(LogLLMClientBase, Warning, TEXT("[NON-STREAM] No choices in response"));
        return;
    }
    
    // Get first choice
    const TSharedPtr<FJsonObject>* ChoiceObj;
    if (!(*ChoicesArray)[0]->TryGetObject(ChoiceObj))
    {
        UE_LOG(LogLLMClientBase, Warning, TEXT("[NON-STREAM] Invalid choice object"));
        return;
    }
    
    // Get the message object (non-streaming uses "message", streaming uses "delta")
    const TSharedPtr<FJsonObject>* MessageObj;
    if (!(*ChoiceObj)->TryGetObjectField(TEXT("message"), MessageObj))
    {
        UE_LOG(LogLLMClientBase, Warning, TEXT("[NON-STREAM] No message in choice"));
        return;
    }
    
    // Check for tool calls
    const TArray<TSharedPtr<FJsonValue>>* ToolCallsArray;
    if ((*MessageObj)->TryGetArrayField(TEXT("tool_calls"), ToolCallsArray) && ToolCallsArray->Num() > 0)
    {
        UE_LOG(LogLLMClientBase, Log, TEXT("[NON-STREAM] Found %d tool calls"), ToolCallsArray->Num());
        bToolCallsDetectedInStream = true;
        
        for (int32 i = 0; i < ToolCallsArray->Num(); i++)
        {
            const TSharedPtr<FJsonObject>* ToolCallObj;
            if (!(*ToolCallsArray)[i]->TryGetObject(ToolCallObj))
            {
                continue;
            }
            
            FMCPToolCall ToolCall;
            ToolCall.Id = (*ToolCallObj)->GetStringField(TEXT("id"));
            
            const TSharedPtr<FJsonObject>* FunctionObj;
            if ((*ToolCallObj)->TryGetObjectField(TEXT("function"), FunctionObj))
            {
                ToolCall.ToolName = (*FunctionObj)->GetStringField(TEXT("name"));
                ToolCall.ArgumentsJson = (*FunctionObj)->GetStringField(TEXT("arguments"));
                
                // Parse arguments JSON
                TSharedRef<TJsonReader<>> ArgReader = TJsonReaderFactory<>::Create(ToolCall.ArgumentsJson);
                FJsonSerializer::Deserialize(ArgReader, ToolCall.Arguments);
            }
            
            UE_LOG(LogLLMClientBase, Log, TEXT("[NON-STREAM] Tool call %d: %s (id=%s)"), i, *ToolCall.ToolName, *ToolCall.Id);
            
            if (CurrentOnToolCall.IsBound())
            {
                CurrentOnToolCall.Execute(ToolCall);
            }
        }
    }
    else
    {
        // Get content
        FString Content;
        if ((*MessageObj)->TryGetStringField(TEXT("content"), Content) && !Content.IsEmpty())
        {
            UE_LOG(LogLLMClientBase, Log, TEXT("[NON-STREAM] Content: %s"), *Content.Left(200));
            
            // Filter thinking tags
            FString CleanContent = FilterThinkingTags(Content);
            
            // Store accumulated content for retrieval (used by summarization)
            AccumulatedContent = CleanContent;
            
            if (!CleanContent.IsEmpty() && CurrentOnChunk.IsBound())
            {
                CurrentOnChunk.Execute(CleanContent);
            }
        }
    }
}
void FLLMClientBase::HandleRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
    // Check for timeout/connection failure first
    if (Request.IsValid())
    {
        EHttpRequestStatus::Type RequestStatus = Request->GetStatus();
        if (RequestStatus == EHttpRequestStatus::Failed)
        {
            UE_LOG(LogLLMClientBase, Error, TEXT("HandleRequestComplete: Request failed with connection error (possibly timeout)"));
            CurrentOnError.ExecuteIfBound(TEXT("Request timed out or connection failed. Please try again."));
            CurrentOnComplete.ExecuteIfBound(false);
            CurrentRequest.Reset();
            ResetStreamingState();
            return;
        }
    }
    
    // ALWAYS log response validity for debugging
    UE_LOG(LogLLMClientBase, Log, TEXT("HandleRequestComplete: Response valid=%s, Connected=%s"), 
        Response.IsValid() ? TEXT("Yes") : TEXT("No"),
        bConnectedSuccessfully ? TEXT("Yes") : TEXT("No"));
    
    if (Response.IsValid())
    {
        // Log response headers for debugging
        int32 ResponseCode = Response->GetResponseCode();
        FString ContentType = Response->GetHeader(TEXT("Content-Type"));
        UE_LOG(LogLLMClientBase, Log, TEXT("HandleRequestComplete: ResponseCode=%d, ContentType=%s"), 
            ResponseCode, *ContentType);
        
        FString ResponseContent = Response->GetContentAsString();
        UE_LOG(LogLLMClientBase, Log, TEXT("HandleRequestComplete: Response content length=%d, StreamBuffer length=%d"), 
            ResponseContent.Len(), StreamBuffer.Len());
        
        // Check if we have content that needs processing
        if (ResponseContent.Len() > 0)
        {
            // Check if this is non-SSE (JSON) content that wasn't processed in progress callback
            FString TrimmedContent = ResponseContent.TrimStart();
            bool bIsSSEContent = TrimmedContent.StartsWith(TEXT("data: "));
            
            // For non-SSE content, we need to process it here since progress callback deferred it
            if (!bIsSSEContent && !bToolCallsDetectedInStream)
            {
                UE_LOG(LogLLMClientBase, Log, TEXT("Processing non-streaming JSON response"));
                UE_LOG(LogLLMClientBase, Log, TEXT("Response preview: %s"), *ResponseContent.Left(1000));
                ProcessNonStreamingResponse(ResponseContent);
            }
            else if (StreamBuffer.Len() == 0)
            {
                // SSE content that wasn't captured by progress callback
                UE_LOG(LogLLMClientBase, Log, TEXT("Processing SSE content that wasn't captured by progress callback"));
                StreamBuffer = ResponseContent;
                ProcessSSEData(ResponseContent);
            }
        }
    }
    
    // Also log the request URL and verb for context
    if (Request.IsValid())
    {
        UE_LOG(LogLLMClientBase, Log, TEXT("HandleRequestComplete: Request URL=%s, Verb=%s"), 
            *Request->GetURL(), *Request->GetVerb());
    }
    
    if (IsDebugLoggingEnabled())
    {
        UE_LOG(LogLLMClientBase, Log, TEXT("========== LLM RESPONSE COMPLETE =========="));
        UE_LOG(LogLLMClientBase, Log, TEXT("Connected: %s"), bConnectedSuccessfully ? TEXT("Yes") : TEXT("No"));
        UE_LOG(LogLLMClientBase, Log, TEXT("Stream buffer size: %d chars"), StreamBuffer.Len());
    }

    // For SSE streaming, bConnectedSuccessfully can be false even when we received data
    // This happens because SSE streams typically end with server closing the connection
    // If we have streaming data, consider it a success
    bool bHasStreamingData = !StreamBuffer.IsEmpty();
    
    if (!bConnectedSuccessfully && !bHasStreamingData)
    {
        UE_LOG(LogLLMClientBase, Error, TEXT("Request failed - connection error (no streaming data received)"));
        CurrentOnError.ExecuteIfBound(TEXT("Failed to connect. Please check your network connection."));
        CurrentOnComplete.ExecuteIfBound(false);
        CurrentRequest.Reset();
        ResetStreamingState();
        return;
    }

    int32 ResponseCode = Response.IsValid() ? Response->GetResponseCode() : 0;
    
    // If we have streaming data but no response code, assume success (SSE completed)
    if (ResponseCode == 0 && bHasStreamingData)
    {
        ResponseCode = 200;
        if (IsDebugLoggingEnabled())
        {
            UE_LOG(LogLLMClientBase, Log, TEXT("No response code but have streaming data - treating as success"));
        }
    }
    
    if (IsDebugLoggingEnabled())
    {
        UE_LOG(LogLLMClientBase, Log, TEXT("Response Code: %d"), ResponseCode);
        UE_LOG(LogLLMClientBase, Log, TEXT("Total response length: %d chars"), StreamBuffer.Len());
        UE_LOG(LogLLMClientBase, Log, TEXT("Tool calls detected: %s"), bToolCallsDetectedInStream ? TEXT("Yes") : TEXT("No"));
        UE_LOG(LogLLMClientBase, Log, TEXT("==========================================="));
    }
    
    if (ResponseCode == 200)
    {
        if (IsDebugLoggingEnabled())
        {
            UE_LOG(LogLLMClientBase, Log, TEXT("[COMPLETE] Request completed successfully"));
            UE_LOG(LogLLMClientBase, Log, TEXT("[COMPLETE] Total stream buffer: %d chars"), StreamBuffer.Len());
            UE_LOG(LogLLMClientBase, Log, TEXT("[COMPLETE] Tool calls fired: %s"), bToolCallsDetectedInStream ? TEXT("Yes") : TEXT("No"));
        }
        else
        {
            UE_LOG(LogLLMClientBase, Verbose, TEXT("Request completed successfully"));
        }
        CurrentOnComplete.ExecuteIfBound(true);
    }
    else
    {
        FString ResponseBody = Response.IsValid() ? Response->GetContentAsString() : TEXT("");
        FString ErrorMessage = ProcessErrorResponse(ResponseCode, ResponseBody);
        
        UE_LOG(LogLLMClientBase, Error, TEXT("Request failed: %s"), *ErrorMessage);
        if (IsDebugLoggingEnabled())
        {
            UE_LOG(LogLLMClientBase, Log, TEXT("Response body: %s"), *ResponseBody.Left(1000));
        }
        CurrentOnError.ExecuteIfBound(ErrorMessage);
        CurrentOnComplete.ExecuteIfBound(false);
    }

    // Clean up
    CurrentRequest.Reset();
    ResetStreamingState();
}
