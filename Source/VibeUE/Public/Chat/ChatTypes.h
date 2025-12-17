// Copyright 2025 Vibe AI. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "ChatTypes.generated.h"

/**
 * Represents a tool call made by the assistant
 */
USTRUCT(BlueprintType)
struct VIBEUE_API FChatToolCall
{
    GENERATED_BODY()
    
    /** Unique identifier for this tool call */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
    FString Id;
    
    /** Name of the tool being called */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
    FString Name;
    
    /** Arguments as JSON string */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
    FString Arguments;
    
    FChatToolCall() {}
    
    FChatToolCall(const FString& InId, const FString& InName, const FString& InArgs)
        : Id(InId), Name(InName), Arguments(InArgs) {}
};

/**
 * Represents a single message in the chat conversation
 */
USTRUCT(BlueprintType)
struct VIBEUE_API FChatMessage
{
    GENERATED_BODY()
    
    /** Role of the message sender: "user", "assistant", "system", or "tool" */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
    FString Role;
    
    /** The text content of the message */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
    FString Content;
    
    /** Chain-of-thought reasoning content (from <think> tags, not shown to user) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
    FString ThinkingContent;
    
    /** When the message was sent or received */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
    FDateTime Timestamp;
    
    /** True while the message is still being streamed from the API */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
    bool bIsStreaming = false;
    
    /** Tool calls made by the assistant (for role="assistant") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
    TArray<FChatToolCall> ToolCalls;
    
    /** Tool call ID this message is responding to (for role="tool") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
    FString ToolCallId;
    
    FChatMessage()
        : Role(TEXT("user"))
        , Content(TEXT(""))
        , Timestamp(FDateTime::Now())
        , bIsStreaming(false)
    {}
    
    FChatMessage(const FString& InRole, const FString& InContent)
        : Role(InRole)
        , Content(InContent)
        , Timestamp(FDateTime::Now())
        , bIsStreaming(false)
    {}
    
    /** Create a JSON object for API requests */
    TSharedPtr<FJsonObject> ToJson() const
    {
        TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
        JsonObject->SetStringField(TEXT("role"), Role);
        
        // For tool messages, include tool_call_id
        if (Role == TEXT("tool"))
        {
            JsonObject->SetStringField(TEXT("tool_call_id"), ToolCallId);
            JsonObject->SetStringField(TEXT("content"), Content);
        }
        // For assistant messages with tool calls
        else if (Role == TEXT("assistant") && ToolCalls.Num() > 0)
        {
            // Content can be null/empty when making tool calls
            if (!Content.IsEmpty())
            {
                JsonObject->SetStringField(TEXT("content"), Content);
            }
            
            TArray<TSharedPtr<FJsonValue>> ToolCallsArray;
            for (const FChatToolCall& TC : ToolCalls)
            {
                TSharedPtr<FJsonObject> TCObj = MakeShared<FJsonObject>();
                TCObj->SetStringField(TEXT("id"), TC.Id);
                TCObj->SetStringField(TEXT("type"), TEXT("function"));
                
                TSharedPtr<FJsonObject> FuncObj = MakeShared<FJsonObject>();
                FuncObj->SetStringField(TEXT("name"), TC.Name);
                FuncObj->SetStringField(TEXT("arguments"), TC.Arguments);
                TCObj->SetObjectField(TEXT("function"), FuncObj);
                
                ToolCallsArray.Add(MakeShared<FJsonValueObject>(TCObj));
            }
            JsonObject->SetArrayField(TEXT("tool_calls"), ToolCallsArray);
        }
        else
        {
            JsonObject->SetStringField(TEXT("content"), Content);
        }
        
        return JsonObject;
    }
    
    /** Create from JSON (for persistence) */
    static FChatMessage FromJson(const TSharedPtr<FJsonObject>& JsonObject)
    {
        FChatMessage Message;
        if (JsonObject.IsValid())
        {
            Message.Role = JsonObject->GetStringField(TEXT("role"));
            JsonObject->TryGetStringField(TEXT("content"), Message.Content);
            JsonObject->TryGetStringField(TEXT("tool_call_id"), Message.ToolCallId);
            
            FString TimestampStr;
            if (JsonObject->TryGetStringField(TEXT("timestamp"), TimestampStr))
            {
                FDateTime::ParseIso8601(*TimestampStr, Message.Timestamp);
            }
            
            // Parse tool calls if present
            const TArray<TSharedPtr<FJsonValue>>* ToolCallsArray;
            if (JsonObject->TryGetArrayField(TEXT("tool_calls"), ToolCallsArray))
            {
                for (const auto& TCVal : *ToolCallsArray)
                {
                    const TSharedPtr<FJsonObject>& TCObj = TCVal->AsObject();
                    if (TCObj.IsValid())
                    {
                        FChatToolCall TC;
                        TCObj->TryGetStringField(TEXT("id"), TC.Id);
                        
                        const TSharedPtr<FJsonObject>* FuncObj;
                        if (TCObj->TryGetObjectField(TEXT("function"), FuncObj))
                        {
                            (*FuncObj)->TryGetStringField(TEXT("name"), TC.Name);
                            (*FuncObj)->TryGetStringField(TEXT("arguments"), TC.Arguments);
                        }
                        Message.ToolCalls.Add(TC);
                    }
                }
            }
        }
        return Message;
    }
    
