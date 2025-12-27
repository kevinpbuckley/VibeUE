// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Chat/ChatTypes.h"
#include "Chat/MCPTypes.h"

/**
 * Common delegates for LLM client callbacks
 */
DECLARE_DELEGATE_OneParam(FOnLLMStreamChunk, const FString& /* ChunkContent */);
DECLARE_DELEGATE_OneParam(FOnLLMStreamComplete, bool /* bSuccess */);
DECLARE_DELEGATE_OneParam(FOnLLMStreamError, const FString& /* ErrorMessage */);
DECLARE_DELEGATE_OneParam(FOnLLMToolCall, const FMCPToolCall& /* ToolCall */);
DECLARE_DELEGATE_TwoParams(FOnLLMUsageReceived, int32 /* PromptTokens */, int32 /* CompletionTokens */);
DECLARE_DELEGATE_TwoParams(FOnLLMModelsFetched, bool /* bSuccess */, const TArray<FOpenRouterModel>& /* Models */);
DECLARE_DELEGATE_OneParam(FOnLLMThinkingStatus, bool /* bIsThinking */);
DECLARE_DELEGATE_OneParam(FOnLLMToolPreparing, const FString& /* ToolName */);  // Called when tool call name detected (before args)

/**
 * Information about an LLM provider
 */
struct VIBEUE_API FLLMProviderInfo
{
    /** Provider identifier */
    FString Id;
    
    /** Display name for UI */
    FString DisplayName;
    
    /** Whether this provider supports model selection (multiple models) */
    bool bSupportsModelSelection = false;
    
    /** Default model ID (if applicable) */
    FString DefaultModelId;
    
    /** Description for tooltips */
    FString Description;
    
    FLLMProviderInfo() = default;
    
    FLLMProviderInfo(const FString& InId, const FString& InDisplayName, bool bInSupportsModelSelection = false, 
                     const FString& InDefaultModelId = TEXT(""), const FString& InDescription = TEXT(""))
        : Id(InId)
        , DisplayName(InDisplayName)
        , bSupportsModelSelection(bInSupportsModelSelection)
        , DefaultModelId(InDefaultModelId)
        , Description(InDescription)
    {
    }
};

/**
 * Abstract interface for LLM API clients
 * Implements the Strategy pattern for swappable LLM providers
 */
class VIBEUE_API ILLMClient
{
public:
    virtual ~ILLMClient() = default;
    
    //~ Provider Information
    
    /** Get information about this provider */
    virtual FLLMProviderInfo GetProviderInfo() const = 0;
    
    //~ Authentication
    
    /** Set the API key for authentication */
    virtual void SetApiKey(const FString& InApiKey) = 0;
    
    /** Check if API key is configured */
    virtual bool HasApiKey() const = 0;
    
    //~ Model Management
    
    /** Whether this provider supports fetching a list of models */
    virtual bool SupportsModelFetching() const { return false; }
    
    /** Fetch available models (optional - some providers have fixed models) */
    virtual void FetchModels(FOnLLMModelsFetched OnComplete) 
    { 
        // Default: return empty list (provider doesn't support model fetching)
        OnComplete.ExecuteIfBound(true, TArray<FOpenRouterModel>());
    }
    
    //~ Chat Completion
    
    /**
     * Send a chat completion request with streaming
     * @param Messages The conversation history
     * @param ModelId The model to use (may be ignored by single-model providers)
     * @param Tools Available tools for the LLM to call
     * @param OnChunk Called for each streamed chunk
     * @param OnComplete Called when streaming finishes
     * @param OnError Called if an error occurs
     * @param OnToolCall Called when LLM requests a tool call
     * @param OnUsage Called when usage stats are received
     */
    virtual void SendChatRequest(
        const TArray<FChatMessage>& Messages,
        const FString& ModelId,
        const TArray<FMCPTool>& Tools,
        FOnLLMStreamChunk OnChunk,
        FOnLLMStreamComplete OnComplete,
        FOnLLMStreamError OnError,
        FOnLLMToolCall OnToolCall,
        FOnLLMUsageReceived OnUsage
    ) = 0;
    
    //~ Request Management
    
    /** Cancel any in-progress streaming request */
    virtual void CancelRequest() = 0;
    
    /** Check if a request is currently in progress */
    virtual bool IsRequestInProgress() const = 0;
};

/**
 * Type alias for shared pointer to LLM client
 */
using ILLMClientPtr = TSharedPtr<ILLMClient>;
