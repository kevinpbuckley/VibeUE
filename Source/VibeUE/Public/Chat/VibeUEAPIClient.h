// Copyright 2025 Vibe AI. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Chat/ChatTypes.h"
#include "Chat/MCPTypes.h"
#include "Chat/ILLMClient.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVibeUEAPIClient, Log, All);

// Type aliases for delegate compatibility (Vibe-prefixed for potential legacy code)
using FOnVibeStreamChunk = FOnLLMStreamChunk;
using FOnVibeStreamComplete = FOnLLMStreamComplete;
using FOnVibeStreamError = FOnLLMStreamError;
using FOnVibeToolCall = FOnLLMToolCall;
using FOnVibeUsageReceived = FOnLLMUsageReceived;

/**
 * HTTP client for VibeUE API with SSE streaming support
 * Connects to the VibeUE-API service (OpenAI-compatible API with tool calling)
 * Implements ILLMClient interface for strategy pattern
 */
class VIBEUE_API FVibeUEAPIClient : public ILLMClient, public TSharedFromThis<FVibeUEAPIClient>
{
public:
    FVibeUEAPIClient();
    virtual ~FVibeUEAPIClient();

    //~ Begin ILLMClient Interface
    virtual FLLMProviderInfo GetProviderInfo() const override;
    virtual void SetApiKey(const FString& InApiKey) override;
    virtual bool HasApiKey() const override;
    virtual bool SupportsModelFetching() const override { return false; }
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

    /** Set the API endpoint URL */
    void SetEndpointUrl(const FString& InUrl);

    /** Get the current endpoint URL */
    const FString& GetEndpointUrl() const { return EndpointUrl; }

    /** Get the default VibeUE API endpoint */
    static FString GetDefaultEndpoint();

    /** Get the default system prompt (same as OpenRouter) */
    static FString GetDefaultSystemPrompt();

private:
    /** API key for VibeUE API (X-API-Key header) */
    FString ApiKey;

    /** API endpoint URL */
    FString EndpointUrl;

    /** Current streaming request (if any) */
    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> CurrentRequest;

    /** Buffer for incomplete SSE data */
    FString StreamBuffer;

    /** Delegates for current request */
    FOnVibeStreamChunk CurrentOnChunk;
    FOnVibeStreamComplete CurrentOnComplete;
    FOnVibeStreamError CurrentOnError;
    FOnVibeToolCall CurrentOnToolCall;
    FOnVibeUsageReceived CurrentOnUsage;

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

    /** HTTP headers */
    static const FString ContentTypeHeader;
    static const FString ApiKeyHeader;
};
