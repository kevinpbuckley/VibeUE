// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UInputService.generated.h"

/**
 * Information about an Input Action
 */
USTRUCT(BlueprintType)
struct FInputActionDetailedInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	FString ActionName;

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	FString ActionPath;

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	FString ValueType;
};

/**
 * Information about a Mapping Context
 */
USTRUCT(BlueprintType)
struct FMappingContextDetailedInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	FString ContextName;

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	FString ContextPath;

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	TArray<FString> MappedActions;
};

/**
 * Input service exposed directly to Python (Enhanced Input System).
 *
 * Python Usage:
 *   import unreal
 *
 *   # List all Input Actions
 *   actions = unreal.InputService.list_input_actions()
 *
 *   # List all Mapping Contexts
 *   contexts = unreal.InputService.list_mapping_contexts()
 *
 * @note This replaces the JSON-based manage_input MCP tool
 */
UCLASS(BlueprintType)
class VIBEUE_API UInputService : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * List all Input Action assets.
	 *
	 * @return Array of Input Action paths
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Input")
	static TArray<FString> ListInputActions();

	/**
	 * List all Input Mapping Context assets.
	 *
	 * @return Array of Mapping Context paths
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Input")
	static TArray<FString> ListMappingContexts();

	/**
	 * Get Input Action information.
	 *
	 * @param ActionPath - Full path to the Input Action
	 * @param OutInfo - Structure containing action details
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Input")
	static bool GetInputActionInfo(const FString& ActionPath, FInputActionDetailedInfo& OutInfo);

	/**
	 * Get Mapping Context information including all mapped actions.
	 *
	 * @param ContextPath - Full path to the Mapping Context
	 * @param OutInfo - Structure containing context and mappings
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Input")
	static bool GetMappingContextInfo(const FString& ContextPath, FMappingContextDetailedInfo& OutInfo);
};
