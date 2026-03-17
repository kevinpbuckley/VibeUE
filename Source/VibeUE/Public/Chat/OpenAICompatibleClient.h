// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Chat/LLMClientBase.h"

DECLARE_LOG_CATEGORY_EXTERN(LogOpenAICompatibleClient, Log, All);

/** Authentication mode for OpenAI-compatible endpoints */
enum class ECustomAuthMode : uint8
{
    Bearer,   // Authorization: Bearer <key>  (vLLM, SGLang, most servers)
    XApiKey,  // X-API-Key: <key>             (some custom deployments)
    None      // No auth header               (Ollama, LM Studio, local servers)
};

/**
 * HTTP client for any OpenAI-compatible endpoint (Ollama, vLLM, LM Studio, etc.)
 *
 * Configurable endpoint URL, auth mode, model ID, and streaming toggle.
 * Inherits SSE parsing, chunk routing, and tool call accumulation from FLLMClientBase.
 */
class VIBEUE_API FOpenAICompatibleClient : public FLLMClientBase, public TSharedFromThis<FOpenAICompatibleClient>
{
public:
    FOpenAICompatibleClient();
    virtual ~FOpenAICompatibleClient() = default;

    //~ Begin ILLMClient Interface
    virtual FLLMProviderInfo GetProviderInfo() const override;
    virtual void SetApiKey(const FString& InApiKey) override;
    virtual bool HasApiKey() const override;
    virtual bool SupportsModelFetching() const override { return true; }
    virtual void FetchModels(FOnLLMModelsFetched OnComplete) override;
    //~ End ILLMClient Interface

    void SetEndpointUrl(const FString& InUrl) { EndpointUrl = InUrl; }
    FString GetEndpointUrl() const { return EndpointUrl; }

    void SetAuthMode(ECustomAuthMode InMode) { AuthMode = InMode; }
    ECustomAuthMode GetAuthMode() const { return AuthMode; }

    void SetConfiguredModelId(const FString& InModelId) { ConfiguredModelId = InModelId; }
    FString GetConfiguredModelId() const { return ConfiguredModelId; }

    void SetStreamingEnabled(bool bInStreaming) { bStreamingEnabled = bInStreaming; }
    bool IsStreamingEnabled() const { return bStreamingEnabled; }

    void SetTemperature(float InTemp) { Temperature = InTemp; }
    float GetTemperature() const { return Temperature; }

    void SetTopP(float InTopP) { TopP = InTopP; }
    float GetTopP() const { return TopP; }

    void SetMaxTokens(int32 InMaxTokens) { MaxTokens = InMaxTokens; }
    int32 GetMaxTokens() const { return MaxTokens; }

    /**
     * Context window size sent as num_ctx in the request body.
     * Ollama accepts this to override its default (typically 2048–4096 tokens).
     * Other OpenAI-compatible servers silently ignore it.
     * Default 32768 — large enough for 10 full tool schemas (~30KB) plus response.
     * Set to 0 to omit from the request (use server default).
     */
    void SetContextSize(int32 InContextSize) { ContextSize = InContextSize; }
    int32 GetContextSize() const { return ContextSize; }

    void SetParallelToolCalls(bool bInParallel) { bParallelToolCalls = bInParallel; }
    bool GetParallelToolCalls() const { return bParallelToolCalls; }

    /** Default endpoint — Ollama's standard local address */
    static const FString DefaultEndpoint;

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
    FString EndpointUrl;
    FString ApiKey;
    ECustomAuthMode AuthMode = ECustomAuthMode::Bearer;
    FString ConfiguredModelId;
    bool bStreamingEnabled = true;
    float Temperature = 0.2f;
    float TopP = 0.95f;
    int32 MaxTokens = 8192;
    int32 ContextSize = 32768;  // num_ctx sent to Ollama; 0 = omit (use server default)
    bool bParallelToolCalls = true;

    /** Derive /v1/models URL from the chat completions URL */
    FString GetModelsUrl() const;

    /** Apply the configured auth header to a request */
    void ApplyAuthHeader(TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request) const;

    /** Handle /v1/models response */
    void HandleModelsFetchComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully, FOnLLMModelsFetched OnComplete);

    static const FString ContentTypeHeader;
};
