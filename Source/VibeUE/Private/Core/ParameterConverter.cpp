// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Core/ParameterConverter.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "UObject/UnrealType.h"
#include "UObject/Class.h"

bool FParameterConverter::ConvertParameter(
	const FString& JsonValue,
	FProperty* Property,
	void* OutValue)
{
	if (!Property || !OutValue)
	{
		return false;
	}

	// Handle different property types
	if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		return ConvertString(JsonValue, *static_cast<FString*>(OutValue));
	}
	else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
	{
		return ConvertInt(JsonValue, *static_cast<int32*>(OutValue));
	}
	else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
	{
		return ConvertFloat(JsonValue, *static_cast<float*>(OutValue));
	}
	else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		return ConvertBool(JsonValue, *static_cast<bool*>(OutValue));
	}
	else if (FObjectProperty* ObjectProp = CastField<FObjectProperty>(Property))
	{
		return ConvertObject(JsonValue, *static_cast<UObject**>(OutValue));
	}
	else if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
	{
		return ConvertArray(JsonValue, ArrayProp, OutValue);
	}

	return false;
}

FString FParameterConverter::ConvertToJson(FProperty* Property, void* Value)
{
	if (!Property || !Value)
	{
		return TEXT("null");
	}

	// Handle different property types
	if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		FString StringValue = StrProp->GetPropertyValue(Value);
		// Escape and quote the string
		return FString::Printf(TEXT("\"%s\""), *StringValue.Replace(TEXT("\""), TEXT("\\\"")));
	}
	else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
	{
		int32 IntValue = IntProp->GetPropertyValue(Value);
		return FString::Printf(TEXT("%d"), IntValue);
	}
	else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
	{
		float FloatValue = FloatProp->GetFloatingPointPropertyValue(Value);
		return FString::Printf(TEXT("%f"), FloatValue);
	}
	else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		bool BoolValue = BoolProp->GetPropertyValue(Value);
		return BoolValue ? TEXT("true") : TEXT("false");
	}
	else if (FObjectProperty* ObjectProp = CastField<FObjectProperty>(Property))
	{
		UObject* ObjectValue = ObjectProp->GetObjectPropertyValue(Value);
		if (ObjectValue)
		{
			return FString::Printf(TEXT("\"%s\""), *ObjectValue->GetPathName());
		}
		return TEXT("null");
	}

	// For complex types, serialize to JSON object
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	// TODO: Implement full serialization for complex types
	return TEXT("{}");
}

bool FParameterConverter::ConvertString(const FString& JsonValue, FString& OutValue)
{
	// Remove quotes if present
	FString CleanValue = JsonValue.TrimStartAndEnd();
	if (CleanValue.StartsWith(TEXT("\"")) && CleanValue.EndsWith(TEXT("\"")))
	{
		CleanValue = CleanValue.Mid(1, CleanValue.Len() - 2);
		// Unescape
		CleanValue = CleanValue.Replace(TEXT("\\\""), TEXT("\""));
		CleanValue = CleanValue.Replace(TEXT("\\n"), TEXT("\n"));
		CleanValue = CleanValue.Replace(TEXT("\\t"), TEXT("\t"));
		CleanValue = CleanValue.Replace(TEXT("\\\\"), TEXT("\\"));
	}
	OutValue = CleanValue;
	return true;
}

bool FParameterConverter::ConvertInt(const FString& JsonValue, int32& OutValue)
{
	FString CleanValue = JsonValue.TrimStartAndEnd();
	return LexTryParseString(OutValue, *CleanValue);
}

bool FParameterConverter::ConvertFloat(const FString& JsonValue, float& OutValue)
{
	FString CleanValue = JsonValue.TrimStartAndEnd();
	return LexTryParseString(OutValue, *CleanValue);
}

bool FParameterConverter::ConvertBool(const FString& JsonValue, bool& OutValue)
{
	FString CleanValue = JsonValue.TrimStartAndEnd().ToLower();
	if (CleanValue == TEXT("true") || CleanValue == TEXT("1"))
	{
		OutValue = true;
		return true;
	}
	else if (CleanValue == TEXT("false") || CleanValue == TEXT("0"))
	{
		OutValue = false;
		return true;
	}
	return false;
}

bool FParameterConverter::ConvertObject(const FString& JsonValue, UObject*& OutValue)
{
	// Try to find object by path
	FString CleanValue = JsonValue.TrimStartAndEnd();
	if (CleanValue.StartsWith(TEXT("\"")) && CleanValue.EndsWith(TEXT("\"")))
	{
		CleanValue = CleanValue.Mid(1, CleanValue.Len() - 2);
	}

	OutValue = FindObject<UObject>(nullptr, *CleanValue);
	return OutValue != nullptr;
}

bool FParameterConverter::ConvertArray(const FString& JsonValue, FArrayProperty* Property, void* OutValue)
{
	// Parse JSON array
	TSharedPtr<FJsonObject> JsonObject = ParseJsonString(JsonValue);
	if (!JsonObject.IsValid())
	{
		return false;
	}

	// TODO: Implement array conversion
	// This requires more complex handling of array elements
	return false;
}

TSharedPtr<FJsonObject> FParameterConverter::ParseJsonString(const FString& JsonValue)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonValue);
	
	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		return JsonObject;
	}

	return nullptr;
}

