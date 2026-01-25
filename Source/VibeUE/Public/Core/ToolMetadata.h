// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ToolMetadata.generated.h"

class UFunction;
class UClass;
class FProperty;

/**
 * Metadata for a single tool parameter
 */
USTRUCT()
struct VIBEUE_API FToolParameter
{
	GENERATED_BODY()

	/** Parameter name (as exposed to AI) */
	UPROPERTY()
	FString Name;

	/** Human-readable description */
	UPROPERTY()
	FString Description;

	/** Parameter type: "string", "int", "float", "bool", "object", "array" */
	UPROPERTY()
	FString Type;

	/** Whether this parameter is required */
	UPROPERTY()
	bool bRequired = false;

	/** Default value (as JSON string) */
	UPROPERTY()
	FString DefaultValue;

	/** Allowed values for enum-like parameters */
	UPROPERTY()
	TArray<FString> AllowedValues;

	/** Item type for array parameters (required for Google/Gemini compatibility) */
	UPROPERTY()
	FString ArrayItemType;

	FToolParameter()
		: bRequired(false)
	{
	}

	// Constructor for use with TOOL_PARAM macros
	FToolParameter(const FString& InName, const FString& InDesc, const FString& InType, bool InRequired, const FString& InDefault = TEXT(""))
		: Name(InName)
		, Description(InDesc)
		, Type(InType)
		, bRequired(InRequired)
		, DefaultValue(InDefault)
	{
		// Default array item type to string if not specified
		if (Type == TEXT("array"))
		{
			ArrayItemType = TEXT("string");
		}
	}

	// Constructor for array parameters with explicit item type
	FToolParameter(const FString& InName, const FString& InDesc, const FString& InType, bool InRequired, const FString& InDefault, const FString& InArrayItemType)
		: Name(InName)
		, Description(InDesc)
		, Type(InType)
		, bRequired(InRequired)
		, DefaultValue(InDefault)
		, ArrayItemType(InArrayItemType)
	{
	}
};

/**
 * Metadata for a single AI tool
 */
USTRUCT()
struct VIBEUE_API FToolMetadata
{
	GENERATED_BODY()

	/** Tool name (as exposed to AI) */
	UPROPERTY()
	FString Name;

	/** Human-readable description */
	UPROPERTY()
	FString Description;

	/** Tool category for organization */
	UPROPERTY()
	FString Category;

	/** Example usage strings */
	UPROPERTY()
	TArray<FString> Examples;

	/** Tool parameters */
	UPROPERTY()
	TArray<FToolParameter> Parameters;

	/** 
	 * If true, this tool is only available to VibeUE internal chat.
	 * It will NOT be exposed via MCP to external clients (e.g., VS Code Copilot).
	 * Use for tools that need direct access to chat session state (e.g., attach_image).
	 */
	UPROPERTY()
	bool bInternalOnly = false;

	/** Reflection function pointer (not serialized) */
	UFunction* Function = nullptr;

	/** Class containing the tool (not serialized) */
	UClass* ToolClass = nullptr;

	FToolMetadata()
		: bInternalOnly(false)
		, Function(nullptr)
		, ToolClass(nullptr)
	{
	}
};

