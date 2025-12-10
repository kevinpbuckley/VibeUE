// Copyright 2025 Vibe AI. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Chat/ChatTypes.h"
#include "Chat/ILLMClient.h"
#include "Chat/OpenRouterClient.h"
#include "Chat/VibeUEAPIClient.h"
#include "Chat/MCPClient.h"

DECLARE_LOG_CATEGORY_EXTERN(LogChatSession, Log, All);

/**
 * Available LLM providers
 */
UENUM()
enum class ELLMProvider : uint8
{
    VibeUE,      // VibeUE API (default)
    OpenRouter   // OpenRouter API
};

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
 * Delegate called when summarization starts
 */
DECLARE_DELEGATE_OneParam(FOnSummarizationStarted, const FString& /* Reason */);

/**
 * Delegate called when summarization completes
 */
DECLARE_DELEGATE_TwoParams(FOnSummarizationComplete, bool /* bSuccess */, const FString& /* Summary */);

/**
 * Delegate called when token budget is updated
 */
DECLARE_DELEGATE_ThreeParams(FOnTokenBudgetUpdated, int32 /* CurrentTokens */, int32 /* MaxTokens */, float /* UtilizationPercent */);

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
    
    /** Set VibeUE API key */
    void SetVibeUEApiKey(const FString& ApiKey);
    
    /** Check if API key is configured (for current provider) */
    bool HasApiKey() const;
    
    /** Get OpenRouter API key from config */
    static FString GetApiKeyFromConfig();
    
    /** Save OpenRouter API key to config */
    static void SaveApiKeyToConfig(const FString& ApiKey);
    
    /** Get VibeUE API key from config */
    static FString GetVibeUEApiKeyFromConfig();
    
    /** Save VibeUE API key to config */
    static void SaveVibeUEApiKeyToConfig(const FString& ApiKey);
    
    /** Get VibeUE API endpoint from config */
    static FString GetVibeUEEndpointFromConfig();
    
    /** Save VibeUE API endpoint to config */
    static void SaveVibeUEEndpointToConfig(const FString& Endpoint);
    
    /** Get current LLM provider */
    ELLMProvider GetCurrentProvider() const { return CurrentProvider; }
    
    /** Set current LLM provider */
    void SetCurrentProvider(ELLMProvider Provider);
    
    /** Get provider from config */
    static ELLMProvider GetProviderFromConfig();
    
    /** Save provider to config */
    static void SaveProviderToConfig(ELLMProvider Provider);
    
    /** Get all available LLM providers */
    static TArray<FLLMProviderInfo> GetAvailableProviders();
    
    /** Get provider info for current provider */
    FLLMProviderInfo GetCurrentProviderInfo() const;
    
    /** Check if current provider supports model selection */
    bool SupportsModelSelection() const;
    
    /** Get estimated token count for current conversation */
    int32 GetEstimatedTokenCount() const;
    
    /** Get context window utilization percentage */
    float GetContextUtilization() const;
    
    /** Get current model context length */
    int32 GetModelContextLength() const;
    
    /** Get token budget (max tokens available for conversation) */
    int32 GetTokenBudget() const;
    
    /** Check if conversation is near context limit */
    bool IsNearContextLimit(float ThresholdPercent = 0.8f) const;
    
    /** Trigger summarization if context is approaching limit */
    void TriggerSummarizationIfNeeded();
    
    /** Force summarization of conversation */
    void ForceSummarize();
    
    /** Check if summarization is in progress */
    bool IsSummarizationInProgress() const { return bIsSummarizing; }
    
    /** Get the current conversation summary (if any) */
    const FString& GetConversationSummary() const { return ConversationSummary; }
    
    /** Get the summarization threshold from config */
    static float GetSummarizationThresholdFromConfig();
    
    /** Save the summarization threshold to config */
    static void SaveSummarizationThresholdToConfig(float Threshold);
    
    /** Get recent messages to keep after summarization from config */
    static int32 GetRecentMessagesToKeepFromConfig();
    
    /** Save recent messages to keep setting to config */
    static void SaveRecentMessagesToKeepToConfig(int32 Count);
    
    /** Check if auto-summarization is enabled */
    static bool IsAutoSummarizeEnabled();
    
    /** Set auto-summarization enabled */
    static void SetAutoSummarizeEnabled(bool bEnabled);
    
    /** Initialize MCP client and discover tools */
    void InitializeMCP(bool bEngineMode);
    
    /** Reinitialize MCP client (shutdown and restart with new mode) */
    void ReinitializeMCP(bool bEngineMode);
    
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
    
    // ============ LLM Generation Parameters ============
    
    /** Get/Set temperature (0.0-2.0, lower = more deterministic) */
    static float GetTemperatureFromConfig();
    static void SaveTemperatureToConfig(float Temperature);
    
    /** Get/Set top_p (0.0-1.0, nucleus sampling) */
    static float GetTopPFromConfig();
    static void SaveTopPToConfig(float TopP);
    
    /** Get/Set max_tokens (256-16384) */
    static int32 GetMaxTokensFromConfig();
    static void SaveMaxTokensToConfig(int32 MaxTokens);
    
    /** Apply LLM parameters to the VibeUE client */
    void ApplyLLMParametersToClient();

    // Delegates
    FOnMessageAdded OnMessageAdded;
    FOnMessageUpdated OnMessageUpdated;
    FOnChatReset OnChatReset;
    FOnChatError OnChatError;
    FOnMCPToolsReady OnMCPToolsReady;
    FOnSummarizationStarted OnSummarizationStarted;
    FOnSummarizationComplete OnSummarizationComplete;
    FOnTokenBudgetUpdated OnTokenBudgetUpdated;

private:
    /** OpenRouter HTTP client */
    TSharedPtr<FOpenRouterClient> OpenRouterClient;
    
    /** VibeUE API HTTP client */
    TSharedPtr<FVibeUEAPIClient> VibeUEClient;
    
    /** Current LLM provider */
    ELLMProvider CurrentProvider = ELLMProvider::VibeUE;
    
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
    
    /** Whether summarization is currently in progress */
    bool bIsSummarizing = false;
    
    /** Message index where summary was generated (summarized up to this point) */
    int32 SummarizedUpToMessageIndex = -1;
    
    /** Request summarization from the LLM */
    void RequestSummarization();
    
    /** Handle summarization response from LLM */
    void HandleSummarizationResponse(const FString& Summary);
    
    /** Apply summary to conversation history */
    void ApplySummaryToHistory(const FString& Summary);
    
    /** Build the summarization prompt */
    FString BuildSummarizationPrompt() const;
    
    /** Build messages to be summarized (excluding recent ones) */
    TArray<FChatMessage> BuildMessagesToSummarize() const;
    
    /** Broadcast token budget update */
    void BroadcastTokenBudgetUpdate();
    
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
    
    /** Handle summarization stream complete */
    void OnSummarizationStreamComplete(bool bSuccess);
    
    /** Handle summarization stream error */
    void OnSummarizationStreamError(const FString& ErrorMessage);
    
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
    
    /** Number of tool call iterations (follow-up rounds) */
    int32 ToolCallIterationCount = 0;
    
    /** Maximum allowed tool call iterations before forcing a text response */
    static constexpr int32 MaxToolCallIterations = 10;
    
    /** Usage statistics tracking */
    FLLMUsageStats UsageStats;
};
