// Copyright 2025 Vibe AI. All Rights Reserved.

#include "Chat/VibeUEAPIClient.h"
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
    return TEXT(
        "You are an AI assistant helping with Unreal Engine game development. "
        "You have access to the Model Context Protocol (MCP) tools to interact with the Unreal Engine editor. "
        "When the user asks you to perform actions in the editor, use the appropriate tools. "
        "Be helpful, concise, and focus on the task at hand."
    );
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
        TSharedPtr<FJsonObject> MsgObj = MakeShareable(new FJsonObject());
        MsgObj->SetStringField(TEXT("role"), Msg.Role);

        // Handle content - could be string or array for tool results
        if (Msg.Role == TEXT("tool"))
        {
            MsgObj->SetStringField(TEXT("content"), Msg.Content);
            if (!Msg.ToolCallId.IsEmpty())
            {
                MsgObj->SetStringField(TEXT("tool_call_id"), Msg.ToolCallId);
            }
        }
        else if (Msg.Role == TEXT("assistant") && Msg.ToolCalls.Num() > 0)
        {
            // Assistant message with tool calls
            if (!Msg.Content.IsEmpty())
            {
                MsgObj->SetStringField(TEXT("content"), Msg.Content);
            }
            else
            {
                MsgObj->SetField(TEXT("content"), MakeShareable(new FJsonValueNull()));
            }

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
        else
        {
            MsgObj->SetStringField(TEXT("content"), Msg.Content);
        }

        MessagesArray.Add(MakeShareable(new FJsonValueObject(MsgObj)));
    }

    // Build request body
    TSharedPtr<FJsonObject> RequestBody = MakeShareable(new FJsonObject());
    RequestBody->SetArrayField(TEXT("messages"), MessagesArray);
    RequestBody->SetBoolField(TEXT("stream"), true);
    
    // Stream options for usage stats
    TSharedPtr<FJsonObject> StreamOptions = MakeShareable(new FJsonObject());
    StreamOptions->SetBoolField(TEXT("include_usage"), true);
    RequestBody->SetObjectField(TEXT("stream_options"), StreamOptions);

    // Add tools if provided (use same format as OpenRouter)
    if (Tools.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> ToolsArray;
        for (const FMCPTool& Tool : Tools)
        {
            ToolsArray.Add(MakeShared<FJsonValueObject>(Tool.ToOpenRouterJson()));
        }
        RequestBody->SetArrayField(TEXT("tools"), ToolsArray);
        
        UE_LOG(LogVibeUEAPIClient, Log, TEXT("Including %d tools in request"), Tools.Num());
    }

    // Serialize to JSON string
    FString RequestBodyString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBodyString);
    FJsonSerializer::Serialize(RequestBody.ToSharedRef(), Writer);

    UE_LOG(LogVibeUEAPIClient, Verbose, TEXT("Sending chat request to VibeUE API: %s"), *EndpointUrl);

    // Create HTTP request
    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(EndpointUrl);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), ContentTypeHeader);
    Request->SetHeader(ApiKeyHeader, ApiKey);
    Request->SetContentAsString(RequestBodyString);

    return Request;
}
