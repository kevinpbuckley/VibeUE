// Copyright 2025 Vibe AI. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Chat/ChatTypes.h"
#include "Chat/MCPTypes.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

DECLARE_LOG_CATEGORY_EXTERN(LogOpenRouterClient, Log, All);

/**
 * Delegate called when a chunk of streamed response is received
 */
DECLARE_DELEGATE_OneParam(FOnStreamChunk, const FString& /* ChunkContent */);

/**
 * Delegate called when streaming completes
 */
DECLARE_DELEGATE_OneParam(FOnStreamComplete, bool /* bSuccess */);

/**
 * Delegate called when a tool call is requested by the LLM
 */
DECLARE_DELEGATE_OneParam(FOnToolCall, const FMCPToolCall& /* ToolCall */);

/**
 * Delegate called when model list is fetched
 */
DECLARE_DELEGATE_TwoParams(FOnModelsFetched, bool /* bSuccess */, const TArray<FOpenRouterModel>& /* Models */);

/**
 * Delegate called on streaming error
 */
DECLARE_DELEGATE_OneParam(FOnStreamError, const FString& /* ErrorMessage */);

/**
 * Delegate called when usage stats are received
 */
DECLARE_DELEGATE_TwoParams(FOnUsageReceived, int32 /* PromptTokens */, int32 /* CompletionTokens */);

/**
 * HTTP client for OpenRouter API with SSE streaming support
 */
class VIBEUE_API FOpenRouterClient : public TSharedFromThis<FOpenRouterClient>
{
public:
    FOpenRouterClient();
    ~FOpenRouterClient();
    
    /** Set the API key for authentication */
    void SetApiKey(const FString& InApiKey);
    
    /** Check if API key is configured */
    bool HasApiKey() const;
    
    /** Fetch available models from OpenRouter */
    void FetchModels(FOnModelsFetched OnComplete);
    
    /**
     * Send a chat completion request with streaming
     * @param Messages The conversation history
     * @param ModelId The model to use (e.g., "anthropic/claude-3.5-sonnet")
     * @param Tools Available tools for the LLM to call (optional)
     * @param OnChunk Called for each streamed chunk
     * @param OnComplete Called when streaming finishes
     * @param OnError Called if an error occurs
     * @param OnToolCall Called when LLM requests a tool call (optional)
     * @param OnUsage Called when usage stats are received (optional)
     */
    void SendChatRequest(
        const TArray<FChatMessage>& Messages,
        const FString& ModelId,
        const TArray<FMCPTool>& Tools,
        FOnStreamChunk OnChunk,
        FOnStreamComplete OnComplete,
        FOnStreamError OnError,
        FOnToolCall OnToolCall = FOnToolCall(),
        FOnUsageReceived OnUsage = FOnUsageReceived()
    );
    
    /** Cancel any in-progress streaming request */
    void CancelRequest();
    
    /** Check if a request is currently in progress */
    bool IsRequestInProgress() const;
    
    /** Get the default system prompt */
    static FString GetDefaultSystemPrompt();
    
private:
    /** API key for OpenRouter */
    FString ApiKey;
    
    /** Current streaming request (if any) */
    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> CurrentRequest;
    
    /** Buffer for incomplete SSE data */
    FString StreamBuffer;
    
    /** Delegates for current request */
    FOnStreamChunk CurrentOnChunk;
    FOnStreamComplete CurrentOnComplete;
    FOnStreamError CurrentOnError;
    FOnToolCall CurrentOnToolCall;
    FOnUsageReceived CurrentOnUsage;
    
    /** Accumulated tool calls from streaming response */
    TMap<int32, FMCPToolCall> PendingToolCalls;
    
    /** Flag to suppress content chunks once tool calls are detected in stream */
    bool bToolCallsDetectedInStream;
    
    /** Process SSE data from HTTP response */
    void ProcessSSEData(const FString& Data);
    
    /** Handle HTTP request progress (for streaming) */
    void HandleRequestProgress(FHttpRequestPtr Request, uint64 BytesSent, uint64 BytesReceived);
    
    /** Handle HTTP request completion */
    void HandleRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);
    
    /** Handle models fetch completion */
    void HandleModelsFetchComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully, FOnModelsFetched OnComplete);
    
    /** OpenRouter API endpoints */
    static const FString ModelsEndpoint;
    static const FString ChatEndpoint;
    
    /** HTTP headers */
    static const FString ContentTypeHeader;
    static const FString AuthorizationHeader;
};
