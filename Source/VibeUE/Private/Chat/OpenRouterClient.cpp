// Copyright 2025 Vibe AI. All Rights Reserved.

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
    : bToolCallsDetectedInStream(false)
{
}

FOpenRouterClient::~FOpenRouterClient()
{
    CancelRequest();
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
    // Try to load instructions from file
    FString InstructionsPath;
    FString InstructionsContent;
    
    // Priority 1: Project plugins (local development)
    InstructionsPath = FPaths::ProjectPluginsDir() / TEXT("VibeUE") / TEXT("Content") / TEXT("vibeue.instructions.md");
    if (FFileHelper::LoadFileToString(InstructionsContent, *InstructionsPath))
    {
        UE_LOG(LogOpenRouterClient, Log, TEXT("Loaded system prompt from: %s"), *InstructionsPath);
        return InstructionsContent;
    }
    
    // Priority 2: Engine marketplace (FAB install) - scan for VibeUE folder
    FString EngineMarketplacePath = FPaths::EnginePluginsDir() / TEXT("Marketplace");
    if (FPaths::DirectoryExists(EngineMarketplacePath))
    {
        IFileManager& FileManager = IFileManager::Get();
        TArray<FString> Directories;
        FileManager.FindFiles(Directories, *(EngineMarketplacePath / TEXT("*")), false, true);
        
        for (const FString& DirName : Directories)
        {
            InstructionsPath = EngineMarketplacePath / DirName / TEXT("Content") / TEXT("vibeue.instructions.md");
            if (FFileHelper::LoadFileToString(InstructionsContent, *InstructionsPath))
            {
                UE_LOG(LogOpenRouterClient, Log, TEXT("Loaded system prompt from: %s"), *InstructionsPath);
                return InstructionsContent;
            }
        }
    }
    
    // Fallback: Built-in minimal prompt
    UE_LOG(LogOpenRouterClient, Warning, TEXT("Could not load vibeue.instructions.md, using fallback prompt"));
    return TEXT(
        "You are an AI assistant integrated into Unreal Engine via the VibeUE plugin. "
        "You help users with Blueprint development, material creation, asset management, "
        "UMG widget design, Enhanced Input setup, and general Unreal Engine questions.\n\n"
        "You have access to MCP tools that can directly manipulate Unreal Engine. "
        "Use get_help(topic=\"overview\") to learn about available tools and workflows.\n\n"
        "Be concise and provide actionable guidance. When suggesting code or Blueprint "
        "logic, be specific about node names and connections."
    );
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

void FOpenRouterClient::SendChatRequest(
    const TArray<FChatMessage>& Messages,
    const FString& ModelId,
    const TArray<FMCPTool>& Tools,
    FOnLLMStreamChunk OnChunk,
    FOnLLMStreamComplete OnComplete,
    FOnLLMStreamError OnError,
    FOnLLMToolCall OnToolCall,
    FOnLLMUsageReceived OnUsage)
{
    if (!HasApiKey())
    {
        OnError.ExecuteIfBound(TEXT("No API key configured. Please set your OpenRouter API key in Editor Preferences."));
        return;
    }
    
    if (IsRequestInProgress())
    {
        OnError.ExecuteIfBound(TEXT("A request is already in progress. Please wait or cancel it."));
        return;
    }
    
    // Store delegates
    CurrentOnChunk = OnChunk;
    CurrentOnComplete = OnComplete;
    CurrentOnError = OnError;
    CurrentOnToolCall = OnToolCall;
    CurrentOnUsage = OnUsage;
    StreamBuffer.Empty();
    PendingToolCalls.Empty();
    bToolCallsDetectedInStream = false;
    
    // Build request body
    TSharedPtr<FJsonObject> RequestBody = MakeShared<FJsonObject>();
    RequestBody->SetStringField(TEXT("model"), ModelId);
    RequestBody->SetBoolField(TEXT("stream"), true);
    
    TArray<TSharedPtr<FJsonValue>> MessagesArray;
    for (const FChatMessage& Message : Messages)
    {
        MessagesArray.Add(MakeShared<FJsonValueObject>(Message.ToJson()));
    }
    RequestBody->SetArrayField(TEXT("messages"), MessagesArray);
    
    // Add tools if available
    if (Tools.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> ToolsArray;
        for (const FMCPTool& Tool : Tools)
        {
            ToolsArray.Add(MakeShared<FJsonValueObject>(Tool.ToOpenRouterJson()));
        }
        RequestBody->SetArrayField(TEXT("tools"), ToolsArray);
        
        UE_LOG(LogOpenRouterClient, Log, TEXT("Including %d tools in request"), Tools.Num());
    }
    
    FString RequestBodyString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBodyString);
    FJsonSerializer::Serialize(RequestBody.ToSharedRef(), Writer);
    
    // Create HTTP request
    CurrentRequest = FHttpModule::Get().CreateRequest();
    CurrentRequest->SetURL(ChatEndpoint);
    CurrentRequest->SetVerb(TEXT("POST"));
    CurrentRequest->SetHeader(TEXT("Content-Type"), ContentTypeHeader);
    CurrentRequest->SetHeader(AuthorizationHeader, FString::Printf(TEXT("Bearer %s"), *ApiKey));
    CurrentRequest->SetHeader(TEXT("HTTP-Referer"), TEXT("https://github.com/VibeUE"));
    CurrentRequest->SetHeader(TEXT("X-Title"), TEXT("VibeUE Plugin"));
    CurrentRequest->SetContentAsString(RequestBodyString);
    
    // Bind callbacks
    CurrentRequest->OnRequestProgress64().BindSP(this, &FOpenRouterClient::HandleRequestProgress);
    CurrentRequest->OnProcessRequestComplete().BindSP(this, &FOpenRouterClient::HandleRequestComplete);
    
    CurrentRequest->ProcessRequest();
    
    UE_LOG(LogOpenRouterClient, Log, TEXT("Sending chat request with model %s"), *ModelId);
}

