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
    , bInToolCallBlock(false)
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
    bInToolCallBlock = false;
    bInThinkingBlock = false;
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
        
        // Check if this is SSE data or plain JSON (non-streaming response)
        // SSE data starts with "data: " prefix, or may start with ":" (comment) followed by data lines
        FString TrimmedContent = NewContent.TrimStart();
        if (TrimmedContent.StartsWith(TEXT("data: ")) || TrimmedContent.StartsWith(TEXT(":")))
        {
            // SSE streaming response (including SSE comments like ": OPENROUTER PROCESSING")
            // Update StreamBuffer ONLY for SSE content that we're processing
            StreamBuffer = ResponseContent;
            // Process the entire content - ProcessSSEData will skip comment lines and process data lines
            ProcessSSEData(NewContent);
        }
        else
        {
            // Non-streaming response - will be processed in HandleRequestComplete
            // Do NOT update StreamBuffer here so HandleRequestComplete knows to process it
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

    // Get content if present
    // Note: Content may come before tool calls in the same response, so we need to capture it
    FString DeltaContent;
    if ((*DeltaObj)->TryGetStringField(TEXT("content"), DeltaContent))
    {
        UE_LOG(LogLLMClientBase, Log, TEXT("Delta content: '%s' (len=%d, bToolCalls=%d)"), 
            *DeltaContent.Left(100), DeltaContent.Len(), bToolCallsDetectedInStream);
        
        // Always process content - it may come before tool calls start
        if (!DeltaContent.IsEmpty())
        {
            // Detect thinking block start/end and fire status callback
            DetectThinkingBlocks(DeltaContent);
            
            // Filter only tool_call tags (those need to be parsed), but pass through thinking tags
            FString CleanContent = FilterToolCallTags(DeltaContent);
            
            if (!CleanContent.IsEmpty() && CurrentOnChunk.IsBound())
            {
                UE_LOG(LogLLMClientBase, Log, TEXT("Sending chunk: '%s'"), *CleanContent.Left(100));
                CurrentOnChunk.Execute(CleanContent);
            }
        }
    }
}

