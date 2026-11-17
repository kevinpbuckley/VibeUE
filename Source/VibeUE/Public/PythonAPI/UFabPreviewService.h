// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AsyncFunctionResult/AsyncFunctionResult.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "ToolsetRegistry/ToolsetImage.h"
#include "UFabPreviewService.generated.h"

/** One numbered cell of a preview sheet. The number drawn in the cell's top-left corner is Index. */
USTRUCT(BlueprintType)
struct FVibeUEPreviewCell
{
	GENERATED_BODY()

	/** The number drawn on the cell (1-based, in request / sort order). */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Fab")
	int32 Index = 0;

	/** The caller's label (ListingImageSheet) or the asset name (PackSheet). */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Fab")
	FString Label;

	/** The image URL (ListingImageSheet) or the asset's object path (PackSheet). */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Fab")
	FString Source;

	/** PackSheet: the asset's class name (StaticMesh, SkeletalMesh, ...). Empty for listing images. */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Fab")
	FString AssetClass;

	/** Where the picture came from: "fab" (downloaded), "cached-thumbnail" (saved in the package),
	 *  "rendered" (rendered now), or "none" (see Error). */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Fab")
	FString ImageOrigin;

	/** Size of the source picture before it was fitted into the cell. */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Fab")
	int32 SourceWidth = 0;

	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Fab")
	int32 SourceHeight = 0;

	/** Downloaded bytes (listing images) or 0. */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Fab")
	int64 SourceBytes = 0;

	/** True when the source picture is too small to judge the asset by (under 256 px on its long side).
	 *  Fab serves a blurred ~143x78 placeholder as the thumbnail of a listing that is no longer live. */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Fab")
	bool bLowDetail = false;

	/** Why the cell is empty, when it is. */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Fab")
	FString Error;
};

/** Asset count of one class under a PackSheet folder. */
USTRUCT(BlueprintType)
struct FVibeUEPreviewClassCount
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Fab")
	FString AssetClass;

	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Fab")
	int32 Count = 0;
};

/** A contact sheet: one image of numbered cells plus what each cell shows. */
USTRUCT(BlueprintType)
struct FVibeUEPreviewSheet
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Fab")
	bool bSuccess = false;

	/** One line on what the sheet shows, and any caveat (low-detail pictures, truncation). */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Fab")
	FString Summary;

	/** The sheet itself (JPEG). Over MCP it arrives as a real image block. */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Fab")
	FToolsetImage Sheet;

	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Fab")
	TArray<FVibeUEPreviewCell> Cells;

	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Fab")
	int32 Columns = 0;

	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Fab")
	int32 CellSize = 0;

	/** Absolute path of the copy written to disk, when SaveImagePath was given. */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Fab")
	FString SavedPath;

	/** PackSheet: every asset under the folder, by class (not only the shown classes). */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Fab")
	TArray<FVibeUEPreviewClassCount> ClassCounts;

	/** PackSheet: assets under the folder, and how many matched the class filter (cells may be fewer: MaxAssets). */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Fab")
	int32 TotalAssets = 0;

	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Fab")
	int32 MatchedAssets = 0;

	/** PackSheet: bytes of every file under the folder on disk. */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Fab")
	int64 BytesOnDisk = 0;
};

/** What RefreshThumbnails did. */
USTRUCT(BlueprintType)
struct FVibeUEThumbnailRefresh
{
	GENERATED_BODY()

	/** True when every asset that needed a new thumbnail got one and the save (if asked) succeeded. */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Fab")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Fab")
	FString Summary;

	/** Assets of the requested classes under the folder. */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Fab")
	int32 Checked = 0;

	/** Object paths whose thumbnail was re-rendered and replaced. */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Fab")
	TArray<FString> Refreshed;

	/** "<object path>: reason" for each asset that could not get a picture. */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Fab")
	TArray<FString> Failed;

	/** Packages written to disk (empty when bSave is false). */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Fab")
	TArray<FString> SavedPackages;
};

/** Async result of ListingImageSheet (the pictures are downloaded first). */
UCLASS(BlueprintType)
class VIBEUE_API UVibeUEPreviewSheetResult : public UAsyncFunctionResult
{
	GENERATED_BODY()

public:
	bool SetValue(const FVibeUEPreviewSheet& InValue)
	{
		return MaybeBroadcastSuccessfulCompletion(FVibeUEPreviewSheet(InValue), Value);
	}

	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Fab")
	FVibeUEPreviewSheet Value;
};

