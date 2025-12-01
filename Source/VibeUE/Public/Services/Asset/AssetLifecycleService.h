// Copyright Kevin Buckley 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Core/Result.h"

/**
 * @struct FAssetDuplicateResult
 * @brief Result data for asset duplication operations
 */
struct FAssetDuplicateResult
{
    FString OriginalPath;
    FString NewPath;
    FString AssetType;
};

/**
 * @struct FAssetReferenceInfo
 * @brief Information about an asset reference
 */
struct FAssetReferenceInfo
{
    /** Path to the referencing asset */
    FString AssetPath;
    
    /** Class type of the referencing asset */
    FString AssetClass;
    
    /** Display name of the asset */
    FString DisplayName;
};

/**
 * @struct FAssetReferencesResult
 * @brief Result data for asset reference queries
 */
struct FAssetReferencesResult
{
    /** The asset path that was queried */
    FString AssetPath;
    
    /** Assets that reference this asset (referencers) */
    TArray<FAssetReferenceInfo> Referencers;
    
    /** Assets that this asset references (dependencies) */
    TArray<FAssetReferenceInfo> Dependencies;
    
    /** Number of referencers */
    int32 ReferencerCount = 0;
    
    /** Number of dependencies */
    int32 DependencyCount = 0;
};

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
     * @brief Save all dirty (modified) assets
     * 
     * Saves all currently modified assets in the project.
     * This is equivalent to "Save All" in the Unreal Editor.
     * 
     * @param bPromptUserToSave If true, shows save dialog to user
     * @return TResult containing the number of assets saved
     */
    TResult<int32> SaveAllDirtyAssets(bool bPromptUserToSave = false);
    
    /**
     * @brief Delete an asset from the project
     * 
     * Removes the specified asset from the project with safety checks.
     * Validates asset existence, checks for references, and optionally
     * shows confirmation dialog before deletion.
     * 
     * @param AssetPath The full path to the asset to delete (e.g., "/Game/Textures/T_Texture")
     * @param bForceDelete If true, attempt deletion even if asset has references
     * @param bShowConfirmation If true, show confirmation dialog to user
     * @return TResult<bool> - Success with true if deleted, or error with appropriate code
     *         Error codes:
     *         - ASSET_NOT_FOUND: Asset doesn't exist at path
     *         - ASSET_IN_USE: Asset has active references (use force_delete=true to override)
     *         - ASSET_READ_ONLY: Asset is in engine content or read-only
     *         - OPERATION_CANCELLED: User cancelled confirmation dialog
     *         - ASSET_DELETE_FAILED: Deletion operation failed
     */
    TResult<bool> DeleteAsset(
        const FString& AssetPath,
        bool bForceDelete = false,
        bool bShowConfirmation = true
    );
    
    /**
     * @brief Duplicate an asset to a new location
     * 
     * Creates a copy of the specified asset at the destination path.
     * Handles name conflicts and validates paths.
     * 
     * @param SourceAssetPath The full path to the source asset (e.g., "/Game/Blueprints/BP_Player")
     * @param DestinationPath The target directory path (e.g., "/Game/Blueprints/Characters")
     * @param NewName Optional new name for the duplicated asset (if empty, auto-generates name)
     * @return TResult containing struct with new asset path and type information
     *         Error codes:
     *         - ASSET_NOT_FOUND: Source asset doesn't exist
     *         - INVALID_PATH: Destination path is invalid
     *         - OPERATION_FAILED: Duplication operation failed
     */
    TResult<FAssetDuplicateResult> DuplicateAsset(
        const FString& SourceAssetPath,
        const FString& DestinationPath,
        const FString& NewName = TEXT("")
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
    
    /**
     * @brief Get all references for an asset
     * 
     * Returns both referencers (assets that reference this asset) and
     * dependencies (assets that this asset references).
     * 
     * @param AssetPath The full path to the asset to query
     * @param bIncludeReferencers If true, include assets that reference this asset
     * @param bIncludeDependencies If true, include assets this asset depends on
     * @return TResult containing reference information
     *         Error codes:
     *         - ASSET_NOT_FOUND: Asset doesn't exist at path
     *         - INTERNAL_ERROR: Failed to access Asset Registry
     */
    TResult<FAssetReferencesResult> GetAssetReferences(
        const FString& AssetPath,
        bool bIncludeReferencers = true,
        bool bIncludeDependencies = true
    );

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
