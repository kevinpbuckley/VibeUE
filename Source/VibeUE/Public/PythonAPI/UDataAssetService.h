// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UDataAssetService.generated.h"

/**
 * DataAsset service exposed directly to Python.
 *
 * Python Usage:
 *   import unreal
 *
 *   # Search for DataAsset types
 *   types = unreal.DataAssetService.search_types("Item")
 *
 *   # List DataAssets of a specific type
 *   assets = unreal.DataAssetService.list_data_assets("ItemDataAsset")
 *
 * @note This replaces the JSON-based manage_dataasset MCP tool
 */
UCLASS(BlueprintType)
class VIBEUE_API UDataAssetService : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Search for DataAsset subclasses.
	 *
	 * @param SearchFilter - Optional search term for class names
	 * @return Array of DataAsset class names
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|DataAssets")
	static TArray<FString> SearchTypes(const FString& SearchFilter = TEXT(""));

	/**
	 * List DataAsset instances.
	 *
	 * @param ClassName - Optional filter by specific DataAsset subclass
	 * @return Array of DataAsset paths
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|DataAssets")
	static TArray<FString> ListDataAssets(const FString& ClassName = TEXT(""));

	/**
	 * Get all property values from a DataAsset as JSON.
	 *
	 * @param AssetPath - Full path to the DataAsset
	 * @return JSON string representation of the DataAsset properties
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|DataAssets")
	static FString GetPropertiesAsJson(const FString& AssetPath);
};
