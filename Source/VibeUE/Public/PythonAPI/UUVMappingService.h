// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UUVMappingService.generated.h"

/**
 * Result of a UV mapping mutation operation.
 */
USTRUCT(BlueprintType)
struct FUVMappingResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	FString MeshPath;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	FString Message;
};

/**
 * Information about a single UV channel on a mesh LOD.
 *
 * Diagnostic stats (overlaps, in-unit-square percentages, texel density) are computed
 * on demand from the mesh description. Texel density is in texels per UE unit at the
 * given reference texture size, averaged across triangles in the channel.
 */
USTRUCT(BlueprintType)
struct FUVChannelInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	int32 LODIndex = 0;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	int32 ChannelIndex = 0;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	int32 VertexInstanceCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	int32 TriangleCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	float MinU = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	float MinV = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	float MaxU = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	float MaxV = 0.0f;

	/** Percentage of triangles whose UV bounding box overlaps another triangle's. 0-100. */
	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	float OverlapPercent = 0.0f;

	/** Percentage of vertex UVs lying inside the [0,1] x [0,1] square. 0-100. */
	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	float InUnitSquarePercent = 0.0f;

	/** Average texels per UE unit at 1024x1024 reference texture size. */
	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	float TexelDensity1k = 0.0f;
};

/**
 * Aggregate UV health report across all LODs and channels.
 *
 * Use this to quickly judge whether a mesh is shippable: any HasOverlaps=true on the
 * lightmap channel is a hard failure for baked lighting; OutOfUnitSquarePercent
 * usually implies tiling textures or unintended overlaps.
 */
USTRUCT(BlueprintType)
struct FUVHealthReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	FString MeshPath;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	int32 LODCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	int32 LightmapCoordinateIndex = 0;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	int32 LightMapResolution = 0;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	bool bGenerateLightmapUVs = false;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	TArray<FUVChannelInfo> Channels;

	/** True if the lightmap channel has any overlapping triangle UVs. */
	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	bool bLightmapHasOverlaps = false;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	TArray<FString> Warnings;
};

/**
 * Description of a single UV island — a connected group of vertex instances that
 * are stitched together in UV space (no seam between them).
 *
 * Returned by IdentifyUVIslands. Pass IslandId to TransformUVIsland to operate on
 * just this island's UVs.
 */
USTRUCT(BlueprintType)
struct FUVIslandInfo
{
	GENERATED_BODY()

	/** Stable id for this island within the (mesh, lod, channel) tuple. Use with TransformUVIsland. */
	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	int32 IslandId = 0;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	int32 VertexInstanceCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	int32 TriangleCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	float MinU = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	float MinV = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	float MaxU = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	float MaxV = 0.0f;

	/** Total UV-space area covered by this island's triangles. */
	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	float UVArea = 0.0f;

	/** Average world-space position of the island's vertices (handy for filtering by region in 3D). */
	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	FVector WorldCenter = FVector::ZeroVector;

	/** Average world-space normal (lets you tell cap-vs-side islands at a glance). */
	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	FVector AverageNormal = FVector::ZeroVector;
};

/**
 * Lightmap-related build settings on a static mesh source model (LOD 0).
 */
USTRUCT(BlueprintType)
struct FUVLightmapSettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	bool bGenerateLightmapUVs = false;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	int32 SourceLightmapIndex = 0;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	int32 DestinationLightmapIndex = 1;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	int32 LightmapCoordinateIndex = 1;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	int32 LightMapResolution = 64;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|UVMapping")
	int32 MinLightmapResolution = 64;
};

