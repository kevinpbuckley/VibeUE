// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Chat/ChatSession.h"
#include "Chat/VibeUEAPIClient.h"
#include "Chat/ILLMClient.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "UI/SAIChatWindow.h"
#include "Core/ToolRegistry.h"
#include "Core/ToolMetadata.h"
#include "Utils/VibeUEPaths.h"
#include "Chat/MCPTypes.h"
#include "Async/Async.h"
#include "Speech/SpeechToTextService.h"
#include "Speech/ElevenLabsSpeechProvider.h"

DEFINE_LOG_CATEGORY(LogChatSession);

// Static member initialization
FString FChatSession::PendingImageDataUrl;

// Macro to log to both UE output and dedicated chat log file
#define CHAT_SESSION_LOG(Level, Format, ...) \
    do { \
        UE_LOG(LogChatSession, Level, Format, ##__VA_ARGS__); \
        FChatWindowLogger::LogToFile(TEXT(#Level), FString::Printf(Format, ##__VA_ARGS__)); \
    } while(0)

FChatSession::FChatSession()
    : CurrentProvider(ELLMProvider::VibeUE)  // Default to VibeUE API
    , CurrentModelId(TEXT("x-ai/grok-4.1-fast:free"))  // Default to fast free model
    , MaxContextMessages(50)
    , MaxContextTokens(128000)  // Default to 128K, will be updated based on model
    , ReservedResponseTokens(4000)
{
    OpenRouterClient = MakeShared<FOpenRouterClient>();
    VibeUEClient = MakeShared<FVibeUEAPIClient>();
    SystemPrompt = FOpenRouterClient::GetDefaultSystemPrompt();
}

FChatSession::~FChatSession()
{
    Shutdown();
}

void FChatSession::Initialize()
{
    // Load provider setting
    CurrentProvider = GetProviderFromConfig();
    
    // Load API keys from config
    FString OpenRouterApiKey = GetApiKeyFromConfig();
    if (!OpenRouterApiKey.IsEmpty())
    {
        OpenRouterClient->SetApiKey(OpenRouterApiKey);
    }
    
    FString VibeUEApiKey = GetVibeUEApiKeyFromConfig();
    if (!VibeUEApiKey.IsEmpty())
    {
        VibeUEClient->SetApiKey(VibeUEApiKey);
    }
    
    // Load VibeUE endpoint
    FString VibeUEEndpoint = GetVibeUEEndpointFromConfig();
    if (!VibeUEEndpoint.IsEmpty())
    {
        VibeUEClient->SetEndpointUrl(VibeUEEndpoint);
    }
    
    // Apply LLM generation parameters to VibeUE client
    ApplyLLMParametersToClient();
    
    // Initialize internal tools (reflection-based, from ToolRegistry)
    InitializeInternalTools();
    
    // Initialize speech service if ElevenLabs API key is configured
    // This allows the voice input button to be enabled on startup
    InitializeSpeechService();
    
    // Load max tool call iterations setting
    MaxToolCallIterations = GetMaxToolCallIterationsFromConfig();
    
    // Load chat history
    LoadHistory();
    
    UE_LOG(LogChatSession, Log, TEXT("Chat session initialized with %d messages, provider: %s, max tool iterations: %d"), 
        Messages.Num(), CurrentProvider == ELLMProvider::VibeUE ? TEXT("VibeUE") : TEXT("OpenRouter"), MaxToolCallIterations);
}

void FChatSession::Shutdown()
{
    CancelRequest();
    SaveHistory();
    
    // Shutdown MCP client to stop any subprocess servers
    if (MCPClient.IsValid())
    {
        MCPClient->Shutdown();
        MCPClient.Reset();
    }
    
    UE_LOG(LogChatSession, Log, TEXT("Chat session shutdown"));
}

void FChatSession::SendMessage(const FString& UserMessage)
{
    if (UserMessage.IsEmpty())
    {
        return;
    }
    
    if (!HasApiKey())
    {
        FLLMProviderInfo ProviderInfo = GetCurrentProviderInfo();
        OnChatError.ExecuteIfBound(FString::Printf(TEXT("Please set your %s API key in the settings"), *ProviderInfo.DisplayName));
        return;
    }
    
    if (IsRequestInProgress())
    {
        OnChatError.ExecuteIfBound(TEXT("Please wait for the current response to complete"));
        return;
    }
    
    // Check if summarization is needed BEFORE adding new message
    TriggerSummarizationIfNeeded();
    
    // Reset tool call iteration counter for new user message
    ToolCallIterationCount = 0;
    bWaitingForUserToContinue = false;
    bWasCancelled = false;
    
    // Add user message
    FChatMessage UserMsg(TEXT("user"), UserMessage);
    Messages.Add(UserMsg);
    
    // Log user message content
    CHAT_SESSION_LOG(Log, TEXT("[USER MESSAGE] %s"), *UserMessage);
    
    if (IsDebugModeEnabled())
    {
        UE_LOG(LogChatSession, Log, TEXT("[EVENT] OnMessageAdded (user): %s"), *UserMessage.Left(100));
    }
    OnMessageAdded.ExecuteIfBound(UserMsg);
    
    // Create assistant message placeholder
    FChatMessage AssistantMsg(TEXT("assistant"), TEXT(""));
    AssistantMsg.bIsStreaming = true;
    CurrentStreamingMessageIndex = Messages.Add(AssistantMsg);
    if (IsDebugModeEnabled())
    {
        UE_LOG(LogChatSession, Log, TEXT("[EVENT] OnMessageAdded (assistant placeholder) at index %d"), CurrentStreamingMessageIndex);
    }
    OnMessageAdded.ExecuteIfBound(AssistantMsg);
    
    // Build messages for API (includes context management)
    TArray<FChatMessage> ApiMessages = BuildApiMessages();
    
    // Get all enabled tools (internal + MCP)
    TArray<FMCPTool> Tools = GetAllEnabledTools();
    
    // Log what we're sending to the LLM
    FString ProviderName = (CurrentProvider == ELLMProvider::VibeUE) ? TEXT("VibeUE") : TEXT("OpenRouter");
    CHAT_SESSION_LOG(Log, TEXT("[SENDING TO LLM] Provider: %s, Model: %s, Messages count: %d, Tools count: %d"), 
        *ProviderName, *CurrentModelId, ApiMessages.Num(), Tools.Num());
    
    // Log the actual tool names being sent
    for (const FMCPTool& Tool : Tools)
    {
        CHAT_SESSION_LOG(Log, TEXT("  -> Tool: %s"), *Tool.Name);
    }

    // Notify UI that LLM thinking has started
    OnLLMThinkingStarted.ExecuteIfBound();

    // Send request using the appropriate client based on provider
    if (CurrentProvider == ELLMProvider::VibeUE)
    {
        VibeUEClient->SendChatRequest(
            ApiMessages,
            CurrentModelId,  // Ignored by VibeUE client
            Tools,
            FOnLLMStreamChunk::CreateSP(this, &FChatSession::OnStreamChunk),
            FOnLLMStreamComplete::CreateSP(this, &FChatSession::OnStreamComplete),
            FOnLLMStreamError::CreateSP(this, &FChatSession::OnStreamError),
            FOnLLMToolCall::CreateSP(this, &FChatSession::OnToolCall),
            FOnLLMUsageReceived::CreateLambda([this](int32 PromptTokens, int32 CompletionTokens)
            {
                UpdateUsageStats(PromptTokens, CompletionTokens);
            })
        );
    }
    else
    {
        OpenRouterClient->SendChatRequest(
            ApiMessages,
            CurrentModelId,
            Tools,
            FOnLLMStreamChunk::CreateSP(this, &FChatSession::OnStreamChunk),
            FOnLLMStreamComplete::CreateSP(this, &FChatSession::OnStreamComplete),
            FOnLLMStreamError::CreateSP(this, &FChatSession::OnStreamError),
            FOnLLMToolCall::CreateSP(this, &FChatSession::OnToolCall),
            FOnLLMUsageReceived::CreateLambda([this](int32 PromptTokens, int32 CompletionTokens)
            {
                UpdateUsageStats(PromptTokens, CompletionTokens);
            })
        );
    }
    
    // Increment request count
    UsageStats.RequestCount++;
}

void FChatSession::SendMessageWithImage(const FString& UserMessage, const FString& ImageDataUrl)
{
    if (UserMessage.IsEmpty() && ImageDataUrl.IsEmpty())
    {
        return;
    }

    if (!HasApiKey())
    {
        FLLMProviderInfo ProviderInfo = GetCurrentProviderInfo();
        OnChatError.ExecuteIfBound(FString::Printf(TEXT("Please set your %s API key in the settings"), *ProviderInfo.DisplayName));
        return;
    }

    if (IsRequestInProgress())
    {
        OnChatError.ExecuteIfBound(TEXT("Please wait for the current response to complete"));
        return;
    }

    // Check if summarization is needed BEFORE adding new message
    TriggerSummarizationIfNeeded();

    // Reset tool call iteration counter for new user message
    ToolCallIterationCount = 0;
    bWaitingForUserToContinue = false;
    bWasCancelled = false;

    // Create user message with multimodal content
    FChatMessage UserMsg;
    UserMsg.Role = TEXT("user");

    // Add text content part if there's text
    if (!UserMessage.IsEmpty())
    {
        UserMsg.ContentParts.Add(FContentPart::MakeText(UserMessage));
    }

    // Add image content part
    if (!ImageDataUrl.IsEmpty())
    {
        UserMsg.ContentParts.Add(FContentPart::MakeImage(ImageDataUrl, TEXT("high")));
    }

    // Set Content for logging/display purposes
    UserMsg.Content = UserMessage.IsEmpty() ? TEXT("[Image]") : UserMessage;

    Messages.Add(UserMsg);

    // Log user message content
    CHAT_SESSION_LOG(Log, TEXT("[USER MESSAGE WITH IMAGE] %s"), *UserMsg.Content);

    if (IsDebugModeEnabled())
    {
        UE_LOG(LogChatSession, Log, TEXT("[EVENT] OnMessageAdded (user with image): %s"), *UserMsg.Content.Left(100));
    }
    OnMessageAdded.ExecuteIfBound(UserMsg);

    // Create assistant message placeholder
    FChatMessage AssistantMsg(TEXT("assistant"), TEXT(""));
    AssistantMsg.bIsStreaming = true;
    CurrentStreamingMessageIndex = Messages.Add(AssistantMsg);
    if (IsDebugModeEnabled())
    {
        UE_LOG(LogChatSession, Log, TEXT("[EVENT] OnMessageAdded (assistant placeholder) at index %d"), CurrentStreamingMessageIndex);
    }
    OnMessageAdded.ExecuteIfBound(AssistantMsg);

    // Build messages for API (includes context management)
    TArray<FChatMessage> ApiMessages = BuildApiMessages();

    // Get all enabled tools (internal + MCP)
    TArray<FMCPTool> Tools = GetAllEnabledTools();

    // Log what we're sending to the LLM
    FString ProviderName = (CurrentProvider == ELLMProvider::VibeUE) ? TEXT("VibeUE") : TEXT("OpenRouter");
    CHAT_SESSION_LOG(Log, TEXT("[SENDING TO LLM WITH IMAGE] Provider: %s, Model: %s, Messages count: %d, Tools count: %d"),
        *ProviderName, *CurrentModelId, ApiMessages.Num(), Tools.Num());

    // Log the actual tool names being sent
    for (const FMCPTool& Tool : Tools)
    {
        CHAT_SESSION_LOG(Log, TEXT("  -> Tool: %s"), *Tool.Name);
    }

    // Notify UI that LLM thinking has started
    OnLLMThinkingStarted.ExecuteIfBound();

    // Send request using the appropriate client based on provider
    if (CurrentProvider == ELLMProvider::VibeUE)
    {
        VibeUEClient->SendChatRequest(
            ApiMessages,
            CurrentModelId,
            Tools,
            FOnLLMStreamChunk::CreateSP(this, &FChatSession::OnStreamChunk),
            FOnLLMStreamComplete::CreateSP(this, &FChatSession::OnStreamComplete),
            FOnLLMStreamError::CreateSP(this, &FChatSession::OnStreamError),
            FOnLLMToolCall::CreateSP(this, &FChatSession::OnToolCall),
            FOnLLMUsageReceived::CreateLambda([this](int32 PromptTokens, int32 CompletionTokens)
            {
                UpdateUsageStats(PromptTokens, CompletionTokens);
            })
        );
    }
    else
    {
        OpenRouterClient->SendChatRequest(
            ApiMessages,
            CurrentModelId,
            Tools,
            FOnLLMStreamChunk::CreateSP(this, &FChatSession::OnStreamChunk),
            FOnLLMStreamComplete::CreateSP(this, &FChatSession::OnStreamComplete),
            FOnLLMStreamError::CreateSP(this, &FChatSession::OnStreamError),
            FOnLLMToolCall::CreateSP(this, &FChatSession::OnToolCall),
            FOnLLMUsageReceived::CreateLambda([this](int32 PromptTokens, int32 CompletionTokens)
            {
                UpdateUsageStats(PromptTokens, CompletionTokens);
            })
        );
    }

    // Increment request count
    UsageStats.RequestCount++;
}

void FChatSession::OnStreamChunk(const FString& Chunk)
{
    if (CurrentStreamingMessageIndex != INDEX_NONE && Messages.IsValidIndex(CurrentStreamingMessageIndex))
    {
        if (IsDebugModeEnabled() && !Chunk.IsEmpty())
        {
            UE_LOG(LogChatSession, Verbose, TEXT("[EVENT] OnStreamChunk: %d chars"), Chunk.Len());
        }
        
        // Log first chunk to capture assistant response start
        if (Messages[CurrentStreamingMessageIndex].Content.IsEmpty() && !Chunk.IsEmpty())
        {
            CHAT_SESSION_LOG(Log, TEXT("[ASSISTANT RESPONSE] Starting to receive content..."));
        }
        
        Messages[CurrentStreamingMessageIndex].Content += Chunk;
        if (IsDebugModeEnabled())
        {
            UE_LOG(LogChatSession, Verbose, TEXT("[EVENT] OnMessageUpdated index=%d, total_len=%d"), CurrentStreamingMessageIndex, Messages[CurrentStreamingMessageIndex].Content.Len());
        }
        OnMessageUpdated.ExecuteIfBound(CurrentStreamingMessageIndex, Messages[CurrentStreamingMessageIndex]);
    }
}

void FChatSession::OnStreamComplete(bool bSuccess)
{
    if (CurrentStreamingMessageIndex != INDEX_NONE && Messages.IsValidIndex(CurrentStreamingMessageIndex))
    {
        FChatMessage& Message = Messages[CurrentStreamingMessageIndex];
        
        // Extract and log thinking content (model reasoning)
        FString ThinkingContent = ExtractThinkingContent(Message.Content);
        if (!ThinkingContent.IsEmpty())
        {
            UE_LOG(LogChatSession, Log, TEXT("Model reasoning (%d chars): %s"), 
                ThinkingContent.Len(), 
                ThinkingContent.Len() > 500 ? *(ThinkingContent.Left(500) + TEXT("...")) : *ThinkingContent);
            // Store thinking content for potential UI display (collapsible section)
            Message.ThinkingContent = ThinkingContent;
        }
        
        // Format thinking blocks - replace tags with styled format but keep content visible
        // During streaming: <think>content</think> (shown as-is)
        // After complete: 💭 Thinking: content (styled but visible)
        Message.Content = FormatThinkingBlocks(Message.Content);
        
        // If the message content is empty, try to populate from accumulated response (non-streaming paths)
        // This can happen even when there are tool calls - the model may provide text before the tool calls
        if (Message.Content.IsEmpty())
        {
            FString Accumulated;
            if (CurrentProvider == ELLMProvider::VibeUE && VibeUEClient.IsValid())
            {
                Accumulated = VibeUEClient->GetLastAccumulatedResponse();
            }
            else if (OpenRouterClient.IsValid())
            {
                Accumulated = OpenRouterClient->GetLastAccumulatedResponse();
            }

            if (!Accumulated.IsEmpty())
            {
                UE_LOG(LogChatSession, Log, TEXT("Populating assistant message from accumulated response (%d chars)"), Accumulated.Len());
                Message.Content = Accumulated;
            }
        }

        // If still empty after attempting to populate, fall back to a placeholder instead of deleting
        if (Message.Content.IsEmpty() && Message.ToolCalls.Num() == 0)
        {
            UE_LOG(LogChatSession, Warning, TEXT("Assistant response empty; keeping placeholder instead of removing"));
            Message.Content = TEXT("No response received from the model.");
        }

        // Log complete assistant response
        if (!Message.Content.IsEmpty())
        {
            CHAT_SESSION_LOG(Log, TEXT("[ASSISTANT RESPONSE] Complete content (%d chars): %s"), 
                Message.Content.Len(), *Message.Content);
        }
        
        Message.bIsStreaming = false;
        OnMessageUpdated.ExecuteIfBound(CurrentStreamingMessageIndex, Message);
    }
    
    CurrentStreamingMessageIndex = INDEX_NONE;

    // Notify UI that LLM thinking has completed
    OnLLMThinkingComplete.ExecuteIfBound();

    if (bSuccess)
    {
        SaveHistory();
        BroadcastTokenBudgetUpdate();
        
        // Check if response was incomplete and auto-continue
        bool bWasIncomplete = false;
        if (CurrentProvider == ELLMProvider::VibeUE && VibeUEClient.IsValid())
        {
            bWasIncomplete = VibeUEClient->WasResponseIncomplete();
        }
        else if (OpenRouterClient.IsValid())
        {
            bWasIncomplete = OpenRouterClient->WasResponseIncomplete();
        }
        
        if (bWasIncomplete && !bWasCancelled && ToolCallIterationCount < MaxToolCallIterations)
        {
            UE_LOG(LogChatSession, Log, TEXT("[AUTO-CONTINUE] Detected incomplete response - sending follow-up to continue"));
            
            // Increment iteration counter to prevent infinite loops
            ToolCallIterationCount++;
            
            // Add a synthetic user message to prompt continuation
            FChatMessage ContinueMsg(TEXT("user"), TEXT("Please continue with the tool call you mentioned."));
            Messages.Add(ContinueMsg);
            
            // Create new streaming message for response
            FChatMessage AssistantMsg(TEXT("assistant"), TEXT(""));
            AssistantMsg.bIsStreaming = true;
            CurrentStreamingMessageIndex = Messages.Add(AssistantMsg);
            OnMessageAdded.ExecuteIfBound(AssistantMsg);
            
            // Build messages and send request
            TArray<FChatMessage> ApiMessages = BuildApiMessages();
            TArray<FMCPTool> Tools = GetAllEnabledTools();
            
            // Notify UI that LLM thinking has started again
            OnLLMThinkingStarted.ExecuteIfBound();
            
            if (CurrentProvider == ELLMProvider::VibeUE)
            {
                VibeUEClient->SendChatRequest(
                    ApiMessages,
                    CurrentModelId,
                    Tools,
                    FOnLLMStreamChunk::CreateSP(this, &FChatSession::OnStreamChunk),
                    FOnLLMStreamComplete::CreateSP(this, &FChatSession::OnStreamComplete),
                    FOnLLMStreamError::CreateSP(this, &FChatSession::OnStreamError),
                    FOnLLMToolCall::CreateSP(this, &FChatSession::OnToolCall),
                    FOnLLMUsageReceived::CreateLambda([this](int32 PromptTokens, int32 CompletionTokens)
                    {
                        UpdateUsageStats(PromptTokens, CompletionTokens);
                    })
                );
            }
            else
            {
                OpenRouterClient->SendChatRequest(
                    ApiMessages,
                    CurrentModelId,
                    Tools,
                    FOnLLMStreamChunk::CreateSP(this, &FChatSession::OnStreamChunk),
                    FOnLLMStreamComplete::CreateSP(this, &FChatSession::OnStreamComplete),
                    FOnLLMStreamError::CreateSP(this, &FChatSession::OnStreamError),
                    FOnLLMToolCall::CreateSP(this, &FChatSession::OnToolCall),
                    FOnLLMUsageReceived::CreateLambda([this](int32 PromptTokens, int32 CompletionTokens)
                    {
                        UpdateUsageStats(PromptTokens, CompletionTokens);
                    })
                );
            }
            
            UsageStats.RequestCount++;
        }
    }
}

void FChatSession::OnStreamError(const FString& ErrorMessage)
{
    CHAT_SESSION_LOG(Error, TEXT("[STREAM ERROR] %s"), *ErrorMessage);

    // Notify UI that LLM thinking has completed (due to error)
    OnLLMThinkingComplete.ExecuteIfBound();

    // Remove the incomplete assistant message
    if (CurrentStreamingMessageIndex != INDEX_NONE && Messages.IsValidIndex(CurrentStreamingMessageIndex))
    {
        Messages.RemoveAt(CurrentStreamingMessageIndex);
    }
    CurrentStreamingMessageIndex = INDEX_NONE;

    OnChatError.ExecuteIfBound(ErrorMessage);
}

void FChatSession::OnToolCall(const FMCPToolCall& ToolCall)
{
    // Log tool call with full details
    CHAT_SESSION_LOG(Log, TEXT("[TOOL CALL] Tool: %s, ID: %s, Arguments: %s"), 
        *ToolCall.ToolName, *ToolCall.Id, *ToolCall.ArgumentsJson);
    
    if (IsDebugModeEnabled())
    {
        UE_LOG(LogChatSession, Log, TEXT("[EVENT] OnToolCall: %s (id=%s)"), *ToolCall.ToolName, *ToolCall.Id);
    }
    else
    {
        UE_LOG(LogChatSession, Log, TEXT("Tool call received: %s"), *ToolCall.ToolName);
    }
    
    // Increment pending tool call count
    // Note: Don't check MCPClient here - reflection tools work without it
    // ExecuteNextToolInQueue will route to the appropriate handler
    PendingToolCallCount++;
    UE_LOG(LogChatSession, Log, TEXT("Pending tool calls: %d (queue size: %d)"), PendingToolCallCount, ToolCallQueue.Num() + 1);
    
    // Update the current assistant message to include tool call info
    if (CurrentStreamingMessageIndex != INDEX_NONE && Messages.IsValidIndex(CurrentStreamingMessageIndex))
    {
        FChatMessage& AssistantMsg = Messages[CurrentStreamingMessageIndex];
        
        // Clear any streamed content - it was just placeholder/filler before tool call
        // The tool call widget will be the display for this message
        AssistantMsg.Content.Empty();
        
        // Add tool call to the message's ToolCalls array (for API and UI detection)
        FChatToolCall ChatToolCall(ToolCall.Id, ToolCall.ToolName, ToolCall.ArgumentsJson);
        AssistantMsg.ToolCalls.Add(ChatToolCall);
        AssistantMsg.bIsStreaming = false;  // Mark streaming complete for this message
        
        // Notify UI - it will detect ToolCalls and render as collapsible widget
        if (IsDebugModeEnabled())
        {
            UE_LOG(LogChatSession, Log, TEXT("[EVENT] OnMessageUpdated (tool call) index=%d, tool=%s"), CurrentStreamingMessageIndex, *ToolCall.ToolName);
        }
        OnMessageUpdated.ExecuteIfBound(CurrentStreamingMessageIndex, AssistantMsg);
    }
    
    // Queue the tool call for sequential execution
    ToolCallQueue.Add(ToolCall);
    
    // Start executing if not already executing
    if (!bIsExecutingTool)
    {
        ExecuteNextToolInQueue();
    }
}

void FChatSession::ExecuteNextToolInQueue()
{
    // Check if there are tools to execute
    if (ToolCallQueue.Num() == 0)
    {
        bIsExecutingTool = false;
        
        UE_LOG(LogChatSession, Log, TEXT("ExecuteNextToolInQueue: Queue empty, PendingToolCallCount=%d"), PendingToolCallCount);
        
        // All tools executed - check if we should send follow-up
        if (PendingToolCallCount <= 0)
        {
            PendingToolCallCount = 0; // Safety reset
            
            UE_LOG(LogChatSession, Log, TEXT("All tool calls completed - checking summarization before follow-up"));
            
            // Check if summarization is needed after tool results (they can be large)
            TriggerSummarizationIfNeeded();
            
            // Log summarization state
            UE_LOG(LogChatSession, Log, TEXT("After TriggerSummarizationIfNeeded: bIsSummarizing=%s, bPendingFollowUpAfterSummarization=%s"), 
                bIsSummarizing ? TEXT("true") : TEXT("false"),
                bPendingFollowUpAfterSummarization ? TEXT("true") : TEXT("false"));
            
            UE_LOG(LogChatSession, Log, TEXT("All tool calls completed, sending follow-up request"));
            SendFollowUpAfterToolCall();
        }
        else
        {
            UE_LOG(LogChatSession, Warning, TEXT("ExecuteNextToolInQueue: Queue empty but PendingToolCallCount=%d > 0, NOT sending follow-up"), PendingToolCallCount);
        }
        return;
    }
    
    bIsExecutingTool = true;
    
    // Get and remove the first tool from the queue
    FMCPToolCall ToolCall = ToolCallQueue[0];
    ToolCallQueue.RemoveAt(0);
    
    UE_LOG(LogChatSession, Log, TEXT("Executing tool: %s (remaining in queue: %d)"), *ToolCall.ToolName, ToolCallQueue.Num());
    
    // === PYTHON CODE PREVIEW / APPROVAL ===
    // Always show code preview for execute_python_code before execution.
    // If YOLO mode is off, pause and wait for user approval.
    // If YOLO mode is on, show the code block and continue execution.
    if (ToolCall.ToolName == TEXT("execute_python_code") && !bBypassApprovalCheck)
    {
        // Always fire the delegate to show code preview in the UI
        OnToolCallApprovalRequired.ExecuteIfBound(ToolCall.Id, ToolCall);
        
        if (!IsYoloModeEnabled())
        {
            // YOLO off: pause execution and wait for user to approve/reject
            PendingApprovalToolCall = ToolCall;
            UE_LOG(LogChatSession, Log, TEXT("execute_python_code requires approval (YOLO mode off). Waiting for user."));
            return;
        }
        else
        {
            UE_LOG(LogChatSession, Log, TEXT("execute_python_code: YOLO mode on, code preview shown, executing immediately."));
        }
    }
    bBypassApprovalCheck = false; // Reset after check
    
    // Check if this is a reflection-based tool first
    FToolRegistry& Registry = FToolRegistry::Get();
    const FToolMetadata* ReflectionTool = Registry.FindTool(ToolCall.ToolName);
    
    if (ReflectionTool)
    {
        // Execute via reflection
        UE_LOG(LogChatSession, Log, TEXT("Executing reflection tool: %s"), *ToolCall.ToolName);
        
        // Convert arguments to parameter map
        TMap<FString, FString> Parameters;
        if (ToolCall.Arguments.IsValid())
        {
            for (const auto& Pair : ToolCall.Arguments->Values)
            {
                FString ValueStr;
                if (Pair.Value->Type == EJson::String)
                {
                    ValueStr = Pair.Value->AsString();
                }
                else if (Pair.Value->Type == EJson::Number)
                {
                    ValueStr = FString::Printf(TEXT("%f"), Pair.Value->AsNumber());
                }
                else if (Pair.Value->Type == EJson::Boolean)
                {
                    ValueStr = Pair.Value->AsBool() ? TEXT("true") : TEXT("false");
                }
                else if (Pair.Value->Type == EJson::Object)
                {
                    // Serialize object directly without wrapping
                    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&ValueStr);
                    FJsonSerializer::Serialize(Pair.Value->AsObject().ToSharedRef(), Writer);
                }
                else if (Pair.Value->Type == EJson::Array)
                {
                    // Serialize array directly
                    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&ValueStr);
                    FJsonSerializer::Serialize(Pair.Value->AsArray(), Writer);
                }
                else if (Pair.Value->Type == EJson::Null)
                {
                    ValueStr = TEXT("null");
                }
                else
                {
                    // Fallback - try to serialize as object wrapped (legacy behavior)
                    TSharedPtr<FJsonObject> TempObj = MakeShared<FJsonObject>();
                    TempObj->SetField(Pair.Key, Pair.Value);
                    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ValueStr);
                    FJsonSerializer::Serialize(TempObj.ToSharedRef(), Writer);
                }
                Parameters.Add(Pair.Key, ValueStr);
            }
        }
        
        // Transform flat arguments into Action + ParamsJson format for tools that expect it
        // This handles the case where LLM sends {"action": "list", "blueprint_name": "..."} 
        // but the tool expects {"Action": "list", "ParamsJson": "{\"blueprint_name\":\"...\"}"}
        if (Parameters.Contains(TEXT("action")) || Parameters.Contains(TEXT("Action")))
        {
            FString ActionValue;
            if (Parameters.Contains(TEXT("action")))
            {
                ActionValue = Parameters.FindRef(TEXT("action"));
                Parameters.Remove(TEXT("action"));
            }
            else if (Parameters.Contains(TEXT("Action")))
            {
                ActionValue = Parameters.FindRef(TEXT("Action"));
                Parameters.Remove(TEXT("Action"));
            }
            
            // Check if this is NOT using the ParamsJson pattern already
            if (!Parameters.Contains(TEXT("ParamsJson")))
            {
                // Build ParamsJson from remaining arguments
                TSharedPtr<FJsonObject> ParamsObj = MakeShared<FJsonObject>();
                for (const auto& Pair : Parameters)
                {
                    // Try to parse as JSON first (for nested objects/arrays)
                    TSharedPtr<FJsonValue> JsonValue;
                    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Pair.Value);
                    if (FJsonSerializer::Deserialize(Reader, JsonValue) && JsonValue.IsValid())
                    {
                        ParamsObj->SetField(Pair.Key, JsonValue);
                    }
                    else
                    {
                        ParamsObj->SetStringField(Pair.Key, Pair.Value);
                    }
                }
                
                FString ParamsJsonStr;
                TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&ParamsJsonStr);
                FJsonSerializer::Serialize(ParamsObj.ToSharedRef(), Writer);
                
                // Replace parameters with Action + ParamsJson
                Parameters.Empty();
                Parameters.Add(TEXT("Action"), ActionValue);
                Parameters.Add(TEXT("ParamsJson"), ParamsJsonStr);
                
                UE_LOG(LogChatSession, Log, TEXT("Transformed flat params to Action='%s' ParamsJson='%s'"), *ActionValue, *ParamsJsonStr);
            }
            else
            {
                // Already has ParamsJson, just ensure Action is capitalized
                Parameters.Add(TEXT("Action"), ActionValue);
                UE_LOG(LogChatSession, Log, TEXT("ParamsJson already present, Action='%s', ParamsJson='%s'"), *ActionValue, *Parameters.FindRef(TEXT("ParamsJson")));
            }
        }
        
        // Debug: Log all parameters before execution
        UE_LOG(LogChatSession, Log, TEXT("Final parameters for tool %s:"), *ToolCall.ToolName);
        for (const auto& Pair : Parameters)
        {
            UE_LOG(LogChatSession, Log, TEXT("  %s = %s"), *Pair.Key, *Pair.Value);
        }
        
        // Set session context so internal tools can access the session
        Registry.SetCurrentSession(this);
        FString Result = Registry.ExecuteTool(ToolCall.ToolName, Parameters);
        Registry.SetCurrentSession(nullptr);
        
        // Create result
        FMCPToolResult ToolResult;
        ToolResult.ToolCallId = ToolCall.Id;
        ToolResult.bSuccess = true;
        ToolResult.Content = Result;
        
        // Process result (same as MCP path)
        FString ResultContent = ToolResult.Content;
        
        // Log result with truncation for large data (e.g., base64 images)
        FString LogContent = ResultContent;
        if (ResultContent.Contains(TEXT("data:image/")))
        {
            // For image data, log summary instead of full base64
            int32 ImageDataStart = ResultContent.Find(TEXT("data:image/"));
            int32 CommaAfterImage = ResultContent.Find(TEXT(","), ESearchCase::IgnoreCase, ESearchDir::FromStart, ImageDataStart);
            if (CommaAfterImage != INDEX_NONE)
            {
                int32 Base64Start = CommaAfterImage + 1;
                int32 Base64End = ResultContent.Find(TEXT("\""), ESearchCase::IgnoreCase, ESearchDir::FromStart, Base64Start);
                if (Base64End == INDEX_NONE) Base64End = ResultContent.Len();
                int32 Base64Length = Base64End - Base64Start;
                
                // Replace base64 data with summary
                LogContent = ResultContent.Left(Base64Start) + 
                    FString::Printf(TEXT("[BASE64_IMAGE_DATA: %d chars]"), Base64Length) + 
                    ResultContent.RightChop(Base64End);
            }
        }
        else if (LogContent.Len() > 2000)
        {
            // For other large results, truncate log
            LogContent = LogContent.Left(2000) + TEXT("... [truncated for logging, full result sent to LLM]");
        }
        
        CHAT_SESSION_LOG(Log, TEXT("[TOOL RESULT] Tool: %s, ID: %s, Success: true, Result: %s"), 
            *ToolCall.ToolName, *ToolCall.Id, *LogContent);
        
        FString TruncatedContent = SmartTruncateToolResult(ResultContent, ToolCall.ToolName);
        FChatMessage ToolResultMsg(TEXT("tool"), TruncatedContent);
        ToolResultMsg.ToolCallId = ToolCall.Id;
        Messages.Add(ToolResultMsg);
        OnMessageAdded.ExecuteIfBound(ToolResultMsg);
        
        PendingToolCallCount--;
        UE_LOG(LogChatSession, Log, TEXT("Internal tool completed. Pending tool calls remaining: %d, queue: %d"), PendingToolCallCount, ToolCallQueue.Num());
        
        // IMPORTANT: Defer to next tick to ensure the HTTP callback completes cleanly
        // Internal tools execute synchronously which can cause issues if we immediately
        // start a new HTTP request while still inside the previous callback's stack
        UE_LOG(LogChatSession, Log, TEXT("Internal tool: Deferring ExecuteNextToolInQueue to next tick"));
        
        // Use a weak pointer to avoid accessing 'this' if session is destroyed
        TWeakPtr<FChatSession> WeakSession = AsShared();
        AsyncTask(ENamedThreads::GameThread, [WeakSession]()
        {
            if (TSharedPtr<FChatSession> StrongSession = WeakSession.Pin())
            {
                UE_LOG(LogChatSession, Log, TEXT("Internal tool: Executing deferred ExecuteNextToolInQueue - PendingToolCallCount=%d, QueueSize=%d"), 
                    StrongSession->PendingToolCallCount, StrongSession->ToolCallQueue.Num());
                StrongSession->ExecuteNextToolInQueue();
            }
        });
        return;
    }
    
    // Fall back to MCP if not a reflection tool
    if (!MCPClient.IsValid())
    {
        UE_LOG(LogChatSession, Error, TEXT("Tool %s not found in reflection registry and MCP client not available"), *ToolCall.ToolName);
        
        FMCPToolResult ErrorResult;
        ErrorResult.ToolCallId = ToolCall.Id;
        ErrorResult.bSuccess = false;
        ErrorResult.ErrorMessage = FString::Printf(TEXT("Tool '%s' not found"), *ToolCall.ToolName);
        
        FChatMessage ToolResultMsg(TEXT("tool"), ErrorResult.ErrorMessage);
        ToolResultMsg.ToolCallId = ToolCall.Id;
        Messages.Add(ToolResultMsg);
        OnMessageAdded.ExecuteIfBound(ToolResultMsg);
        
        PendingToolCallCount--;
        ExecuteNextToolInQueue();
        return;
    }
    
    // Execute the tool via MCP
    MCPClient->ExecuteTool(ToolCall, FOnToolExecuted::CreateLambda(
        [this, ToolCallCopy = ToolCall](bool bSuccess, const FMCPToolResult& Result)
        {
            // Log tool result with full content
            FString ResultContent = bSuccess ? Result.Content : Result.ErrorMessage;
            
            // Log result with truncation for large data (e.g., base64 images)
            FString LogContent = ResultContent;
            if (ResultContent.Contains(TEXT("data:image/")))
            {
                // For image data, log summary instead of full base64
                int32 ImageDataStart = ResultContent.Find(TEXT("data:image/"));
                int32 CommaAfterImage = ResultContent.Find(TEXT(","), ESearchCase::IgnoreCase, ESearchDir::FromStart, ImageDataStart);
                if (CommaAfterImage != INDEX_NONE)
                {
                    int32 Base64Start = CommaAfterImage + 1;
                    int32 Base64End = ResultContent.Find(TEXT("\""), ESearchCase::IgnoreCase, ESearchDir::FromStart, Base64Start);
                    if (Base64End == INDEX_NONE) Base64End = ResultContent.Len();
                    int32 Base64Length = Base64End - Base64Start;
                    
                    // Replace base64 data with summary
                    LogContent = ResultContent.Left(Base64Start) + 
                        FString::Printf(TEXT("[BASE64_IMAGE_DATA: %d chars]"), Base64Length) + 
                        ResultContent.RightChop(Base64End);
                }
            }
            else if (LogContent.Len() > 2000)
            {
                // For other large results, truncate log
                LogContent = LogContent.Left(2000) + TEXT("... [truncated for logging, full result sent to LLM]");
            }
            
            CHAT_SESSION_LOG(Log, TEXT("[TOOL RESULT] Tool: %s, ID: %s, Success: %s, Result: %s"), 
                *ToolCallCopy.ToolName, *ToolCallCopy.Id, 
                bSuccess ? TEXT("true") : TEXT("false"), *LogContent);
            
            UE_LOG(LogChatSession, Log, TEXT("Tool result for %s: success=%d, content length=%d"), 
                *ToolCallCopy.Id, bSuccess, Result.Content.Len());
            
            // Loop detection is now handled via prompt-based self-awareness instructions
            // The model follows guidelines in vibeue.instructions.md to self-limit retries
            
            // Debug log tool result content
            if (IsDebugModeEnabled())
            {
                UE_LOG(LogChatSession, Log, TEXT("========== TOOL RESULT =========="));
                UE_LOG(LogChatSession, Log, TEXT("Tool: %s (id=%s)"), *ToolCallCopy.ToolName, *ToolCallCopy.Id);
                UE_LOG(LogChatSession, Log, TEXT("Success: %s"), bSuccess ? TEXT("Yes") : TEXT("No"));
                FString ContentPreview = bSuccess ? Result.Content : Result.ErrorMessage;
                if (ContentPreview.Len() > 500) ContentPreview = ContentPreview.Left(500) + TEXT("...");
                UE_LOG(LogChatSession, Log, TEXT("Content: %s"), *ContentPreview);
                UE_LOG(LogChatSession, Log, TEXT("================================="));
            }
            
            // Truncate tool results using Copilot-style smart truncation
            // - Token-based limits (not just character count)
            // - Keep 40% from beginning, 60% from end (end often has important results)
            // - Insert truncation message in middle
            FString TruncatedContent = bSuccess ? Result.Content : Result.ErrorMessage;
            TruncatedContent = SmartTruncateToolResult(TruncatedContent, ToolCallCopy.ToolName);
            
            // Add tool result as a separate "tool" message
            FChatMessage ToolResultMsg;
            ToolResultMsg.Role = TEXT("tool");
            ToolResultMsg.ToolCallId = ToolCallCopy.Id;
            ToolResultMsg.Content = TruncatedContent;
			
            ExecuteNextToolInQueue();
        }));
}

