// Copyright Kevin Buckley 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"

class FProperty;
class FJsonObject;

/**
 * Converts between JSON strings and C++ types for tool parameters
 */
class VIBEUE_API FParameterConverter
{
public:
	/**
	 * Convert JSON string to appropriate C++ type for function parameter
	 * @param JsonValue JSON string representation of the value
	 * @param Property Reflection property describing the expected type
	 * @param OutValue Pointer to memory where converted value should be written
	 * @return True if conversion succeeded, false otherwise
	 */
	static bool ConvertParameter(
		const FString& JsonValue,
		FProperty* Property,
		void* OutValue
	);

	/**
	 * Convert property value to JSON string
	 * @param Property Reflection property describing the type
	 * @param Value Pointer to memory containing the value
	 * @return JSON string representation
	 */
	static FString ConvertToJson(FProperty* Property, void* Value);

private:
	/** Convert JSON string to FString */
	static bool ConvertString(const FString& JsonValue, FString& OutValue);

	/** Convert JSON string to int32 */
	static bool ConvertInt(const FString& JsonValue, int32& OutValue);

	/** Convert JSON string to float */
	static bool ConvertFloat(const FString& JsonValue, float& OutValue);

	/** Convert JSON string to bool */
	static bool ConvertBool(const FString& JsonValue, bool& OutValue);

	/** Convert JSON string to UObject* */
	static bool ConvertObject(const FString& JsonValue, UObject*& OutValue);

	/** Convert JSON string to array */
	static bool ConvertArray(const FString& JsonValue, class FArrayProperty* Property, void* OutValue);

	/** Parse JSON string to FJsonObject */
	static TSharedPtr<FJsonObject> ParseJsonString(const FString& JsonValue);
};

