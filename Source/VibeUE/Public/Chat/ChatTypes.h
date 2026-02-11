// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "ChatTypes.generated.h"

/**
 * Status of a task in the task list
 */
UENUM()
enum class EVibeUETaskStatus : uint8
{
    NotStarted    UMETA(DisplayName = "Not Started"),
    InProgress    UMETA(DisplayName = "In Progress"),
    Completed     UMETA(DisplayName = "Completed")
};

/**
 * A single task item in the task list
 */
USTRUCT()
struct VIBEUE_API FVibeUETaskItem
{
    GENERATED_BODY()

    UPROPERTY()
    int32 Id = 0;

    UPROPERTY()
    FString Title;

    UPROPERTY()
    EVibeUETaskStatus Status = EVibeUETaskStatus::NotStarted;

    /** Convert to JSON */
    TSharedPtr<FJsonObject> ToJson() const;

    /** Create from JSON */
    static FVibeUETaskItem FromJson(const TSharedPtr<FJsonObject>& JsonObject);

    /** Get status as string ("not-started", "in-progress", "completed") */
    FString GetStatusString() const;

    /** Parse status from string */
    static EVibeUETaskStatus ParseStatus(const FString& StatusStr);
};

/**
 * Content part for multimodal messages (vision support)
 * Represents a single part of a message that can be text or an image
 */
USTRUCT(BlueprintType)
struct VIBEUE_API FContentPart
{
    GENERATED_BODY()
    
    /** Type of content: "text" or "image_url" */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
    FString Type;
    
    /** Text content (when Type="text") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
    FString Text;
    
    /** Image URL (when Type="image_url") - can be data URL (base64) or HTTP(S) URL */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
    FString ImageUrl;
    
    /** Image detail level: "auto", "low", or "high" (optional) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
    FString ImageDetail;
    
    FContentPart() : Type(TEXT("text")) {}
    
    /** Create a text content part */
    static FContentPart MakeText(const FString& InText)
    {
        FContentPart Part;
        Part.Type = TEXT("text");
        Part.Text = InText;
        return Part;
    }
    
    /** Create an image_url content part */
    static FContentPart MakeImage(const FString& InImageUrl, const FString& InDetail = TEXT("auto"))
    {
        FContentPart Part;
        Part.Type = TEXT("image_url");
        Part.ImageUrl = InImageUrl;
        Part.ImageDetail = InDetail;
        return Part;
    }
    
    /** Convert to JSON for API requests */
    TSharedPtr<FJsonObject> ToJson() const
    {
        TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
        JsonObject->SetStringField(TEXT("type"), Type);
        
        if (Type == TEXT("text"))
        {
            JsonObject->SetStringField(TEXT("text"), Text);
        }
        else if (Type == TEXT("image_url"))
        {
            TSharedPtr<FJsonObject> ImageUrlObj = MakeShared<FJsonObject>();
            ImageUrlObj->SetStringField(TEXT("url"), ImageUrl);
            if (!ImageDetail.IsEmpty())
            {
                ImageUrlObj->SetStringField(TEXT("detail"), ImageDetail);
            }
            JsonObject->SetObjectField(TEXT("image_url"), ImageUrlObj);
        }
        
        return JsonObject;
    }
};

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
    
    /** Multimodal content parts (for vision/image support)
     * If this array is non-empty, it takes precedence over Content field
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
    TArray<FContentPart> ContentParts;
    
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
    
    /** Check if this message uses multimodal content (has content parts) */
    bool IsMultimodal() const
    {
        return ContentParts.Num() > 0;
    }
    
    /** Check if this message contains vision content (images) */
    bool HasVisionContent() const
    {
        for (const FContentPart& Part : ContentParts)
        {
            if (Part.Type == TEXT("image_url"))
            {
                return true;
            }
        }
        return false;
    }
    
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
        // Multimodal content (vision support) - use content parts array
        else if (IsMultimodal())
        {
            TArray<TSharedPtr<FJsonValue>> ContentArray;
            for (const FContentPart& Part : ContentParts)
            {
                ContentArray.Add(MakeShared<FJsonValueObject>(Part.ToJson()));
            }
            JsonObject->SetArrayField(TEXT("content"), ContentArray);
        }
        // Standard text content (backwards compatible)
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
    
    /** Community rating from VibeUE website: "great", "good", "moderate", "bad", or empty for unrated */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
    FString Rating;
    
    FOpenRouterModel() = default;
    
    /** Get numeric rating tier for sorting (higher = better). 0 = unrated */
    int32 GetRatingTier() const
    {
        if (Rating == TEXT("great")) return 4;
        if (Rating == TEXT("good")) return 3;
        if (Rating == TEXT("moderate")) return 2;
        if (Rating == TEXT("bad")) return 1;
        return 0; // unrated
    }
    
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
        FString Prefix;
        if (Rating == TEXT("great"))
        {
            Prefix = TEXT("\u2B50 "); // Gold star emoji
        }
        
        if (IsFree())
        {
            return FString::Printf(TEXT("%s[FREE] %s (%dK)"), *Prefix, *Name, ContextLength / 1024);
        }
        else
        {
            return FString::Printf(TEXT("%s%s (%dK) $%.2f/1M"), *Prefix, *Name, ContextLength / 1024, PricingPrompt);
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