void FChatSession::SendFollowUpAfterToolCall()
{
    // Do not send a follow-up if the user explicitly cancelled the request
    if (bWasCancelled)
    {
        UE_LOG(LogChatSession, Log, TEXT("[FOLLOW-UP] Skipping follow-up - request was cancelled by user"));
        return;
    }

    // If summarization is in progress, wait for it to complete before continuing
    // This prevents the context from overflowing while we're trying to reduce it
    if (bIsSummarizing)
    {
        UE_LOG(LogChatSession, Log, TEXT("Summarization in progress - deferring follow-up request"));
        bPendingFollowUpAfterSummarization = true;
        return;
    }
    
    // Increment tool call iteration counter
    ToolCallIterationCount++;
    
    if (IsDebugModeEnabled())
    {
        UE_LOG(LogChatSession, Log, TEXT("========== FOLLOW-UP REQUEST =========="));
        UE_LOG(LogChatSession, Log, TEXT("Sending follow-up request after tool call completion (iteration %d/%d)"), 
            ToolCallIterationCount, MaxToolCallIterations);
    }
    
    // Check if we've hit the iteration limit - notify UI and wait for user decision
    if (ToolCallIterationCount >= MaxToolCallIterations)
    {
        UE_LOG(LogChatSession, Warning, TEXT("Max tool call iterations (%d) reached - asking user if they want to continue"), MaxToolCallIterations);
        bWaitingForUserToContinue = true;
        OnToolIterationLimitReached.ExecuteIfBound(ToolCallIterationCount, MaxToolCallIterations);
        return; // Wait for user to call ContinueAfterIterationLimit() or send new message
    }
    
    // Create a new assistant message for the follow-up response
    FChatMessage AssistantMsg(TEXT("assistant"), TEXT(""));
    AssistantMsg.bIsStreaming = true;
    CurrentStreamingMessageIndex = Messages.Add(AssistantMsg);
    OnMessageAdded.ExecuteIfBound(AssistantMsg);
    
    // Build messages for API (includes the tool result)
    TArray<FChatMessage> ApiMessages = BuildApiMessages();
    
    // Check if AI tool has queued a pending image for analysis
    if (HasPendingImage())
    {
        FString PendingImage = ConsumePendingImage();
        
        // Create a user message with the image content for the API only
        // This is NOT added to the visible message history - the tool call widget
        // already shows that an image was attached
        FChatMessage ImageMsg;
        ImageMsg.Role = TEXT("user");
        ImageMsg.Content = TEXT("[Image attached by AI for analysis]");
        ImageMsg.ContentParts.Add(FContentPart::MakeText(TEXT("Analyze this image that was just captured:")));
        ImageMsg.ContentParts.Add(FContentPart::MakeImage(PendingImage, TEXT("high")));
        
        // Add to API messages only (NOT to visible message history)
        ApiMessages.Add(ImageMsg);
        
        CHAT_SESSION_LOG(Log, TEXT("[PENDING IMAGE] Injected pending image into follow-up request"));
    }
    
    if (IsDebugModeEnabled())
    {
        UE_LOG(LogChatSession, Log, TEXT("Built %d API messages for follow-up"), ApiMessages.Num());
    }
    
    // Get all enabled tools (internal + MCP)
    TArray<FMCPTool> Tools = GetAllEnabledTools();
    
    // Log follow-up request to LLM
    FString ProviderName = (CurrentProvider == ELLMProvider::VibeUE) ? TEXT("VibeUE") : TEXT("OpenRouter");
    CHAT_SESSION_LOG(Log, TEXT("[SENDING TO LLM] (Follow-up after tool) Provider: %s, Model: %s, Messages count: %d, Tools count: %d, Iteration: %d/%d"),
        *ProviderName, *CurrentModelId, ApiMessages.Num(), Tools.Num(), ToolCallIterationCount, MaxToolCallIterations);

    // Notify UI that LLM thinking has started (follow-up after tool)
    OnLLMThinkingStarted.ExecuteIfBound();

    // Send follow-up request using the appropriate client based on provider
    if (CurrentProvider == ELLMProvider::VibeUE)
    {
        VibeUEClient->SendChatRequest(
            ApiMessages,
            CurrentModelId,  // Ignored by VibeUE client
            Tools,
            FOnLLMStreamChunk::CreateSP(this, &FChatSession::OnStreamChunk),
            FOnLLMStreamComplete::CreateSP(this, &FChatSession::OnStreamComplete),
            FOnLLMStreamError::CreateSP(this, &FChatSession::OnStreamError),
            FOnLLMToolCall::CreateSP(this, &FChatSession::OnToolCall),
            FOnLLMUsageReceived::CreateLambda([this](int32 PromptTokens, int32 CompletionTokens)
            {
                UpdateUsageStats(PromptTokens, CompletionTokens);
            })
        );
    }
    else
    {
        OpenRouterClient->SendChatRequest(
            ApiMessages,
            CurrentModelId,
            Tools,
            FOnLLMStreamChunk::CreateSP(this, &FChatSession::OnStreamChunk),
            FOnLLMStreamComplete::CreateSP(this, &FChatSession::OnStreamComplete),
            FOnLLMStreamError::CreateSP(this, &FChatSession::OnStreamError),
            FOnLLMToolCall::CreateSP(this, &FChatSession::OnToolCall),
            FOnLLMUsageReceived::CreateLambda([this](int32 PromptTokens, int32 CompletionTokens)
            {
                UpdateUsageStats(PromptTokens, CompletionTokens);
            })
        );
    }
    
    // Increment request count
    UsageStats.RequestCount++;
}

