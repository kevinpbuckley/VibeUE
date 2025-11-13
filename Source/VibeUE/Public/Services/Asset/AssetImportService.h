// Copyright VibeUE 2025

#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Core/Result.h"

// Forward declarations
class UTexture2D;

/**
 * @struct FTextureImportResult
 * @brief Result data for texture import operations
 */
struct FTextureImportResult
{
    FString AssetPath;
    FString DestinationPath;
    FString SourceFile;
    FString AssetClass;
};

/**
 * @struct FTextureExportResult
 * @brief Result data for texture export operations
 */
struct FTextureExportResult
{
    FString AssetPath;
    FString TempFilePath;
    FString ExportFormat;
    int32 ExportedWidth;
    int32 ExportedHeight;
    int64 FileSize;
};

/**
 * @class FAssetImportService
 * @brief Service responsible for asset import and export operations
 * 
 * This service provides asset import/export functionality including:
 * - Importing textures from external files
 * - Exporting textures for analysis
 * - Format conversion and validation
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
class VIBEUE_API FAssetImportService : public FServiceBase
{
public:
    /**
     * @brief Constructor
     * @param Context Service context for shared state
     */
    explicit FAssetImportService(TSharedPtr<FServiceContext> Context);

    // ========================================================================
    // Import Operations
    // ========================================================================
    
    /**
     * @brief Import a texture from a file
     * 
     * Imports a raster image file (PNG, JPG, TGA, etc.) as a Texture2D asset.
     * Supports automatic path creation and replacement of existing assets.
     * 
     * @param SourceFile Path to the source image file
     * @param DestinationPath Target package path in Unreal (e.g., "/Game/Textures/Imported")
     * @param TextureName Optional name for the texture (auto-generated if empty)
     * @param bReplaceExisting Whether to replace existing asset with same name
     * @param bSave Whether to save the asset after creation
     * @return TResult containing import result information
     */
    TResult<FTextureImportResult> ImportTexture(
        const FString& SourceFile,
        const FString& DestinationPath,
        const FString& TextureName = TEXT(""),
        bool bReplaceExisting = true,
        bool bSave = true
    );
    
    // ========================================================================
    // Export Operations
    // ========================================================================
    
    /**
     * @brief Export a texture for external analysis
     * 
     * Exports a Texture2D asset to a temporary file for AI or external analysis.
     * Supports format conversion and size constraints.
     * 
     * @param AssetPath The full path to the texture asset
     * @param ExportFormat The format to export to ("PNG" or "TGA")
     * @param TempFolder Optional temp folder path (uses default if empty)
     * @param MaxWidth Optional maximum width constraint (0 = no limit)
     * @param MaxHeight Optional maximum height constraint (0 = no limit)
     * @return TResult containing export result information
     */
    TResult<FTextureExportResult> ExportTextureForAnalysis(
        const FString& AssetPath,
        const FString& ExportFormat = TEXT("PNG"),
        const FString& TempFolder = TEXT(""),
        int32 MaxWidth = 0,
        int32 MaxHeight = 0
    );

protected:
    virtual FString GetServiceName() const override { return TEXT("AssetImportService"); }

private:
    /**
     * @brief Validate source file exists and is readable
     * @param SourceFile Path to validate
     * @return Success or error result
     */
    TResult<void> ValidateSourceFile(const FString& SourceFile) const;
    
    /**
     * @brief Ensure destination directory exists
     * @param DestinationPath Path to validate/create
     * @return Success or error result
     */
    TResult<void> EnsureDestinationPath(const FString& DestinationPath) const;
    
    /**
     * @brief Load and decode image file
     * @param SourceFile File to load
     * @param OutWidth Decoded image width
     * @param OutHeight Decoded image height
     * @param OutPixels Decoded pixel data
     * @return Success or error result
     */
    TResult<void> LoadImageFile(
        const FString& SourceFile,
        int32& OutWidth,
        int32& OutHeight,
        TArray<FColor>& OutPixels
    ) const;
    
    /**
     * @brief Create texture asset from pixel data
     * @param Width Image width
     * @param Height Image height
     * @param Pixels Pixel data
     * @param PackagePath Package path for asset
     * @param TextureName Name for texture
     * @return Created UTexture2D or nullptr
     */
    UTexture2D* CreateTextureAsset(
        int32 Width,
        int32 Height,
        const TArray<FColor>& Pixels,
        const FString& PackagePath,
        const FString& TextureName
    ) const;
    
    /**
     * @brief Read texture data from a Texture2D asset
     * @param Texture The texture to read
     * @param OutPixels Output pixel data
     * @return Success or error result
     */
    TResult<void> ReadTextureData(UTexture2D* Texture, TArray<FColor>& OutPixels) const;
    
    /**
     * @brief Resize image data
     * @param InPixels Source pixels
     * @param InWidth Source width
     * @param InHeight Source height
     * @param OutWidth Target width
     * @param OutHeight Target height
     * @param OutPixels Resized pixel data
     */
    void ResizeImage(
        const TArray<FColor>& InPixels,
        int32 InWidth,
        int32 InHeight,
        int32 OutWidth,
        int32 OutHeight,
        TArray<FColor>& OutPixels
    ) const;
    
    /**
     * @brief Save pixel data to file
     * @param Pixels Pixel data to save
     * @param Width Image width
     * @param Height Image height
     * @param FilePath Output file path
     * @param Format Output format ("PNG" or "TGA")
     * @return Success or error result
     */
    TResult<void> SaveImageToFile(
        const TArray<FColor>& Pixels,
        int32 Width,
        int32 Height,
        const FString& FilePath,
        const FString& Format
    ) const;
};