FString FLLMClientBase::FilterToolCallTags(const FString& Content)
{
    // Filter tool_call tags from content - these need to be parsed for tool execution
    // Keep thinking tags (<think>, <thinking>) visible to the user
    FString CleanContent = Content;
    
    // Filter <tool_call> tags (Qwen sometimes outputs these in text instead of using native tool calls)
    CleanContent = FilterTagBlock(CleanContent, TEXT("<tool_call>"), TEXT("</tool_call>"), bInToolCallBlock);
    
    // Filter bracket-style [tool_call: ...] or [Tool call: ...] patterns
    // Use a simple state-based approach for streaming (can't use regex on partial chunks)
    int32 BracketStart;
    while ((BracketStart = CleanContent.Find(TEXT("[tool_call:"), ESearchCase::IgnoreCase)) != INDEX_NONE)
    {
        int32 BracketEnd = CleanContent.Find(TEXT("]"), ESearchCase::IgnoreCase, ESearchDir::FromStart, BracketStart);
        if (BracketEnd != INDEX_NONE)
        {
            // Complete bracket tool call - remove it
            CleanContent = CleanContent.Left(BracketStart) + CleanContent.Mid(BracketEnd + 1);
        }
        else
        {
            // Incomplete - truncate at bracket start (will accumulate in next chunk)
            CleanContent = CleanContent.Left(BracketStart);
            break;
        }
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

void FLLMClientBase::DetectThinkingBlocks(const FString& Content)
{
    // Detect start and end of thinking blocks and fire status callback
    // Supports: <think>, <thinking>, <reasoning>, <thought>
    
    bool bWasInThinkingBlock = bInThinkingBlock;
    
    // Check for thinking block start tags
    static const TArray<FString> OpenTags = { 
        TEXT("<think>"), TEXT("<thinking>"), TEXT("<reasoning>"), TEXT("<thought>") 
    };
    
    // Check for thinking block end tags
    static const TArray<FString> CloseTags = { 
        TEXT("</think>"), TEXT("</thinking>"), TEXT("</reasoning>"), TEXT("</thought>") 
    };
    
    // Check if any open tag is present
    for (const FString& OpenTag : OpenTags)
    {
        if (Content.Contains(OpenTag, ESearchCase::IgnoreCase))
        {
            bInThinkingBlock = true;
            break;
        }
    }
    
    // Check if any close tag is present
    for (const FString& CloseTag : CloseTags)
    {
        if (Content.Contains(CloseTag, ESearchCase::IgnoreCase))
        {
            bInThinkingBlock = false;
            break;
        }
    }
    
    // Fire callback if state changed
    if (bWasInThinkingBlock != bInThinkingBlock && CurrentOnThinkingStatus.IsBound())
    {
        UE_LOG(LogLLMClientBase, Log, TEXT("[THINKING] Status changed: %s"), bInThinkingBlock ? TEXT("started") : TEXT("ended"));
        CurrentOnThinkingStatus.Execute(bInThinkingBlock);
    }
}

TArray<FMCPToolCall> FLLMClientBase::ParseBracketStyleToolCalls(const FString& Content)
{
    // Parse bracket-style tool calls from content
    // Formats supported:
    //   [tool_call: func_name(arg1="value1", arg2=value2)]
    //   [Tool call: func_name(arg1="value1", arg2=value2)]
    
    TArray<FMCPToolCall> ToolCalls;
    
    // Find all [tool_call: ...] or [Tool call: ...] patterns
    int32 SearchStart = 0;
    int32 CallIndex = 0;
    
    while (SearchStart < Content.Len())
    {
        // Find start of tool call - case insensitive
        int32 BracketStart = Content.Find(TEXT("[tool_call:"), ESearchCase::IgnoreCase, ESearchDir::FromStart, SearchStart);
        if (BracketStart == INDEX_NONE)
        {
            // Try alternate format "[Tool call:"
            BracketStart = Content.Find(TEXT("[Tool call:"), ESearchCase::IgnoreCase, ESearchDir::FromStart, SearchStart);
        }
        
        if (BracketStart == INDEX_NONE)
        {
            break;
        }
        
        // Find the closing bracket
        int32 BracketEnd = Content.Find(TEXT("]"), ESearchCase::IgnoreCase, ESearchDir::FromStart, BracketStart);
        if (BracketEnd == INDEX_NONE)
        {
            // Incomplete - might continue in next chunk
            break;
        }
        
        // Extract the content between [tool_call: and ]
        int32 ContentStart = BracketStart;
        // Skip past "[tool_call:" or "[Tool call:"
        while (ContentStart < BracketEnd && Content[ContentStart] != TCHAR(':'))
        {
            ContentStart++;
        }
        ContentStart++; // Skip the colon
        
        FString CallContent = Content.Mid(ContentStart, BracketEnd - ContentStart).TrimStartAndEnd();
        
        // Parse: func_name(args)
        int32 ParenStart = CallContent.Find(TEXT("("));
        if (ParenStart == INDEX_NONE)
        {
            // No arguments - just function name
            FMCPToolCall ToolCall;
            ToolCall.ToolName = CallContent.TrimStartAndEnd();
            ToolCall.Id = FString::Printf(TEXT("bracket_call_%d_%lld"), CallIndex, FDateTime::UtcNow().GetTicks());
            ToolCall.ArgumentsJson = TEXT("{}");
            
            if (!ToolCall.ToolName.IsEmpty())
            {
                UE_LOG(LogLLMClientBase, Log, TEXT("[BRACKET PARSE] Tool call (no args): %s"), *ToolCall.ToolName);
                ToolCalls.Add(ToolCall);
                CallIndex++;
            }
        }
        else
        {
            // Has arguments
            FString FuncName = CallContent.Left(ParenStart).TrimStartAndEnd();
            
            // Find matching closing paren
            int32 ParenEnd = CallContent.Len() - 1;
            while (ParenEnd > ParenStart && CallContent[ParenEnd] != TCHAR(')'))
            {
                ParenEnd--;
            }
            
            FString ArgsStr = CallContent.Mid(ParenStart + 1, ParenEnd - ParenStart - 1);
            
            // Parse arguments into JSON object
            // Format: key1="value1", key2=value2, key3=true
            TSharedPtr<FJsonObject> ArgsObj = MakeShared<FJsonObject>();
            
            // Simple parser for key=value pairs
            TArray<FString> ArgPairs;
            
            // Split by comma, but respect quotes and nested braces
            FString CurrentArg;
            int32 BraceDepth = 0;
            bool bInQuotes = false;
            TCHAR QuoteChar = 0;
            
            for (int32 i = 0; i < ArgsStr.Len(); i++)
            {
                TCHAR c = ArgsStr[i];
                
                if (!bInQuotes && (c == TCHAR('"') || c == TCHAR('\'')))
                {
                    bInQuotes = true;
                    QuoteChar = c;
                    CurrentArg.AppendChar(c);
                }
                else if (bInQuotes && c == QuoteChar)
                {
                    bInQuotes = false;
                    CurrentArg.AppendChar(c);
                }
                else if (!bInQuotes && (c == TCHAR('{') || c == TCHAR('[')))
                {
                    BraceDepth++;
                    CurrentArg.AppendChar(c);
                }
                else if (!bInQuotes && (c == TCHAR('}') || c == TCHAR(']')))
                {
                    BraceDepth--;
                    CurrentArg.AppendChar(c);
                }
                else if (!bInQuotes && BraceDepth == 0 && c == TCHAR(','))
                {
                    // End of argument
                    FString TrimmedArg = CurrentArg.TrimStartAndEnd();
                    if (!TrimmedArg.IsEmpty())
                    {
                        ArgPairs.Add(TrimmedArg);
                    }
                    CurrentArg.Empty();
                }
                else
                {
                    CurrentArg.AppendChar(c);
                }
            }
            
            // Don't forget the last argument
            FString TrimmedArg = CurrentArg.TrimStartAndEnd();
            if (!TrimmedArg.IsEmpty())
            {
                ArgPairs.Add(TrimmedArg);
            }
            
            // Parse each key=value pair
            for (const FString& Pair : ArgPairs)
            {
                int32 EqPos = Pair.Find(TEXT("="));
                if (EqPos != INDEX_NONE)
                {
                    FString Key = Pair.Left(EqPos).TrimStartAndEnd();
                    FString Value = Pair.Mid(EqPos + 1).TrimStartAndEnd();
                    
                    // Remove quotes from string values
                    if ((Value.StartsWith(TEXT("\"")) && Value.EndsWith(TEXT("\""))) ||
                        (Value.StartsWith(TEXT("'")) && Value.EndsWith(TEXT("'"))))
                    {
                        Value = Value.Mid(1, Value.Len() - 2);
                        ArgsObj->SetStringField(Key, Value);
                    }
                    else if (Value.Equals(TEXT("true"), ESearchCase::IgnoreCase))
                    {
                        ArgsObj->SetBoolField(Key, true);
                    }
                    else if (Value.Equals(TEXT("false"), ESearchCase::IgnoreCase))
                    {
                        ArgsObj->SetBoolField(Key, false);
                    }
                    else if (Value.StartsWith(TEXT("{")) || Value.StartsWith(TEXT("[")))
                    {
                        // Try to parse as JSON
                        TSharedPtr<FJsonValue> JsonValue;
                        TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(Value);
                        if (FJsonSerializer::Deserialize(JsonReader, JsonValue) && JsonValue.IsValid())
                        {
                            ArgsObj->SetField(Key, JsonValue);
                        }
                        else
                        {
                            ArgsObj->SetStringField(Key, Value);
                        }
                    }
                    else if (Value.IsNumeric())
                    {
                        // Number
                        ArgsObj->SetNumberField(Key, FCString::Atod(*Value));
                    }
                    else
                    {
                        // Plain string without quotes
                        ArgsObj->SetStringField(Key, Value);
                    }
                }
            }
            
            // Serialize arguments to JSON
            FMCPToolCall ToolCall;
            ToolCall.ToolName = FuncName;
            ToolCall.Id = FString::Printf(TEXT("bracket_call_%d_%lld"), CallIndex, FDateTime::UtcNow().GetTicks());
            ToolCall.Arguments = ArgsObj;
            
            TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ToolCall.ArgumentsJson);
            FJsonSerializer::Serialize(ArgsObj.ToSharedRef(), Writer);
            
            if (!ToolCall.ToolName.IsEmpty())
            {
                UE_LOG(LogLLMClientBase, Log, TEXT("[BRACKET PARSE] Tool call: %s, args: %s"), *ToolCall.ToolName, *ToolCall.ArgumentsJson.Left(200));
                ToolCalls.Add(ToolCall);
                CallIndex++;
            }
        }
        
        SearchStart = BracketEnd + 1;
    }
    
    return ToolCalls;
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
        
        // VALIDATION: Skip tool calls with empty name or ID (malformed streaming response)
        if (ToolCall.ToolName.IsEmpty())
        {
            UE_LOG(LogLLMClientBase, Warning, TEXT("Skipping tool call with empty name at index %d (ID=%s, Args=%s)"), 
                Index, *ToolCall.Id, *ToolCall.ArgumentsJson.Left(100));
            // Create error result for this malformed tool call
            if (!ToolCall.Id.IsEmpty())
            {
                // If we have an ID but no name, we need to report an error
                FMCPToolCall ErrorCall = ToolCall;
                ErrorCall.ToolName = TEXT("__error__");
                CurrentOnToolCall.Execute(ErrorCall);
            }
            continue;
        }
        
        // Generate a fallback ID if empty (some providers don't send IDs correctly)
        if (ToolCall.Id.IsEmpty())
        {
            ToolCall.Id = FString::Printf(TEXT("call_%d_%lld"), Index, FDateTime::UtcNow().GetTicks());
            UE_LOG(LogLLMClientBase, Warning, TEXT("Generated fallback ID for tool call: %s -> %s"), 
                *ToolCall.ToolName, *ToolCall.Id);
        }
        
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
    
    // Clear pending tool calls after firing to prevent duplicate execution
    PendingToolCalls.Empty();
    
    if (IsDebugLoggingEnabled())
    {
        UE_LOG(LogLLMClientBase, Log, TEXT("=================================="));
    }
}

void FLLMClientBase::ProcessNonStreamingResponse(const FString& ResponseContent)
{
    UE_LOG(LogLLMClientBase, Log, TEXT("[NON-STREAM] Processing non-streaming response"));
    
    // Track completion tokens to detect API filtering
    int32 CompletionTokensInResponse = 0;
    
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
        CompletionTokensInResponse = CompletionTokens;  // Save for later check
        
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
    
    // ALWAYS extract and display content first, even when tool_calls are present
    // This shows the LLM's reasoning/status message alongside tool execution
    FString Content;
    if ((*MessageObj)->TryGetStringField(TEXT("content"), Content) && !Content.IsEmpty())
    {
        UE_LOG(LogLLMClientBase, Log, TEXT("[NON-STREAM] Content (with tool_calls check pending): %s"), *Content.Left(200));
        
        // Filter tool_call tags from content
        FString CleanContent = FilterToolCallTags(Content);
        
        // Remove any text-based tool_call blocks from displayed content
        // (these will be parsed separately if no JSON tool_calls array exists)
        
        // Filter XML-style </tool_call> tags
        int32 FirstToolCallIndex = CleanContent.Find(TEXT("</tool_call>"));
        if (FirstToolCallIndex > 0)
        {
            CleanContent = CleanContent.Left(FirstToolCallIndex).TrimEnd();
        }
        else if (CleanContent.StartsWith(TEXT("</tool_call>")))
        {
            CleanContent.Empty();
        }
        
        // Filter bracket-style [tool_call: ...] patterns
        // Remove all occurrences of [tool_call: func_name(...)]
        FRegexPattern BracketToolCallPattern(TEXT("\\[tool_call:\\s*[^\\]]+\\]"));
        FRegexMatcher BracketMatcher(BracketToolCallPattern, CleanContent);
        while (BracketMatcher.FindNext())
        {
            // Replace from match start to end with empty string
            int32 MatchStart = BracketMatcher.GetMatchBeginning();
            int32 MatchEnd = BracketMatcher.GetMatchEnding();
            CleanContent = CleanContent.Left(MatchStart) + CleanContent.Mid(MatchEnd);
            // Reset matcher with updated string
            BracketMatcher = FRegexMatcher(BracketToolCallPattern, CleanContent);
        }
        
        // Clean up any extra whitespace/newlines from removed tool calls
        CleanContent = CleanContent.TrimStartAndEnd();
        while (CleanContent.Contains(TEXT("\n\n\n")))
        {
            CleanContent = CleanContent.Replace(TEXT("\n\n\n"), TEXT("\n\n"));
        }
        
        if (!CleanContent.IsEmpty())
        {
            AccumulatedContent = CleanContent;
            if (CurrentOnChunk.IsBound())
            {
                UE_LOG(LogLLMClientBase, Log, TEXT("[NON-STREAM] Displaying content to user: %s"), *CleanContent.Left(200));
                CurrentOnChunk.Execute(CleanContent);
            }
        }
    }
    
    // Check for tool calls in JSON format
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
        return; // Tool calls handled via JSON array, skip text-based parsing
    }
    
    // No JSON tool_calls array - check for text-based tool calls in content
    // First try bracket format: [tool_call: func(args)]
    if (!Content.IsEmpty() && (Content.Contains(TEXT("[tool_call:")) || Content.Contains(TEXT("[Tool call:"))))
    {
        UE_LOG(LogLLMClientBase, Log, TEXT("[NON-STREAM] No JSON tool_calls, checking for bracket-format tool calls..."));
        
        TArray<FMCPToolCall> BracketToolCalls = ParseBracketStyleToolCalls(Content);
        
        if (BracketToolCalls.Num() > 0)
        {
            UE_LOG(LogLLMClientBase, Log, TEXT("[NON-STREAM] Found %d bracket-format tool calls"), BracketToolCalls.Num());
            bToolCallsDetectedInStream = true;
            
            for (const FMCPToolCall& ToolCall : BracketToolCalls)
            {
                if (CurrentOnToolCall.IsBound())
                {
                    CurrentOnToolCall.Execute(ToolCall);
                }
            }
            return; // Tool calls handled via bracket format
        }
    }
    
    // Try Qwen/vLLM function format: <function=name><parameter=key>value</parameter>...</function>
    // This format is used by some models when they don't properly support tool_calls
    if (!Content.IsEmpty() && Content.Contains(TEXT("<function=")))
    {
        UE_LOG(LogLLMClientBase, Log, TEXT("[NON-STREAM] No JSON tool_calls, checking for <function=name> style tool calls..."));
        
        FString WorkContent = Content;
        TArray<FMCPToolCall> ParsedToolCalls;
        int32 ToolCallIndex = 0;
        
        // Pattern: <function=tool_name><parameter=param_name>value</parameter>...</function>
        FRegexPattern FunctionPattern(TEXT("<function=([^>]+)>([\\s\\S]*?)(?:</function>|</tool_call>|$)"));
        FRegexMatcher FunctionMatcher(FunctionPattern, WorkContent);
        
        while (FunctionMatcher.FindNext())
        {
            FString ToolName = FunctionMatcher.GetCaptureGroup(1);
            FString ParametersContent = FunctionMatcher.GetCaptureGroup(2);
            
            // Clean up tool name (might have trailing whitespace or newlines)
            ToolName = ToolName.TrimStartAndEnd();
            
            UE_LOG(LogLLMClientBase, Log, TEXT("[NON-STREAM] Found <function=%s> with content length %d"), *ToolName, ParametersContent.Len());
            
            // Extract parameters: <parameter=key>value</parameter>
            TSharedPtr<FJsonObject> Arguments = MakeShared<FJsonObject>();
            FRegexPattern ParamPattern(TEXT("<parameter=([^>]+)>([\\s\\S]*?)</parameter>"));
            FRegexMatcher ParamMatcher(ParamPattern, ParametersContent);
            
            while (ParamMatcher.FindNext())
            {
                FString ParamName = ParamMatcher.GetCaptureGroup(1);
                FString ParamValue = ParamMatcher.GetCaptureGroup(2);
                
                // Trim whitespace
                ParamName = ParamName.TrimStartAndEnd();
                ParamValue = ParamValue.TrimStartAndEnd();
                
                UE_LOG(LogLLMClientBase, Log, TEXT("[NON-STREAM] Parameter: %s = %s"), *ParamName, *ParamValue.Left(100));
                
                // Try to parse as JSON if it looks like JSON
                if (ParamValue.StartsWith(TEXT("{")) || ParamValue.StartsWith(TEXT("[")))
                {
                    TSharedPtr<FJsonValue> JsonValue;
                    TSharedRef<TJsonReader<>> ParamReader = TJsonReaderFactory<>::Create(ParamValue);
                    if (FJsonSerializer::Deserialize(ParamReader, JsonValue))
                    {
                        Arguments->SetField(ParamName, JsonValue);
                    }
                    else
                    {
                        // Failed to parse as JSON, store as string
                        Arguments->SetStringField(ParamName, ParamValue);
                    }
                }
                else
                {
                    Arguments->SetStringField(ParamName, ParamValue);
                }
            }
            
            // Build the tool call
            FMCPToolCall ToolCall;
            ToolCall.ToolName = ToolName;
            ToolCall.Arguments = Arguments;
            ToolCall.Id = FString::Printf(TEXT("func_call_%d_%lld"), ToolCallIndex, FDateTime::UtcNow().GetTicks());
            
            // Serialize arguments to JSON string
            TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ToolCall.ArgumentsJson);
            FJsonSerializer::Serialize(Arguments.ToSharedRef(), Writer);
            
            UE_LOG(LogLLMClientBase, Log, TEXT("[NON-STREAM] Parsed <function> tool call: %s (id=%s, args=%s)"), 
                *ToolCall.ToolName, *ToolCall.Id, *ToolCall.ArgumentsJson.Left(200));
            
            if (!ToolCall.ToolName.IsEmpty())
            {
                ParsedToolCalls.Add(ToolCall);
            }
            
            ToolCallIndex++;
        }
        
        // Fire all parsed tool calls
        if (ParsedToolCalls.Num() > 0)
        {
            bToolCallsDetectedInStream = true;
            for (const FMCPToolCall& ToolCall : ParsedToolCalls)
            {
                if (CurrentOnToolCall.IsBound())
                {
                    CurrentOnToolCall.Execute(ToolCall);
                }
            }
            return; // Tool calls handled via function format
        }
    }
    
    // Try XML-style format: <tool_call>...</tool_call> or just <tool_call>... (some models don't close)
    if (!Content.IsEmpty() && (Content.Contains(TEXT("<tool_call>")) || Content.Contains(TEXT("</tool_call>"))))
    {
        UE_LOG(LogLLMClientBase, Log, TEXT("[NON-STREAM] No JSON tool_calls, checking for XML-style tool calls in content..."));
        
        // Extract content between <tool_call> tags (or after <tool_call> if no closing tag)
        FString WorkContent = Content;
        TArray<FMCPToolCall> ParsedToolCalls;
        int32 ToolCallIndex = 0;
        
        int32 StartIdx = 0;
        while ((StartIdx = WorkContent.Find(TEXT("<tool_call>"), ESearchCase::IgnoreCase, ESearchDir::FromStart, StartIdx)) != INDEX_NONE)
        {
            // Move past the opening tag
            int32 ContentStart = StartIdx + 11; // Length of "<tool_call>"
            
            // Find the closing tag or end of string
            int32 EndIdx = WorkContent.Find(TEXT("</tool_call>"), ESearchCase::IgnoreCase, ESearchDir::FromStart, ContentStart);
            FString ToolCallContent;
            if (EndIdx != INDEX_NONE)
            {
                ToolCallContent = WorkContent.Mid(ContentStart, EndIdx - ContentStart);
                StartIdx = EndIdx + 12; // Move past closing tag
            }
            else
            {
                // No closing tag - take rest of string
                ToolCallContent = WorkContent.Mid(ContentStart);
                StartIdx = WorkContent.Len(); // End loop
            }
            
            // Trim and look for JSON
            ToolCallContent = ToolCallContent.TrimStartAndEnd();
            
            // Skip if empty
            if (ToolCallContent.IsEmpty())
            {
                continue;
            }
            
            // Find the JSON object in this content
            int32 JsonStart = ToolCallContent.Find(TEXT("{"));
            if (JsonStart == INDEX_NONE)
            {
                continue;
            }
            
            // Find matching closing brace
            int32 BraceCount = 0;
            int32 JsonEnd = JsonStart;
            for (int32 i = JsonStart; i < ToolCallContent.Len(); i++)
            {
                if (ToolCallContent[i] == TEXT('{')) BraceCount++;
                else if (ToolCallContent[i] == TEXT('}')) BraceCount--;
                
                if (BraceCount == 0)
                {
                    JsonEnd = i + 1;
                    break;
                }
            }
            
            FString JsonStr = ToolCallContent.Mid(JsonStart, JsonEnd - JsonStart);
            
            // Parse the JSON
            TSharedPtr<FJsonObject> ToolCallJson;
            TSharedRef<TJsonReader<>> ToolReader = TJsonReaderFactory<>::Create(JsonStr);
            if (!FJsonSerializer::Deserialize(ToolReader, ToolCallJson) || !ToolCallJson.IsValid())
            {
                UE_LOG(LogLLMClientBase, Warning, TEXT("[NON-STREAM] Failed to parse XML-style tool call JSON: %s"), *JsonStr.Left(200));
                continue;
            }
            
            // Extract tool call info
            FMCPToolCall ToolCall;
            ToolCall.ToolName = ToolCallJson->GetStringField(TEXT("name"));
            
            // Get arguments - could be object or string
            const TSharedPtr<FJsonObject>* ArgsObj;
            if (ToolCallJson->TryGetObjectField(TEXT("arguments"), ArgsObj))
            {
                // Arguments is an object, serialize to string
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ToolCall.ArgumentsJson);
                FJsonSerializer::Serialize(ArgsObj->ToSharedRef(), Writer);
                ToolCall.Arguments = *ArgsObj;
            }
            else
            {
                // Arguments might be a string
                ToolCall.ArgumentsJson = ToolCallJson->GetStringField(TEXT("arguments"));
                TSharedRef<TJsonReader<>> ArgReader = TJsonReaderFactory<>::Create(ToolCall.ArgumentsJson);
                FJsonSerializer::Deserialize(ArgReader, ToolCall.Arguments);
            }
            
            // Generate ID since text format doesn't include one
            ToolCall.Id = FString::Printf(TEXT("xml_call_%d_%lld"), ToolCallIndex, FDateTime::UtcNow().GetTicks());
            
            if (!ToolCall.ToolName.IsEmpty())
            {
                UE_LOG(LogLLMClientBase, Log, TEXT("[NON-STREAM] Parsed XML-style tool call: %s (id=%s)"), *ToolCall.ToolName, *ToolCall.Id);
                ParsedToolCalls.Add(ToolCall);
            }
            
            ToolCallIndex++;
        }
        
        // Fire all parsed tool calls
        if (ParsedToolCalls.Num() > 0)
        {
            bToolCallsDetectedInStream = true;
            for (const FMCPToolCall& ToolCall : ParsedToolCalls)
            {
                if (CurrentOnToolCall.IsBound())
                {
                    CurrentOnToolCall.Execute(ToolCall);
                }
            }
            return; // Tool calls handled
        }
    }
    
    // Final check: if no content and no tool calls but completion tokens > threshold,
    // the API is likely filtering the response (e.g., content moderation)
    if (AccumulatedContent.IsEmpty() && !bToolCallsDetectedInStream && CompletionTokensInResponse > 50)
    {
        UE_LOG(LogLLMClientBase, Warning, TEXT("[NON-STREAM] API filtering detected: %d completion tokens consumed but empty response"), CompletionTokensInResponse);
        
        // Generate a synthetic status message explaining the issue
        FString FilteredMsg = FString::Printf(TEXT("⚠️ API response filtered (%d tokens consumed). The model generated content that was filtered by the API. Try rephrasing your request."), CompletionTokensInResponse);
        if (CurrentOnChunk.IsBound())
        {
            CurrentOnChunk.Execute(FilteredMsg);
        }
        AccumulatedContent = FilteredMsg;
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
            UE_LOG(LogLLMClientBase, Error, TEXT("HandleRequestComplete: OnError bound=%s, OnComplete bound=%s"), 
                CurrentOnError.IsBound() ? TEXT("Yes") : TEXT("No"),
                CurrentOnComplete.IsBound() ? TEXT("Yes") : TEXT("No"));
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
        
        // Log raw response to dedicated file for debugging (if file logging enabled)
        if (FChatSession::IsFileLoggingEnabled())
        {
            FString RawLogPath = FPaths::ProjectSavedDir() / TEXT("Logs") / TEXT("VibeUE_RawLLM.log");
            FString ResponseLog = FString::Printf(TEXT("\n========== RESPONSE [%s] ==========\nHTTP %d, Content-Type: %s\n%s\n"),
                *FDateTime::Now().ToString(),
                ResponseCode,
                *ContentType,
                *ResponseContent);
            FFileHelper::SaveStringToFile(ResponseLog, *RawLogPath, FFileHelper::EEncodingOptions::ForceUTF8, &IFileManager::Get(), FILEWRITE_Append);
            UE_LOG(LogLLMClientBase, Log, TEXT("Raw response logged to: %s"), *RawLogPath);
        }
        
        // Check if we have content that needs processing
        if (ResponseContent.Len() > 0)
        {
            // Check if this is non-SSE (JSON) content that wasn't processed in progress callback
            FString TrimmedContent = ResponseContent.TrimStart();
            bool bIsSSEContent = TrimmedContent.StartsWith(TEXT("data: ")) || TrimmedContent.StartsWith(TEXT(":"));
            
            // If we already have data in StreamBuffer, it means SSE processing already happened
            bool bAlreadyProcessedAsStream = StreamBuffer.Len() > 0;
            
            // For non-SSE content, we need to process it here since progress callback deferred it
            if (!bIsSSEContent && !bToolCallsDetectedInStream && !bAlreadyProcessedAsStream)
            {
                UE_LOG(LogLLMClientBase, Log, TEXT("Processing non-streaming JSON response"));
                UE_LOG(LogLLMClientBase, Log, TEXT("Response preview: %s"), *ResponseContent.Left(1000));
                ProcessNonStreamingResponse(ResponseContent);
            }
            else if (StreamBuffer.Len() == 0 && bIsSSEContent)
            {
                // SSE content that wasn't captured by progress callback
                UE_LOG(LogLLMClientBase, Log, TEXT("Processing SSE content that wasn't captured by progress callback"));
                StreamBuffer = ResponseContent;
                ProcessSSEData(ResponseContent);
            }
            else if (bAlreadyProcessedAsStream && ResponseContent.Len() > StreamBuffer.Len())
            {
                // There's more content in ResponseContent than what we processed - final chunk was deferred
                // Extract and process the unprocessed portion
                FString UnprocessedContent = ResponseContent.Right(ResponseContent.Len() - StreamBuffer.Len());
                if (!UnprocessedContent.IsEmpty())
                {
                    UE_LOG(LogLLMClientBase, Log, TEXT("Processing %d chars of deferred SSE content from final chunk"), UnprocessedContent.Len());
                    StreamBuffer = ResponseContent;
                    ProcessSSEData(UnprocessedContent);
                }
            }
        }
    }
    
    // Fire any pending tool calls that weren't fired (e.g., if [DONE] was in a deferred chunk)
    if (PendingToolCalls.Num() > 0)
    {
        UE_LOG(LogLLMClientBase, Log, TEXT("HandleRequestComplete: Firing %d pending tool calls that weren't fired during stream"), PendingToolCalls.Num());
        FirePendingToolCalls();
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