    /** Convert to JSON for persistence */
    TSharedPtr<FJsonObject> ToJsonForPersistence() const
    {
        TSharedPtr<FJsonObject> JsonObject = ToJson();
        JsonObject->SetStringField(TEXT("timestamp"), Timestamp.ToIso8601());
        return JsonObject;
    }
};

/**
 * Represents an OpenRouter model with its metadata
 */
USTRUCT(BlueprintType)
struct VIBEUE_API FOpenRouterModel
{
    GENERATED_BODY()
    
    /** Model identifier, e.g., "anthropic/claude-3.5-sonnet" */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
    FString Id;
    
    /** Human-readable name, e.g., "Claude 3.5 Sonnet" */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
    FString Name;
    
    /** Maximum context length in tokens */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
    int32 ContextLength = 0;
    
    /** Price per 1M prompt tokens (USD) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
    float PricingPrompt = 0.0f;
    
    /** Price per 1M completion tokens (USD) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
    float PricingCompletion = 0.0f;
    
    /** Whether this model supports tool/function calling */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
    bool bSupportsTools = false;
    
    FOpenRouterModel() = default;
    
    /** Create from OpenRouter API JSON response */
    static FOpenRouterModel FromJson(const TSharedPtr<FJsonObject>& JsonObject)
    {
        FOpenRouterModel Model;
        if (JsonObject.IsValid())
        {
            Model.Id = JsonObject->GetStringField(TEXT("id"));
            Model.Name = JsonObject->GetStringField(TEXT("name"));
            Model.ContextLength = JsonObject->GetIntegerField(TEXT("context_length"));
            
            const TSharedPtr<FJsonObject>* PricingObject;
            if (JsonObject->TryGetObjectField(TEXT("pricing"), PricingObject))
            {
                FString PromptStr = (*PricingObject)->GetStringField(TEXT("prompt"));
                FString CompletionStr = (*PricingObject)->GetStringField(TEXT("completion"));
                Model.PricingPrompt = FCString::Atof(*PromptStr) * 1000000.0f; // Convert to per 1M tokens
                Model.PricingCompletion = FCString::Atof(*CompletionStr) * 1000000.0f;
            }
            
            // Check for tool support in supported_parameters array
            const TArray<TSharedPtr<FJsonValue>>* SupportedParams;
            if (JsonObject->TryGetArrayField(TEXT("supported_parameters"), SupportedParams))
            {
                for (const auto& Param : *SupportedParams)
                {
                    if (Param->AsString() == TEXT("tools"))
                    {
                        Model.bSupportsTools = true;
                        break;
                    }
                }
            }
        }
        return Model;
    }
    
    /** Check if this model is free */
    bool IsFree() const
    {
        return PricingPrompt == 0.0f && PricingCompletion == 0.0f;
    }
    
    /** Get display string for dropdown */
    FString GetDisplayString() const
    {
        if (IsFree())
        {
            return FString::Printf(TEXT("[FREE] %s (%dK)"), *Name, ContextLength / 1024);
        }
        else
        {
            // Show price per 1M tokens
            return FString::Printf(TEXT("%s (%dK) $%.2f/1M"), *Name, ContextLength / 1024, PricingPrompt);
        }
    }
};

/**
 * Chat history persistence format
 */
USTRUCT()
struct VIBEUE_API FChatHistory
{
    GENERATED_BODY()
    
    /** Version for forward compatibility */
    UPROPERTY()
    int32 Version = 1;
    
    /** Last used model ID */
    UPROPERTY()
    FString LastModel;
    
    /** All messages in the conversation */
    UPROPERTY()
    TArray<FChatMessage> Messages;
    
    /** Convert to JSON for file storage */
    FString ToJsonString() const;
    
    /** Load from JSON file content */
    static FChatHistory FromJsonString(const FString& JsonString);
};