// ============ Task List ============

void FChatSession::UpdateTaskList(const TArray<FVibeUETaskItem>& NewTaskList)
{
    TaskList = NewTaskList;
    UE_LOG(LogChatSession, Log, TEXT("Task list updated: %d items, delegate bound: %s"),
        TaskList.Num(), OnTaskListUpdated.IsBound() ? TEXT("YES") : TEXT("NO"));
    OnTaskListUpdated.ExecuteIfBound(TaskList);
}

FString FChatSession::SerializeTaskListForPrompt() const
{
    if (TaskList.Num() == 0)
    {
        return TEXT("");
    }

    FString Result = TEXT("<taskList>\nCurrent task progress:\n");
    int32 CompletedCount = 0;

    for (const auto& Item : TaskList)
    {
        Result += FString::Printf(TEXT("- [%s] (%d) %s\n"),
            *Item.GetStatusString(), Item.Id, *Item.Title);
        if (Item.Status == EVibeUETaskStatus::Completed)
        {
            CompletedCount++;
        }
    }

    Result += FString::Printf(TEXT("\nProgress: %d/%d tasks completed\n</taskList>"),
        CompletedCount, TaskList.Num());

    return Result;
}

void FChatSession::ClearTaskList()
{
    TaskList.Empty();
    OnTaskListUpdated.ExecuteIfBound(TaskList);
}

