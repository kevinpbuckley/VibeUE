// Copyright 2025 Vibe AI. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Chat/ILLMClient.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

DECLARE_LOG_CATEGORY_EXTERN(LogLLMClientBase, Log, All);

/**
 * Base class for LLM API clients with shared SSE streaming logic
 * 
 * This class handles common functionality:
 * - SSE (Server-Sent Events) parsing
 * - Thinking tag filtering (<thinking>, <think>)
 * - Tool call accumulation from streaming responses
 * - Request lifecycle management
 * 
 * Subclasses only need to implement:
 * - BuildHttpRequest() - Provider-specific request construction
 * - GetProviderInfo() - Provider identification
 * - SetApiKey()/HasApiKey() - Authentication
 */
class VIBEUE_API FLLMClientBase : public ILLMClient
{
public:
    FLLMClientBase();
    virtual ~FLLMClientBase();

    //~ Begin ILLMClient Interface
    virtual void SendChatRequest(
        const TArray<FChatMessage>& Messages,
        const FString& ModelId,
        const TArray<FMCPTool>& Tools,
        FOnLLMStreamChunk OnChunk,
        FOnLLMStreamComplete OnComplete,
        FOnLLMStreamError OnError,
        FOnLLMToolCall OnToolCall,
        FOnLLMUsageReceived OnUsage
    ) override;
    virtual void CancelRequest() override;
    virtual bool IsRequestInProgress() const override;
    //~ End ILLMClient Interface

protected:
    /**
     * Build the HTTP request for this provider
     * Subclasses implement this to set endpoint URL, headers, and body
     * @param Messages Conversation history
     * @param ModelId Model identifier (may be empty for single-model providers)
     * @param Tools Available MCP tools
     * @return Configured HTTP request ready to send, or nullptr on error
     */
    virtual TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> BuildHttpRequest(
        const TArray<FChatMessage>& Messages,
        const FString& ModelId,
        const TArray<FMCPTool>& Tools
    ) = 0;

    /**
     * Called when request fails before sending (e.g., missing API key)
     * Subclass can override to provide custom error handling
     */
    virtual void OnPreRequestError(const FString& ErrorMessage);

    /**
     * Process provider-specific error responses
     * @param ResponseCode HTTP response code
     * @param ResponseBody Response body text
     * @return Error message to display, or empty string if handled
     */
    virtual FString ProcessErrorResponse(int32 ResponseCode, const FString& ResponseBody);

    /** Reset all streaming state - call at start of new request */
    void ResetStreamingState();

    /** Current streaming delegates */
    FOnLLMStreamChunk CurrentOnChunk;
    FOnLLMStreamComplete CurrentOnComplete;
    FOnLLMStreamError CurrentOnError;
    FOnLLMToolCall CurrentOnToolCall;
    FOnLLMUsageReceived CurrentOnUsage;

private:
    /** Handle HTTP request progress (streaming data) */
    void HandleRequestProgress(FHttpRequestPtr Request, uint64 BytesSent, uint64 BytesReceived);

    /** Handle HTTP request completion */
    void HandleRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);

    /** Process SSE data from the stream */
    void ProcessSSEData(const FString& Data);

    /** Process a single SSE JSON chunk */
    void ProcessSSEChunk(const FString& JsonData);

    /** Filter thinking tags from content */
    FString FilterThinkingTags(const FString& Content);

    /** Fire accumulated tool calls */
    void FirePendingToolCalls();

    /** Current HTTP request */
    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> CurrentRequest;

    /** Buffer for accumulating streaming response */
    FString StreamBuffer;

    /** Accumulated tool calls during streaming */
    TMap<int32, FMCPToolCall> PendingToolCalls;

    /** Flag: tool calls detected in stream (suppress content after this) */
    bool bToolCallsDetectedInStream;

    /** Flag: currently inside a <thinking> block */
    bool bInThinkingBlock;
};
