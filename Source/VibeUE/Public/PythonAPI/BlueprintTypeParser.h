// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphPin.h"

/**
 * Helper class for parsing type strings into EdGraphPinType structures.
 * Supports all Blueprint-compatible types including primitives, structs, objects, enums, and containers.
 *
 * Usage:
 *   FEdGraphPinType PinType;
 *   FString ErrorMsg;
 *   if (FBlueprintTypeParser::ParseTypeString("FVector", PinType, false, "", ErrorMsg))
 *   {
 *       // Use PinType
 *   }
 */
class VIBEUE_API FBlueprintTypeParser
{
public:
	/**
	 * Parse a type string into an EdGraphPinType.
	 *
	 * @param TypeString - Type name (e.g., "float", "FVector", "AActor", "TSubclassOf<AActor>")
	 * @param OutPinType - Resulting pin type structure
	 * @param bIsArray - Whether this is an array container
	 * @param ContainerType - Container type: "Array", "Set", "Map", or empty
	 * @param OutErrorMessage - Error message if parsing fails
	 * @return True if parsing succeeded
	 */
	static bool ParseTypeString(
		const FString& TypeString,
		FEdGraphPinType& OutPinType,
		bool bIsArray = false,
		const FString& ContainerType = TEXT(""),
		FString& OutErrorMessage = GetDefaultErrorMessage()
	);

	/**
	 * Get a friendly type name from an EdGraphPinType (for display/debugging).
	 *
	 * @param PinType - The pin type to convert
	 * @return Human-readable type string
	 */
	static FString GetFriendlyTypeName(const FEdGraphPinType& PinType);

	// Type category detection (public for use by other services)
	static bool IsBasicType(const FString& TypeString);
	static bool IsStructType(const FString& TypeString);
	static bool IsObjectType(const FString& TypeString);
	static bool IsClassType(const FString& TypeString);  // TSubclassOf<T>
	static bool IsEnumType(const FString& TypeString);

	// Type resolution (public for use by UEnumStructService and other services)
	static UScriptStruct* FindStructByName(const FString& StructName);
	static UClass* FindClassByName(const FString& ClassName);
	static UEnum* FindEnumByName(const FString& EnumName);

private:

	// Parse basic types
	static bool ParseBasicType(const FString& TypeString, FEdGraphPinType& OutPinType, FString& OutErrorMessage);

	// Parse struct types
	static bool ParseStructType(const FString& TypeString, FEdGraphPinType& OutPinType, FString& OutErrorMessage);

	// Parse object types
	static bool ParseObjectType(const FString& TypeString, FEdGraphPinType& OutPinType, FString& OutErrorMessage);

	// Parse class types (TSubclassOf<T>)
	static bool ParseClassType(const FString& TypeString, FEdGraphPinType& OutPinType, FString& OutErrorMessage);

	// Parse enum types
	static bool ParseEnumType(const FString& TypeString, FEdGraphPinType& OutPinType, FString& OutErrorMessage);

	// Container handling
	static EPinContainerType GetContainerTypeEnum(const FString& ContainerString);

	// Type aliases
	static FString ResolveTypeAlias(const FString& TypeString);

	// Default error message storage
	static FString& GetDefaultErrorMessage()
	{
		static FString DefaultError;
		return DefaultError;
	}

	// Basic type mapping
	static const TMap<FString, FName>& GetBasicTypeMap();

	// Type aliases for user convenience
	static const TMap<FString, FString>& GetTypeAliases();
};