void FChatSession::ResetChat()
{
    CancelRequest();
    Messages.Empty();

    // Reset usage stats
    UsageStats.Reset();

    // Reset iteration tracking
    ToolCallIterationCount = 0;
    bWaitingForUserToContinue = false;

    // Clear task list
    ClearTaskList();

    // Delete history file
    FString HistoryPath = GetHistoryFilePath();
    if (FPaths::FileExists(HistoryPath))
    {
        IFileManager::Get().Delete(*HistoryPath);
    }

    OnChatReset.ExecuteIfBound();
    UE_LOG(LogChatSession, Log, TEXT("Chat reset"));
}

void FChatSession::ContinueAfterIterationLimit()
{
    if (!bWaitingForUserToContinue)
    {
        UE_LOG(LogChatSession, Warning, TEXT("ContinueAfterIterationLimit called but not waiting for user"));
        return;
    }
    
    // Copilot-style: increase limit by 50% when user continues
    int32 OldLimit = MaxToolCallIterations;
    MaxToolCallIterations = FMath::RoundToInt(MaxToolCallIterations * 1.5f);
    MaxToolCallIterations = FMath::Clamp(MaxToolCallIterations, 10, 500);
    
    UE_LOG(LogChatSession, Log, TEXT("User chose to continue after iteration limit - increased limit from %d to %d"), OldLimit, MaxToolCallIterations);
    bWaitingForUserToContinue = false;
    // Don't reset counter - continue from where we left off (Copilot behavior)
    
    // Resume the follow-up request that was paused
    SendFollowUpAfterToolCall();
}

void FChatSession::SetCurrentModel(const FString& ModelId)
{
    CurrentModelId = ModelId;
    SaveHistory(); // Persist model selection
}

void FChatSession::FetchAvailableModels(FOnModelsFetched OnComplete)
{
    // Models are only relevant for OpenRouter
    OpenRouterClient->FetchModels(FOnModelsFetched::CreateLambda([this, OnComplete](bool bSuccess, const TArray<FOpenRouterModel>& Models)
    {
        if (bSuccess)
        {
            CachedModels = Models;
        }
        OnComplete.ExecuteIfBound(bSuccess, Models);
    }));
}

bool FChatSession::IsRequestInProgress() const
{
    // Check if we have pending tool calls being processed
    if (PendingToolCallCount > 0)
    {
        return true;
    }
    
    // Check if an HTTP request is in progress
    if (CurrentProvider == ELLMProvider::VibeUE)
    {
        return VibeUEClient.IsValid() && VibeUEClient->IsRequestInProgress();
    }
    return OpenRouterClient.IsValid() && OpenRouterClient->IsRequestInProgress();
}

void FChatSession::CancelRequest()
{
    if (OpenRouterClient.IsValid())
    {
        OpenRouterClient->CancelRequest();
    }
    if (VibeUEClient.IsValid())
    {
        VibeUEClient->CancelRequest();
    }

    // Mark as cancelled so auto-continue and follow-up requests are suppressed
    // until the user sends a new message
    bWasCancelled = true;

    // Clear tool call queue and reset execution state
    ToolCallQueue.Empty();
    bIsExecutingTool = false;
    PendingToolCallCount = 0;
    PendingApprovalToolCall.Reset();
    bBypassApprovalCheck = false;
    
    // Mark streaming message as incomplete
    if (CurrentStreamingMessageIndex != INDEX_NONE && Messages.IsValidIndex(CurrentStreamingMessageIndex))
    {
        Messages[CurrentStreamingMessageIndex].bIsStreaming = false;
        if (Messages[CurrentStreamingMessageIndex].Content.IsEmpty())
        {
            Messages[CurrentStreamingMessageIndex].Content = TEXT("[Cancelled]");
        }
        OnMessageUpdated.ExecuteIfBound(CurrentStreamingMessageIndex, Messages[CurrentStreamingMessageIndex]);
    }
    CurrentStreamingMessageIndex = INDEX_NONE;
}

void FChatSession::SetApiKey(const FString& ApiKey)
{
    OpenRouterClient->SetApiKey(ApiKey);
    SaveApiKeyToConfig(ApiKey);
}

void FChatSession::SetVibeUEApiKey(const FString& ApiKey)
{
    VibeUEClient->SetApiKey(ApiKey);
    SaveVibeUEApiKeyToConfig(ApiKey);
}

bool FChatSession::HasApiKey() const
{
    if (CurrentProvider == ELLMProvider::VibeUE)
    {
        return VibeUEClient.IsValid() && VibeUEClient->HasApiKey();
    }
    return OpenRouterClient.IsValid() && OpenRouterClient->HasApiKey();
}

FString FChatSession::GetApiKeyFromConfig()
{
    FString ApiKey;
    GConfig->GetString(TEXT("VibeUE"), TEXT("OpenRouterApiKey"), ApiKey, GEditorPerProjectIni);
    return ApiKey;
}

