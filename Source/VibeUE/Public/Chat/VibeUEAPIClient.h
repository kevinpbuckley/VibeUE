// Copyright 2025 Vibe AI. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Chat/LLMClientBase.h"

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
 * 
 * Inherits streaming/SSE parsing from FLLMClientBase
 */
class VIBEUE_API FVibeUEAPIClient : public FLLMClientBase, public TSharedFromThis<FVibeUEAPIClient>
{
public:
    FVibeUEAPIClient();
    virtual ~FVibeUEAPIClient() = default;

    //~ Begin ILLMClient Interface
    virtual FLLMProviderInfo GetProviderInfo() const override;
    virtual void SetApiKey(const FString& InApiKey) override;
    virtual bool HasApiKey() const override;
    virtual bool SupportsModelFetching() const override { return false; }
    //~ End ILLMClient Interface

    /** Set the API endpoint URL */
    void SetEndpointUrl(const FString& InUrl);

    /** Get the current endpoint URL */
    const FString& GetEndpointUrl() const { return EndpointUrl; }

    /** Get the default VibeUE API endpoint */
    static FString GetDefaultEndpoint();

    /** Get the default system prompt */
    static FString GetDefaultSystemPrompt();

protected:
    //~ Begin FLLMClientBase Interface
    virtual TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> BuildHttpRequest(
        const TArray<FChatMessage>& Messages,
        const FString& ModelId,
        const TArray<FMCPTool>& Tools
    ) override;
    
    virtual FString ProcessErrorResponse(int32 ResponseCode, const FString& ResponseBody) override;
    //~ End FLLMClientBase Interface

private:
    /** API key for VibeUE API (X-API-Key header) */
    FString ApiKey;

    /** API endpoint URL */
    FString EndpointUrl;

    /** HTTP headers */
    static const FString ContentTypeHeader;
    static const FString ApiKeyHeader;
};
