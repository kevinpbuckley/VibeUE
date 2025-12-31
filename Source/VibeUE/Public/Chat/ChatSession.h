// Copyright Buckley Builds LLC 2025 All Rights Reserved.

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
 * Delegate called when tools are ready (internal + MCP)
 */
DECLARE_DELEGATE_TwoParams(FOnToolsReady, bool /* bSuccess */, int32 /* ToolCount */);

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
 * Delegate called when tool call iteration limit is reached
 * UI should prompt user if they want to continue
 */
DECLARE_DELEGATE_TwoParams(FOnToolIterationLimitReached, int32 /* CurrentIteration */, int32 /* MaxIterations */);

/**
 * Delegate called when thinking state changes (model is reasoning)
 */
DECLARE_DELEGATE_OneParam(FOnThinkingStatusChanged, bool /* bIsThinking */);

/**
 * Delegate called when a tool is being prepared (name detected, args still streaming)
 */
DECLARE_DELEGATE_OneParam(FOnToolPreparing, const FString& /* ToolName */);

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
    
    /** Get estimated token count for current conversation (uses smart heuristic) */
    int32 GetEstimatedTokenCount() const;

    /** Get accurate token count from API for current conversation (async) */
    void GetAccurateTokenCount(TFunction<void(bool bSuccess, int32 TokenCount)> OnComplete);

    /** Get accurate token count from API for specific text (async) */
    void GetAccurateTokenCountForText(const FString& Text, TFunction<void(bool bSuccess, int32 TokenCount)> OnComplete);

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
    void InitializeMCP();
    
    /** Get MCP client */
    TSharedPtr<FMCPClient> GetMCPClient() const { return MCPClient; }
    
    /** Get VibeUE API client */
    TSharedPtr<FVibeUEAPIClient> GetVibeUEClient() const { return VibeUEClient; }
    
    /** Get all enabled tools for AI use (merged: internal + MCP, filtered by enabled state) */
    TArray<FMCPTool> GetAllEnabledTools() const;
    
    /** Get count of all enabled tools (internal + MCP) */
    int32 GetEnabledToolCount() const;
    
    /** Check if MCP is initialized */
    bool IsMCPInitialized() const;
    
    /** Initialize internal tools (reflection-based, from ToolRegistry) */
    void InitializeInternalTools();
    
    /** Get internal tools converted to MCP tool format (for API compatibility) */
    TArray<FMCPTool> GetInternalToolsAsMCP() const;
    
    /** Get usage statistics */
    const FLLMUsageStats& GetUsageStats() const { return UsageStats; }
    
    /** Update usage stats from response */
    void UpdateUsageStats(int32 PromptTokens, int32 CompletionTokens);
    
    /** Check if debug mode is enabled */
    static bool IsDebugModeEnabled();
    
    /** Set debug mode */
    static void SetDebugModeEnabled(bool bEnabled);
    
    /** Check if file logging is enabled (chat and raw LLM logs) */
    static bool IsFileLoggingEnabled();
    
    /** Set file logging enabled */
    static void SetFileLoggingEnabled(bool bEnabled);
    
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
    
    /** Get/Set max tool call iterations (10-500, default 200 like Copilot) */
    static int32 GetMaxToolCallIterationsFromConfig();
    static void SaveMaxToolCallIterationsToConfig(int32 MaxIterations);
    
    /** Set max tool iterations for current session (doesn't persist to config) */
    void SetMaxToolCallIterations(int32 NewMax);
    
    /** Get/Set parallel tool calls (true = LLM can return multiple tool calls at once) */
    static bool GetParallelToolCallsFromConfig();
    static void SaveParallelToolCallsToConfig(bool bParallelToolCalls);
    
    /** Apply LLM parameters to the VibeUE client */
    void ApplyLLMParametersToClient();

    /** Continue tool calls after iteration limit was reached (user chose to continue) */
    void ContinueAfterIterationLimit();

    /** Check if session is waiting for user to decide whether to continue after iteration limit */
    bool IsWaitingForUserToContinue() const { return bWaitingForUserToContinue; }

    // ============ MCP Server Settings (expose internal tools via Streamable HTTP) ============
    
    /** Get/Set MCP Server enabled state (default: true) */
    static bool GetMCPServerEnabledFromConfig();
    static void SaveMCPServerEnabledToConfig(bool bEnabled);
    
    /** Get/Set MCP Server port (default: 8080) */
    static int32 GetMCPServerPortFromConfig();
    static void SaveMCPServerPortToConfig(int32 Port);
    
    /** Get MCP Server API key from config */
    static FString GetMCPServerApiKeyFromConfig();
    
    /** Save MCP Server API key to config */
    static void SaveMCPServerApiKeyToConfig(const FString& ApiKey);
    
    /** Default MCP Server port */
    static constexpr int32 DefaultMCPServerPort = 8080;

    // Delegates
    FOnMessageAdded OnMessageAdded;
    FOnMessageUpdated OnMessageUpdated;
    FOnChatReset OnChatReset;
    FOnChatError OnChatError;
    FOnToolsReady OnToolsReady;
    FOnSummarizationStarted OnSummarizationStarted;
    FOnSummarizationComplete OnSummarizationComplete;
    FOnTokenBudgetUpdated OnTokenBudgetUpdated;
    FOnToolIterationLimitReached OnToolIterationLimitReached;
    FOnThinkingStatusChanged OnThinkingStatusChanged;
    FOnToolPreparing OnToolPreparing;

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
    
    /** Whether a follow-up request is pending after summarization completes */
    bool bPendingFollowUpAfterSummarization = false;
    
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
    
    /** Smart truncate tool result using Copilot-style approach:
     *  - Token-based limits
     *  - Keep 40% from beginning, 60% from end
     *  - Insert truncation message in middle
     */
    FString SmartTruncateToolResult(const FString& Content, const FString& ToolName) const;
    
    /** Format thinking/reasoning blocks with visual indicator.
     *  Replaces <think>content</think> with styled format like "💭 **Thinking:** content"
     *  Keeps thinking content visible but formatted, rather than removing it.
     */
    static FString FormatThinkingBlocks(const FString& Text);
    
    /** Strip thinking/reasoning tags from model output (removes entirely).
     *  Some models (Qwen3, Claude, etc.) output chain-of-thought in special tags.
     *  This content should not be included in final output.
     */
    static FString StripThinkingTags(const FString& Text);
    
    /** Extract thinking/reasoning content from model output.
     *  Returns the content from <think>...</think> and similar tags.
     *  Used for logging and potential UI display in a collapsible section.
     */
    static FString ExtractThinkingContent(const FString& Text);
    
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
    
    /** Internal tools cached in MCP format (from ToolRegistry) */
    mutable TArray<FMCPTool> CachedInternalTools;
    
    /** Whether internal tools have been cached */
    mutable bool bInternalToolsCached = false;
    
    /** Number of tool calls pending completion */
    int32 PendingToolCallCount = 0;
    
    /** Queue for sequential tool execution */
    TArray<FMCPToolCall> ToolCallQueue;
    
    /** Whether a tool call is currently being executed */
    bool bIsExecutingTool = false;
    
    /** Execute the next tool in the queue (sequential execution) */
    void ExecuteNextToolInQueue();
    
    /** Number of tool call iterations (follow-up rounds) */
    int32 ToolCallIterationCount = 0;
    
    /** Maximum allowed tool call iterations (soft limit - shows warning but continues) */
    int32 MaxToolCallIterations = 200;
    
    /** Whether we're waiting for user to decide if they want to continue after hitting iteration limit */
    bool bWaitingForUserToContinue = false;
    
    /** Default value for MaxToolCallIterations - same as Copilot (200) */
    static constexpr int32 DefaultMaxToolCallIterations = 200;
    
    /** Usage statistics tracking */
    FLLMUsageStats UsageStats;
    
    // Loop detection is handled via prompt-based self-awareness instructions
    // See vibeue.instructions.md for details
};
