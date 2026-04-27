// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Chat/OpenRouterClient.h"
#include "Chat/ChatSession.h"
#include "HttpModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY(LogOpenRouterClient);

enum class ECacheStrategy : uint8 { AnthropicExplicit, Auto };

static ECacheStrategy DetectCacheStrategy(const FString& ModelId)
{
    FString Lower = ModelId.ToLower();
    if (Lower.StartsWith(TEXT("anthropic/")) || Lower == TEXT("openrouter/auto"))
    {
        return ECacheStrategy::AnthropicExplicit;
    }
    return ECacheStrategy::Auto;
}

static void ApplyPromptCaching(TArray<FChatMessage>& Messages)
{
    // Mark the system message as an explicit cache breakpoint.
    // System prompt is large (~7K tokens) and never changes — always gets cache hits.
    // The growing conversation tail is handled by top-level automatic caching
    // (cache_control on the request body), so no rolling user-message breakpoints needed.
    if (Messages.Num() > 0 && Messages[0].Role == TEXT("system"))
    {
        Messages[0].bCacheBreakpoint = true;
    }
}

const FString FOpenRouterClient::ModelsEndpoint = TEXT("https://openrouter.ai/api/v1/models");
const FString FOpenRouterClient::ChatEndpoint = TEXT("https://openrouter.ai/api/v1/chat/completions");
const FString FOpenRouterClient::ContentTypeHeader = TEXT("application/json");
const FString FOpenRouterClient::AuthorizationHeader = TEXT("Authorization");

FOpenRouterClient::FOpenRouterClient()
{
}

FLLMProviderInfo FOpenRouterClient::GetProviderInfo() const
{
    return FLLMProviderInfo(
        TEXT("OpenRouter"),
        TEXT("OpenRouter"),
        true,  // Supports model selection
        TEXT("x-ai/grok-4.1-fast:free"),
        TEXT("Access multiple LLM providers through OpenRouter API")
    );
}

void FOpenRouterClient::SetApiKey(const FString& InApiKey)
{
    ApiKey = InApiKey;
}

bool FOpenRouterClient::HasApiKey() const
{
    return !ApiKey.IsEmpty();
}

FString FOpenRouterClient::GetDefaultSystemPrompt()
{
    // Use shared system prompt loading from base class
    return FLLMClientBase::LoadSystemPromptFromFile();
}

void FOpenRouterClient::FetchModels(FOnLLMModelsFetched OnComplete)
{
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(ModelsEndpoint);
    Request->SetVerb(TEXT("GET"));
    if (HasApiKey())
    {
        Request->SetHeader(AuthorizationHeader, FString::Printf(TEXT("Bearer %s"), *ApiKey));
    }
    Request->SetHeader(TEXT("HTTP-Referer"), TEXT("https://www.vibeue.com"));
    Request->SetHeader(TEXT("X-OpenRouter-Title"), TEXT("VibeUE"));
    Request->SetHeader(TEXT("X-OpenRouter-Categories"), TEXT("ide-extension"));
    
    Request->OnProcessRequestComplete().BindSP(this, &FOpenRouterClient::HandleModelsFetchComplete, OnComplete);
    Request->ProcessRequest();
    
    UE_LOG(LogOpenRouterClient, Log, TEXT("Fetching models from OpenRouter..."));
}

