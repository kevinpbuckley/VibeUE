// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Chat/VibeUEAPIClient.h"
#include "Chat/ChatSession.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY(LogVibeUEAPIClient);

const FString FVibeUEAPIClient::ContentTypeHeader = TEXT("application/json");
const FString FVibeUEAPIClient::ApiKeyHeader = TEXT("X-API-Key");

FString FVibeUEAPIClient::GetDefaultEndpoint()
{
    // VibeUE LLM API endpoint
    return TEXT("https://llm.vibeue.com/v1/chat/completions");
}

FString FVibeUEAPIClient::GetDefaultSystemPrompt()
{
    // Use shared system prompt loading from base class
    return FLLMClientBase::LoadSystemPromptFromFile();
}

FVibeUEAPIClient::FVibeUEAPIClient()
    : EndpointUrl(GetDefaultEndpoint())
{
}

FLLMProviderInfo FVibeUEAPIClient::GetProviderInfo() const
{
    return FLLMProviderInfo(
        TEXT("VibeUE"),
        TEXT("VibeUE"),
        false,  // Does not support model selection (single model)
        TEXT(""),  // No default model ID needed
        TEXT("VibeUE's own LLM API service")
    );
}

void FVibeUEAPIClient::SetApiKey(const FString& InApiKey)
{
    ApiKey = InApiKey;
}

bool FVibeUEAPIClient::HasApiKey() const
{
    return !ApiKey.IsEmpty();
}

void FVibeUEAPIClient::SetEndpointUrl(const FString& InUrl)
{
    EndpointUrl = InUrl;
}

FString FVibeUEAPIClient::ProcessErrorResponse(int32 ResponseCode, const FString& ResponseBody)
{
    if (ResponseCode == 401)
    {
        return TEXT("Invalid VibeUE API key. Please check your API key in settings.");
    }
    
    // Use base class implementation for other errors
    return FLLMClientBase::ProcessErrorResponse(ResponseCode, ResponseBody);
}

TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> FVibeUEAPIClient::BuildHttpRequest(
    const TArray<FChatMessage>& Messages,
    const FString& ModelId,
    const TArray<FMCPTool>& Tools)
{
    // Check for API key
    if (!HasApiKey())
    {
        OnPreRequestError(TEXT("VibeUE API key not configured. Please set your API key in the settings."));
        return nullptr;
    }

    // Build messages array
    TArray<TSharedPtr<FJsonValue>> MessagesArray;
    for (const FChatMessage& Msg : Messages)
    {
        // Sanitize message content before serialization
        FChatMessage SanitizedMessage = Msg;
        SanitizedMessage.Content = SanitizeForLLM(Msg.Content);
        
        // Use ToJson() for consistent serialization (supports multimodal content)
        MessagesArray.Add(MakeShared<FJsonValueObject>(SanitizedMessage.ToJson()));
    }

    // Build request body
    TSharedPtr<FJsonObject> RequestBody = MakeShareable(new FJsonObject());
    RequestBody->SetArrayField(TEXT("messages"), MessagesArray);
    
    // TEMPORARY: Disable streaming to test non-streaming flow
    // TODO: Re-enable streaming once we have a proper SSE implementation
    RequestBody->SetBoolField(TEXT("stream"), false);
    
    // LLM generation parameters
    RequestBody->SetNumberField(TEXT("temperature"), Temperature);
    RequestBody->SetNumberField(TEXT("top_p"), TopP);
    RequestBody->SetNumberField(TEXT("max_tokens"), MaxTokens);
    
    UE_LOG(LogVibeUEAPIClient, Log, TEXT("LLM params: temperature=%.2f, top_p=%.2f, max_tokens=%d, stream=false"), 
        Temperature, TopP, MaxTokens);
    
    // Note: stream_options not needed when streaming is disabled
    // TSharedPtr<FJsonObject> StreamOptions = MakeShareable(new FJsonObject());
    // StreamOptions->SetBoolField(TEXT("include_usage"), true);
    // RequestBody->SetObjectField(TEXT("stream_options"), StreamOptions);

    // Add tools if provided (use same format as OpenRouter)
    if (Tools.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> ToolsArray;
        for (const FMCPTool& Tool : Tools)
        {
            UE_LOG(LogVibeUEAPIClient, Warning, TEXT("  Sending tool to VibeUE: %s"), *Tool.Name);
            ToolsArray.Add(MakeShared<FJsonValueObject>(Tool.ToOpenRouterJson()));
        }
        RequestBody->SetArrayField(TEXT("tools"), ToolsArray);
        
        // Control parallel tool calls - when false, LLM returns one tool call at a time
        RequestBody->SetBoolField(TEXT("parallel_tool_calls"), bParallelToolCalls);
        
        UE_LOG(LogVibeUEAPIClient, Log, TEXT("Including %d tools in request (parallel_tool_calls=%s)"), 
            Tools.Num(), bParallelToolCalls ? TEXT("true") : TEXT("false"));
    }

    // Serialize to JSON string
    FString RequestBodyString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBodyString);
    FJsonSerializer::Serialize(RequestBody.ToSharedRef(), Writer);

    UE_LOG(LogVibeUEAPIClient, Verbose, TEXT("Sending chat request to VibeUE API: %s"), *EndpointUrl);
    
    // Log request summary to dedicated file for debugging (if file logging enabled)
    if (FChatSession::IsFileLoggingEnabled())
    {
        FString RawLogPath = FPaths::ProjectSavedDir() / TEXT("Logs") / TEXT("VibeUE_RawLLM.log");
        // Log summary instead of full body to avoid massive log files from tool schemas
        FString RequestLog = FString::Printf(TEXT("\n========== REQUEST [%s] ==========\nURL: %s\nMessages: %d, Tools: %d, Temperature: %.2f\n"),
            *FDateTime::Now().ToString(),
            *EndpointUrl,
            Messages.Num(),
            Tools.Num(),
            Temperature);
        FFileHelper::SaveStringToFile(RequestLog, *RawLogPath, FFileHelper::EEncodingOptions::ForceUTF8, &IFileManager::Get(), FILEWRITE_Append);
        UE_LOG(LogVibeUEAPIClient, Verbose, TEXT("Request summary logged to: %s"), *RawLogPath);
    }

    // Create HTTP request
    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(EndpointUrl);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), ContentTypeHeader);
    Request->SetHeader(ApiKeyHeader, ApiKey);
    // Disable keep-alive to prevent stale connections causing stuck requests
    Request->SetHeader(TEXT("Connection"), TEXT("close"));
    Request->SetContentAsString(RequestBodyString);
    // Set timeout to 120 seconds - LLM inference with tools and large context can take time
    // especially with chain-of-thought reasoning enabled
    Request->SetTimeout(120.0f);

    return Request;
}