void FOpenRouterClient::HandleRequestProgress(FHttpRequestPtr Request, uint64 BytesSent, uint64 BytesReceived)
{
    if (!Request.IsValid() || Request != CurrentRequest)
    {
        return;
    }
    
    FHttpResponsePtr Response = Request->GetResponse();
    if (!Response.IsValid())
    {
        return;
    }
    
    // Get the current content
    FString Content = Response->GetContentAsString();
    
    // Only process new data
    if (Content.Len() > StreamBuffer.Len())
    {
        FString NewData = Content.RightChop(StreamBuffer.Len());
        StreamBuffer = Content;
        ProcessSSEData(NewData);
    }
}

void FOpenRouterClient::ProcessSSEData(const FString& Data)
{
    // SSE format: "data: {json}\n\n"
    TArray<FString> Lines;
    Data.ParseIntoArrayLines(Lines);
    
    for (const FString& Line : Lines)
    {
        if (Line.StartsWith(TEXT("data: ")))
        {
            FString JsonStr = Line.RightChop(6); // Remove "data: " prefix
            
            if (JsonStr == TEXT("[DONE]"))
            {
                // Stream complete - fire any pending tool calls
                for (auto& Pair : PendingToolCalls)
                {
                    if (CurrentOnToolCall.IsBound() && !Pair.Value.ToolName.IsEmpty())
                    {
                        // Parse accumulated arguments JSON
                        if (!Pair.Value.ArgumentsJson.IsEmpty())
                        {
                            TSharedRef<TJsonReader<>> ArgReader = TJsonReaderFactory<>::Create(Pair.Value.ArgumentsJson);
                            FJsonSerializer::Deserialize(ArgReader, Pair.Value.Arguments);
                        }
                        
                        UE_LOG(LogOpenRouterClient, Log, TEXT("Tool call: %s with args: %s"), 
                            *Pair.Value.ToolName, *Pair.Value.ArgumentsJson);
                        CurrentOnToolCall.Execute(Pair.Value);
                    }
                }
                continue;
            }
            
            // Parse the JSON chunk
            TSharedPtr<FJsonObject> ChunkObject;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
            
            if (FJsonSerializer::Deserialize(Reader, ChunkObject) && ChunkObject.IsValid())
            {
                // Check for usage stats (OpenRouter sends these in the stream)
                const TSharedPtr<FJsonObject>* UsageObject;
                if (ChunkObject->TryGetObjectField(TEXT("usage"), UsageObject))
                {
                    int32 PromptTokens = 0;
                    int32 CompletionTokens = 0;
                    (*UsageObject)->TryGetNumberField(TEXT("prompt_tokens"), PromptTokens);
                    (*UsageObject)->TryGetNumberField(TEXT("completion_tokens"), CompletionTokens);
                    
                    if (PromptTokens > 0 || CompletionTokens > 0)
                    {
                        UE_LOG(LogOpenRouterClient, Log, TEXT("Usage: prompt=%d, completion=%d, total=%d"), 
                            PromptTokens, CompletionTokens, PromptTokens + CompletionTokens);
                        CurrentOnUsage.ExecuteIfBound(PromptTokens, CompletionTokens);
                    }
                }
                
                // Extract content from choices[0].delta
                const TArray<TSharedPtr<FJsonValue>>* ChoicesArray;
                if (ChunkObject->TryGetArrayField(TEXT("choices"), ChoicesArray) && ChoicesArray->Num() > 0)
                {
                    const TSharedPtr<FJsonObject>* ChoiceObject;
                    if ((*ChoicesArray)[0]->TryGetObject(ChoiceObject))
                    {
                        const TSharedPtr<FJsonObject>* DeltaObject;
                        if ((*ChoiceObject)->TryGetObjectField(TEXT("delta"), DeltaObject))
                        {
                            // Check for tool calls FIRST - if detected, we'll suppress content
                            // Handle tool calls (streaming format)
                            const TArray<TSharedPtr<FJsonValue>>* ToolCallsArray;
                            if ((*DeltaObject)->TryGetArrayField(TEXT("tool_calls"), ToolCallsArray))
                            {
                                // Mark that tool calls have been detected - suppress any content from now on
                                bToolCallsDetectedInStream = true;
                                
                                for (const auto& ToolCallValue : *ToolCallsArray)
                                {
                                    const TSharedPtr<FJsonObject>* ToolCallObj;
                                    if (ToolCallValue->TryGetObject(ToolCallObj))
                                    {
                                        int32 Index = 0;
                                        (*ToolCallObj)->TryGetNumberField(TEXT("index"), Index);
                                        
                                        // Get or create pending tool call
                                        FMCPToolCall& PendingCall = PendingToolCalls.FindOrAdd(Index);
                                        
                                        // Update ID if present
                                        FString Id;
                                        if ((*ToolCallObj)->TryGetStringField(TEXT("id"), Id))
                                        {
                                            PendingCall.Id = Id;
                                        }
                                        
                                        // Update function info if present
                                        const TSharedPtr<FJsonObject>* FunctionObj;
                                        if ((*ToolCallObj)->TryGetObjectField(TEXT("function"), FunctionObj))
                                        {
                                            FString FuncName;
                                            if ((*FunctionObj)->TryGetStringField(TEXT("name"), FuncName))
                                            {
                                                PendingCall.ToolName = FuncName;
                                            }
                                            
                                            FString Arguments;
                                            if ((*FunctionObj)->TryGetStringField(TEXT("arguments"), Arguments))
                                            {
                                                // Accumulate argument chunks
                                                PendingCall.ArgumentsJson += Arguments;
                                            }
                                        }
                                    }
                                }
                            }
                            
                            // Handle text content - only if no tool calls detected yet
                            // When LLM decides to use tools, it sometimes streams "thinking" text 
                            // before the tool call data arrives - we suppress that
                            FString Content;
                            if (!bToolCallsDetectedInStream && (*DeltaObject)->TryGetStringField(TEXT("content"), Content))
                            {
                                CurrentOnChunk.ExecuteIfBound(Content);
                            }
                        }
                    }
                }
            }
        }
    }
}

