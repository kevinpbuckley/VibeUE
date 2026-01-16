// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Chat/OpenRouterClient.h"
#include "HttpModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY(LogOpenRouterClient);

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
    if (!HasApiKey())
    {
        UE_LOG(LogOpenRouterClient, Warning, TEXT("Cannot fetch models: No API key configured"));
        OnComplete.ExecuteIfBound(false, TArray<FOpenRouterModel>());
        return;
    }
    
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(ModelsEndpoint);
    Request->SetVerb(TEXT("GET"));
    Request->SetHeader(AuthorizationHeader, FString::Printf(TEXT("Bearer %s"), *ApiKey));
    Request->SetHeader(TEXT("HTTP-Referer"), TEXT("https://github.com/VibeUE"));
    Request->SetHeader(TEXT("X-Title"), TEXT("VibeUE Plugin"));
    
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
    
    // Build request body
    TSharedPtr<FJsonObject> RequestBody = MakeShared<FJsonObject>();
    RequestBody->SetStringField(TEXT("model"), ModelId);
    RequestBody->SetBoolField(TEXT("stream"), true);
    
    TArray<TSharedPtr<FJsonValue>> MessagesArray;
    for (const FChatMessage& Message : Messages)
    {
        // Create a sanitized copy to remove NUL characters and other problematic bytes
        FChatMessage SanitizedMessage = Message;
        SanitizedMessage.Content = SanitizeForLLM(Message.Content);
        for (FChatToolCall& TC : SanitizedMessage.ToolCalls)
        {
            TC.Arguments = SanitizeForLLM(TC.Arguments);
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
    
    // Create HTTP request
    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(ChatEndpoint);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), ContentTypeHeader);
    Request->SetHeader(AuthorizationHeader, FString::Printf(TEXT("Bearer %s"), *ApiKey));
    Request->SetHeader(TEXT("HTTP-Referer"), TEXT("https://github.com/VibeUE"));
    Request->SetHeader(TEXT("X-Title"), TEXT("VibeUE Plugin"));
    Request->SetContentAsString(RequestBodyString);
    
    UE_LOG(LogOpenRouterClient, Log, TEXT("Sending chat request with model %s"), *ModelId);
    
    return Request;
}