void FVibeUEAPIClient::FetchModelInfo(TFunction<void(bool bSuccess, int32 ContextLength, const FString& ModelId)> OnComplete)
{
    if (!OnComplete)
    {
        return;
    }
    
    // Build the models endpoint URL from the chat endpoint
    // e.g., https://llm.vibeue.com/v1/chat/completions -> https://llm.vibeue.com/v1/models
    FString ModelsUrl = EndpointUrl;
    ModelsUrl.ReplaceInline(TEXT("/v1/chat/completions"), TEXT("/v1/models"));
    
    UE_LOG(LogVibeUEAPIClient, Log, TEXT("Fetching model info from: %s"), *ModelsUrl);
    
    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(ModelsUrl);
    Request->SetVerb(TEXT("GET"));
    Request->SetHeader(TEXT("Content-Type"), ContentTypeHeader);
    if (HasApiKey())
    {
        Request->SetHeader(ApiKeyHeader, ApiKey);
    }
    Request->SetTimeout(10.0f);  // Short timeout for simple GET request
    
    Request->OnProcessRequestComplete().BindLambda([OnComplete](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
    {
        if (!bConnectedSuccessfully || !Response.IsValid())
        {
            UE_LOG(LogVibeUEAPIClient, Warning, TEXT("Failed to fetch model info - connection error"));
            OnComplete(false, 131072, TEXT("")); // Default fallback
            return;
        }
        
        int32 ResponseCode = Response->GetResponseCode();
        if (ResponseCode != 200)
        {
            UE_LOG(LogVibeUEAPIClient, Warning, TEXT("Failed to fetch model info - HTTP %d"), ResponseCode);
            OnComplete(false, 131072, TEXT("")); // Default fallback
            return;
        }
        
        FString ResponseBody = Response->GetContentAsString();
        
        // Parse JSON response
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
        if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
        {
            UE_LOG(LogVibeUEAPIClient, Warning, TEXT("Failed to parse model info JSON"));
            OnComplete(false, 131072, TEXT(""));
            return;
        }
        
        // Get the data array
        const TArray<TSharedPtr<FJsonValue>>* DataArray;
        if (!JsonObject->TryGetArrayField(TEXT("data"), DataArray) || DataArray->Num() == 0)
        {
            UE_LOG(LogVibeUEAPIClient, Warning, TEXT("No models in response"));
            OnComplete(false, 131072, TEXT(""));
            return;
        }
        
        // Get the first model
        TSharedPtr<FJsonObject> ModelObject = (*DataArray)[0]->AsObject();
        if (!ModelObject.IsValid())
        {
            UE_LOG(LogVibeUEAPIClient, Warning, TEXT("Invalid model object"));
            OnComplete(false, 131072, TEXT(""));
            return;
        }
        
        // Extract context_length and model id
        int32 ContextLength = ModelObject->GetIntegerField(TEXT("context_length"));
        FString ModelId = ModelObject->GetStringField(TEXT("id"));
        
        if (ContextLength <= 0)
        {
            ContextLength = 131072; // Default fallback
        }
        
        UE_LOG(LogVibeUEAPIClient, Log, TEXT("Fetched model info: id=%s, context_length=%d"), *ModelId, ContextLength);
        OnComplete(true, ContextLength, ModelId);
    });
    
    Request->ProcessRequest();
}

void FVibeUEAPIClient::CountTokens(const FString& Text, TFunction<void(bool bSuccess, int32 TokenCount)> OnComplete)
{
    if (!OnComplete)
    {
        return;
    }

    // Build the tokenize endpoint URL from the chat endpoint
    // e.g., https://llm.vibeue.com/v1/chat/completions -> https://llm.vibeue.com/v1/tokenize
    FString TokenizeUrl = EndpointUrl;
    TokenizeUrl.ReplaceInline(TEXT("/v1/chat/completions"), TEXT("/v1/tokenize"));

    UE_LOG(LogVibeUEAPIClient, Verbose, TEXT("Counting tokens for text (%d chars) using: %s"), Text.Len(), *TokenizeUrl);

    // Build request body
    TSharedPtr<FJsonObject> RequestBody = MakeShareable(new FJsonObject());
    RequestBody->SetStringField(TEXT("text"), Text);

    // Serialize to JSON
    FString RequestBodyString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBodyString);
    FJsonSerializer::Serialize(RequestBody.ToSharedRef(), Writer);

    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(TokenizeUrl);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), ContentTypeHeader);
    // Note: Tokenize endpoint doesn't require authentication
    Request->SetContentAsString(RequestBodyString);
    Request->SetTimeout(10.0f);

    Request->OnProcessRequestComplete().BindLambda([OnComplete](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
    {
        if (!bConnectedSuccessfully || !Response.IsValid())
        {
            UE_LOG(LogVibeUEAPIClient, Warning, TEXT("Failed to count tokens - connection error"));
            OnComplete(false, 0);
            return;
        }

        int32 ResponseCode = Response->GetResponseCode();
        if (ResponseCode != 200)
        {
            UE_LOG(LogVibeUEAPIClient, Warning, TEXT("Failed to count tokens - HTTP %d"), ResponseCode);
            OnComplete(false, 0);
            return;
        }

        FString ResponseBody = Response->GetContentAsString();

        // Parse JSON response
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
        if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
        {
            UE_LOG(LogVibeUEAPIClient, Warning, TEXT("Failed to parse token count JSON"));
            OnComplete(false, 0);
            return;
        }

        // Extract token_count
        if (!JsonObject->HasField(TEXT("token_count")))
        {
            UE_LOG(LogVibeUEAPIClient, Warning, TEXT("Missing token_count field in response"));
            OnComplete(false, 0);
            return;
        }
        int32 TokenCount = JsonObject->GetIntegerField(TEXT("token_count"));
        UE_LOG(LogVibeUEAPIClient, Verbose, TEXT("Token count: %d"), TokenCount);
        OnComplete(true, TokenCount);
    });

    Request->ProcessRequest();
}