/**
 * See what Fab content looks like, as real images: the companion of the engine's FabToolset
 * (SearchFabMyLibrary / SearchFabListings / InstallFabListing), which reports a listing's picture only as a
 * thumbnailUrl and says nothing about what an installed pack holds.
 *
 * - ListingImageSheet: download the thumbnailUrl (or any https image URL) of up to 48 listings and tile them into
 *   one numbered contact sheet, so a whole search page is judged in one look. Each cell reports the picture's real
 *   size; bLowDetail marks pictures too small to judge by. Fab serves a blurred ~143x78 placeholder for a listing
 *   that is no longer live (an owned entitlement still installs): install it and use PackSheet instead.
 * - PackSheet: after an install, tile the assets under a content folder (default: static and skeletal meshes) into
 *   one numbered sheet from their saved thumbnails, rendering the ones that have none, plus the folder's asset
 *   count by class and its size on disk. A saved thumbnail that holds no picture (blank, or the near-uniform
 *   empty-scene checker the editor saves when it renders one while the asset is still building) is not trusted:
 *   it is rendered again when bRenderMissing is true. For one asset at full size use EditorToolset.EditorAppToolset
 *   CaptureAssetImage.
 * - RefreshThumbnails: re-render and save the blank saved thumbnails under a folder, so they stay fixed.
 *
 * Over MCP the sheet arrives as an image block and the rest as JSON. In Python the struct comes back with
 * Sheet.data (base64 JPEG); pass save_image_path to also write it to disk (.png or .jpg by extension).
 *
 * Python:
 *   s = unreal.FabPreviewService.pack_sheet("/Game/VigilanteContent/Vehicles/East_Radar_S400")
 *   r = await call_tool("VibeUE.FabPreviewService.ListingImageSheet", imageUrls=[...], labels=[...])
 */
UCLASS(BlueprintType)
class VIBEUE_API UFabPreviewService : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Download listing pictures and tile them into one numbered contact sheet.
	 *
	 * @param ImageUrls - https image URLs, e.g. the thumbnailUrl of SearchFabMyLibrary / SearchFabListings results (1-48)
	 * @param Labels - optional label per URL (the listing title), echoed in Cells
	 * @param CellSize - cell edge in pixels (64-512, default 256)
	 * @param Columns - columns of the grid; 0 picks a near-square grid no wider than 2048 px
	 * @param SaveImagePath - optional absolute file path to also write the sheet to (.png or .jpg)
	 * @return the sheet, with one cell per URL (ImageOrigin "fab", SourceWidth/Height, bLowDetail, Error)
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Fab")
	static UVibeUEPreviewSheetResult* ListingImageSheet(
		const TArray<FString>& ImageUrls,
		const TArray<FString>& Labels,
		int32 CellSize = 256,
		int32 Columns = 0,
		const FString& SaveImagePath = TEXT(""));

	/**
	 * Tile the assets under a content folder (recursive) into one numbered contact sheet.
	 *
	 * @param ContentPath - content folder, e.g. /Game/VigilanteContent/Vehicles/East_Radar_S400
	 * @param ClassNames - comma-separated asset classes to show (default "StaticMesh,SkeletalMesh"; "*" = all)
	 * @param MaxAssets - most cells (1-64, default 30), in path order; Summary says when more matched
	 * @param CellSize - cell edge in pixels (64-512, default 256)
	 * @param Columns - columns; 0 picks a near-square grid no wider than 2048 px
	 * @param bRenderMissing - render assets that have no saved thumbnail (loads them; default true)
	 * @param SaveImagePath - optional absolute file path to also write the sheet to (.png or .jpg)
	 * @return the sheet, Cells (asset path, class, ImageOrigin), ClassCounts, TotalAssets, MatchedAssets, BytesOnDisk
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Fab")
	static FVibeUEPreviewSheet PackSheet(
		const FString& ContentPath,
		const FString& ClassNames = TEXT("StaticMesh,SkeletalMesh"),
		int32 MaxAssets = 30,
		int32 CellSize = 256,
		int32 Columns = 0,
		bool bRenderMissing = true,
		const FString& SaveImagePath = TEXT(""));

	/**
	 * Re-render the saved thumbnails of the assets under a content folder (recursive) and save them into their
	 * packages, so the Content Browser and PackSheet show the asset. The editor renders a thumbnail at save time
	 * without waiting for the asset to finish building, so a mesh saved right after a rebuild (a move, a collision
	 * or Nanite change) can keep a blank empty-scene thumbnail; this finishes the compiles first.
	 *
	 * @param ContentPath - content folder, e.g. /Game/MyPack (at most 256 matching assets per call)
	 * @param ClassNames - comma-separated asset classes (default "StaticMesh,SkeletalMesh"; "*" = all)
	 * @param bOnlyBlank - replace only thumbnails that are missing or hold no picture (default true)
	 * @param bSave - save the changed packages (default true; false leaves them dirty)
	 * @return Refreshed / Failed object paths, SavedPackages, Summary
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Fab")
	static FVibeUEThumbnailRefresh RefreshThumbnails(
		const FString& ContentPath,
		const FString& ClassNames = TEXT("StaticMesh,SkeletalMesh"),
		bool bOnlyBlank = true,
		bool bSave = true);

	/**
	 * The blank-thumbnail test PackSheet and RefreshThumbnails use, on an image file (PNG, JPEG, BMP, ...).
	 * For tests and diagnosis; not an AI tool.
	 * @return 1 = holds no picture (no opaque pixel, or near-uniform), 0 = holds a picture, -1 = not readable
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Fab")
	static int32 ClassifyImageFile(const FString& FilePath);
};
