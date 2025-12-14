// Copyright 2025 Vibe AI. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Chat/LLMClientBase.h"

DECLARE_LOG_CATEGORY_EXTERN(LogOpenRouterClient, Log, All);

// Type aliases for delegate compatibility
using FOnStreamChunk = FOnLLMStreamChunk;
using FOnStreamComplete = FOnLLMStreamComplete;
using FOnStreamError = FOnLLMStreamError;
using FOnToolCall = FOnLLMToolCall;
using FOnUsageReceived = FOnLLMUsageReceived;
using FOnModelsFetched = FOnLLMModelsFetched;

/**
 * HTTP client for OpenRouter API with SSE streaming support
 * 
 * Inherits streaming/SSE parsing from FLLMClientBase
 */
class VIBEUE_API FOpenRouterClient : public FLLMClientBase, public TSharedFromThis<FOpenRouterClient>
{
public:
    FOpenRouterClient();
    virtual ~FOpenRouterClient() = default;
    
    //~ Begin ILLMClient Interface
    virtual FLLMProviderInfo GetProviderInfo() const override;
    virtual void SetApiKey(const FString& InApiKey) override;
    virtual bool HasApiKey() const override;
    virtual bool SupportsModelFetching() const override { return true; }
    virtual void FetchModels(FOnLLMModelsFetched OnComplete) override;
    //~ End ILLMClient Interface
    
    /** Get the default system prompt */
    static FString GetDefaultSystemPrompt();
    
    /** Set parallel tool calls setting */
    void SetParallelToolCalls(bool bInParallelToolCalls) { bParallelToolCalls = bInParallelToolCalls; }
    
    /** Get parallel tool calls setting */
    bool GetParallelToolCalls() const { return bParallelToolCalls; }
    
    /** Default value for parallel tool calls */
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
    /** API key for OpenRouter */
    FString ApiKey;
    
    /** Whether to allow parallel tool calls (multiple tool calls in single response) */
    bool bParallelToolCalls = DefaultParallelToolCalls;
    
    /** Handle models fetch completion */
    void HandleModelsFetchComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully, FOnModelsFetched OnComplete);
    
    /** OpenRouter API endpoints */
    static const FString ModelsEndpoint;
    static const FString ChatEndpoint;
    
    /** HTTP headers */
    static const FString ContentTypeHeader;
    static const FString AuthorizationHeader;
};