void FOpenRouterClient::HandleRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
    if (!Request.IsValid() || Request != CurrentRequest)
    {
        return;
    }
    
    CurrentRequest.Reset();
    
    if (!bConnectedSuccessfully)
    {
        CurrentOnError.ExecuteIfBound(TEXT("Failed to connect to OpenRouter. Check your internet connection."));
        CurrentOnComplete.ExecuteIfBound(false);
        return;
    }
    
    if (!Response.IsValid())
    {
        CurrentOnError.ExecuteIfBound(TEXT("Invalid response from OpenRouter."));
        CurrentOnComplete.ExecuteIfBound(false);
        return;
    }
    
    int32 ResponseCode = Response->GetResponseCode();
    if (ResponseCode == 401)
    {
        CurrentOnError.ExecuteIfBound(TEXT("Invalid API key. Please check your OpenRouter API key."));
        CurrentOnComplete.ExecuteIfBound(false);
        return;
    }
    else if (ResponseCode == 429)
    {
        CurrentOnError.ExecuteIfBound(TEXT("Rate limit exceeded. Please wait a moment and try again."));
        CurrentOnComplete.ExecuteIfBound(false);
        return;
    }
    else if (ResponseCode != 200)
    {
        FString ErrorMsg = FString::Printf(TEXT("OpenRouter request failed with code %d"), ResponseCode);
        
        // Try to extract error message from response
        TSharedPtr<FJsonObject> ErrorObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
        if (FJsonSerializer::Deserialize(Reader, ErrorObject) && ErrorObject.IsValid())
        {
            const TSharedPtr<FJsonObject>* ErrorDetail;
            if (ErrorObject->TryGetObjectField(TEXT("error"), ErrorDetail))
            {
                FString Message;
                if ((*ErrorDetail)->TryGetStringField(TEXT("message"), Message))
                {
                    ErrorMsg = Message;
                }
            }
        }
        
        CurrentOnError.ExecuteIfBound(ErrorMsg);
        CurrentOnComplete.ExecuteIfBound(false);
        return;
    }
    
    // Success - process any remaining data
    FString FinalContent = Response->GetContentAsString();
    if (FinalContent.Len() > StreamBuffer.Len())
    {
        FString NewData = FinalContent.RightChop(StreamBuffer.Len());
        ProcessSSEData(NewData);
    }
    
    UE_LOG(LogOpenRouterClient, Log, TEXT("Chat request completed successfully"));
    CurrentOnComplete.ExecuteIfBound(true);
}

void FOpenRouterClient::CancelRequest()
{
    if (CurrentRequest.IsValid())
    {
        CurrentRequest->CancelRequest();
        CurrentRequest.Reset();
        UE_LOG(LogOpenRouterClient, Log, TEXT("Cancelled in-progress request"));
    }
    
    StreamBuffer.Empty();
}

bool FOpenRouterClient::IsRequestInProgress() const
{
    return CurrentRequest.IsValid() && CurrentRequest->GetStatus() == EHttpRequestStatus::Processing;
}
