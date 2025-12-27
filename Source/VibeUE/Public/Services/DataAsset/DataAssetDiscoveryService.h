// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/TResult.h"
#include "Services/DataAsset/Types/DataAssetTypes.h"

// Forward declarations
class FServiceContext;
class UDataAsset;
class UClass;

/**
 * Service for discovering data asset types and instances
 * 
 * Provides functionality to:
 * - Search for available data asset class types
 * - List data asset instances in the project
 * - Find and load data assets by path or name
 * - Find data asset classes by name or path
 */
class VIBEUE_API FDataAssetDiscoveryService
{
public:
	/**
	 * Constructor
	 * @param InServiceContext Shared service context
	 */
	explicit FDataAssetDiscoveryService(TSharedPtr<FServiceContext> InServiceContext);

	// ========== Type Discovery ==========

	/**
	 * @brief Search for available data asset class types
	 * 
	 * Iterates all UClass objects to find non-abstract subclasses of UDataAsset.
	 * 
	 * @param SearchFilter Optional filter string to match class names
	 * @return TResult containing array of FDataAssetTypeInfo or error
	 */
	TResult<TArray<FDataAssetTypeInfo>> SearchTypes(const FString& SearchFilter = TEXT(""));

	// ========== Asset Discovery ==========

	/**
	 * @brief List data asset instances in the project
	 * 
	 * @param AssetType Optional class type to filter by
	 * @param SearchPath Path prefix filter (default: /Game)
	 * @return TResult containing array of FDataAssetInfo or error
	 */
	TResult<TArray<FDataAssetInfo>> ListAssets(
		const FString& AssetType = TEXT(""),
		const FString& SearchPath = TEXT("/Game"));

	// ========== Asset Loading ==========

	/**
	 * @brief Load a data asset by path
	 * 
	 * Attempts multiple strategies:
	 * 1. Direct load by full path
	 * 2. Load with .AssetName suffix
	 * 3. Search asset registry by name
	 * 
	 * @param AssetPath Full path like "/Game/Data/MyAsset" or just "MyAsset"
	 * @return TResult containing UDataAsset pointer or error
	 */
	TResult<UDataAsset*> LoadDataAsset(const FString& AssetPath);

	/**
	 * @brief Find a data asset class by name or path
	 * 
	 * @param ClassNameOrPath Class name like "MyDataAsset" or path like "/Script/MyGame.MyDataAsset"
	 * @return TResult containing UClass pointer or error
	 */
	TResult<UClass*> FindDataAssetClass(const FString& ClassNameOrPath);

private:
	/** Shared service context */
	TSharedPtr<FServiceContext> ServiceContext;
};
