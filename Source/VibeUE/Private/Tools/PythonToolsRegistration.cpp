// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Tools/PythonTools.h"
#include "Core/ToolRegistry.h"
#include "Json.h"

// Helper function to extract a field from ParamsJson
static FString ExtractParamFromJson(const TMap<FString, FString>& Params, const FString& FieldName)
{
	// First check if parameter exists directly (case-insensitive check for action/Action)
	const FString* DirectParam = Params.Find(FieldName);
	if (DirectParam)
	{
		return *DirectParam;
	}
	
	// Also check capitalized version (MCP server capitalizes 'action' to 'Action')
	FString CapitalizedField = FieldName;
	if (CapitalizedField.Len() > 0)
	{
		CapitalizedField[0] = FChar::ToUpper(CapitalizedField[0]);
	}
	DirectParam = Params.Find(CapitalizedField);
	if (DirectParam)
	{
		return *DirectParam;
	}

	// Otherwise, try to extract from ParamsJson
	const FString* ParamsJsonStr = Params.Find(TEXT("ParamsJson"));
	if (!ParamsJsonStr)
	{
		return FString();
	}

	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(*ParamsJsonStr);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		return FString();
	}

	FString Value;
	if (JsonObj->TryGetStringField(FieldName, Value))
	{
		return Value;
	}

	return FString();
}

// Helper to extract with default value
static FString ExtractParamWithDefault(const TMap<FString, FString>& Params, const FString& FieldName, const FString& DefaultValue)
{
	FString Value = ExtractParamFromJson(Params, FieldName);
	return Value.IsEmpty() ? DefaultValue : Value;
}

// Helper to extract boolean
static bool ExtractBoolParam(const TMap<FString, FString>& Params, const FString& FieldName, bool DefaultValue)
{
	FString Value = ExtractParamFromJson(Params, FieldName);
	if (Value.IsEmpty())
	{
		return DefaultValue;
	}
	return Value.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("1"));
}

// Helper to extract integer
static int32 ExtractIntParam(const TMap<FString, FString>& Params, const FString& FieldName, int32 DefaultValue)
{
	FString Value = ExtractParamFromJson(Params, FieldName);
	if (Value.IsEmpty())
	{
		return DefaultValue;
	}
	return FCString::Atoi(*Value);
}

// Helper to extract a parameter that may hold one value or several. Accepts a plain
// string, a comma-separated list, a JSON-array-encoded string ('["a","b"]'), or a real
// JSON array inside ParamsJson (string extraction fails on those, so it is checked here).
static TArray<FString> ExtractStringListParam(const TMap<FString, FString>& Params, const FString& FieldName)
{
	TArray<FString> Values;

	auto AddValue = [&Values](FString Value)
	{
		Value.TrimStartAndEndInline();
		if (!Value.IsEmpty())
		{
			Values.Add(MoveTemp(Value));
		}
	};

	FString Raw = ExtractParamFromJson(Params, FieldName).TrimStartAndEnd();
	if (!Raw.IsEmpty())
	{
		if (Raw.StartsWith(TEXT("[")))
		{
			TArray<TSharedPtr<FJsonValue>> JsonValues;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
			if (FJsonSerializer::Deserialize(Reader, JsonValues))
			{
				for (const TSharedPtr<FJsonValue>& Value : JsonValues)
				{
					FString Str;
					if (Value.IsValid() && Value->TryGetString(Str))
					{
						AddValue(Str);
					}
				}
				return Values;
			}
		}
		TArray<FString> Parts;
		Raw.ParseIntoArray(Parts, TEXT(","), true);
		for (FString& Part : Parts)
		{
			AddValue(Part);
		}
		return Values;
	}

	const FString* ParamsJsonStr = Params.Find(TEXT("ParamsJson"));
	if (ParamsJsonStr)
	{
		TSharedPtr<FJsonObject> JsonObj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(*ParamsJsonStr);
		if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
		{
			const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
			if (JsonObj->TryGetArrayField(FieldName, Arr) && Arr)
			{
				for (const TSharedPtr<FJsonValue>& Value : *Arr)
				{
					FString Str;
					if (Value.IsValid() && Value->TryGetString(Str))
					{
						AddValue(Str);
					}
				}
			}
		}
	}
	return Values;
}

// Helper to build a serialized JSON error object
static FString MakeErrorJson(const FString& ErrorCode, const FString& ErrorMessage, const FString& ClassName = FString())
{
	TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
	ErrorObj->SetBoolField(TEXT("success"), false);
	if (!ClassName.IsEmpty())
	{
		ErrorObj->SetStringField(TEXT("class_name"), ClassName);
	}
	ErrorObj->SetStringField(TEXT("error_code"), ErrorCode);
	ErrorObj->SetStringField(TEXT("error_message"), ErrorMessage);
	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(ErrorObj.ToSharedRef(), Writer);
	return JsonString;
}

