// Copyright 2025 Vibe AI. All Rights Reserved.

#include "Chat/VibeUEAPIClient.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/ConfigCacheIni.h"

DEFINE_LOG_CATEGORY(LogVibeUEAPIClient);

const FString FVibeUEAPIClient::ContentTypeHeader = TEXT("application/json");
const FString FVibeUEAPIClient::ApiKeyHeader = TEXT("X-API-Key");

FString FVibeUEAPIClient::GetDefaultEndpoint()
{
    // Direct RunPod endpoint (fallback while llm.vibeue.com DNS is being fixed)
    return TEXT("https://hy6vuonocdjtwu-8000.proxy.runpod.net/v1/chat/completions");
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
    , bToolCallsDetectedInStream(false)
{
}

FVibeUEAPIClient::~FVibeUEAPIClient()
{
    CancelRequest();
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

void FVibeUEAPIClient::CancelRequest()
{
    if (CurrentRequest.IsValid())
    {
        CurrentRequest->CancelRequest();
        CurrentRequest.Reset();
    }
    StreamBuffer.Empty();
    PendingToolCalls.Empty();
    bToolCallsDetectedInStream = false;
}

bool FVibeUEAPIClient::IsRequestInProgress() const
{
    return CurrentRequest.IsValid() && 
           CurrentRequest->GetStatus() == EHttpRequestStatus::Processing;
}

void FVibeUEAPIClient::SendChatRequest(
    const TArray<FChatMessage>& Messages,
    const FString& ModelId,  // Ignored - VibeUE uses a single model
    const TArray<FMCPTool>& Tools,
    FOnLLMStreamChunk OnChunk,
    FOnLLMStreamComplete OnComplete,
    FOnLLMStreamError OnError,
    FOnLLMToolCall OnToolCall,
    FOnLLMUsageReceived OnUsage)
{
    // Cancel any existing request
    CancelRequest();

    // Store delegates
    CurrentOnChunk = OnChunk;
    CurrentOnComplete = OnComplete;
    CurrentOnError = OnError;
    CurrentOnToolCall = OnToolCall;
    CurrentOnUsage = OnUsage;

    // Check for API key
    if (!HasApiKey())
    {
        UE_LOG(LogVibeUEAPIClient, Error, TEXT("No VibeUE API key configured"));
        if (OnError.IsBound())
        {
            OnError.Execute(TEXT("VibeUE API key not configured. Please set your API key in the settings."));
        }
        if (OnComplete.IsBound())
        {
            OnComplete.Execute(false);
        }
        return;
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

    // Add tools if provided
    if (Tools.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> ToolsArray;
        for (const FMCPTool& Tool : Tools)
        {
            TSharedPtr<FJsonObject> ToolObj = MakeShareable(new FJsonObject());
            ToolObj->SetStringField(TEXT("type"), TEXT("function"));

            TSharedPtr<FJsonObject> FunctionObj = MakeShareable(new FJsonObject());
            FunctionObj->SetStringField(TEXT("name"), Tool.Name);
            FunctionObj->SetStringField(TEXT("description"), Tool.Description);

            // InputSchema is already a TSharedPtr<FJsonObject>
            if (Tool.InputSchema.IsValid())
            {
                FunctionObj->SetObjectField(TEXT("parameters"), Tool.InputSchema);
            }
            else
            {
                // Empty parameters object if no schema
                TSharedPtr<FJsonObject> EmptyParams = MakeShareable(new FJsonObject());
                EmptyParams->SetStringField(TEXT("type"), TEXT("object"));
                EmptyParams->SetObjectField(TEXT("properties"), MakeShareable(new FJsonObject()));
                FunctionObj->SetObjectField(TEXT("parameters"), EmptyParams);
            }

            ToolObj->SetObjectField(TEXT("function"), FunctionObj);
            ToolsArray.Add(MakeShareable(new FJsonValueObject(ToolObj)));
        }
        RequestBody->SetArrayField(TEXT("tools"), ToolsArray);
        RequestBody->SetStringField(TEXT("tool_choice"), TEXT("auto"));
    }

    // Serialize to JSON string
    FString RequestBodyString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBodyString);
    FJsonSerializer::Serialize(RequestBody.ToSharedRef(), Writer);

    UE_LOG(LogVibeUEAPIClient, Verbose, TEXT("Sending chat request to VibeUE API: %s"), *EndpointUrl);

    // Create HTTP request
    CurrentRequest = FHttpModule::Get().CreateRequest();
    CurrentRequest->SetURL(EndpointUrl);
    CurrentRequest->SetVerb(TEXT("POST"));
    CurrentRequest->SetHeader(TEXT("Content-Type"), ContentTypeHeader);
    CurrentRequest->SetHeader(ApiKeyHeader, ApiKey);
    CurrentRequest->SetContentAsString(RequestBodyString);

    // Bind streaming callbacks
    CurrentRequest->OnRequestProgress64().BindRaw(this, &FVibeUEAPIClient::HandleRequestProgress);
    CurrentRequest->OnProcessRequestComplete().BindRaw(this, &FVibeUEAPIClient::HandleRequestComplete);

    // Send the request
    CurrentRequest->ProcessRequest();
}

void FVibeUEAPIClient::HandleRequestProgress(FHttpRequestPtr Request, uint64 BytesSent, uint64 BytesReceived)
{
    if (!Request.IsValid() || !Request->GetResponse().IsValid())
    {
        return;
    }

    FString ResponseContent = Request->GetResponse()->GetContentAsString();
    
    // Only process new content
    if (ResponseContent.Len() > StreamBuffer.Len())
    {
        FString NewContent = ResponseContent.RightChop(StreamBuffer.Len());
        StreamBuffer = ResponseContent;
        ProcessSSEData(NewContent);
    }
}

void FVibeUEAPIClient::ProcessSSEData(const FString& Data)
{
    // Split by newlines and process each SSE event
    TArray<FString> Lines;
    Data.ParseIntoArray(Lines, TEXT("\n"), true);

    for (const FString& Line : Lines)
    {
        // Skip empty lines and comments
        FString TrimmedLine = Line.TrimStartAndEnd();
        if (TrimmedLine.IsEmpty() || TrimmedLine.StartsWith(TEXT(":")))
        {
            continue;
        }

        // Handle SSE data lines
        if (TrimmedLine.StartsWith(TEXT("data: ")))
        {
            FString JsonData = TrimmedLine.RightChop(6); // Remove "data: " prefix
            
            // Check for stream end
            if (JsonData == TEXT("[DONE]"))
            {
                // Process any pending tool calls
                if (PendingToolCalls.Num() > 0 && CurrentOnToolCall.IsBound())
                {
                    // Sort by index and execute
                    TArray<int32> Indices;
                    PendingToolCalls.GetKeys(Indices);
                    Indices.Sort();

                    for (int32 Index : Indices)
                    {
                        CurrentOnToolCall.Execute(PendingToolCalls[Index]);
                    }
                }
                continue;
            }

            // Parse JSON
            TSharedPtr<FJsonObject> JsonObject;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonData);
            if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
            {
                continue;
            }

            // Check for error
            if (JsonObject->HasField(TEXT("error")))
            {
                const TSharedPtr<FJsonObject>* ErrorObj;
                if (JsonObject->TryGetObjectField(TEXT("error"), ErrorObj))
                {
                    FString ErrorMessage = (*ErrorObj)->GetStringField(TEXT("message"));
                    UE_LOG(LogVibeUEAPIClient, Error, TEXT("VibeUE API error: %s"), *ErrorMessage);
                    if (CurrentOnError.IsBound())
                    {
                        CurrentOnError.Execute(ErrorMessage);
                    }
                }
                continue;
            }

            // Check for usage stats
            const TSharedPtr<FJsonObject>* UsageObj;
            if (JsonObject->TryGetObjectField(TEXT("usage"), UsageObj) && CurrentOnUsage.IsBound())
            {
                int32 PromptTokens = (*UsageObj)->GetIntegerField(TEXT("prompt_tokens"));
                int32 CompletionTokens = (*UsageObj)->GetIntegerField(TEXT("completion_tokens"));
                CurrentOnUsage.Execute(PromptTokens, CompletionTokens);
            }

            // Process choices
            const TArray<TSharedPtr<FJsonValue>>* ChoicesArray;
            if (JsonObject->TryGetArrayField(TEXT("choices"), ChoicesArray))
            {
                for (const TSharedPtr<FJsonValue>& ChoiceValue : *ChoicesArray)
                {
                    const TSharedPtr<FJsonObject>* ChoiceObj;
                    if (!ChoiceValue->TryGetObject(ChoiceObj))
                    {
                        continue;
                    }

                    // Get delta object (for streaming)
                    const TSharedPtr<FJsonObject>* DeltaObj;
                    if ((*ChoiceObj)->TryGetObjectField(TEXT("delta"), DeltaObj))
                    {
                        // Check for tool calls in delta
                        const TArray<TSharedPtr<FJsonValue>>* ToolCallsArray;
                        if ((*DeltaObj)->TryGetArrayField(TEXT("tool_calls"), ToolCallsArray))
                        {
                            bToolCallsDetectedInStream = true;
                            
                            for (const TSharedPtr<FJsonValue>& ToolCallValue : *ToolCallsArray)
                            {
                                const TSharedPtr<FJsonObject>* ToolCallObj;
                                if (!ToolCallValue->TryGetObject(ToolCallObj))
                                {
                                    continue;
                                }

                                int32 ToolIndex = (*ToolCallObj)->GetIntegerField(TEXT("index"));
                                
                                // Initialize tool call if not exists
                                if (!PendingToolCalls.Contains(ToolIndex))
                                {
                                    FMCPToolCall NewToolCall;
                                    PendingToolCalls.Add(ToolIndex, NewToolCall);
                                }

                                FMCPToolCall& ToolCall = PendingToolCalls[ToolIndex];

                                // Get ID if present
                                FString ToolCallId;
                                if ((*ToolCallObj)->TryGetStringField(TEXT("id"), ToolCallId))
                                {
                                    ToolCall.Id = ToolCallId;
                                }

                                // Get function details
                                const TSharedPtr<FJsonObject>* FunctionObj;
                                if ((*ToolCallObj)->TryGetObjectField(TEXT("function"), FunctionObj))
                                {
                                    FString FunctionName;
                                    if ((*FunctionObj)->TryGetStringField(TEXT("name"), FunctionName))
                                    {
                                        ToolCall.ToolName = FunctionName;
                                    }

                                    FString FunctionArgs;
                                    if ((*FunctionObj)->TryGetStringField(TEXT("arguments"), FunctionArgs))
                                    {
                                        ToolCall.ArgumentsJson += FunctionArgs; // Accumulate arguments
                                    }
                                }
                            }
                        }

                        // Get content if present (skip if we have tool calls)
                        FString DeltaContent;
                        if ((*DeltaObj)->TryGetStringField(TEXT("content"), DeltaContent) && 
                            !bToolCallsDetectedInStream && 
                            !DeltaContent.IsEmpty())
                        {
                            // Strip <think>...</think> tags from Qwen3 responses
                            // The thinking content is between <think> and </think> tags
                            static bool bInThinkBlock = false;
                            FString CleanContent;
                            
                            int32 ThinkStart = DeltaContent.Find(TEXT("<think>"), ESearchCase::IgnoreCase);
                            int32 ThinkEnd = DeltaContent.Find(TEXT("</think>"), ESearchCase::IgnoreCase);
                            
                            if (ThinkStart != INDEX_NONE || bInThinkBlock)
                            {
                                if (ThinkStart != INDEX_NONE && ThinkEnd != INDEX_NONE && ThinkEnd > ThinkStart)
                                {
                                    // Complete think block in this chunk
                                    CleanContent = DeltaContent.Left(ThinkStart) + DeltaContent.Mid(ThinkEnd + 8);
                                    bInThinkBlock = false;
                                }
                                else if (ThinkStart != INDEX_NONE)
                                {
                                    // Think block starts but doesn't end
                                    CleanContent = DeltaContent.Left(ThinkStart);
                                    bInThinkBlock = true;
                                }
                                else if (ThinkEnd != INDEX_NONE)
                                {
                                    // Think block ends
                                    CleanContent = DeltaContent.Mid(ThinkEnd + 8);
                                    bInThinkBlock = false;
                                }
                                else
                                {
                                    // Still inside think block
                                    CleanContent = TEXT("");
                                }
                            }
                            else
                            {
                                CleanContent = DeltaContent;
                            }

                            if (!CleanContent.IsEmpty() && CurrentOnChunk.IsBound())
                            {
                                CurrentOnChunk.Execute(CleanContent);
                            }
                        }
                    }
                }
            }
        }
    }
}

void FVibeUEAPIClient::HandleRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
    if (!bConnectedSuccessfully)
    {
        UE_LOG(LogVibeUEAPIClient, Error, TEXT("VibeUE API request failed - connection error"));
        if (CurrentOnError.IsBound())
        {
            CurrentOnError.Execute(TEXT("Failed to connect to VibeUE API. Please check your network connection."));
        }
        if (CurrentOnComplete.IsBound())
        {
            CurrentOnComplete.Execute(false);
        }
        CurrentRequest.Reset();
        return;
    }

    int32 ResponseCode = Response.IsValid() ? Response->GetResponseCode() : 0;
    
    if (ResponseCode == 200)
    {
        UE_LOG(LogVibeUEAPIClient, Verbose, TEXT("VibeUE API request completed successfully"));
        if (CurrentOnComplete.IsBound())
        {
            CurrentOnComplete.Execute(true);
        }
    }
    else if (ResponseCode == 401)
    {
        UE_LOG(LogVibeUEAPIClient, Error, TEXT("VibeUE API authentication failed - invalid API key"));
        if (CurrentOnError.IsBound())
        {
            CurrentOnError.Execute(TEXT("Invalid VibeUE API key. Please check your API key in settings."));
        }
        if (CurrentOnComplete.IsBound())
        {
            CurrentOnComplete.Execute(false);
        }
    }
    else
    {
        FString ErrorMessage = FString::Printf(TEXT("VibeUE API error (HTTP %d)"), ResponseCode);
        if (Response.IsValid())
        {
            FString ResponseBody = Response->GetContentAsString();
            if (!ResponseBody.IsEmpty())
            {
                // Try to parse error message from JSON
                TSharedPtr<FJsonObject> JsonObject;
                TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
                if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
                {
                    FString Detail;
                    if (JsonObject->TryGetStringField(TEXT("detail"), Detail))
                    {
                        ErrorMessage = Detail;
                    }
                }
            }
        }
        UE_LOG(LogVibeUEAPIClient, Error, TEXT("VibeUE API error: %s"), *ErrorMessage);
        if (CurrentOnError.IsBound())
        {
            CurrentOnError.Execute(ErrorMessage);
        }
        if (CurrentOnComplete.IsBound())
        {
            CurrentOnComplete.Execute(false);
        }
    }

    // Clean up
    CurrentRequest.Reset();
    StreamBuffer.Empty();
    PendingToolCalls.Empty();
    bToolCallsDetectedInStream = false;
}