/**
 * UV mapping service exposed directly to Python.
 *
 * Provides automation-grade UV channel manipulation for StaticMesh and SkeletalMesh
 * assets. All mesh-mutating actions automatically mark the package dirty and rebuild
 * render data; callers should follow with a save (manage_asset save) once a batch of
 * edits is complete.
 *
 * Action groups:
 *
 * Inspect:
 * - list_uv_channels           : Per-LOD channel inventory and stats
 * - get_uv_channel_info        : Stats for one (LOD, channel) pair
 * - get_uv_health              : Aggregate health report across all LODs
 *
 * Channel lifecycle:
 * - add_uv_channel             : Append a new empty UV channel to a LOD
 * - remove_uv_channel          : Drop a channel and shift remaining channels down
 * - copy_uv_channel            : Duplicate one channel's UVs into another
 * - set_uv_channel_count       : Resize the channel array exactly
 *
 * Generation:
 * - generate_lightmap_uvs      : Build a packed lightmap UV channel from a source channel
 * - auto_unwrap_uvs            : Box / planar / cylindrical projection unwrap into a channel
 * - pack_uvs                   : Repack islands in an existing channel with padding
 *
 * Transform:
 * - transform_uvs              : Scale, rotate, translate UVs in a channel
 * - flip_uvs                   : Mirror U and/or V in a channel
 *
 * Lightmap settings:
 * - get_lightmap_settings      : Read source/dest indices, resolution, generate flag
 * - set_lightmap_settings      : Configure all lightmap-related fields in one call
 *
 * Visualize:
 * - export_uv_layout_image     : Render a UV layout PNG for AI / human inspection
 *
 * Existence:
 * - mesh_has_uv_channel        : Quick check before reading or writing
 *
 * Python usage:
 *   import unreal
 *
 *   # Quick health check before edits
 *   ok, health = unreal.UVMappingService.get_uv_health("/Game/Meshes/SM_Wall")
 *
 *   # Generate lightmap UVs into channel 1 from channel 0
 *   result = unreal.UVMappingService.generate_lightmap_uvs(
 *       "/Game/Meshes/SM_Wall", 0, 1, 1.0)
 *
 *   # Configure lightmap settings (resolution + indices) in one call
 *   unreal.UVMappingService.set_lightmap_settings(
 *       "/Game/Meshes/SM_Wall", 1, 0, 128, True)
 *
 *   # Auto-unwrap a custom UV channel using box projection
 *   unreal.UVMappingService.auto_unwrap_uvs(
 *       "/Game/Meshes/SM_Wall", 0, 2, "Box", 66.0)
 *
 *   # Export the lightmap channel for visual review
 *   unreal.UVMappingService.export_uv_layout_image(
 *       "/Game/Meshes/SM_Wall", 0, 1, "D:/uv_wall_lm.png", 1024)
 */
