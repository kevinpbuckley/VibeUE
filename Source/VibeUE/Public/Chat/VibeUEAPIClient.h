// Copyright Buckley Builds LLC 2026 All Rights Reserved.

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

    /** Set LLM generation parameters */
    void SetTemperature(float InTemperature) { Temperature = FMath::Clamp(InTemperature, 0.0f, 2.0f); }
    void SetTopP(float InTopP) { TopP = FMath::Clamp(InTopP, 0.0f, 1.0f); }
    void SetMaxTokens(int32 InMaxTokens) { MaxTokens = FMath::Clamp(InMaxTokens, 256, 16384); }
    void SetParallelToolCalls(bool bInParallelToolCalls) { bParallelToolCalls = bInParallelToolCalls; }
    
    /** Get LLM generation parameters */
    float GetTemperature() const { return Temperature; }
    float GetTopP() const { return TopP; }
    int32 GetMaxTokens() const { return MaxTokens; }
    bool GetParallelToolCalls() const { return bParallelToolCalls; }
    
    /** Fetch model info from /v1/models endpoint */
    void FetchModelInfo(TFunction<void(bool bSuccess, int32 ContextLength, const FString& ModelId)> OnComplete);

    /** Count tokens in text using /v1/tokenize endpoint */
    void CountTokens(const FString& Text, TFunction<void(bool bSuccess, int32 TokenCount)> OnComplete);

    /** Count tokens in messages using /v1/tokenize endpoint */
    void CountTokensInMessages(const TArray<FChatMessage>& Messages, const FString& ModelId, TFunction<void(bool bSuccess, int32 TokenCount)> OnComplete);

    /** Default values (optimized for coding assistants) */
    static constexpr float DefaultTemperature = 0.2f;
    static constexpr float DefaultTopP = 0.95f;
    static constexpr int32 DefaultMaxTokens = 8192;
    static constexpr int32 MinMaxTokens = 256;
    static constexpr int32 MaxMaxTokens = 16384;
    static constexpr bool DefaultParallelToolCalls = true;

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
    
    /** LLM generation parameters */
    float Temperature = DefaultTemperature;
    float TopP = DefaultTopP;
    int32 MaxTokens = DefaultMaxTokens;
    bool bParallelToolCalls = DefaultParallelToolCalls;

    /** HTTP headers */
    static const FString ContentTypeHeader;
    static const FString ApiKeyHeader;
};
