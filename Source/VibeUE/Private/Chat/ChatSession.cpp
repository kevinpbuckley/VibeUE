// Copyright 2025 Vibe AI. All Rights Reserved.

#include "Chat/ChatSession.h"
#include "Chat/VibeUEAPIClient.h"
#include "Chat/ILLMClient.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/ConfigCacheIni.h"

DEFINE_LOG_CATEGORY(LogChatSession);

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
    
    // Add user message
    FChatMessage UserMsg(TEXT("user"), UserMessage);
    Messages.Add(UserMsg);
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
    
    // Get available tools
    TArray<FMCPTool> Tools = GetAvailableTools();
    
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

void FChatSession::OnStreamChunk(const FString& Chunk)
{
    if (CurrentStreamingMessageIndex != INDEX_NONE && Messages.IsValidIndex(CurrentStreamingMessageIndex))
    {
        if (IsDebugModeEnabled() && !Chunk.IsEmpty())
        {
            UE_LOG(LogChatSession, Verbose, TEXT("[EVENT] OnStreamChunk: %d chars"), Chunk.Len());
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
        
        // If the message is empty and has no tool calls, remove it (failed/empty response)
        if (Message.Content.IsEmpty() && Message.ToolCalls.Num() == 0)
        {
            UE_LOG(LogChatSession, Warning, TEXT("Removing empty assistant message at index %d"), CurrentStreamingMessageIndex);
            Messages.RemoveAt(CurrentStreamingMessageIndex);
            // Trigger a rebuild to remove the empty message from UI
            OnChatReset.ExecuteIfBound();
            for (int32 i = 0; i < Messages.Num(); i++)
            {
                OnMessageAdded.ExecuteIfBound(Messages[i]);
            }
        }
        else
        {
            Message.bIsStreaming = false;
            OnMessageUpdated.ExecuteIfBound(CurrentStreamingMessageIndex, Message);
        }
    }
    
    CurrentStreamingMessageIndex = INDEX_NONE;
    
    if (bSuccess)
    {
        SaveHistory();
        BroadcastTokenBudgetUpdate();
    }
}

void FChatSession::OnStreamError(const FString& ErrorMessage)
{
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
    if (IsDebugModeEnabled())
    {
        UE_LOG(LogChatSession, Log, TEXT("[EVENT] OnToolCall: %s (id=%s)"), *ToolCall.ToolName, *ToolCall.Id);
    }
    else
    {
        UE_LOG(LogChatSession, Log, TEXT("Tool call received: %s"), *ToolCall.ToolName);
    }
    
    if (!MCPClient.IsValid())
    {
        UE_LOG(LogChatSession, Error, TEXT("MCP client not available for tool call"));
        return;
    }
    
    // Increment pending tool call count
    PendingToolCallCount++;
    UE_LOG(LogChatSession, Log, TEXT("Pending tool calls: %d"), PendingToolCallCount);
    
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
    
    // Execute the tool via MCP
    MCPClient->ExecuteTool(ToolCall, FOnToolExecuted::CreateLambda(
        [this, ToolCallCopy = ToolCall](bool bSuccess, const FMCPToolResult& Result)
        {
            UE_LOG(LogChatSession, Log, TEXT("Tool result for %s: success=%d, content length=%d"), 
                *ToolCallCopy.Id, bSuccess, Result.Content.Len());
            
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
            
            // Add tool result as a separate "tool" message
            FChatMessage ToolResultMsg(TEXT("tool"), bSuccess ? Result.Content : Result.ErrorMessage);
            ToolResultMsg.ToolCallId = ToolCallCopy.Id;
            Messages.Add(ToolResultMsg);
            OnMessageAdded.ExecuteIfBound(ToolResultMsg);
            
            // Decrement pending tool call count
            PendingToolCallCount--;
            UE_LOG(LogChatSession, Log, TEXT("Tool completed. Pending tool calls remaining: %d"), PendingToolCallCount);
            
            // Only send follow-up when ALL tool calls have completed
            if (PendingToolCallCount <= 0)
            {
                PendingToolCallCount = 0; // Safety reset
                
                // Check if summarization is needed after tool results (they can be large)
                TriggerSummarizationIfNeeded();
                
                UE_LOG(LogChatSession, Log, TEXT("All tool calls completed, sending follow-up request"));
                SendFollowUpAfterToolCall();
            }
        }));
}

void FChatSession::SendFollowUpAfterToolCall()
{
    // Increment tool call iteration counter
    ToolCallIterationCount++;
    
    if (IsDebugModeEnabled())
    {
        UE_LOG(LogChatSession, Log, TEXT("========== FOLLOW-UP REQUEST =========="));
        UE_LOG(LogChatSession, Log, TEXT("Sending follow-up request after tool call completion (iteration %d/%d)"), 
            ToolCallIterationCount, MaxToolCallIterations);
    }
    
    // Create a new assistant message for the follow-up response
    FChatMessage AssistantMsg(TEXT("assistant"), TEXT(""));
    AssistantMsg.bIsStreaming = true;
    CurrentStreamingMessageIndex = Messages.Add(AssistantMsg);
    OnMessageAdded.ExecuteIfBound(AssistantMsg);
    
    // Build messages for API (includes the tool result)
    TArray<FChatMessage> ApiMessages = BuildApiMessages();
    
    if (IsDebugModeEnabled())
    {
        UE_LOG(LogChatSession, Log, TEXT("Built %d API messages for follow-up"), ApiMessages.Num());
    }
    
    // Get available tools - but if we've hit the iteration limit, send empty tools to force text response
    TArray<FMCPTool> Tools;
    if (ToolCallIterationCount < MaxToolCallIterations)
    {
        Tools = GetAvailableTools();
    }
    else
    {
        UE_LOG(LogChatSession, Warning, TEXT("Max tool call iterations (%d) reached - forcing text response (no tools)"), MaxToolCallIterations);
    }
    
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

void FChatSession::ResetChat()
{
    CancelRequest();
    Messages.Empty();
    
    // Reset usage stats
    UsageStats.Reset();
    
    // Delete history file
    FString HistoryPath = GetHistoryFilePath();
    if (FPaths::FileExists(HistoryPath))
    {
        IFileManager::Get().Delete(*HistoryPath);
    }
    
    OnChatReset.ExecuteIfBound();
    UE_LOG(LogChatSession, Log, TEXT("Chat reset"));
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
    // Approximate: ~4 characters per token for English text
    // This is a rough estimate; actual tokenization varies by model
    return FMath::CeilToInt(Text.Len() / 4.0f);
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
    
    int32 AvailableTokens = GetCurrentModelContextLength() - ReservedResponseTokens;
    int32 UsedTokens = EstimateTokenCount(SystemPrompt);
    
    // Always include system prompt
    ApiMessages.Add(FChatMessage(TEXT("system"), SystemPrompt));
    
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

void FChatSession::InitializeMCP(bool bEngineMode)
{
    if (bMCPInitialized)
    {
        UE_LOG(LogChatSession, Log, TEXT("MCP already initialized"));
        return;
    }
    
    MCPClient = MakeShared<FMCPClient>();
    MCPClient->Initialize(bEngineMode);
    
    // Discover available tools
    MCPClient->DiscoverTools(FOnToolsDiscovered::CreateLambda([this](bool bSuccess, const TArray<FMCPTool>& Tools)
    {
        bMCPInitialized = true;
        UE_LOG(LogChatSession, Log, TEXT("MCP initialized with %d tools"), Tools.Num());
        OnMCPToolsReady.ExecuteIfBound(bSuccess, Tools.Num());
    }));
}

void FChatSession::ReinitializeMCP(bool bEngineMode)
{
    // Shutdown existing MCP if initialized
    if (MCPClient.IsValid())
    {
        MCPClient->Shutdown();
        MCPClient.Reset();
    }
    bMCPInitialized = false;
    
    UE_LOG(LogChatSession, Log, TEXT("Reinitializing MCP in %s mode"), bEngineMode ? TEXT("Engine") : TEXT("Local"));
    
    // Now initialize fresh
    InitializeMCP(bEngineMode);
}

const TArray<FMCPTool>& FChatSession::GetAvailableTools() const
{
    static TArray<FMCPTool> EmptyTools;
    if (MCPClient.IsValid())
    {
        return MCPClient->GetAvailableTools();
    }
    return EmptyTools;
}

int32 FChatSession::GetMCPToolCount() const
{
    if (MCPClient.IsValid())
    {
        return MCPClient->GetToolCount();
    }
    return 0;
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
    int32 MaxIterations = DefaultMaxToolCallIterations; // Default 25
    GConfig->GetInt(TEXT("VibeUE"), TEXT("MaxToolCallIterations"), MaxIterations, GEditorPerProjectIni);
    return FMath::Clamp(MaxIterations, 5, 100);
}

void FChatSession::SaveMaxToolCallIterationsToConfig(int32 MaxIterations)
{
    MaxIterations = FMath::Clamp(MaxIterations, 5, 100);
    GConfig->SetInt(TEXT("VibeUE"), TEXT("MaxToolCallIterations"), MaxIterations, GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
}

void FChatSession::ApplyLLMParametersToClient()
{
    if (VibeUEClient.IsValid())
    {
        VibeUEClient->SetTemperature(GetTemperatureFromConfig());
        VibeUEClient->SetTopP(GetTopPFromConfig());
        VibeUEClient->SetMaxTokens(GetMaxTokensFromConfig());
        
        UE_LOG(LogChatSession, Log, TEXT("Applied LLM params: temperature=%.2f, top_p=%.2f, max_tokens=%d"),
            VibeUEClient->GetTemperature(), VibeUEClient->GetTopP(), VibeUEClient->GetMaxTokens());
    }
}
