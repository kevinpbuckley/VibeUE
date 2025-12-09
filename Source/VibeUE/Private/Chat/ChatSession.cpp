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
    
    // Load chat history
    LoadHistory();
    
    UE_LOG(LogChatSession, Log, TEXT("Chat session initialized with %d messages, provider: %s"), 
        Messages.Num(), CurrentProvider == ELLMProvider::VibeUE ? TEXT("VibeUE") : TEXT("OpenRouter"));
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
    
    // Reset tool call iteration counter for new user message
    ToolCallIterationCount = 0;
    
    // Add user message
    FChatMessage UserMsg(TEXT("user"), UserMessage);
    Messages.Add(UserMsg);
    OnMessageAdded.ExecuteIfBound(UserMsg);
    
    // Create assistant message placeholder
    FChatMessage AssistantMsg(TEXT("assistant"), TEXT(""));
    AssistantMsg.bIsStreaming = true;
    CurrentStreamingMessageIndex = Messages.Add(AssistantMsg);
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
        Messages[CurrentStreamingMessageIndex].Content += Chunk;
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
    UE_LOG(LogChatSession, Log, TEXT("Tool call received: %s"), *ToolCall.ToolName);
    
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
        return 262144; // 256K for Qwen3-30B-A3B-Instruct
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
