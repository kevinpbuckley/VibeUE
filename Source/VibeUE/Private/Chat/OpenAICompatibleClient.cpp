// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Chat/OpenAICompatibleClient.h"
#include "Chat/ChatSession.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY(LogOpenAICompatibleClient);

const FString FOpenAICompatibleClient::DefaultEndpoint = TEXT("http://localhost:11434/v1/chat/completions");
const FString FOpenAICompatibleClient::ContentTypeHeader = TEXT("application/json");

FOpenAICompatibleClient::FOpenAICompatibleClient()
    : EndpointUrl(DefaultEndpoint)
{
}

FLLMProviderInfo FOpenAICompatibleClient::GetProviderInfo() const
{
    return FLLMProviderInfo(
        TEXT("OpenAICompatible"),
        TEXT("OpenAI Compatible"),
        true,            // Supports model selection via /v1/models
        ConfiguredModelId,
        TEXT("Any OpenAI-compatible endpoint (Ollama, vLLM, LM Studio, etc.)")
    );
}

void FOpenAICompatibleClient::SetApiKey(const FString& InApiKey)
{
    ApiKey = InApiKey;
}

bool FOpenAICompatibleClient::HasApiKey() const
{
    // None mode needs no key; other modes require one
    return AuthMode == ECustomAuthMode::None || !ApiKey.IsEmpty();
}

FString FOpenAICompatibleClient::GetModelsUrl() const
{
    FString ModelsUrl = EndpointUrl;
    ModelsUrl.ReplaceInline(TEXT("/chat/completions"), TEXT("/models"));
    return ModelsUrl;
}

void FOpenAICompatibleClient::ApplyAuthHeader(TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request) const
{
    if (AuthMode == ECustomAuthMode::Bearer && !ApiKey.IsEmpty())
    {
        Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ApiKey));
    }
    else if (AuthMode == ECustomAuthMode::XApiKey && !ApiKey.IsEmpty())
    {
        Request->SetHeader(TEXT("X-API-Key"), ApiKey);
    }
    // ECustomAuthMode::None: no auth header
}

void FOpenAICompatibleClient::FetchModels(FOnLLMModelsFetched OnComplete)
{
    FString ModelsUrl = GetModelsUrl();
    UE_LOG(LogOpenAICompatibleClient, Log, TEXT("Fetching models from: %s"), *ModelsUrl);

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(ModelsUrl);
    Request->SetVerb(TEXT("GET"));
    Request->SetHeader(TEXT("Content-Type"), ContentTypeHeader);
    Request->SetTimeout(10.0f);
    ApplyAuthHeader(Request);

    Request->OnProcessRequestComplete().BindSP(this, &FOpenAICompatibleClient::HandleModelsFetchComplete, OnComplete);
    Request->ProcessRequest();
}

void FOpenAICompatibleClient::HandleModelsFetchComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully, FOnLLMModelsFetched OnComplete)
{
    if (!bConnectedSuccessfully || !Response.IsValid() || Response->GetResponseCode() != 200)
    {
        // Not an error — many local servers don't implement /v1/models; user enters model ID manually
        UE_LOG(LogOpenAICompatibleClient, Log, TEXT("Model list unavailable — user will enter model ID manually"));
        OnComplete.ExecuteIfBound(false, TArray<FOpenRouterModel>());
        return;
    }

    TSharedPtr<FJsonObject> RootObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        OnComplete.ExecuteIfBound(false, TArray<FOpenRouterModel>());
        return;
    }

    TArray<FOpenRouterModel> Models;
    const TArray<TSharedPtr<FJsonValue>>* DataArray;
    if (RootObject->TryGetArrayField(TEXT("data"), DataArray))
    {
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
    }

    UE_LOG(LogOpenAICompatibleClient, Log, TEXT("Fetched %d models"), Models.Num());
    OnComplete.ExecuteIfBound(true, Models);
}

FString FOpenAICompatibleClient::ProcessErrorResponse(int32 ResponseCode, const FString& ResponseBody)
{
    if (ResponseCode == 401)
    {
        return TEXT("Authentication failed. Check your API key or switch auth mode to None for local servers.");
    }
    if (ResponseCode == 404)
    {
        return TEXT("Endpoint not found. Check the endpoint URL in settings.");
    }
    if (ResponseCode == 0)
    {
        return TEXT("Could not connect. Check the server is running and the endpoint URL is correct.");
    }
    return FLLMClientBase::ProcessErrorResponse(ResponseCode, ResponseBody);
}

TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> FOpenAICompatibleClient::BuildHttpRequest(
    const TArray<FChatMessage>& Messages,
    const FString& ModelId,
    const TArray<FMCPTool>& Tools)
{
    if (!HasApiKey())
    {
        OnPreRequestError(TEXT("API key required. Set your key in Settings, or switch auth mode to None for local servers."));
        return nullptr;
    }

    // Build messages array
    TArray<TSharedPtr<FJsonValue>> MessagesArray;
    for (const FChatMessage& Msg : Messages)
    {
        FChatMessage SanitizedMessage = Msg;
        SanitizedMessage.Content = SanitizeForLLM(Msg.Content);
        for (FChatToolCall& TC : SanitizedMessage.ToolCalls)
        {
            TC.Arguments = SanitizeForLLM(TC.Arguments);
        }
        MessagesArray.Add(MakeShared<FJsonValueObject>(SanitizedMessage.ToJson()));
    }

    // When tools are present the request body grows to ~30-35 KB (10 full JSON schemas).
    // WinHTTP uploads the large body while Ollama simultaneously streams back chunk1
    // (the tool_calls delta, ~340 bytes).  WinHTTP only surfaces the LAST received chunk
    // via GetContentAsString() once the upload finishes — chunk1 is silently discarded.
    // Disable streaming for tool-bearing requests so the full JSON response arrives in one
    // shot via HandleRequestComplete (non-streaming path is confirmed reliable).
    // Pure text conversations (no tools) are not affected and still stream normally.
    bool bEffectiveStreaming = bStreamingEnabled && (Tools.Num() == 0);

    TSharedPtr<FJsonObject> RequestBody = MakeShared<FJsonObject>();
    RequestBody->SetArrayField(TEXT("messages"), MessagesArray);
    RequestBody->SetBoolField(TEXT("stream"), bEffectiveStreaming);

    // ConfiguredModelId takes precedence; fall back to the session's ModelId arg
    FString EffectiveModelId = ConfiguredModelId.IsEmpty() ? ModelId : ConfiguredModelId;
    if (!EffectiveModelId.IsEmpty())
    {
        RequestBody->SetStringField(TEXT("model"), EffectiveModelId);
    }

    RequestBody->SetNumberField(TEXT("temperature"), Temperature);
    RequestBody->SetNumberField(TEXT("top_p"), TopP);
    RequestBody->SetNumberField(TEXT("max_tokens"), MaxTokens);

    // num_ctx: Ollama-specific context window override.
    // The 10 full MCP tool schemas consume ~30KB / ~8000 tokens — well beyond Ollama's
    // default context (typically 2048–4096). Other servers ignore this field silently.
    if (ContextSize > 0)
    {
        RequestBody->SetNumberField(TEXT("num_ctx"), ContextSize);
        UE_LOG(LogOpenAICompatibleClient, Log, TEXT("Setting num_ctx=%d (Ollama context override)"), ContextSize);
    }

    UE_LOG(LogOpenAICompatibleClient, Log, TEXT("LLM params: model=%s, temperature=%.2f, top_p=%.2f, max_tokens=%d, stream=%s (tools=%d, streaming overridden=%s)"),
        *EffectiveModelId, Temperature, TopP, MaxTokens, bEffectiveStreaming ? TEXT("true") : TEXT("false"),
        Tools.Num(), (!bEffectiveStreaming && bStreamingEnabled) ? TEXT("yes") : TEXT("no"));

    if (Tools.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> ToolsArray;
        for (const FMCPTool& Tool : Tools)
        {
            ToolsArray.Add(MakeShared<FJsonValueObject>(Tool.ToOpenRouterJson()));
        }
        RequestBody->SetArrayField(TEXT("tools"), ToolsArray);
        RequestBody->SetBoolField(TEXT("parallel_tool_calls"), bParallelToolCalls);
        UE_LOG(LogOpenAICompatibleClient, Log, TEXT("Including %d tools (parallel_tool_calls=%s)"),
            Tools.Num(), bParallelToolCalls ? TEXT("true") : TEXT("false"));
    }

    FString RequestBodyString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBodyString);
    FJsonSerializer::Serialize(RequestBody.ToSharedRef(), Writer);

    if (FChatSession::IsFileLoggingEnabled())
    {
        FString RawLogPath = FPaths::ProjectSavedDir() / TEXT("Logs") / TEXT("VibeUE_RawLLM.log");
        FString RequestLog = FString::Printf(
            TEXT("\n========== REQUEST [%s] ==========\nURL: %s\nModel: %s, Messages: %d, Tools: %d, Temperature: %.2f\n%s\n"),
            *FDateTime::Now().ToString(), *EndpointUrl, *EffectiveModelId,
            Messages.Num(), Tools.Num(), Temperature, *RequestBodyString);
        FFileHelper::SaveStringToFile(RequestLog, *RawLogPath, FFileHelper::EEncodingOptions::ForceUTF8, &IFileManager::Get(), FILEWRITE_Append);
    }

    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(EndpointUrl);
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Content-Type"), ContentTypeHeader);
    // Do NOT set Connection: close for streaming (SSE) requests — WinHTTP buffers
    // differently with Connection: close and may discard early SSE chunks (chunk1 lost).
    // For non-streaming, keep-alive is also fine (connection closes after response anyway).
    HttpRequest->SetTimeout(120.0f);
    // Local models (Ollama, LM Studio, etc.) may take 30–90s to load weights into
    // memory before generating the first token.  The UE default activity timeout is
    // 30s — extend it to match the total timeout so we don't abort on cold starts.
    HttpRequest->SetActivityTimeout(120.0f);
    ApplyAuthHeader(HttpRequest);
    HttpRequest->SetContentAsString(RequestBodyString);

    return HttpRequest;
}