void FOpenRouterClient::HandleModelsFetchComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully, FOnModelsFetched OnComplete)
{
    TArray<FOpenRouterModel> Models;
    
    if (!bConnectedSuccessfully || !Response.IsValid())
    {
        UE_LOG(LogOpenRouterClient, Error, TEXT("Failed to connect to OpenRouter models endpoint"));
        OnComplete.ExecuteIfBound(false, Models);
        return;
    }
    
    int32 ResponseCode = Response->GetResponseCode();
    if (ResponseCode != 200)
    {
        UE_LOG(LogOpenRouterClient, Error, TEXT("OpenRouter models request failed with code %d: %s"), 
            ResponseCode, *Response->GetContentAsString());
        OnComplete.ExecuteIfBound(false, Models);
        return;
    }
    
    // Parse response
    TSharedPtr<FJsonObject> RootObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
    
    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        UE_LOG(LogOpenRouterClient, Error, TEXT("Failed to parse models response JSON"));
        OnComplete.ExecuteIfBound(false, Models);
        return;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* DataArray;
    if (!RootObject->TryGetArrayField(TEXT("data"), DataArray))
    {
        UE_LOG(LogOpenRouterClient, Error, TEXT("Models response missing 'data' array"));
        OnComplete.ExecuteIfBound(false, Models);
        return;
    }
    
    for (const TSharedPtr<FJsonValue>& Value : *DataArray)
    {
        if (Value.IsValid() && Value->Type == EJson::Object)
        {
            FOpenRouterModel Model = FOpenRouterModel::FromJson(Value->AsObject());
            if (!Model.Id.IsEmpty())
            {
                Models.Add(Model);
            }
        }
    }
    
    UE_LOG(LogOpenRouterClient, Log, TEXT("Fetched %d models from OpenRouter"), Models.Num());
    OnComplete.ExecuteIfBound(true, Models);
}

FString FOpenRouterClient::ProcessErrorResponse(int32 ResponseCode, const FString& ResponseBody)
{
    if (ResponseCode == 401)
    {
        return TEXT("Invalid API key. Please check your OpenRouter API key.");
    }
    else if (ResponseCode == 429)
    {
        return TEXT("Rate limit exceeded. Please wait a moment and try again.");
    }
    
    // Use base class implementation for other errors
    return FLLMClientBase::ProcessErrorResponse(ResponseCode, ResponseBody);
}

TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> FOpenRouterClient::BuildHttpRequest(
    const TArray<FChatMessage>& Messages,
    const FString& ModelId,
    const TArray<FMCPTool>& Tools)
{
    if (!HasApiKey())
    {
        OnPreRequestError(TEXT("No API key configured. Please set your OpenRouter API key in Editor Preferences."));
        return nullptr;
    }
    
    // Apply prompt caching for Anthropic models
    TArray<FChatMessage> CachedMessages = Messages;
    const bool bAnthropicCaching = DetectCacheStrategy(ModelId) == ECacheStrategy::AnthropicExplicit;
    if (bAnthropicCaching)
    {
        ApplyPromptCaching(CachedMessages);
    }

    // Build request body
    TSharedPtr<FJsonObject> RequestBody = MakeShared<FJsonObject>();
    RequestBody->SetStringField(TEXT("model"), ModelId);
    RequestBody->SetBoolField(TEXT("stream"), true);

    // Anthropic automatic caching: top-level cache_control auto-applies a breakpoint
    // to the last cacheable block, so the growing conversation tail is always cached.
    if (bAnthropicCaching)
    {
        TSharedPtr<FJsonObject> CacheControl = MakeShared<FJsonObject>();
        CacheControl->SetStringField(TEXT("type"), TEXT("ephemeral"));
        RequestBody->SetObjectField(TEXT("cache_control"), CacheControl);
    }

    // Reasoning blocks (reasoning_details / reasoning_content) are provider-specific.
    // Each provider returns reasoning in its own encrypted/signed format that only it
    // can validate. Replaying one provider's reasoning to another causes 400 errors
    // like "reasoning_content must be passed back to the API". Strip foreign reasoning
    // by comparing the "provider/" prefix on ModelUsed vs the target ModelId.
    auto GetProviderPrefix = [](const FString& ModelString) -> FString
    {
        int32 SlashIdx;
        if (ModelString.FindChar(TEXT('/'), SlashIdx))
        {
            return ModelString.Left(SlashIdx);
        }
        return ModelString;
    };
    const FString TargetProvider = GetProviderPrefix(ModelId);

    TArray<TSharedPtr<FJsonValue>> MessagesArray;
    for (const FChatMessage& Message : CachedMessages)
    {
        // Create a sanitized copy to remove NUL characters and other problematic bytes
        FChatMessage SanitizedMessage = Message;
        SanitizedMessage.Content = SanitizeForLLM(Message.Content);
        for (FChatToolCall& TC : SanitizedMessage.ToolCalls)
        {
            TC.Arguments = SanitizeForLLM(TC.Arguments);
        }

        // Strip reasoning from cross-provider assistant messages.
        if (Message.Role == TEXT("assistant") && !Message.ModelUsed.IsEmpty() && !TargetProvider.IsEmpty())
        {
            const FString MsgProvider = GetProviderPrefix(Message.ModelUsed);
            if (!MsgProvider.IsEmpty() && MsgProvider != TargetProvider)
            {
                SanitizedMessage.ThinkingContent.Empty();
                SanitizedMessage.ReasoningDetailsJson.Empty();
            }
        }

        MessagesArray.Add(MakeShared<FJsonValueObject>(SanitizedMessage.ToJson()));
    }
    RequestBody->SetArrayField(TEXT("messages"), MessagesArray);
    
    // Add tools if available
    if (Tools.Num() > 0)
    {
        UE_LOG(LogOpenRouterClient, Warning, TEXT("=== TOOLS BEING SENT TO LLM ==="));
        TArray<TSharedPtr<FJsonValue>> ToolsArray;
        for (const FMCPTool& Tool : Tools)
        {
            UE_LOG(LogOpenRouterClient, Warning, TEXT("  Sending tool: %s"), *Tool.Name);
            ToolsArray.Add(MakeShared<FJsonValueObject>(Tool.ToOpenRouterJson()));
        }
        UE_LOG(LogOpenRouterClient, Warning, TEXT("=== END TOOLS (%d total) ==="), Tools.Num());

        // Cache tool definitions for Anthropic models — add cache_control to the last tool.
        // Anthropic caches everything up to and including the breakpoint, so marking the
        // last tool covers the entire tools array on every subsequent request.
        if (bAnthropicCaching && ToolsArray.Num() > 0)
        {
            TSharedPtr<FJsonObject> LastTool = ToolsArray.Last()->AsObject();
            if (LastTool.IsValid())
            {
                TSharedPtr<FJsonObject> CacheControl = MakeShared<FJsonObject>();
                CacheControl->SetStringField(TEXT("type"), TEXT("ephemeral"));
                LastTool->SetObjectField(TEXT("cache_control"), CacheControl);
            }
        }

        RequestBody->SetArrayField(TEXT("tools"), ToolsArray);
        
        // Control parallel tool calls - when false, model makes one tool call at a time
        // This allows showing progress and results between tool calls
        RequestBody->SetBoolField(TEXT("parallel_tool_calls"), bParallelToolCalls);
        
        UE_LOG(LogOpenRouterClient, Log, TEXT("Including %d tools in request (parallel_tool_calls=%s)"), 
            Tools.Num(), bParallelToolCalls ? TEXT("true") : TEXT("false"));
    }
    
    FString RequestBodyString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBodyString);
    FJsonSerializer::Serialize(RequestBody.ToSharedRef(), Writer);
    
    // Log full request body to dedicated file for debugging (if file logging enabled)
    if (FChatSession::IsFileLoggingEnabled())
    {
        FString RawLogPath = FPaths::ProjectSavedDir() / TEXT("Logs") / TEXT("VibeUE_RawLLM.log");
        FString RequestLog = FString::Printf(TEXT("\n========== REQUEST [%s] ==========\nURL: %s\nModel: %s, Messages: %d, Tools: %d\n%s\n"),
            *FDateTime::Now().ToString(),
            *ChatEndpoint,
            *ModelId,
            CachedMessages.Num(),
            Tools.Num(),
            *RequestBodyString);
        FFileHelper::SaveStringToFile(RequestLog, *RawLogPath, FFileHelper::EEncodingOptions::ForceUTF8, &IFileManager::Get(), FILEWRITE_Append);
    }

    // Create HTTP request
    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(ChatEndpoint);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), ContentTypeHeader);
    Request->SetHeader(AuthorizationHeader, FString::Printf(TEXT("Bearer %s"), *ApiKey));
    Request->SetHeader(TEXT("HTTP-Referer"), TEXT("https://www.vibeue.com"));
    Request->SetHeader(TEXT("X-OpenRouter-Title"), TEXT("VibeUE"));
    Request->SetHeader(TEXT("X-OpenRouter-Categories"), TEXT("ide-extension"));
    Request->SetContentAsString(RequestBodyString);

    UE_LOG(LogOpenRouterClient, Log, TEXT("Sending chat request with model %s"), *ModelId);

    return Request;
}