UCLASS(BlueprintType)
class VIBEUE_API UUVMappingService : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	// =================================================================
	// Inspect
	// =================================================================

	/**
	 * List every UV channel on every LOD with diagnostic stats.
	 * Maps to action="list_uv_channels".
	 *
	 * @param MeshPath - Full path to a StaticMesh or SkeletalMesh asset.
	 * @return Array of channel info, ordered by LOD then channel.
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|UVMapping", meta = (AICallable, DisplayName = "List UV Channels"))
	static TArray<FUVChannelInfo> ListUVChannels(const FString& MeshPath);

	/**
	 * Get diagnostic stats for a single (LOD, channel) pair.
	 * Maps to action="get_uv_channel_info".
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|UVMapping", meta = (AICallable, DisplayName = "Get UV Channel Info"))
	static bool GetUVChannelInfo(
		const FString& MeshPath,
		int32 LODIndex,
		int32 ChannelIndex,
		FUVChannelInfo& OutInfo);

	/**
	 * Build an aggregate UV health report. Single call to judge ship-readiness.
	 * Maps to action="get_uv_health".
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|UVMapping", meta = (AICallable, DisplayName = "Get UV Health"))
	static bool GetUVHealth(const FString& MeshPath, FUVHealthReport& OutReport);

	// =================================================================
	// Channel lifecycle
	// =================================================================

	/**
	 * Append a new (zeroed) UV channel to a LOD.
	 * Maps to action="add_uv_channel".
	 *
	 * @param MeshPath - StaticMesh asset path. SkeletalMesh not supported for add/remove (use set count).
	 * @param LODIndex - LOD to modify.
	 * @return Result with the new channel index in Message.
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|UVMapping", meta = (AICallable, DisplayName = "Add UV Channel"))
	static FUVMappingResult AddUVChannel(const FString& MeshPath, int32 LODIndex = 0);

	/**
	 * Remove a UV channel; channels above it shift down by one.
	 * Maps to action="remove_uv_channel".
	 *
	 * @note Channel 0 cannot be removed. If the lightmap coordinate index points at or
	 *       above the removed channel, it is adjusted automatically.
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|UVMapping", meta = (AICallable, DisplayName = "Remove UV Channel"))
	static FUVMappingResult RemoveUVChannel(
		const FString& MeshPath,
		int32 LODIndex,
		int32 ChannelIndex);

	/**
	 * Copy UVs from one channel into another (creating the destination if needed).
	 * Maps to action="copy_uv_channel".
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|UVMapping", meta = (AICallable, DisplayName = "Copy UV Channel"))
	static FUVMappingResult CopyUVChannel(
		const FString& MeshPath,
		int32 LODIndex,
		int32 SourceChannel,
		int32 DestChannel);

	/**
	 * Resize the UV channel array on a LOD to exactly Count channels.
	 * Maps to action="set_uv_channel_count".
	 *
	 * @param Count - 1..MAX_MESH_TEXTURE_COORDS_MD (typically 8). Must be >= 1.
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|UVMapping", meta = (AICallable, DisplayName = "Set UV Channel Count"))
	static FUVMappingResult SetUVChannelCount(
		const FString& MeshPath,
		int32 LODIndex,
		int32 Count);

	// =================================================================
	// Generation
	// =================================================================

	/**
	 * Generate a packed lightmap UV channel from an existing source channel.
	 * Maps to action="generate_lightmap_uvs".
	 *
	 * Uses FStaticMeshOperations::GenerateUniqueUVsForStaticMesh to produce a non-overlapping
	 * unwrap of every triangle, packed into [0,1]^2 with the requested chart spacing.
	 *
	 * @param MeshPath - StaticMesh path (skeletal meshes not supported).
	 * @param SourceUVIndex - Channel to read seam information from (typically 0).
	 * @param DestUVIndex - Channel to write the generated lightmap into (typically 1).
	 * @param MinChartSpacingPercent - Padding between charts as a percent of UV space (0.5..2.0 typical).
	 *
	 * @note Also sets LightMapCoordinateIndex = DestUVIndex on the static mesh.
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|UVMapping", meta = (AICallable, DisplayName = "Generate Lightmap UVs"))
	static FUVMappingResult GenerateLightmapUVs(
		const FString& MeshPath,
		int32 SourceUVIndex = 0,
		int32 DestUVIndex = 1,
		float MinChartSpacingPercent = 1.0f);

	/**
	 * Auto-unwrap a UV channel using a projection.
	 * Maps to action="auto_unwrap_uvs".
	 *
	 * @param ProjectionType - "Planar", "Box", or "Cylindrical" (case-insensitive).
	 * @param HardAngleThreshold - Degrees. Edges sharper than this become chart seams (Box mode only).
	 *
	 * @note "Planar" projects along world +Z. "Cylindrical" wraps around world Z axis.
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|UVMapping", meta = (AICallable, DisplayName = "Auto Unwrap UVs"))
	static FUVMappingResult AutoUnwrapUVs(
		const FString& MeshPath,
		int32 LODIndex,
		int32 ChannelIndex,
		const FString& ProjectionType = TEXT("Box"),
		float HardAngleThreshold = 66.0f);

	/**
	 * Repack the UV islands of an existing channel into [0,1]^2 with padding.
	 * Maps to action="pack_uvs".
	 *
	 * @param PaddingPercent - Padding between islands as percent of UV space.
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|UVMapping", meta = (AICallable, DisplayName = "Pack UVs"))
	static FUVMappingResult PackUVs(
		const FString& MeshPath,
		int32 LODIndex,
		int32 ChannelIndex,
		float PaddingPercent = 1.0f);

	// =================================================================
	// Transform
	// =================================================================

	/**
	 * Apply an affine transform to every UV in a channel: scale, rotate (about origin),
	 * then translate.
	 * Maps to action="transform_uvs".
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|UVMapping", meta = (AICallable, DisplayName = "Transform UVs"))
	static FUVMappingResult TransformUVs(
		const FString& MeshPath,
		int32 LODIndex,
		int32 ChannelIndex,
		float ScaleU = 1.0f,
		float ScaleV = 1.0f,
		float RotationDegrees = 0.0f,
		float OffsetU = 0.0f,
		float OffsetV = 0.0f);

	/**
	 * Mirror UVs across U=0.5 and/or V=0.5 in a channel.
	 * Maps to action="flip_uvs".
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|UVMapping", meta = (AICallable, DisplayName = "Flip UVs"))
	static FUVMappingResult FlipUVs(
		const FString& MeshPath,
		int32 LODIndex,
		int32 ChannelIndex,
		bool bFlipU = false,
		bool bFlipV = false);

	// =================================================================
	// Per-region selection & transform (artist-grade)
	// =================================================================

	/**
	 * Apply an affine UV transform ONLY to vertex instances whose normal direction is within
	 * a dot-product window relative to a reference axis. Use this to fix one face/region of
	 * a multi-faceted mesh without touching the others.
	 *
	 * Common patterns (axis = world +Z, i.e. (0,0,1)):
	 *   - Top cap of a vertical cylinder:    MinDot=  0.5,  MaxDot=  1.0
	 *   - Bottom cap:                        MinDot= -1.0,  MaxDot= -0.5
	 *   - Side strip (radial-pointing tris): MinDot= -0.5,  MaxDot=  0.5
	 *   - All upward-facing surfaces:        MinDot=  0.7,  MaxDot=  1.0
	 *
	 * Use CountVerticesByNormal first to preview how many vertex instances the filter selects.
	 * Maps to action="transform_uvs_by_normal".
	 *
	 * @param AxisX/Y/Z - Reference axis (will be normalized; (0,0,1) for world up).
	 * @param MinDot/MaxDot - Filter window for dot(normal, axis). Range [-1, 1].
	 * @param ScaleU/V, RotationDegrees, OffsetU/V - Affine transform applied to selected UVs.
	 * @return Result; Message reports how many vertex instances were transformed.
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|UVMapping", meta = (AICallable, DisplayName = "Transform UVs By Normal"))
	static FUVMappingResult TransformUVsByNormal(
		const FString& MeshPath,
		int32 LODIndex,
		int32 ChannelIndex,
		float AxisX,
		float AxisY,
		float AxisZ,
		float MinDot,
		float MaxDot,
		float ScaleU = 1.0f,
		float ScaleV = 1.0f,
		float RotationDegrees = 0.0f,
		float OffsetU = 0.0f,
		float OffsetV = 0.0f);

	/**
	 * Preview how many vertex instances a normal-direction filter selects, without modifying anything.
	 * Maps to action="count_vertices_by_normal".
	 *
	 * @return Number of vertex instances whose normal dotted with the axis lands in [MinDot, MaxDot].
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|UVMapping", meta = (AICallable, DisplayName = "Count Vertices By Normal"))
	static int32 CountVerticesByNormal(
		const FString& MeshPath,
		int32 LODIndex,
		float AxisX,
		float AxisY,
		float AxisZ,
		float MinDot,
		float MaxDot);

	/**
	 * Apply an affine UV transform ONLY to triangles assigned to the given polygon group
	 * (typically corresponds to a material slot). Use this when a mesh has clean per-section
	 * material slots — e.g. a wall with separate "trim" and "body" slots.
	 * Maps to action="transform_uvs_by_polygon_group".
	 *
	 * @param PolygonGroupName - Slot name (use ListPolygonGroups to discover).
	 *                           If empty or not found, returns failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|UVMapping", meta = (AICallable, DisplayName = "Transform UVs By Polygon Group"))
	static FUVMappingResult TransformUVsByPolygonGroup(
		const FString& MeshPath,
		int32 LODIndex,
		int32 ChannelIndex,
		const FString& PolygonGroupName,
		float ScaleU = 1.0f,
		float ScaleV = 1.0f,
		float RotationDegrees = 0.0f,
		float OffsetU = 0.0f,
		float OffsetV = 0.0f);

	/**
	 * List polygon group names (material slot identifiers) on a LOD. Use these as
	 * input to TransformUVsByPolygonGroup.
	 * Maps to action="list_polygon_groups".
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|UVMapping", meta = (AICallable, DisplayName = "List Polygon Groups"))
	static TArray<FString> ListPolygonGroups(const FString& MeshPath, int32 LODIndex);

	/**
	 * Detect connected UV islands. Two vertex instances are in the same island if they
	 * are reachable through triangles with continuous UVs (no seam crossing). Returned
	 * island ids are stable for a given mesh state — call TransformUVIsland with the id.
	 *
	 * Use this when a mesh has multiple distinct UV regions (e.g. a cylinder cap + side +
	 * bottom = 3 islands) and you want to operate on one without touching the others.
	 *
	 * Maps to action="identify_uv_islands".
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|UVMapping", meta = (AICallable, DisplayName = "Identify UV Islands"))
	static TArray<FUVIslandInfo> IdentifyUVIslands(
		const FString& MeshPath,
		int32 LODIndex,
		int32 ChannelIndex);

	/**
	 * Apply an affine UV transform to exactly one UV island.
	 *
	 * Pair with IdentifyUVIslands: call that to inventory islands, pick the id you want
	 * (e.g. by inspecting AverageNormal or WorldCenter), then call this with the same
	 * (mesh, lod, channel) and the chosen IslandId. Island ids are stable as long as the
	 * mesh's UV topology doesn't change.
	 *
	 * Maps to action="transform_uv_island".
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|UVMapping", meta = (AICallable, DisplayName = "Transform UV Island"))
	static FUVMappingResult TransformUVIsland(
		const FString& MeshPath,
		int32 LODIndex,
		int32 ChannelIndex,
		int32 IslandId,
		float ScaleU = 1.0f,
		float ScaleV = 1.0f,
		float RotationDegrees = 0.0f,
		float OffsetU = 0.0f,
		float OffsetV = 0.0f);

	// =================================================================
	// Lightmap settings
	// =================================================================

	/**
	 * Read lightmap-related fields from a static mesh.
	 * Maps to action="get_lightmap_settings".
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|UVMapping", meta = (AICallable, DisplayName = "Get Lightmap Settings"))
	static bool GetLightmapSettings(const FString& MeshPath, FUVLightmapSettings& OutSettings);

	/**
	 * Configure all lightmap-related fields in one call.
	 * Maps to action="set_lightmap_settings".
	 *
	 * @param LightmapCoordinateIndex - The UV channel that lightmaps sample from at runtime.
	 * @param SourceLightmapIndex - The UV channel that GenerateLightmapUVs reads seams from.
	 * @param LightMapResolution - Power-of-two preferred (32..512 typical).
	 * @param bGenerateLightmapUVs - If true, the next build will regenerate the lightmap channel.
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|UVMapping", meta = (AICallable, DisplayName = "Set Lightmap Settings"))
	static FUVMappingResult SetLightmapSettings(
		const FString& MeshPath,
		int32 LightmapCoordinateIndex = 1,
		int32 SourceLightmapIndex = 0,
		int32 LightMapResolution = 64,
		bool bGenerateLightmapUVs = true);

	// =================================================================
	// Visualize
	// =================================================================

	/**
	 * Render a UV layout to a PNG file. Triangles are drawn as outlines on a transparent
	 * background; the [0,1] square is drawn as a faint grid for reference.
	 * Maps to action="export_uv_layout_image".
	 *
	 * @param OutputPath - Absolute filesystem path (any extension; .png is forced).
	 * @param ImageSize - Output resolution in pixels (square). 256..4096.
	 * @return Result with the resolved output path in Message.
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|UVMapping", meta = (AICallable, DisplayName = "Export UV Layout Image"))
	static FUVMappingResult ExportUVLayoutImage(
		const FString& MeshPath,
		int32 LODIndex,
		int32 ChannelIndex,
		const FString& OutputPath,
		int32 ImageSize = 1024);

	// =================================================================
	// Existence
	// =================================================================

	/**
	 * Cheap check: does this mesh have a UV channel at (LOD, channel)?
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|UVMapping|Exists")
	static bool MeshHasUVChannel(const FString& MeshPath, int32 LODIndex, int32 ChannelIndex);

private:
	// Asset loading helpers
	static class UStaticMesh* LoadStaticMeshAsset(const FString& MeshPath);
	static class USkeletalMesh* LoadSkeletalMeshAsset(const FString& MeshPath);
	static bool IsStaticMeshPath(const FString& MeshPath);

	// MeshDescription helpers (StaticMesh only; SkeletalMesh edits go through LODModels directly)
	static struct FMeshDescription* GetStaticMeshDescription(class UStaticMesh* Mesh, int32 LODIndex);
	static void CommitStaticMeshDescription(class UStaticMesh* Mesh, int32 LODIndex);

	// Stat computation (channel-level)
	static void ComputeChannelStats(
		const struct FMeshDescription& MeshDesc,
		int32 ChannelIndex,
		FUVChannelInfo& OutInfo);

	static int32 GetStaticMeshNumUVChannels(const class UStaticMesh* Mesh, int32 LODIndex);
	static int32 GetSkeletalMeshNumUVChannels(const class USkeletalMesh* Mesh, int32 LODIndex);

	// Result builders
	static FUVMappingResult MakeFailure(const FString& MeshPath, const FString& Message);
	static FUVMappingResult MakeSuccess(const FString& MeshPath, const FString& Message);
};
