#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Core/Result.h"

/**
 * @class FAssetLifecycleService
 * @brief Service responsible for asset lifecycle operations
 * 
 * This service provides asset lifecycle management functionality including:
 * - Opening assets in their appropriate editor
 * - Saving assets
 * - Deleting assets
 * - Validating asset state
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
class VIBEUE_API FAssetLifecycleService : public FServiceBase
{
public:
    /**
     * @brief Constructor
     * @param Context Service context for shared state
     */
    explicit FAssetLifecycleService(TSharedPtr<FServiceContext> Context);

    // ========================================================================
    // Editor Operations
    // ========================================================================
    
    /**
     * @brief Open an asset in its appropriate editor
     * 
     * Opens the specified asset in the Unreal Engine editor appropriate for its type.
     * Handles different asset types (Textures, Materials, Blueprints, etc.)
     * 
     * @param AssetPath The full path to the asset to open
     * @param bForceOpen If true, closes and reopens if already open
     * @return TResult containing success information with editor type
     */
    TResult<FString> OpenAssetInEditor(const FString& AssetPath, bool bForceOpen = false);
    
    /**
     * @brief Check if an asset is currently open in an editor
     * 
     * Verifies whether the specified asset has an active editor instance.
     * 
     * @param AssetPath The full path to the asset
     * @return TResult containing true if asset is open, false otherwise
     */
    TResult<bool> IsAssetOpen(const FString& AssetPath);
    
    /**
     * @brief Close an asset editor
     * 
     * Closes the editor instance for the specified asset.
     * 
     * @param AssetPath The full path to the asset
     * @return Success or error result
     */
    TResult<void> CloseAsset(const FString& AssetPath);
    
    // ========================================================================
    // Asset Management
    // ========================================================================
    
    /**
     * @brief Save an asset to disk
     * 
     * Saves the specified asset and marks the package as saved.
     * 
     * @param AssetPath The full path to the asset to save
     * @return Success or error result
     */
    TResult<void> SaveAsset(const FString& AssetPath);
    
    /**
     * @brief Delete an asset from the project
     * 
     * Removes the specified asset from the project with enhanced safety checks:
     * - Validates asset exists
     * - Checks for references (can be overridden with bForceDelete)
     * - Checks if asset is read-only or in engine content
     * - Optional user confirmation dialog
     * 
     * @param AssetPath Full package path to the asset (e.g., "/Game/Textures/T_Texture")
     * @param bForceDelete If true, attempt deletion even if asset has references
     * @param bShowConfirmation If true, show confirmation dialog to user
     * @return TResult<bool> - Success with true if deleted, or error with appropriate code
     */
    TResult<bool> DeleteAsset(
        const FString& AssetPath,
        bool bForceDelete = false,
        bool bShowConfirmation = true
    );
    
    /**
     * @brief Check if an asset exists
     * 
     * Verifies whether an asset exists at the specified path.
     * 
     * @param AssetPath The full path to check
     * @return TResult containing true if asset exists, false otherwise
     */
    TResult<bool> DoesAssetExist(const FString& AssetPath);

protected:
    virtual FString GetServiceName() const override { return TEXT("AssetLifecycleService"); }

private:
    /**
     * @brief Load an asset from path
     * @param AssetPath The asset path
     * @return Loaded UObject or nullptr
     */
    UObject* LoadAsset(const FString& AssetPath) const;
    
    /**
     * @brief Normalize asset path (add /Game prefix if needed)
     * @param AssetPath The asset path to normalize
     * @return Normalized path
     */
    FString NormalizeAssetPath(const FString& AssetPath) const;
    
    /**
     * @brief Get editor type name for an asset
     * @param Asset The asset object
     * @return Human-readable editor type name
     */
    FString GetEditorTypeName(UObject* Asset) const;
};
