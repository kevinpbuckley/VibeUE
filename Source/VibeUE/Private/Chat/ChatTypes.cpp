// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#include "Chat/ChatTypes.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

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
