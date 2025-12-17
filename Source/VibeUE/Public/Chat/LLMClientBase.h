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
    FOnLLMThinkingStatus CurrentOnThinkingStatus;
    FOnLLMToolPreparing CurrentOnToolPreparing;
    
public:
    /** Set optional thinking status callback (called when <thinking> blocks start/end) */
    void SetThinkingStatusCallback(FOnLLMThinkingStatus InCallback) { CurrentOnThinkingStatus = InCallback; }
    
    /** Set optional tool preparing callback (called when tool name detected before full args) */
    void SetToolPreparingCallback(FOnLLMToolPreparing InCallback) { CurrentOnToolPreparing = InCallback; }

private:
    /** Handle HTTP request progress (streaming data) */
    void HandleRequestProgress(FHttpRequestPtr Request, uint64 BytesSent, uint64 BytesReceived);

    /** Handle HTTP request completion */
    void HandleRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);

    /** Process SSE data from the stream */
    void ProcessSSEData(const FString& Data);

    /** Process a single SSE JSON chunk */
    void ProcessSSEChunk(const FString& JsonData);

    /** Filter tool_call tags from content (keeps thinking tags visible) */
    FString FilterToolCallTags(const FString& Content);

    /** Helper to filter a specific tag block from content */
    FString FilterTagBlock(const FString& Content, const FString& OpenTag, const FString& CloseTag, bool& bInBlock);

    /** Fire accumulated tool calls */
    void FirePendingToolCalls();

    /** Process a non-streaming JSON response */
    void ProcessNonStreamingResponse(const FString& ResponseContent);

    /** Current HTTP request */
    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> CurrentRequest;

    /** Buffer for accumulating streaming response */
    FString StreamBuffer;

    /** Accumulated content from response (for non-streaming or final content) */
    FString AccumulatedContent;

    /** Accumulated tool calls during streaming */
    TMap<int32, FMCPToolCall> PendingToolCalls;

    /** Flag: tool calls detected in stream (suppress content after this) */
    bool bToolCallsDetectedInStream;

    /** Flag: currently inside a <tool_call> block (Qwen models output these in text) */
    bool bInToolCallBlock;

public:
    /** Get the accumulated response content (for non-streaming summarization) */
    const FString& GetLastAccumulatedResponse() const { return AccumulatedContent; }

    /** 
     * Sanitize a string for LLM communication - removes NUL characters and other problematic bytes
     * Call this on any content that might contain binary data or encoding artifacts
     */
    static FString SanitizeForLLM(const FString& Input);
    
    /**
     * Load system prompt from vibeue.instructions.md file
     * Searches in multiple locations (project plugins, engine marketplace)
     * Falls back to built-in prompt if file not found
     * @return The system prompt content
     */
    static FString LoadSystemPromptFromFile();
};