void FVibeUEAPIClient::CountTokensInMessages(const TArray<FChatMessage>& Messages, const FString& ModelId, TFunction<void(bool bSuccess, int32 TokenCount)> OnComplete)
{
    if (!OnComplete)
    {
        return;
    }

    // Build the tokenize endpoint URL
    FString TokenizeUrl = EndpointUrl;
    TokenizeUrl.ReplaceInline(TEXT("/v1/chat/completions"), TEXT("/v1/tokenize"));

    UE_LOG(LogVibeUEAPIClient, Verbose, TEXT("Counting tokens for %d messages using: %s"), Messages.Num(), *TokenizeUrl);

    // Build messages array
    TArray<TSharedPtr<FJsonValue>> MessagesArray;
    for (const FChatMessage& Msg : Messages)
    {
        TSharedPtr<FJsonObject> MsgObj = MakeShareable(new FJsonObject());
        MsgObj->SetStringField(TEXT("role"), Msg.Role);

        if (!Msg.Content.IsEmpty())
        {
            MsgObj->SetStringField(TEXT("content"), Msg.Content);
        }
        else
        {
            MsgObj->SetField(TEXT("content"), MakeShareable(new FJsonValueNull()));
        }

        // Include tool_calls if present
        if (Msg.ToolCalls.Num() > 0)
        {
            TArray<TSharedPtr<FJsonValue>> ToolCallsArray;
            for (const FChatToolCall& ToolCall : Msg.ToolCalls)
            {
                TSharedPtr<FJsonObject> ToolCallObj = MakeShareable(new FJsonObject());
                ToolCallObj->SetStringField(TEXT("id"), ToolCall.Id);
                ToolCallObj->SetStringField(TEXT("type"), TEXT("function"));

                TSharedPtr<FJsonObject> FunctionObj = MakeShareable(new FJsonObject());
                FunctionObj->SetStringField(TEXT("name"), ToolCall.Name);
                FunctionObj->SetStringField(TEXT("arguments"), ToolCall.Arguments);
                ToolCallObj->SetObjectField(TEXT("function"), FunctionObj);

                ToolCallsArray.Add(MakeShareable(new FJsonValueObject(ToolCallObj)));
            }
            MsgObj->SetArrayField(TEXT("tool_calls"), ToolCallsArray);
        }

        if (!Msg.ToolCallId.IsEmpty())
        {
            MsgObj->SetStringField(TEXT("tool_call_id"), Msg.ToolCallId);
        }

        MessagesArray.Add(MakeShareable(new FJsonValueObject(MsgObj)));
    }

    // Build request body
    TSharedPtr<FJsonObject> RequestBody = MakeShareable(new FJsonObject());
    RequestBody->SetArrayField(TEXT("messages"), MessagesArray);

    if (!ModelId.IsEmpty())
    {
        RequestBody->SetStringField(TEXT("model"), ModelId);
    }

    // Serialize to JSON
    FString RequestBodyString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBodyString);
    FJsonSerializer::Serialize(RequestBody.ToSharedRef(), Writer);

    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(TokenizeUrl);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), ContentTypeHeader);
    Request->SetContentAsString(RequestBodyString);
    Request->SetTimeout(10.0f);

    Request->OnProcessRequestComplete().BindLambda([OnComplete](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
    {
        if (!bConnectedSuccessfully || !Response.IsValid())
        {
            UE_LOG(LogVibeUEAPIClient, Warning, TEXT("Failed to count message tokens - connection error"));
            OnComplete(false, 0);
            return;
        }

        int32 ResponseCode = Response->GetResponseCode();
        if (ResponseCode != 200)
        {
            UE_LOG(LogVibeUEAPIClient, Warning, TEXT("Failed to count message tokens - HTTP %d"), ResponseCode);
            OnComplete(false, 0);
            return;
        }

        FString ResponseBody = Response->GetContentAsString();

        // Parse JSON response
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
        if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
        {
            UE_LOG(LogVibeUEAPIClient, Warning, TEXT("Failed to parse message token count JSON"));
            OnComplete(false, 0);
            return;
        }

        // Extract token_count
        if (!JsonObject->HasField(TEXT("token_count")))
        {
            UE_LOG(LogVibeUEAPIClient, Warning, TEXT("Missing token_count field in message response"));
            OnComplete(false, 0);
            return;
        }
        int32 TokenCount = JsonObject->GetIntegerField(TEXT("token_count"));
        UE_LOG(LogVibeUEAPIClient, Verbose, TEXT("Message token count: %d"), TokenCount);
        OnComplete(true, TokenCount);
    });

    Request->ProcessRequest();
}
