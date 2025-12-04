// Copyright 2025 Vibe AI. All Rights Reserved.

#include "Chat/ChatSession.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/ConfigCacheIni.h"

DEFINE_LOG_CATEGORY(LogChatSession);

FChatSession::FChatSession()
    : CurrentModelId(TEXT("x-ai/grok-4.1-fast:free"))  // Default to fast free model
    , MaxContextMessages(50)
    , MaxContextTokens(128000)  // Default to 128K, will be updated based on model
    , ReservedResponseTokens(4000)
{
    Client = MakeShared<FOpenRouterClient>();
    SystemPrompt = FOpenRouterClient::GetDefaultSystemPrompt();
}

FChatSession::~FChatSession()
{
    Shutdown();
}

void FChatSession::Initialize()
{
    // Load API key from config
    FString ApiKey = GetApiKeyFromConfig();
    if (!ApiKey.IsEmpty())
    {
        Client->SetApiKey(ApiKey);
    }
    
    // Load chat history
    LoadHistory();
    
    UE_LOG(LogChatSession, Log, TEXT("Chat session initialized with %d messages"), Messages.Num());
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
        OnChatError.ExecuteIfBound(TEXT("Please set your OpenRouter API key in Editor Preferences → Plugins → VibeUE"));
        return;
    }
    
    if (IsRequestInProgress())
    {
        OnChatError.ExecuteIfBound(TEXT("Please wait for the current response to complete"));
        return;
    }
    
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
    
    // Send request with tools
    Client->SendChatRequest(
        ApiMessages,
        CurrentModelId,
        Tools,
        FOnStreamChunk::CreateSP(this, &FChatSession::OnStreamChunk),
        FOnStreamComplete::CreateSP(this, &FChatSession::OnStreamComplete),
        FOnStreamError::CreateSP(this, &FChatSession::OnStreamError),
        FOnToolCall::CreateSP(this, &FChatSession::OnToolCall),
        FOnUsageReceived::CreateLambda([this](int32 PromptTokens, int32 CompletionTokens)
        {
            UpdateUsageStats(PromptTokens, CompletionTokens);
        })
    );
    
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
        Messages[CurrentStreamingMessageIndex].bIsStreaming = false;
        OnMessageUpdated.ExecuteIfBound(CurrentStreamingMessageIndex, Messages[CurrentStreamingMessageIndex]);
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
    // Create a new assistant message for the follow-up response
    FChatMessage AssistantMsg(TEXT("assistant"), TEXT(""));
    AssistantMsg.bIsStreaming = true;
    CurrentStreamingMessageIndex = Messages.Add(AssistantMsg);
    OnMessageAdded.ExecuteIfBound(AssistantMsg);
    
    // Build messages for API (includes the tool result)
    TArray<FChatMessage> ApiMessages = BuildApiMessages();
    
    // Get available tools
    TArray<FMCPTool> Tools = GetAvailableTools();
    
    // Send follow-up request
    Client->SendChatRequest(
        ApiMessages,
        CurrentModelId,
        Tools,
        FOnStreamChunk::CreateSP(this, &FChatSession::OnStreamChunk),
        FOnStreamComplete::CreateSP(this, &FChatSession::OnStreamComplete),
        FOnStreamError::CreateSP(this, &FChatSession::OnStreamError),
        FOnToolCall::CreateSP(this, &FChatSession::OnToolCall),
        FOnUsageReceived::CreateLambda([this](int32 PromptTokens, int32 CompletionTokens)
        {
            UpdateUsageStats(PromptTokens, CompletionTokens);
        })
    );
    
    // Increment request count
    UsageStats.RequestCount++;
}

void FChatSession::ResetChat()
{
    CancelRequest();
    Messages.Empty();
    
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
    Client->FetchModels(FOnModelsFetched::CreateLambda([this, OnComplete](bool bSuccess, const TArray<FOpenRouterModel>& Models)
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
    return Client.IsValid() && Client->IsRequestInProgress();
}

void FChatSession::CancelRequest()
{
    if (Client.IsValid())
    {
        Client->CancelRequest();
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
    Client->SetApiKey(ApiKey);
    SaveApiKeyToConfig(ApiKey);
}

bool FChatSession::HasApiKey() const
{
    return Client.IsValid() && Client->HasApiKey();
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
    if (CurrentModelId.Contains(TEXT("grok")))
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
