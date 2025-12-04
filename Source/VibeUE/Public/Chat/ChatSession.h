// Copyright 2025 Vibe AI. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Chat/ChatTypes.h"
#include "Chat/OpenRouterClient.h"
#include "Chat/MCPClient.h"

DECLARE_LOG_CATEGORY_EXTERN(LogChatSession, Log, All);

/**
 * Usage statistics from LLM response
 */
struct FLLMUsageStats
{
    int32 PromptTokens = 0;
    int32 CompletionTokens = 0;
    int32 TotalTokens = 0;
    int32 RequestCount = 0;
    int32 TotalPromptTokens = 0;
    int32 TotalCompletionTokens = 0;
    
    void Reset()
    {
        PromptTokens = 0;
        CompletionTokens = 0;
        TotalTokens = 0;
        RequestCount = 0;
        TotalPromptTokens = 0;
        TotalCompletionTokens = 0;
    }
};

/**
 * Delegate called when MCP tools are ready
 */
DECLARE_DELEGATE_TwoParams(FOnMCPToolsReady, bool /* bSuccess */, int32 /* ToolCount */);

/**
 * Delegate called when a new message is added to the conversation
 */
DECLARE_DELEGATE_OneParam(FOnMessageAdded, const FChatMessage& /* Message */);

/**
 * Delegate called when a message is updated (during streaming)
 */
DECLARE_DELEGATE_TwoParams(FOnMessageUpdated, int32 /* MessageIndex */, const FChatMessage& /* Message */);

/**
 * Delegate called when chat is reset
 */
DECLARE_DELEGATE(FOnChatReset);

/**
 * Delegate called when an error occurs
 */
DECLARE_DELEGATE_OneParam(FOnChatError, const FString& /* ErrorMessage */);

/**
 * Manages conversation state, message history, and persistence
 */
class VIBEUE_API FChatSession : public TSharedFromThis<FChatSession>
{
public:
    FChatSession();
    ~FChatSession();
    
    /** Initialize the session, loading any persisted history */
    void Initialize();
    
    /** Shutdown the session, saving history */
    void Shutdown();
    
    /** Send a user message and get AI response */
    void SendMessage(const FString& UserMessage);
    
    /** Reset the conversation (clears history and persistence) */
    void ResetChat();
    
    /** Get all messages in the conversation */
    const TArray<FChatMessage>& GetMessages() const { return Messages; }
    
    /** Get the current model ID */
    const FString& GetCurrentModel() const { return CurrentModelId; }
    
    /** Set the current model ID */
    void SetCurrentModel(const FString& ModelId);
    
    /** Get available models (fetches from API if needed) */
    void FetchAvailableModels(FOnModelsFetched OnComplete);
    
    /** Get cached models (may be empty if not yet fetched) */
    const TArray<FOpenRouterModel>& GetCachedModels() const { return CachedModels; }
    
    /** Check if a request is in progress */
    bool IsRequestInProgress() const;
    
    /** Cancel any in-progress request */
    void CancelRequest();
    
    /** Set API key */
    void SetApiKey(const FString& ApiKey);
    
    /** Check if API key is configured */
    bool HasApiKey() const;
    
    /** Get API key from config */
    static FString GetApiKeyFromConfig();
    
    /** Save API key to config */
    static void SaveApiKeyToConfig(const FString& ApiKey);
    
    /** Get estimated token count for current conversation */
    int32 GetEstimatedTokenCount() const;
    
    /** Get context window utilization percentage */
    float GetContextUtilization() const;
    
    /** Get current model context length */
    int32 GetModelContextLength() const;
    
    /** Initialize MCP client and discover tools */
    void InitializeMCP(bool bEngineMode);
    
    /** Get MCP client */
    TSharedPtr<FMCPClient> GetMCPClient() const { return MCPClient; }
    
    /** Get available MCP tools */
    const TArray<FMCPTool>& GetAvailableTools() const;
    
    /** Get count of available MCP tools */
    int32 GetMCPToolCount() const;
    
    /** Check if MCP is initialized */
    bool IsMCPInitialized() const;
    
    /** Get usage statistics */
    const FLLMUsageStats& GetUsageStats() const { return UsageStats; }
    
    /** Update usage stats from response */
    void UpdateUsageStats(int32 PromptTokens, int32 CompletionTokens);
    
    /** Check if debug mode is enabled */
    static bool IsDebugModeEnabled();
    
    /** Set debug mode */
    static void SetDebugModeEnabled(bool bEnabled);

    // Delegates
    FOnMessageAdded OnMessageAdded;
    FOnMessageUpdated OnMessageUpdated;
    FOnChatReset OnChatReset;
    FOnChatError OnChatError;
    FOnMCPToolsReady OnMCPToolsReady;private:
    /** OpenRouter HTTP client */
    TSharedPtr<FOpenRouterClient> Client;
    
    /** Conversation messages */
    TArray<FChatMessage> Messages;
    
    /** Current model ID */
    FString CurrentModelId;
    
    /** Cached model list */
    TArray<FOpenRouterModel> CachedModels;
    
    /** Maximum messages to keep in history */
    int32 MaxContextMessages = 50;
    
    /** Maximum tokens for context (will be updated based on model) */
    int32 MaxContextTokens = 8000;
    
    /** Reserved tokens for response */
    int32 ReservedResponseTokens = 2000;
    
    /** System prompt */
    FString SystemPrompt;
    
    /** Summarized conversation context (used when history exceeds context window) */
    FString ConversationSummary;
    
    /** Load chat history from file */
    void LoadHistory();
    
    /** Save chat history to file */
    void SaveHistory();
    
    /** Get the persistence file path */
    FString GetHistoryFilePath() const;
    
    /** Handle streaming chunk */
    void OnStreamChunk(const FString& Chunk);
    
    /** Handle streaming complete */
    void OnStreamComplete(bool bSuccess);
    
    /** Handle streaming error */
    void OnStreamError(const FString& ErrorMessage);
    
    /** Handle tool call from LLM */
    void OnToolCall(const FMCPToolCall& ToolCall);
    
    /** Send follow-up request after tool execution to get LLM response */
    void SendFollowUpAfterToolCall();
    
    /** Index of the current assistant message being streamed */
    int32 CurrentStreamingMessageIndex = INDEX_NONE;
    
    /** Estimate token count for a string (approximate: ~4 chars per token) */
    static int32 EstimateTokenCount(const FString& Text);
    
    /** Get the current model's context length */
    int32 GetCurrentModelContextLength() const;
    
    /** Build messages array for API, respecting context window */
    TArray<FChatMessage> BuildApiMessages() const;
    
    /** Check if conversation needs summarization */
    bool NeedsSummarization() const;
    
    /** Request summarization of conversation history */
    void SummarizeConversation();
    
    /** MCP client for tool support */
    TSharedPtr<FMCPClient> MCPClient;
    
    /** Whether MCP has been initialized */
    bool bMCPInitialized = false;
    
    /** Number of tool calls pending completion */
    int32 PendingToolCallCount = 0;
    
    /** Usage statistics tracking */
    FLLMUsageStats UsageStats;
};
