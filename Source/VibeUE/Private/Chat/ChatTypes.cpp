// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Chat/ChatTypes.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// ============ FVibeUETaskItem ============

TSharedPtr<FJsonObject> FVibeUETaskItem::ToJson() const
{
    TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
    JsonObject->SetNumberField(TEXT("id"), Id);
    JsonObject->SetStringField(TEXT("title"), Title);
    JsonObject->SetStringField(TEXT("status"), GetStatusString());
    return JsonObject;
}

FVibeUETaskItem FVibeUETaskItem::FromJson(const TSharedPtr<FJsonObject>& JsonObject)
{
    FVibeUETaskItem Item;
    if (JsonObject.IsValid())
    {
        Item.Id = JsonObject->GetIntegerField(TEXT("id"));
        JsonObject->TryGetStringField(TEXT("title"), Item.Title);

        FString StatusStr;
        if (JsonObject->TryGetStringField(TEXT("status"), StatusStr))
        {
            Item.Status = ParseStatus(StatusStr);
        }
    }
    return Item;
}

FString FVibeUETaskItem::GetStatusString() const
{
    switch (Status)
    {
        case EVibeUETaskStatus::NotStarted:  return TEXT("not-started");
        case EVibeUETaskStatus::InProgress:  return TEXT("in-progress");
        case EVibeUETaskStatus::Completed:   return TEXT("completed");
        default:                             return TEXT("not-started");
    }
}

EVibeUETaskStatus FVibeUETaskItem::ParseStatus(const FString& StatusStr)
{
    if (StatusStr.Equals(TEXT("in-progress"), ESearchCase::IgnoreCase) ||
        StatusStr.Equals(TEXT("in_progress"), ESearchCase::IgnoreCase))
    {
        return EVibeUETaskStatus::InProgress;
    }
    if (StatusStr.Equals(TEXT("completed"), ESearchCase::IgnoreCase))
    {
        return EVibeUETaskStatus::Completed;
    }
    return EVibeUETaskStatus::NotStarted;
}

// ============ FChatHistory ============

FString FChatHistory::ToJsonString() const
{
    TSharedPtr<FJsonObject> RootObject = MakeShared<FJsonObject>();
    RootObject->SetNumberField(TEXT("version"), Version);
    RootObject->SetStringField(TEXT("lastModel"), LastModel);
    
    TArray<TSharedPtr<FJsonValue>> MessagesArray;
    for (const FChatMessage& Message : Messages)
    {
        MessagesArray.Add(MakeShared<FJsonValueObject>(Message.ToJsonForPersistence()));
    }
    RootObject->SetArrayField(TEXT("messages"), MessagesArray);
    
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);
    
    return OutputString;
}

FChatHistory FChatHistory::FromJsonString(const FString& JsonString)
{
    FChatHistory History;
    
    TSharedPtr<FJsonObject> RootObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    
    if (FJsonSerializer::Deserialize(Reader, RootObject) && RootObject.IsValid())
    {
        History.Version = RootObject->GetIntegerField(TEXT("version"));
        History.LastModel = RootObject->GetStringField(TEXT("lastModel"));
        
        const TArray<TSharedPtr<FJsonValue>>* MessagesArray;
        if (RootObject->TryGetArrayField(TEXT("messages"), MessagesArray))
        {
            for (const TSharedPtr<FJsonValue>& Value : *MessagesArray)
            {
                if (Value.IsValid() && Value->Type == EJson::Object)
                {
                    History.Messages.Add(FChatMessage::FromJson(Value->AsObject()));
                }
            }
        }
    }
    
    return History;
}