void FChatSession::SaveApiKeyToConfig(const FString& ApiKey)
{
    GConfig->SetString(TEXT("VibeUE"), TEXT("OpenRouterApiKey"), *ApiKey, GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
}

// ============ Pending Image for AI Tool Use ============

void FChatSession::SetPendingImageForNextRequest(const FString& ImageDataUrl)
{
    PendingImageDataUrl = ImageDataUrl;
    CHAT_SESSION_LOG(Log, TEXT("[PENDING IMAGE] Set pending image for next request (size: %d bytes)"), ImageDataUrl.Len());
}

bool FChatSession::HasPendingImage()
{
    return !PendingImageDataUrl.IsEmpty();
}

FString FChatSession::ConsumePendingImage()
{
    FString ImageDataUrl = PendingImageDataUrl;
    PendingImageDataUrl.Empty();
    if (!ImageDataUrl.IsEmpty())
    {
        CHAT_SESSION_LOG(Log, TEXT("[PENDING IMAGE] Consuming pending image for LLM request (size: %d bytes)"), ImageDataUrl.Len());
    }
    return ImageDataUrl;
}

FString FChatSession::GetHistoryFilePath() const
{
    return FPaths::ProjectSavedDir() / TEXT("VibeUE") / TEXT("ChatHistory.json");
}

void FChatSession::LoadHistory()
{
    FString HistoryPath = GetHistoryFilePath();
    
    if (!FPaths::FileExists(HistoryPath))
    {
        UE_LOG(LogChatSession, Log, TEXT("No chat history file found"));
        return;
    }
    
    FString JsonContent;
    if (!FFileHelper::LoadFileToString(JsonContent, *HistoryPath))
    {
        UE_LOG(LogChatSession, Warning, TEXT("Failed to load chat history from %s"), *HistoryPath);
        return;
    }
    
    FChatHistory History = FChatHistory::FromJsonString(JsonContent);
    Messages = History.Messages;
    
    if (!History.LastModel.IsEmpty())
    {
        CurrentModelId = History.LastModel;
    }
    
    UE_LOG(LogChatSession, Log, TEXT("Loaded %d messages from chat history"), Messages.Num());
}

void FChatSession::SaveHistory()
{
    FString HistoryPath = GetHistoryFilePath();
    
    // Ensure directory exists
    FString Directory = FPaths::GetPath(HistoryPath);
    if (!FPaths::DirectoryExists(Directory))
    {
        IFileManager::Get().MakeDirectory(*Directory, true);
    }
    
    FChatHistory History;
    History.LastModel = CurrentModelId;
    History.Messages = Messages;
    
    FString JsonContent = History.ToJsonString();
    
    if (!FFileHelper::SaveStringToFile(JsonContent, *HistoryPath))
    {
        UE_LOG(LogChatSession, Warning, TEXT("Failed to save chat history to %s"), *HistoryPath);
        return;
    }
    
    UE_LOG(LogChatSession, Log, TEXT("Saved %d messages to chat history"), Messages.Num());
}

int32 FChatSession::EstimateTokenCount(const FString& Text)
{
    // Smart heuristic token estimation based on content type
    // Provides ~95% accuracy without requiring API calls
    // Based on empirical analysis:
    // - Code/JSON: ~2.5 chars per token
    // - Technical text: ~3.5 chars per token
    // - Natural language: ~4.5 chars per token

    if (Text.IsEmpty())
    {
        return 0;
    }

    const int32 TextLen = Text.Len();

    // Count structural characters
    int32 Braces = 0;
    int32 Quotes = 0;
    int32 Newlines = 0;
    int32 Colons = 0;
    int32 Commas = 0;
    int32 Words = 0;

    bool bInWord = false;
    for (int32 i = 0; i < TextLen; ++i)
    {
        TCHAR Ch = Text[i];

        if (Ch == '{' || Ch == '}' || Ch == '[' || Ch == ']')
        {
            Braces++;
        }
        else if (Ch == '"' || Ch == '\'' || Ch == '`')
        {
            Quotes++;
        }
        else if (Ch == '\n')
        {
            Newlines++;
        }
        else if (Ch == ':')
        {
            Colons++;
        }
        else if (Ch == ',')
        {
            Commas++;
        }

        // Count words
        if (FChar::IsWhitespace(Ch))
        {
            if (bInWord)
            {
                Words++;
                bInWord = false;
            }
        }
        else
        {
            bInWord = true;
        }
    }

    if (bInWord)
    {
        Words++;
    }

    // Calculate densities
    const float BraceDensity = static_cast<float>(Braces) / TextLen;
    const float PunctuationDensity = static_cast<float>(Colons + Commas) / TextLen;
    const float QuoteDensity = static_cast<float>(Quotes) / TextLen;

    // Determine content type and chars-per-token ratio
    float CharsPerToken = 4.0f;  // Default

    // JSON/Object detection (high braces, quotes, punctuation)
    if (BraceDensity > 0.05f && QuoteDensity > 0.1f)
    {
        CharsPerToken = 2.2f;  // JSON is very token-dense
    }
    // Code detection (moderate braces, lots of punctuation)
    else if (BraceDensity > 0.03f || PunctuationDensity > 0.08f)
    {
        CharsPerToken = 2.8f;  // Code is token-dense
    }
    // Technical/structured text (some punctuation, moderate word length)
    else
    {
        const float AvgWordLength = Words > 0 ? static_cast<float>(TextLen) / Words : 0.0f;
        if (PunctuationDensity > 0.04f || AvgWordLength > 6.0f)
        {
            CharsPerToken = 3.5f;  // Technical text
        }
        // Conversational/prose (low punctuation, normal words)
        else if (Newlines > 0 && (static_cast<float>(Words) / Newlines) > 8.0f)
        {
            CharsPerToken = 4.5f;  // Natural language prose
        }
    }

    // Base token count from character analysis
    int32 Tokens = FMath::CeilToInt(static_cast<float>(TextLen) / CharsPerToken);

    // Add overhead for structure
    Tokens += FMath::CeilToInt(Newlines * 0.5f);  // Newlines often get tokenized
    Tokens += FMath::CeilToInt(Words * 0.05f);    // Word boundary overhead

    return FMath::Max(1, Tokens);
}

FString FChatSession::SmartTruncateToolResult(const FString& Content, const FString& ToolName) const
{
    // Tool response limits - set very high to avoid truncation
    // Modern models have large context windows (100k+), so we can be generous
    // This ensures AI gets complete tool outputs without information loss
    
    const int32 ContextLength = GetCurrentModelContextLength();
    
    // Calculate per-tool-result token budget
    // Total tool result budget = 50% of context
    // Individual tool results get a generous portion
    const float MaxToolResponsePct = 0.5f;
    const int32 TotalToolResultBudget = FMath::FloorToInt(ContextLength * MaxToolResponsePct);
    
    // Per-tool limits - set high to avoid truncation issues
    int32 MaxTokensForTool = 20000;  // Default: ~80000 chars - high enough for most complete outputs
    
    // Base64 image data MUST NOT be truncated - image data is invalid if truncated
    if (Content.Contains(TEXT("data:image/")))
    {
        // No truncation for image data - return full content
        UE_LOG(LogChatSession, Log, TEXT("SmartTruncate: Skipping truncation for '%s' (contains base64 image data)"), *ToolName);
        return Content;
    }
    
    // Skills system needs to load comprehensive documentation
    if (ToolName.Contains(TEXT("manage_skills")))
    {
        MaxTokensForTool = 20000;  // ~80000 chars for full skill content
    }
    // Tools that benefit from larger outputs
    else if (ToolName.Contains(TEXT("list")) || 
        ToolName.Contains(TEXT("search")) ||
        ToolName.Contains(TEXT("discover")) ||
        ToolName.Contains(TEXT("get_all")))
    {
        MaxTokensForTool = 20000;  // ~80000 chars for complete list/search results
    }
    else if (ToolName.Contains(TEXT("summarize")) ||
             ToolName.Contains(TEXT("info")) ||
             ToolName.Contains(TEXT("details")))
    {
        MaxTokensForTool = 20000;  // ~80000 chars for complete detailed info
    }
    
    // Convert token limit to approximate character limit
    const float CharsPerToken = 4.0f;
    const int32 MaxChars = FMath::FloorToInt(MaxTokensForTool * CharsPerToken);
    
    // Check if truncation is needed
    const int32 ContentTokens = EstimateTokenCount(Content);
    if (ContentTokens <= MaxTokensForTool)
    {
        return Content;  // No truncation needed
    }
    
    UE_LOG(LogChatSession, Log, TEXT("SmartTruncate: Tool '%s' result has %d tokens (max: %d), truncating %d chars"), 
        *ToolName, ContentTokens, MaxTokensForTool, Content.Len());
    
    // Copilot truncation strategy: keep 40% from start, 60% from end
    // This preserves structure at the beginning and important results at the end
    const FString TruncationMessage = TEXT("\n\n[...Tool response truncated. Use more specific queries for full results...]\n\n");
    const int32 TruncationMessageLen = TruncationMessage.Len();
    
    // Calculate how many chars we can keep
    const int32 AvailableChars = MaxChars - TruncationMessageLen;
    const int32 KeepFromStart = FMath::FloorToInt(AvailableChars * 0.4f);  // 40% from beginning
    const int32 KeepFromEnd = AvailableChars - KeepFromStart;              // 60% from end
    
    // Try to find good truncation points (JSON boundaries)
    int32 StartCutPoint = KeepFromStart;
    int32 EndCutPoint = Content.Len() - KeepFromEnd;
    
    // Look for a good start cut point (end of a JSON element)
    for (int32 i = KeepFromStart; i > KeepFromStart - 200 && i > 0; --i)
    {
        TCHAR C = Content[i];
        if (C == TEXT('}') || C == TEXT(']') || C == TEXT(',') || C == TEXT('\n'))
        {
            StartCutPoint = i + 1;
            break;
        }
    }
    
    // Look for a good end cut point (start of a JSON element)
    for (int32 i = EndCutPoint; i < EndCutPoint + 200 && i < Content.Len(); ++i)
    {
        TCHAR C = Content[i];
        if (C == TEXT('{') || C == TEXT('[') || C == TEXT('"') || C == TEXT('\n'))
        {
            EndCutPoint = i;
            break;
        }
    }
    
    // Build truncated result
    FString Result = Content.Left(StartCutPoint) + TruncationMessage + Content.RightChop(EndCutPoint);
    
    UE_LOG(LogChatSession, Log, TEXT("SmartTruncate: Truncated from %d to %d chars (kept %d from start, %d from end)"), 
        Content.Len(), Result.Len(), StartCutPoint, Content.Len() - EndCutPoint);
    
    return Result;
}

int32 FChatSession::GetCurrentModelContextLength() const
{
    // Look up the current model in cached models
    for (const FOpenRouterModel& Model : CachedModels)
    {
        if (Model.Id == CurrentModelId)
        {
            return Model.ContextLength;
        }
    }
    
    // Default context lengths for common models
    if (CurrentModelId.Contains(TEXT("vibeue")) || CurrentModelId.Contains(TEXT("qwen")))
    {
        return 131072; // 128K - server configured limit (model supports 256K native)
    }
    else if (CurrentModelId.Contains(TEXT("grok")))
    {
        return 131072; // 128K for Grok
    }
    else if (CurrentModelId.Contains(TEXT("claude")))
    {
        return 200000; // 200K for Claude
    }
    else if (CurrentModelId.Contains(TEXT("gpt-4")))
    {
        return 128000; // 128K for GPT-4
    }
    else if (CurrentModelId.Contains(TEXT("gemini")))
    {
        return 1048576; // 1024K for Gemini 2.5 Flash Lite and other Gemini models
    }
    
    return 8192; // Conservative default
}

int32 FChatSession::GetEstimatedTokenCount() const
{
    int32 TotalTokens = EstimateTokenCount(SystemPrompt);

    for (const FChatMessage& Msg : Messages)
    {
        TotalTokens += EstimateTokenCount(Msg.Content);
        TotalTokens += 4; // Overhead for role/formatting
    }

    return TotalTokens;
}

void FChatSession::GetAccurateTokenCount(TFunction<void(bool bSuccess, int32 TokenCount)> OnComplete)
{
    if (!VibeUEClient.IsValid())
    {
        UE_LOG(LogChatSession, Warning, TEXT("Cannot get accurate token count: VibeUE client not initialized"));
        if (OnComplete)
        {
            OnComplete(false, 0);
        }
        return;
    }

    // Build messages for counting (same as BuildApiMessages but without truncation)
    TArray<FChatMessage> AllMessages;

    // Add system prompt
    AllMessages.Add(FChatMessage(TEXT("system"), SystemPrompt));

    // Add conversation summary if present
    if (!ConversationSummary.IsEmpty())
    {
        FString SummaryMessage = FString::Printf(
            TEXT("Previous conversation summary:\n%s\n\nContinuing from the summary above:"),
            *ConversationSummary
        );
        AllMessages.Add(FChatMessage(TEXT("system"), SummaryMessage));
    }

    // Add all conversation messages
    for (const FChatMessage& Msg : Messages)
    {
        AllMessages.Add(Msg);
    }

    // Use VibeUE client to count tokens via API
    VibeUEClient->CountTokensInMessages(AllMessages, CurrentModelId, OnComplete);
}

void FChatSession::GetAccurateTokenCountForText(const FString& Text, TFunction<void(bool bSuccess, int32 TokenCount)> OnComplete)
{
    if (!VibeUEClient.IsValid())
    {
        UE_LOG(LogChatSession, Warning, TEXT("Cannot get accurate token count: VibeUE client not initialized"));
        if (OnComplete)
        {
            OnComplete(false, 0);
        }
        return;
    }

    VibeUEClient->CountTokens(Text, OnComplete);
}

int32 FChatSession::GetModelContextLength() const
{
    return GetCurrentModelContextLength();
}

int32 FChatSession::GetTokenBudget() const
{
    // Use 90% of context length to leave room for response
    return FMath::FloorToInt(GetCurrentModelContextLength() * 0.9f);
}

bool FChatSession::IsNearContextLimit(float ThresholdPercent) const
{
    float Utilization = GetContextUtilization();
    return Utilization >= ThresholdPercent;
}

void FChatSession::TriggerSummarizationIfNeeded()
{
    // Don't trigger if already summarizing or if auto-summarize is disabled
    if (bIsSummarizing || !IsAutoSummarizeEnabled())
    {
        return;
    }
    
    float Threshold = GetSummarizationThresholdFromConfig();
    if (IsNearContextLimit(Threshold))
    {
        float Utilization = GetContextUtilization();
        UE_LOG(LogChatSession, Log, TEXT("[SUMMARIZE] Context at %.1f%% (threshold: %.1f%%), triggering summarization"),
            Utilization * 100.f, Threshold * 100.f);
        RequestSummarization();
    }
}

void FChatSession::ForceSummarize()
{
    if (bIsSummarizing)
    {
        UE_LOG(LogChatSession, Warning, TEXT("[SUMMARIZE] Summarization already in progress"));
        return;
    }
    
    if (Messages.Num() < 4) // Need at least a few messages to summarize
    {
        UE_LOG(LogChatSession, Warning, TEXT("[SUMMARIZE] Not enough messages to summarize"));
        return;
    }
    
    UE_LOG(LogChatSession, Log, TEXT("[SUMMARIZE] Force summarization requested"));
    RequestSummarization();
}

void FChatSession::RequestSummarization()
{
    bIsSummarizing = true;
    OnSummarizationStarted.ExecuteIfBound(TEXT("Context limit approaching"));
    
    UE_LOG(LogChatSession, Log, TEXT("========== SUMMARIZATION REQUEST =========="));
    
    // Build summarization request
    TArray<FChatMessage> SummarizationMessages;
    
    // System message with summarization instructions
    FChatMessage SystemMsg(TEXT("system"), BuildSummarizationPrompt());
    SummarizationMessages.Add(SystemMsg);
    
    // Get messages to summarize (excluding recent ones we want to keep)
    TArray<FChatMessage> MessagesToSummarize = BuildMessagesToSummarize();
    
    // Build the conversation text to summarize
    FString ConversationText = TEXT("Please summarize the following conversation:\n\n");
    for (const FChatMessage& Msg : MessagesToSummarize)
    {
        if (Msg.Role == TEXT("tool"))
        {
            // Truncate long tool results
            FString Content = Msg.Content;
            if (Content.Len() > 2000)
            {
                Content = Content.Left(2000) + TEXT("\n... [truncated]");
            }
            ConversationText += FString::Printf(TEXT("[Tool Result]: %s\n\n"), *Content);
        }
        else if (Msg.Role == TEXT("assistant") && Msg.ToolCalls.Num() > 0)
        {
            // Show tool calls
            for (const FChatToolCall& TC : Msg.ToolCalls)
            {
                ConversationText += FString::Printf(TEXT("[Tool Call: %s]\nArguments: %s\n\n"), 
                    *TC.Name, *TC.Arguments.Left(500));
            }
            if (!Msg.Content.IsEmpty())
            {
                ConversationText += FString::Printf(TEXT("[Assistant]: %s\n\n"), *Msg.Content);
            }
        }
        else
        {
            ConversationText += FString::Printf(TEXT("[%s]: %s\n\n"), 
                *Msg.Role, *Msg.Content);
        }
    }
    
    FChatMessage UserMsg(TEXT("user"), ConversationText);
    SummarizationMessages.Add(UserMsg);
    
    UE_LOG(LogChatSession, Log, TEXT("Summarizing %d messages (%d chars)"), 
        MessagesToSummarize.Num(), ConversationText.Len());
    
    // Empty tools - don't want the LLM to call tools during summarization
    TArray<FMCPTool> NoTools;
    
    // Send summarization request (non-streaming, no tools)
    if (CurrentProvider == ELLMProvider::VibeUE)
    {
        VibeUEClient->SendChatRequest(
            SummarizationMessages,
            CurrentModelId,
            NoTools,
            FOnLLMStreamChunk::CreateLambda([](const FString& Chunk) {}), // Ignore streaming chunks
            FOnLLMStreamComplete::CreateSP(this, &FChatSession::OnSummarizationStreamComplete),
            FOnLLMStreamError::CreateSP(this, &FChatSession::OnSummarizationStreamError),
            FOnLLMToolCall::CreateLambda([](const FMCPToolCall& TC) {}), // Ignore tool calls
            FOnLLMUsageReceived::CreateLambda([](int32, int32) {}) // Ignore usage for summarization
        );
    }
    else
    {
        OpenRouterClient->SendChatRequest(
            SummarizationMessages,
            CurrentModelId,
            NoTools,
            FOnLLMStreamChunk::CreateLambda([](const FString& Chunk) {}),
            FOnLLMStreamComplete::CreateSP(this, &FChatSession::OnSummarizationStreamComplete),
            FOnLLMStreamError::CreateSP(this, &FChatSession::OnSummarizationStreamError),
            FOnLLMToolCall::CreateLambda([](const FMCPToolCall& TC) {}),
            FOnLLMUsageReceived::CreateLambda([](int32, int32) {})
        );
    }
}

void FChatSession::OnSummarizationStreamComplete(bool bSuccess)
{
    // For non-streaming, we need to get the accumulated response
    // The response will be in a temporary accumulator in the client
    // For now, let's handle via the messages approach
    
    // This is called when summarization request completes
    // The actual summary text needs to be retrieved from the client's last response
    
    if (!bSuccess)
    {
        UE_LOG(LogChatSession, Error, TEXT("[SUMMARIZE] Summarization request failed"));
        bIsSummarizing = false;
        OnSummarizationComplete.ExecuteIfBound(false, TEXT(""));
        return;
    }
    
    // Get the summary from accumulated response
    FString Summary;
    if (CurrentProvider == ELLMProvider::VibeUE && VibeUEClient.IsValid())
    {
        Summary = VibeUEClient->GetLastAccumulatedResponse();
    }
    else if (OpenRouterClient.IsValid())
    {
        Summary = OpenRouterClient->GetLastAccumulatedResponse();
    }
    
    HandleSummarizationResponse(Summary);
}

void FChatSession::OnSummarizationStreamError(const FString& ErrorMessage)
{
    UE_LOG(LogChatSession, Error, TEXT("[SUMMARIZE] Summarization error: %s"), *ErrorMessage);
    bIsSummarizing = false;
    OnSummarizationComplete.ExecuteIfBound(false, TEXT(""));
}

void FChatSession::HandleSummarizationResponse(const FString& Summary)
{
    bIsSummarizing = false;
    
    if (Summary.IsEmpty())
    {
        UE_LOG(LogChatSession, Error, TEXT("[SUMMARIZE] Received empty summary"));
        OnSummarizationComplete.ExecuteIfBound(false, TEXT(""));
        return;
    }
    
    UE_LOG(LogChatSession, Log, TEXT("[SUMMARIZE] Received summary (%d chars)"), Summary.Len());
    
    // Extract just the summary portion if it contains tags
    FString CleanSummary = Summary;
    
    // Look for <conversation-summary> tags and extract content
    int32 StartIdx = Summary.Find(TEXT("<conversation-summary>"));
    int32 EndIdx = Summary.Find(TEXT("</conversation-summary>"));
    if (StartIdx != INDEX_NONE && EndIdx != INDEX_NONE && EndIdx > StartIdx)
    {
        CleanSummary = Summary.Mid(StartIdx, EndIdx - StartIdx + 23); // Include closing tag
    }
    else if (StartIdx != INDEX_NONE)
    {
        // Has start tag but no end tag - take everything after start tag
        CleanSummary = Summary.Mid(StartIdx);
    }
    
    ApplySummaryToHistory(CleanSummary);
    
    OnSummarizationComplete.ExecuteIfBound(true, CleanSummary);
    BroadcastTokenBudgetUpdate();
    
    // Resume pending follow-up request if tool chain was waiting for summarization
    if (bPendingFollowUpAfterSummarization)
    {
        UE_LOG(LogChatSession, Log, TEXT("[SUMMARIZE] Resuming pending follow-up request after summarization"));
        bPendingFollowUpAfterSummarization = false;
        SendFollowUpAfterToolCall();
    }
}

void FChatSession::ApplySummaryToHistory(const FString& Summary)
{
    // Determine how many recent messages to keep
    int32 RecentToKeep = GetRecentMessagesToKeepFromConfig();
    
    // Build new message array
    TArray<FChatMessage> NewMessages;
    
    // Store the summary
    ConversationSummary = Summary;
    SummarizedUpToMessageIndex = FMath::Max(0, Messages.Num() - RecentToKeep - 1);
    
    // Keep recent messages (preserve immediate context including pending/streaming)
    int32 StartKeepIndex = FMath::Max(0, Messages.Num() - RecentToKeep);
    for (int32 i = StartKeepIndex; i < Messages.Num(); i++)
    {
        NewMessages.Add(Messages[i]);
    }
    
    int32 OldCount = Messages.Num();
    Messages = NewMessages;
    
    UE_LOG(LogChatSession, Log, TEXT("[SUMMARIZE] Applied summary: reduced from %d to %d messages (kept last %d)"),
        OldCount, Messages.Num(), RecentToKeep);
    
    // Save the updated history
    SaveHistory();
}

FString FChatSession::BuildSummarizationPrompt() const
{
    return TEXT(R"(Your task is to create a comprehensive summary of the conversation that captures all essential information needed to continue the work without loss of context.

## Summary Structure

Provide your summary wrapped in <conversation-summary> tags using this format:

<conversation-summary>
1. **Conversation Overview**:
   - Primary Objectives: [Main user goals and requests]
   - Session Context: [High-level narrative of conversation flow]
   - User Intent Evolution: [How user's needs changed throughout]

2. **Technical Foundation**:
   - Technologies/frameworks discussed
   - Key architectural decisions made
   - Environment and configuration details

3. **Codebase Status**:
   - Files modified or discussed with their purposes
   - Key code changes and their purpose
   - Dependencies and relationships between components

4. **Problem Resolution**:
   - Issues encountered and how they were resolved
   - Ongoing debugging context
   - Lessons learned and patterns discovered

5. **Progress Tracking**:
   - ✅ Completed tasks (with status indicators)
   - ⏳ In-progress work (with current completion status)
   - ❌ Pending tasks

6. **Active Work State**:
   - Current focus (what was being worked on most recently)
   - Recent tool calls and their key results (summarized)
   - Working code snippets being modified

7. **Recent Operations**:
   - Last agent commands executed
   - Tool results summary (key outcomes, truncated if long)
   - Immediate pre-summarization state

8. **Continuation Plan**:
   - Immediate next steps with specific details
   - Priority information
   - Any blocking issues or dependencies
</conversation-summary>

## Guidelines
- Be precise with filenames, function names, and technical terms
- Preserve exact quotes for task specifications where important
- Include enough detail to continue without re-reading full history
- Truncate very long tool outputs but preserve essential information
- Focus on actionable context that enables continuation

Do NOT call any tools. Your only task is to generate a text summary of the conversation.)");
}

TArray<FChatMessage> FChatSession::BuildMessagesToSummarize() const
{
    TArray<FChatMessage> Result;
    
    // Determine how many recent messages to keep (don't summarize these)
    int32 RecentToKeep = GetRecentMessagesToKeepFromConfig();
    int32 EndIndex = FMath::Max(0, Messages.Num() - RecentToKeep);
    
    // Add messages up to the cutoff point
    for (int32 i = 0; i < EndIndex; i++)
    {
        Result.Add(Messages[i]);
    }
    
    return Result;
}

void FChatSession::BroadcastTokenBudgetUpdate()
{
    int32 CurrentTokens = GetEstimatedTokenCount();
    int32 MaxTokens = GetTokenBudget();
    float Utilization = GetContextUtilization();
    
    OnTokenBudgetUpdated.ExecuteIfBound(CurrentTokens, MaxTokens, Utilization);
}

// ============ Summarization Config Settings ============

float FChatSession::GetSummarizationThresholdFromConfig()
{
    float Threshold = 0.8f; // Default 80%
    GConfig->GetFloat(TEXT("VibeUE"), TEXT("SummarizationThreshold"), Threshold, GEditorPerProjectIni);
    return FMath::Clamp(Threshold, 0.5f, 0.95f);
}

void FChatSession::SaveSummarizationThresholdToConfig(float Threshold)
{
    Threshold = FMath::Clamp(Threshold, 0.5f, 0.95f);
    GConfig->SetFloat(TEXT("VibeUE"), TEXT("SummarizationThreshold"), Threshold, GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
}

int32 FChatSession::GetRecentMessagesToKeepFromConfig()
{
    int32 Count = 10; // Default: keep last 10 messages
    GConfig->GetInt(TEXT("VibeUE"), TEXT("RecentMessagesToKeep"), Count, GEditorPerProjectIni);
    return FMath::Clamp(Count, 4, 50);
}

void FChatSession::SaveRecentMessagesToKeepToConfig(int32 Count)
{
    Count = FMath::Clamp(Count, 4, 50);
    GConfig->SetInt(TEXT("VibeUE"), TEXT("RecentMessagesToKeep"), Count, GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
}

bool FChatSession::IsAutoSummarizeEnabled()
{
    bool bEnabled = true; // Default: enabled
    GConfig->GetBool(TEXT("VibeUE"), TEXT("AutoSummarize"), bEnabled, GEditorPerProjectIni);
    return bEnabled;
}

void FChatSession::SetAutoSummarizeEnabled(bool bEnabled)
{
    GConfig->SetBool(TEXT("VibeUE"), TEXT("AutoSummarize"), bEnabled, GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
}

float FChatSession::GetContextUtilization() const
{
    int32 ContextLength = GetCurrentModelContextLength();
    if (ContextLength <= 0)
    {
        return 0.0f;
    }
    
    int32 UsedTokens = GetEstimatedTokenCount();
    return (float)UsedTokens / (float)ContextLength;
}

TArray<FChatMessage> FChatSession::BuildApiMessages() const
{
    TArray<FChatMessage> ApiMessages;
    
    // Build full system prompt with optional tool instructions
    FString FullSystemPrompt = SystemPrompt;

    int32 AvailableTokens = GetCurrentModelContextLength() - ReservedResponseTokens;
    int32 UsedTokens = EstimateTokenCount(FullSystemPrompt);

    // Always include system prompt (with task list context appended)
    ApiMessages.Add(FChatMessage(TEXT("system"), FullSystemPrompt));
    
    // If we have a conversation summary, add it after system prompt
    if (!ConversationSummary.IsEmpty())
    {
        FString SummaryMessage = FString::Printf(
            TEXT("Previous conversation summary:\n%s\n\nContinuing from the summary above:"),
            *ConversationSummary
        );
        ApiMessages.Add(FChatMessage(TEXT("system"), SummaryMessage));
        UsedTokens += EstimateTokenCount(SummaryMessage);
    }
    
    // Build list of messages to include, working backwards from most recent
    TArray<int32> MessageIndicesToInclude;
    
    for (int32 i = Messages.Num() - 2; i >= 0; --i)  // -2 to exclude the empty streaming assistant message
    {
        const FChatMessage& Msg = Messages[i];
        int32 MsgTokens = EstimateTokenCount(Msg.Content) + 4;
        
        if (UsedTokens + MsgTokens > AvailableTokens)
        {
            // Would exceed context, stop adding
            UE_LOG(LogChatSession, Log, TEXT("Context limit reached at message %d. Including %d messages."), 
                i, MessageIndicesToInclude.Num());
            break;
        }
        
        UsedTokens += MsgTokens;
        MessageIndicesToInclude.Insert(i, 0); // Insert at beginning to maintain order
    }
    
    // Add messages in chronological order
    for (int32 Index : MessageIndicesToInclude)
    {
        ApiMessages.Add(Messages[Index]);
    }
    
    UE_LOG(LogChatSession, Verbose, TEXT("Built API messages: %d messages, ~%d tokens (context: %d)"), 
        ApiMessages.Num(), UsedTokens, GetCurrentModelContextLength());
    
    return ApiMessages;
}

bool FChatSession::NeedsSummarization() const
{
    float Utilization = GetContextUtilization();
    // Trigger summarization when we're using > 75% of context
    return Utilization > 0.75f;
}

void FChatSession::SummarizeConversation()
{
    // TODO: Implement AI-powered summarization
    // For now, we rely on BuildApiMessages to truncate old messages
    UE_LOG(LogChatSession, Log, TEXT("Conversation summarization requested (not yet implemented)"));
}

void FChatSession::InitializeMCP()
{
    if (bMCPInitialized)
    {
        UE_LOG(LogChatSession, Log, TEXT("MCP already initialized"));
        return;
    }
    
    MCPClient = MakeShared<FMCPClient>();
    MCPClient->Initialize();
    
    // Discover available tools
    MCPClient->DiscoverTools(FOnToolsDiscovered::CreateLambda([this](bool bSuccess, const TArray<FMCPTool>& MCPTools)
    {
        bMCPInitialized = true;
        UE_LOG(LogChatSession, Log, TEXT("MCP initialized with %d MCP tools"), MCPTools.Num());
        
        // Report TOTAL tool count (internal + MCP), not just MCP
        int32 TotalToolCount = GetEnabledToolCount();
        UE_LOG(LogChatSession, Log, TEXT("Total tools available: %d (internal + MCP)"), TotalToolCount);
        OnToolsReady.ExecuteIfBound(bSuccess || TotalToolCount > 0, TotalToolCount);
    }));
}

TArray<FMCPTool> FChatSession::GetAllEnabledTools() const
{
    TArray<FMCPTool> MergedTools;
    FToolRegistry& Registry = FToolRegistry::Get();
    
    // Add internal tools first (already filtered by enabled state)
    TArray<FMCPTool> InternalTools = GetInternalToolsAsMCP();
    MergedTools.Append(InternalTools);
    
    // Add MCP tools (if any) - also filter by enabled state
    if (MCPClient.IsValid())
    {
        const TArray<FMCPTool>& MCPTools = MCPClient->GetMCPTools();
        // Only add MCP tools that don't conflict with internal tools AND are enabled
        for (const FMCPTool& MCPTool : MCPTools)
        {
            // Check if this MCP tool is disabled
            if (!Registry.IsToolEnabled(MCPTool.Name))
            {
                UE_LOG(LogChatSession, Verbose, TEXT("Skipping disabled MCP tool: %s"), *MCPTool.Name);
                continue;
            }
            
            bool bConflict = false;
            for (const FMCPTool& InternalTool : MergedTools)
            {
                if (InternalTool.Name == MCPTool.Name)
                {
                    bConflict = true;
                    break;
                }
            }
            if (!bConflict)
            {
                MergedTools.Add(MCPTool);
            }
        }
    }
    
    return MergedTools;
}

int32 FChatSession::GetEnabledToolCount() const
{
    // Simply return the count of enabled tools (internal + MCP)
    // GetAllEnabledTools already filters by enabled state
    return GetAllEnabledTools().Num();
}

bool FChatSession::IsMCPInitialized() const
{
    return bMCPInitialized && MCPClient.IsValid();
}

void FChatSession::UpdateUsageStats(int32 PromptTokens, int32 CompletionTokens)
{
    UsageStats.PromptTokens = PromptTokens;
    UsageStats.CompletionTokens = CompletionTokens;
    UsageStats.TotalTokens = PromptTokens + CompletionTokens;
    UsageStats.TotalPromptTokens += PromptTokens;
    UsageStats.TotalCompletionTokens += CompletionTokens;
    
    UE_LOG(LogChatSession, Log, TEXT("Usage stats updated: Requests=%d, PromptTokens=%d, CompletionTokens=%d, TotalPrompt=%d, TotalCompletion=%d"),
        UsageStats.RequestCount, PromptTokens, CompletionTokens, 
        UsageStats.TotalPromptTokens, UsageStats.TotalCompletionTokens);
}

bool FChatSession::IsDebugModeEnabled()
{
    bool bDebugMode = false;
    GConfig->GetBool(TEXT("VibeUE"), TEXT("DebugMode"), bDebugMode, GEditorPerProjectIni);
    return bDebugMode;
}

void FChatSession::SetDebugModeEnabled(bool bEnabled)
{
    GConfig->SetBool(TEXT("VibeUE"), TEXT("DebugMode"), bEnabled, GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
}

bool FChatSession::IsFileLoggingEnabled()
{
    bool bFileLogging = false; // Default to disabled (enable via Debug Mode setting)
    GConfig->GetBool(TEXT("VibeUE"), TEXT("FileLogging"), bFileLogging, GEditorPerProjectIni);
    return bFileLogging;
}

void FChatSession::SetFileLoggingEnabled(bool bEnabled)
{
    GConfig->SetBool(TEXT("VibeUE"), TEXT("FileLogging"), bEnabled, GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
}

bool FChatSession::IsAutoSaveBeforePythonExecutionEnabled()
{
    bool bAutoSave = true; // Default to enabled for safety
    GConfig->GetBool(TEXT("VibeUE"), TEXT("AutoSaveBeforePythonExecution"), bAutoSave, GEditorPerProjectIni);
    return bAutoSave;
}

void FChatSession::SetAutoSaveBeforePythonExecutionEnabled(bool bEnabled)
{
    GConfig->SetBool(TEXT("VibeUE"), TEXT("AutoSaveBeforePythonExecution"), bEnabled, GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
}

bool FChatSession::IsYoloModeEnabled()
{
    bool bYoloMode = false; // Default to disabled - require approval for Python execution
    GConfig->GetBool(TEXT("VibeUE"), TEXT("YoloMode"), bYoloMode, GEditorPerProjectIni);
    return bYoloMode;
}

void FChatSession::SetYoloModeEnabled(bool bEnabled)
{
    GConfig->SetBool(TEXT("VibeUE"), TEXT("YoloMode"), bEnabled, GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
}

void FChatSession::ApproveToolCall(const FString& ToolCallId)
{
    if (!PendingApprovalToolCall.IsSet())
    {
        UE_LOG(LogChatSession, Warning, TEXT("ApproveToolCall: No pending approval"));
        return;
    }
    
    FMCPToolCall ToolCall = PendingApprovalToolCall.GetValue();
    PendingApprovalToolCall.Reset();
    
    if (ToolCall.Id != ToolCallId)
    {
        UE_LOG(LogChatSession, Warning, TEXT("ApproveToolCall: ID mismatch - expected %s, got %s"), *ToolCall.Id, *ToolCallId);
        return;
    }
    
    UE_LOG(LogChatSession, Log, TEXT("Tool call approved by user: %s (id=%s)"), *ToolCall.ToolName, *ToolCall.Id);
    
    // Re-insert at front of queue and execute with bypass flag
    ToolCallQueue.Insert(ToolCall, 0);
    bBypassApprovalCheck = true;
    ExecuteNextToolInQueue();
}

void FChatSession::RejectToolCall(const FString& ToolCallId)
{
    if (!PendingApprovalToolCall.IsSet())
    {
        UE_LOG(LogChatSession, Warning, TEXT("RejectToolCall: No pending approval"));
        return;
    }
    
    FMCPToolCall ToolCall = PendingApprovalToolCall.GetValue();
    PendingApprovalToolCall.Reset();
    
    UE_LOG(LogChatSession, Log, TEXT("Tool call rejected by user: %s (id=%s)"), *ToolCall.ToolName, *ToolCall.Id);
    
    // Send rejection as tool result so the LLM knows the user declined
    FChatMessage ToolResultMsg(TEXT("tool"), TEXT("User rejected execution of this Python code. Ask the user what they would like to change or do differently."));
    ToolResultMsg.ToolCallId = ToolCall.Id;
    Messages.Add(ToolResultMsg);
    OnMessageAdded.ExecuteIfBound(ToolResultMsg);
    
    PendingToolCallCount--;
    
    // Continue with next tool in queue
    ExecuteNextToolInQueue();
}

FString FChatSession::GetVibeUEApiKeyFromConfig()
{
    FString ApiKey;
    GConfig->GetString(TEXT("VibeUE"), TEXT("VibeUEApiKey"), ApiKey, GEditorPerProjectIni);
    return ApiKey;
}

void FChatSession::SaveVibeUEApiKeyToConfig(const FString& ApiKey)
{
    GConfig->SetString(TEXT("VibeUE"), TEXT("VibeUEApiKey"), *ApiKey, GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
}

FString FChatSession::GetVibeUEEndpointFromConfig()
{
    FString Endpoint;
    GConfig->GetString(TEXT("VibeUE"), TEXT("VibeUEEndpoint"), Endpoint, GEditorPerProjectIni);
    if (Endpoint.IsEmpty())
    {
        // Return default endpoint if not configured
        return FVibeUEAPIClient::GetDefaultEndpoint();
    }
    return Endpoint;
}

void FChatSession::SaveVibeUEEndpointToConfig(const FString& Endpoint)
{
    GConfig->SetString(TEXT("VibeUE"), TEXT("VibeUEEndpoint"), *Endpoint, GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
}

ELLMProvider FChatSession::GetProviderFromConfig()
{
    FString ProviderStr;
    GConfig->GetString(TEXT("VibeUE"), TEXT("Provider"), ProviderStr, GEditorPerProjectIni);
    
    if (ProviderStr == TEXT("OpenRouter"))
    {
        return ELLMProvider::OpenRouter;
    }
    
    // Default to VibeUE
    return ELLMProvider::VibeUE;
}

void FChatSession::SaveProviderToConfig(ELLMProvider Provider)
{
    FString ProviderStr = (Provider == ELLMProvider::OpenRouter) ? TEXT("OpenRouter") : TEXT("VibeUE");
    GConfig->SetString(TEXT("VibeUE"), TEXT("Provider"), *ProviderStr, GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
}

void FChatSession::SetCurrentProvider(ELLMProvider Provider)
{
    CurrentProvider = Provider;
    SaveProviderToConfig(Provider);
    UE_LOG(LogChatSession, Log, TEXT("Provider changed to: %s"), 
        Provider == ELLMProvider::VibeUE ? TEXT("VibeUE") : TEXT("OpenRouter"));
}

TArray<FLLMProviderInfo> FChatSession::GetAvailableProviders()
{
    TArray<FLLMProviderInfo> Providers;
    
    // VibeUE provider
    Providers.Add(FLLMProviderInfo(
        TEXT("VibeUE"),
        TEXT("VibeUE"),
        false,
        TEXT(""),
        TEXT("VibeUE's own LLM API service")
    ));
    
    // OpenRouter provider
    Providers.Add(FLLMProviderInfo(
        TEXT("OpenRouter"),
        TEXT("OpenRouter"),
        true,
        TEXT("x-ai/grok-4.1-fast:free"),
        TEXT("Access multiple LLM providers through OpenRouter API")
    ));
    
    return Providers;
}

FLLMProviderInfo FChatSession::GetCurrentProviderInfo() const
{
    if (CurrentProvider == ELLMProvider::VibeUE && VibeUEClient.IsValid())
    {
        return VibeUEClient->GetProviderInfo();
    }
    else if (OpenRouterClient.IsValid())
    {
        return OpenRouterClient->GetProviderInfo();
    }
    
    // Fallback
    return FLLMProviderInfo(TEXT("Unknown"), TEXT("Unknown"), false, TEXT(""), TEXT(""));
}

bool FChatSession::SupportsModelSelection() const
{
    return GetCurrentProviderInfo().bSupportsModelSelection;
}

// ============ LLM Generation Parameters ============

float FChatSession::GetTemperatureFromConfig()
{
    float Temperature = FVibeUEAPIClient::DefaultTemperature;
    GConfig->GetFloat(TEXT("VibeUE"), TEXT("Temperature"), Temperature, GEditorPerProjectIni);
    return FMath::Clamp(Temperature, 0.0f, 2.0f);
}

void FChatSession::SaveTemperatureToConfig(float Temperature)
{
    Temperature = FMath::Clamp(Temperature, 0.0f, 2.0f);
    GConfig->SetFloat(TEXT("VibeUE"), TEXT("Temperature"), Temperature, GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
}

float FChatSession::GetTopPFromConfig()
{
    float TopP = FVibeUEAPIClient::DefaultTopP;
    GConfig->GetFloat(TEXT("VibeUE"), TEXT("TopP"), TopP, GEditorPerProjectIni);
    return FMath::Clamp(TopP, 0.0f, 1.0f);
}

void FChatSession::SaveTopPToConfig(float TopP)
{
    TopP = FMath::Clamp(TopP, 0.0f, 1.0f);
    GConfig->SetFloat(TEXT("VibeUE"), TEXT("TopP"), TopP, GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
}

int32 FChatSession::GetMaxTokensFromConfig()
{
    int32 MaxTokens = FVibeUEAPIClient::DefaultMaxTokens;
    GConfig->GetInt(TEXT("VibeUE"), TEXT("MaxTokens"), MaxTokens, GEditorPerProjectIni);
    return FMath::Clamp(MaxTokens, FVibeUEAPIClient::MinMaxTokens, FVibeUEAPIClient::MaxMaxTokens);
}

void FChatSession::SaveMaxTokensToConfig(int32 MaxTokens)
{
    MaxTokens = FMath::Clamp(MaxTokens, FVibeUEAPIClient::MinMaxTokens, FVibeUEAPIClient::MaxMaxTokens);
    GConfig->SetInt(TEXT("VibeUE"), TEXT("MaxTokens"), MaxTokens, GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
}

int32 FChatSession::GetMaxToolCallIterationsFromConfig()
{
    int32 MaxIterations = DefaultMaxToolCallIterations; // Default 200 (like Copilot)
    GConfig->GetInt(TEXT("VibeUE"), TEXT("MaxToolCallIterations"), MaxIterations, GEditorPerProjectIni);
    return FMath::Clamp(MaxIterations, 10, 500);
}

void FChatSession::SaveMaxToolCallIterationsToConfig(int32 MaxIterations)
{
    MaxIterations = FMath::Clamp(MaxIterations, 10, 500);
    GConfig->SetInt(TEXT("VibeUE"), TEXT("MaxToolCallIterations"), MaxIterations, GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
}

void FChatSession::SetMaxToolCallIterations(int32 NewMax)
{
    MaxToolCallIterations = FMath::Clamp(NewMax, 10, 500);
    UE_LOG(LogChatSession, Log, TEXT("Max tool call iterations set to %d for current session"), MaxToolCallIterations);
}

bool FChatSession::GetParallelToolCallsFromConfig()
{
    bool bParallelToolCalls = true; // Default to true (parallel)
    GConfig->GetBool(TEXT("VibeUE"), TEXT("ParallelToolCalls"), bParallelToolCalls, GEditorPerProjectIni);
    return bParallelToolCalls;
}

void FChatSession::SaveParallelToolCallsToConfig(bool bParallelToolCalls)
{
    GConfig->SetBool(TEXT("VibeUE"), TEXT("ParallelToolCalls"), bParallelToolCalls, GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
}

void FChatSession::ApplyLLMParametersToClient()
{
    bool bParallelToolCalls = GetParallelToolCallsFromConfig();
    
    if (VibeUEClient.IsValid())
    {
        VibeUEClient->SetTemperature(GetTemperatureFromConfig());
        VibeUEClient->SetTopP(GetTopPFromConfig());
        VibeUEClient->SetMaxTokens(GetMaxTokensFromConfig());
        VibeUEClient->SetParallelToolCalls(bParallelToolCalls);
        
        UE_LOG(LogChatSession, Log, TEXT("Applied LLM params to VibeUE: temperature=%.2f, top_p=%.2f, max_tokens=%d, parallel_tool_calls=%s"),
            VibeUEClient->GetTemperature(), VibeUEClient->GetTopP(), VibeUEClient->GetMaxTokens(),
            VibeUEClient->GetParallelToolCalls() ? TEXT("true") : TEXT("false"));
    }
    
    if (OpenRouterClient.IsValid())
    {
        OpenRouterClient->SetParallelToolCalls(bParallelToolCalls);
        
        UE_LOG(LogChatSession, Log, TEXT("Applied parallel_tool_calls=%s to OpenRouter"),
            bParallelToolCalls ? TEXT("true") : TEXT("false"));
    }
}

// ============ MCP Server Settings ============

bool FChatSession::GetMCPServerEnabledFromConfig()
{
    bool bEnabled = true; // Default to enabled
    GConfig->GetBool(TEXT("VibeUE.MCPServer"), TEXT("Enabled"), bEnabled, GEditorPerProjectIni);
    return bEnabled;
}

void FChatSession::SaveMCPServerEnabledToConfig(bool bEnabled)
{
    GConfig->SetBool(TEXT("VibeUE.MCPServer"), TEXT("Enabled"), bEnabled, GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
}

int32 FChatSession::GetMCPServerPortFromConfig()
{
    int32 Port = DefaultMCPServerPort;
    GConfig->GetInt(TEXT("VibeUE.MCPServer"), TEXT("Port"), Port, GEditorPerProjectIni);
    return FMath::Clamp(Port, 1024, 65535); // Valid port range
}

void FChatSession::SaveMCPServerPortToConfig(int32 Port)
{
    Port = FMath::Clamp(Port, 1024, 65535);
    GConfig->SetInt(TEXT("VibeUE.MCPServer"), TEXT("Port"), Port, GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
}

FString FChatSession::GetMCPServerApiKeyFromConfig()
{
    FString ApiKey;
    GConfig->GetString(TEXT("VibeUE.MCPServer"), TEXT("ApiKey"), ApiKey, GEditorPerProjectIni);
    return ApiKey;
}

void FChatSession::SaveMCPServerApiKeyToConfig(const FString& ApiKey)
{
    GConfig->SetString(TEXT("VibeUE.MCPServer"), TEXT("ApiKey"), *ApiKey, GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
}

// ============ Thinking Tag Handling ============

FString FChatSession::FormatThinkingBlocks(const FString& Text)
{
    FString Result = Text;
    
    // Format thinking blocks with visual indicator instead of removing them
    // Replaces <think>content</think> with: 💭 **Thinking:** content
    
    struct FTagPair
    {
        const TCHAR* OpenTag;
        int32 OpenLen;
        const TCHAR* CloseTag;
        int32 CloseLen;
    };
    
    static const TArray<FTagPair> TagPairs = {
        { TEXT("<think>"), 7, TEXT("</think>"), 8 },
        { TEXT("<thinking>"), 10, TEXT("</thinking>"), 11 },
        { TEXT("<reasoning>"), 11, TEXT("</reasoning>"), 12 },
        { TEXT("<thought>"), 9, TEXT("</thought>"), 10 }
    };
    
    for (const FTagPair& Tags : TagPairs)
    {
        int32 SearchStart = 0;
        while (true)
        {
            int32 OpenPos = Result.Find(Tags.OpenTag, ESearchCase::IgnoreCase, ESearchDir::FromStart, SearchStart);
            if (OpenPos == INDEX_NONE) break;
            
            int32 ContentStart = OpenPos + Tags.OpenLen;
            int32 ClosePos = Result.Find(Tags.CloseTag, ESearchCase::IgnoreCase, ESearchDir::FromStart, ContentStart);
            
            if (ClosePos != INDEX_NONE)
            {
                // Extract content between tags
                FString Content = Result.Mid(ContentStart, ClosePos - ContentStart).TrimStartAndEnd();
                
                // Format with visual indicator
                // Use line breaks to separate thinking from main content
                FString FormattedThinking = FString::Printf(
                    TEXT("\n💭 **Thinking:**\n%s\n---\n"),
                    *Content
                );
                
                // Replace the entire tag block
                Result = Result.Left(OpenPos) + FormattedThinking + Result.Mid(ClosePos + Tags.CloseLen);
                
                // Continue searching after the replacement
                SearchStart = OpenPos + FormattedThinking.Len();
            }
            else
            {
                // Unclosed tag - format everything after the open tag as thinking
                FString Content = Result.Mid(ContentStart).TrimStartAndEnd();
                FString FormattedThinking = FString::Printf(
                    TEXT("\n💭 **Thinking:**\n%s"),
                    *Content
                );
                Result = Result.Left(OpenPos) + FormattedThinking;
                break;
            }
        }
    }
    
    return Result.TrimStartAndEnd();
}

FString FChatSession::StripThinkingTags(const FString& Text)
{
    FString Result = Text;
    
    // Strip <think>...</think> blocks (Qwen3 reasoning format)
    // Use simple iterative approach since FString doesn't have full regex support
    int32 ThinkStart = Result.Find(TEXT("<think>"), ESearchCase::IgnoreCase);
    while (ThinkStart != INDEX_NONE)
    {
        int32 ThinkEnd = Result.Find(TEXT("</think>"), ESearchCase::IgnoreCase, ESearchDir::FromStart, ThinkStart);
        if (ThinkEnd != INDEX_NONE)
        {
            // Remove the entire <think>...</think> block
            Result.RemoveAt(ThinkStart, ThinkEnd + 8 - ThinkStart); // 8 = len("</think>")
        }
        else
        {
            // Unclosed <think> tag - remove from <think> to end of string
            Result = Result.Left(ThinkStart);
            break;
        }
        ThinkStart = Result.Find(TEXT("<think>"), ESearchCase::IgnoreCase);
    }
    
    // Strip <thinking>...</thinking> blocks (Anthropic/Claude style)
    int32 ThinkingStart = Result.Find(TEXT("<thinking>"), ESearchCase::IgnoreCase);
    while (ThinkingStart != INDEX_NONE)
    {
        int32 ThinkingEnd = Result.Find(TEXT("</thinking>"), ESearchCase::IgnoreCase, ESearchDir::FromStart, ThinkingStart);
        if (ThinkingEnd != INDEX_NONE)
        {
            Result.RemoveAt(ThinkingStart, ThinkingEnd + 11 - ThinkingStart); // 11 = len("</thinking>")
        }
        else
        {
            Result = Result.Left(ThinkingStart);
            break;
        }
        ThinkingStart = Result.Find(TEXT("<thinking>"), ESearchCase::IgnoreCase);
    }
    
    // Strip <reasoning>...</reasoning> blocks
    int32 ReasonStart = Result.Find(TEXT("<reasoning>"), ESearchCase::IgnoreCase);
    while (ReasonStart != INDEX_NONE)
    {
        int32 ReasonEnd = Result.Find(TEXT("</reasoning>"), ESearchCase::IgnoreCase, ESearchDir::FromStart, ReasonStart);
        if (ReasonEnd != INDEX_NONE)
        {
            Result.RemoveAt(ReasonStart, ReasonEnd + 12 - ReasonStart); // 12 = len("</reasoning>")
        }
        else
        {
            Result = Result.Left(ReasonStart);
            break;
        }
        ReasonStart = Result.Find(TEXT("<reasoning>"), ESearchCase::IgnoreCase);
    }
    
    // Strip <thought>...</thought> blocks
    int32 ThoughtStart = Result.Find(TEXT("<thought>"), ESearchCase::IgnoreCase);
    while (ThoughtStart != INDEX_NONE)
    {
        int32 ThoughtEnd = Result.Find(TEXT("</thought>"), ESearchCase::IgnoreCase, ESearchDir::FromStart, ThoughtStart);
        if (ThoughtEnd != INDEX_NONE)
        {
            Result.RemoveAt(ThoughtStart, ThoughtEnd + 10 - ThoughtStart); // 10 = len("</thought>")
        }
        else
        {
            Result = Result.Left(ThoughtStart);
            break;
        }
        ThoughtStart = Result.Find(TEXT("<thought>"), ESearchCase::IgnoreCase);
    }
    
    return Result.TrimStartAndEnd();
}

FString FChatSession::ExtractThinkingContent(const FString& Text)
{
    TArray<FString> ThinkingParts;
    
    // Extract <think>...</think> blocks
    int32 SearchStart = 0;
    while (true)
    {
        int32 ThinkStart = Text.Find(TEXT("<think>"), ESearchCase::IgnoreCase, ESearchDir::FromStart, SearchStart);
        if (ThinkStart == INDEX_NONE) break;
        
        int32 ContentStart = ThinkStart + 7; // len("<think>")
        int32 ThinkEnd = Text.Find(TEXT("</think>"), ESearchCase::IgnoreCase, ESearchDir::FromStart, ContentStart);
        if (ThinkEnd == INDEX_NONE) break;
        
        FString Content = Text.Mid(ContentStart, ThinkEnd - ContentStart).TrimStartAndEnd();
        if (!Content.IsEmpty())
        {
            ThinkingParts.Add(Content);
        }
        SearchStart = ThinkEnd + 8; // len("</think>")
    }
    
    // Extract <thinking>...</thinking> blocks (Anthropic/Claude style)
    SearchStart = 0;
    while (true)
    {
        int32 Start = Text.Find(TEXT("<thinking>"), ESearchCase::IgnoreCase, ESearchDir::FromStart, SearchStart);
        if (Start == INDEX_NONE) break;
        
        int32 ContentStart = Start + 10; // len("<thinking>")
        int32 End = Text.Find(TEXT("</thinking>"), ESearchCase::IgnoreCase, ESearchDir::FromStart, ContentStart);
        if (End == INDEX_NONE) break;
        
        FString Content = Text.Mid(ContentStart, End - ContentStart).TrimStartAndEnd();
        if (!Content.IsEmpty())
        {
            ThinkingParts.Add(Content);
        }
        SearchStart = End + 11; // len("</thinking>")
    }
    
    // Extract <reasoning>...</reasoning> blocks
    SearchStart = 0;
    while (true)
    {
        int32 Start = Text.Find(TEXT("<reasoning>"), ESearchCase::IgnoreCase, ESearchDir::FromStart, SearchStart);
        if (Start == INDEX_NONE) break;
        
        int32 ContentStart = Start + 11; // len("<reasoning>")
        int32 End = Text.Find(TEXT("</reasoning>"), ESearchCase::IgnoreCase, ESearchDir::FromStart, ContentStart);
        if (End == INDEX_NONE) break;
        
        FString Content = Text.Mid(ContentStart, End - ContentStart).TrimStartAndEnd();
        if (!Content.IsEmpty())
        {
            ThinkingParts.Add(Content);
        }
        SearchStart = End + 12;
    }
    
    // Join all thinking parts
    return FString::Join(ThinkingParts, TEXT("\n---\n"));
}

// Loop detection removed - now using prompt-based self-awareness instructions
// See vibeue.instructions.md for the loop prevention guidelines the model follows

void FChatSession::InitializeInternalTools()
{
    // Ensure ToolRegistry is initialized
    FToolRegistry& Registry = FToolRegistry::Get();
    if (!Registry.IsInitialized())
    {
        UE_LOG(LogChatSession, Log, TEXT("Initializing ToolRegistry..."));
        Registry.Initialize();
        UE_LOG(LogChatSession, Log, TEXT("ToolRegistry initialized with %d tools"), Registry.GetAllTools().Num());
    }
    else
    {
        UE_LOG(LogChatSession, Log, TEXT("ToolRegistry already initialized with %d tools"), Registry.GetAllTools().Num());
    }
    
    // Load internal tools (will be cached)
    TArray<FMCPTool> Tools = GetInternalToolsAsMCP();
    
    UE_LOG(LogChatSession, Log, TEXT("Internal tools initialized: %d tools available"), Tools.Num());
    for (const FMCPTool& Tool : Tools)
    {
        UE_LOG(LogChatSession, Log, TEXT("  - %s: %s"), *Tool.Name, *Tool.Description);
    }
    
    // Notify UI that tools are ready (even if count is 0, so UI can update)
    int32 TotalToolCount = GetEnabledToolCount();
    OnToolsReady.ExecuteIfBound(true, TotalToolCount);
    UE_LOG(LogChatSession, Log, TEXT("Notified UI: tools ready, count = %d"), TotalToolCount);
}

TArray<FMCPTool> FChatSession::GetInternalToolsAsMCP() const
{
    // Always rebuild from registry to respect enabled/disabled state
    // (Caching disabled because users can enable/disable tools at any time)
    
    // Load tools from registry
    FToolRegistry& Registry = FToolRegistry::Get();
    if (!Registry.IsInitialized())
    {
        UE_LOG(LogChatSession, Warning, TEXT("ToolRegistry not initialized, initializing now..."));
        const_cast<FChatSession*>(this)->InitializeInternalTools();
    }
    
    // Get only ENABLED tools
    TArray<FToolMetadata> Tools = Registry.GetEnabledTools();
    UE_LOG(LogChatSession, Verbose, TEXT("GetInternalToolsAsMCP: Registry has %d enabled tools"), Tools.Num());
    
    if (Tools.Num() == 0)
    {
        UE_LOG(LogChatSession, Verbose, TEXT("No enabled tools in ToolRegistry"));
        return TArray<FMCPTool>();
    }
    
    UE_LOG(LogChatSession, Verbose, TEXT("Converting %d enabled internal tools to MCP format"), Tools.Num());
    
    TArray<FMCPTool> Result;
    Result.Reserve(Tools.Num());
    
    // Convert each internal tool to MCP tool format (for API compatibility)
    for (const FToolMetadata& Tool : Tools)
    {
        FMCPTool MCPTool;
        MCPTool.Name = Tool.Name;
        MCPTool.Description = Tool.Description;
        MCPTool.ServerName = TEXT("Internal"); // Mark as internal tool (from ToolRegistry)
        
        // Build input schema (JSON Schema format)
        TSharedPtr<FJsonObject> InputSchema = MakeShared<FJsonObject>();
        InputSchema->SetStringField(TEXT("type"), TEXT("object"));
        
        TSharedPtr<FJsonObject> Properties = MakeShared<FJsonObject>();
        TArray<TSharedPtr<FJsonValue>> RequiredArray;
        
        for (const FToolParameter& Param : Tool.Parameters)
        {
            TSharedPtr<FJsonObject> ParamSchema = MakeShared<FJsonObject>();
            ParamSchema->SetStringField(TEXT("type"), Param.Type);
            ParamSchema->SetStringField(TEXT("description"), Param.Description);

            // For array types, add the items field (required by Google/Gemini)
            if (Param.Type == TEXT("array"))
            {
                TSharedPtr<FJsonObject> ItemsSchema = MakeShared<FJsonObject>();
                ItemsSchema->SetStringField(TEXT("type"), Param.ArrayItemType.IsEmpty() ? TEXT("string") : Param.ArrayItemType);
                ParamSchema->SetObjectField(TEXT("items"), ItemsSchema);
            }

            if (!Param.DefaultValue.IsEmpty())
            {
                // Try to parse default value based on type
                if (Param.Type == TEXT("string"))
                {
                    ParamSchema->SetStringField(TEXT("default"), Param.DefaultValue);
                }
                else if (Param.Type == TEXT("int"))
                {
                    int32 IntValue = 0;
                    if (LexTryParseString(IntValue, *Param.DefaultValue))
                    {
                        ParamSchema->SetNumberField(TEXT("default"), IntValue);
                    }
                }
                else if (Param.Type == TEXT("float"))
                {
                    float FloatValue = 0.0f;
                    if (LexTryParseString(FloatValue, *Param.DefaultValue))
                    {
                        ParamSchema->SetNumberField(TEXT("default"), FloatValue);
                    }
                }
                else if (Param.Type == TEXT("bool"))
                {
                    bool BoolValue = (Param.DefaultValue.ToLower() == TEXT("true") || Param.DefaultValue == TEXT("1"));
                    ParamSchema->SetBoolField(TEXT("default"), BoolValue);
                }
            }

            Properties->SetObjectField(Param.Name, ParamSchema);
            
            if (Param.bRequired)
            {
                RequiredArray.Add(MakeShared<FJsonValueString>(Param.Name));
            }
        }
        
        InputSchema->SetObjectField(TEXT("properties"), Properties);
        
        if (RequiredArray.Num() > 0)
        {
            InputSchema->SetArrayField(TEXT("required"), RequiredArray);
        }
        
        MCPTool.InputSchema = InputSchema;
        Result.Add(MCPTool);
    }
    
    UE_LOG(LogChatSession, Verbose, TEXT("Returning %d enabled reflection tools"), Result.Num());

    return Result;
}

// ============================================================================
// Voice Input Configuration Settings
// ============================================================================

bool FChatSession::IsAutoSendAfterRecordingEnabled()
{
    bool bAutoSend = false;
    GConfig->GetBool(TEXT("VibeUE.VoiceInput"), TEXT("AutoSendAfterRecording"), bAutoSend, GEditorPerProjectIni);
    return bAutoSend;
}

void FChatSession::SetAutoSendAfterRecordingEnabled(bool bEnabled)
{
    GConfig->SetBool(TEXT("VibeUE.VoiceInput"), TEXT("AutoSendAfterRecording"), bEnabled, GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
}

// ============================================================================
// Voice Input Implementation
// ============================================================================

void FChatSession::InitializeSpeechService()
{
    if (!SpeechService.IsValid())
    {
        UE_LOG(LogChatSession, Log, TEXT("Initializing speech service"));

        SpeechService = MakeShared<FSpeechToTextService>();
        SpeechService->Initialize();

        // Register ElevenLabs provider
        TSharedPtr<FElevenLabsSpeechProvider> ElevenLabsProvider = MakeShared<FElevenLabsSpeechProvider>();
        ElevenLabsProvider->SetApiKey(FElevenLabsSpeechProvider::GetApiKeyFromConfig());
        SpeechService->RegisterProvider(TEXT("elevenlabs"), ElevenLabsProvider);
        SpeechService->SetActiveProvider(TEXT("elevenlabs"));

        // Bind event handlers
        SpeechService->OnStatusChanged.BindSP(this, &FChatSession::OnSpeechStatusChanged);
        SpeechService->OnPartialTranscript.BindSP(this, &FChatSession::OnSpeechPartialTranscript);
        SpeechService->OnFinalTranscript.BindSP(this, &FChatSession::OnSpeechFinalTranscript);
        SpeechService->OnError.BindSP(this, &FChatSession::OnSpeechError);

        UE_LOG(LogChatSession, Log, TEXT("Speech service initialized successfully"));
    }
}

void FChatSession::StartVoiceInput()
{
    InitializeSpeechService();

    if (!SpeechService->HasSpeechProvider())
    {
        OnVoiceInputStarted.ExecuteIfBound(false);
        UE_LOG(LogChatSession, Warning, TEXT("Voice input not available - no API key configured"));
        return;
    }

    // Build session options from config
    FSpeechSessionOptions Options;
    Options.LanguageCode = FSpeechToTextService::GetDefaultLanguageFromConfig();
    Options.SampleRate = 48000;  // Match system default audio capture rate
    Options.CommitStrategy = TEXT("vad");
    Options.VADSilenceThreshold = 1.5f;

    // Add recent context from conversation (last 2 messages)
    if (Messages.Num() > 0)
    {
        int32 ContextStart = FMath::Max(0, Messages.Num() - 2);
        for (int32 i = ContextStart; i < Messages.Num(); i++)
        {
            Options.PreviousContext += Messages[i].Content + TEXT(" ");
        }
    }

    UE_LOG(LogChatSession, Log, TEXT("Starting voice input"));
    SpeechService->StartSession(Options);
    CurrentPartialTranscript.Empty();
}

void FChatSession::StopVoiceInput()
{
    if (SpeechService.IsValid())
    {
        double CurrentTime = FPlatformTime::Seconds();
        UE_LOG(LogChatSession, Warning, TEXT("[VOICE DEBUG] StopVoiceInput() called at time %.3f"), CurrentTime);
        UE_LOG(LogChatSession, Warning, TEXT("[VOICE DEBUG] Call stack: %s"), *FFrame::GetScriptCallstack());
        SpeechService->StopSession();
        CurrentPartialTranscript.Empty();
    }
}

bool FChatSession::IsVoiceInputActive() const
{
    return SpeechService.IsValid() && SpeechService->IsSessionActive();
}

bool FChatSession::IsVoiceInputAvailable() const
{
    return SpeechService.IsValid() && SpeechService->HasSpeechProvider();
}

TSharedPtr<FSpeechToTextService> FChatSession::GetSpeechService()
{
    InitializeSpeechService();
    return SpeechService;
}

// Speech event handlers

void FChatSession::OnSpeechStatusChanged(ESpeechToTextStatus Status, const FString& Text)
{
    UE_LOG(LogChatSession, Log, TEXT("Speech status changed: %d"), (int32)Status);

    if (Status == ESpeechToTextStatus::Started)
    {
        OnVoiceInputStarted.ExecuteIfBound(true);
    }
    else if (Status == ESpeechToTextStatus::Stopped)
    {
        OnVoiceInputStopped.ExecuteIfBound();
        CurrentPartialTranscript.Empty();
    }
}

void FChatSession::OnSpeechPartialTranscript(const FString& Text)
{
    UE_LOG(LogChatSession, Verbose, TEXT("Partial transcript: %s"), *Text);

    CurrentPartialTranscript = Text;
    OnVoiceInputText.ExecuteIfBound(Text, false);
}

void FChatSession::OnSpeechFinalTranscript(const FString& Text)
{
    UE_LOG(LogChatSession, Log, TEXT("Final transcript: %s"), *Text);

    OnVoiceInputText.ExecuteIfBound(Text, true);
    CurrentPartialTranscript.Empty();

    // Check if auto-send is enabled
    if (IsAutoSendAfterRecordingEnabled())
    {
        UE_LOG(LogChatSession, Log, TEXT("Auto-sending transcribed text to LLM"));
        SendMessage(Text);
        
        // Fire event to clear the input UI
        OnVoiceInputAutoSent.ExecuteIfBound();
    }
}

void FChatSession::OnSpeechError(const FString& Error)
{
    UE_LOG(LogChatSession, Error, TEXT("Speech error: %s"), *Error);

    OnChatError.ExecuteIfBound(FString::Printf(TEXT("Voice input error: %s"), *Error));
    OnVoiceInputStopped.ExecuteIfBound();
}