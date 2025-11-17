// Copyright Kevin Buckley 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Core/Result.h"
#include "AssetRegistry/AssetData.h"

/**
 * @class FAssetDiscoveryService
 * @brief Service responsible for asset discovery and search operations
 * 
 * This service provides asset search and discovery functionality including:
 * - Searching assets by name or type
 * - Finding assets by specific criteria
 * - Getting assets filtered by type
 * 
 * Extracted from AssetCommands.cpp as part of Phase 4 refactoring (Task 19)
 * to create focused services that operate with TResult instead of JSON for type safety.
 * 
 * All methods return TResult<T> for type-safe error handling.
 * 
 * @note This is part of Phase 4 refactoring (Task 19) to extract Asset domain into
 * focused services as per CPP_REFACTORING_DESIGN.md
 * 
 * @see TResult
 * @see FServiceBase
 * @see Issue #19
 */
class VIBEUE_API FAssetDiscoveryService : public FServiceBase
{
public:
    /**
     * @brief Constructor
     * @param Context Service context for shared state
     */
    explicit FAssetDiscoveryService(TSharedPtr<FServiceContext> Context);

    // ========================================================================
    // Search Operations
    // ========================================================================
    
    /**
     * @brief Search for assets by name and/or type
     * 
     * Searches the asset registry for assets matching the given search term and type.
     * 
     * @param SearchTerm The search term to match against asset names (case-insensitive)
     * @param AssetType Optional asset type filter (e.g., "Texture2D", "Blueprint", "WidgetBlueprint")
     * @return TResult containing array of matching FAssetData
     */
    TResult<TArray<FAssetData>> SearchAssets(const FString& SearchTerm, const FString& AssetType = TEXT(""));
    
    /**
     * @brief Get all assets of a specific type
     * 
     * Retrieves all assets in the project of the specified type.
     * 
     * @param AssetType The asset class name to filter by (e.g., "Texture2D", "Blueprint")
     * @return TResult containing array of FAssetData for the specified type
     */
    TResult<TArray<FAssetData>> GetAssetsByType(const FString& AssetType);
    
    /**
     * @brief Find a specific asset by exact name
     * 
     * Searches for an asset with an exact name match.
     * 
     * @param AssetName The exact name of the asset to find
     * @param AssetType Optional type filter to narrow search
     * @return TResult containing the FAssetData if found
     */
    TResult<FAssetData> FindAssetByName(const FString& AssetName, const FString& AssetType = TEXT(""));
    
    /**
     * @brief Find asset by path
     * 
     * Retrieves asset data for an asset at a specific path.
     * 
     * @param AssetPath The full asset path (e.g., "/Game/Textures/T_MyTexture")
     * @return TResult containing the FAssetData if found
     */
    TResult<FAssetData> FindAssetByPath(const FString& AssetPath);

protected:
    virtual FString GetServiceName() const override { return TEXT("AssetDiscoveryService"); }

private:
    /**
     * @brief Get the asset registry module
     * @return Pointer to the asset registry interface
     */
    class IAssetRegistry* GetAssetRegistry() const;
    
    /**
     * @brief Convert asset class name to FTopLevelAssetPath
     * @param ClassName The class name (e.g., "Texture2D", "Blueprint")
     * @return The FTopLevelAssetPath for the class
     */
    FTopLevelAssetPath GetAssetClassPath(const FString& ClassName) const;
};
