// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Json.h"
#include "Core/TResult.h"
#include "Services/DataAsset/Types/DataAssetTypes.h"

// Forward declarations
class FServiceContext;
class UDataAsset;
class UClass;

/**
 * Service for data asset lifecycle management
 * 
 * Provides functionality to:
 * - Create new data assets of specified types
 * - Apply initial properties during creation
 */
class VIBEUE_API FDataAssetLifecycleService
{
public:
	/**
	 * Constructor
	 * @param InServiceContext Shared service context
	 */
	explicit FDataAssetLifecycleService(TSharedPtr<FServiceContext> InServiceContext);

	/**
	 * @brief Create a new data asset
	 * 
	 * Creates a new data asset of the specified class type at the given path.
	 * Optionally applies initial property values.
	 * 
	 * @param DataAssetClass The class to create an instance of
	 * @param AssetPath Directory path for the new asset (e.g., "/Game/Data")
	 * @param AssetName Name for the new asset
	 * @param InitialProperties Optional JSON object with initial property values
	 * @return TResult containing FDataAssetCreateResult or error
	 */
	TResult<FDataAssetCreateResult> CreateDataAsset(
		UClass* DataAssetClass,
		const FString& AssetPath,
		const FString& AssetName,
		const TSharedPtr<FJsonObject>& InitialProperties = nullptr);

private:
	/** Shared service context */
	TSharedPtr<FServiceContext> ServiceContext;
};
