// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UMapBlockoutService.generated.h"

/**
 * Procedural FPS-map blockout generator. Reads weight layers from a VibeUE-generated
 * landscape and produces a gated, AAA-style map blockout: roads, POIs, fields, foliage,
 * railway/bridges, plus per-stage PNG snapshots and heatmaps. Each stage runs the
 * MapDesigner spec's pass/fail checks; the orchestrator stops at the first failing
 * gate and reports which check failed.
 *
 * Authoritative spec: docs/design/map-designer-spec.md.
 */


// =========================================================================
// Stage Validation
// =========================================================================

/** Single named check inside a stage gate. */
USTRUCT(BlueprintType)
struct FMapBlockoutCheckResult
{
	GENERATED_BODY()

	/** Human-readable check name (matches a line in docs/design/map-designer-spec.md). */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FString Name;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	bool bPassed = false;

	/** Optional message: what specifically failed, or what passed and why. */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FString Message;
};

/** Aggregate gate result for one stage (Stage 1..5, Stage 0, Final Pass). */
USTRUCT(BlueprintType)
struct FMapBlockoutGateResult
{
	GENERATED_BODY()

	/** Stage number (0..5, or 6 = Final Pass). */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	int32 Stage = 0;

	/** Ordered list of every check this stage ran. */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	TArray<FMapBlockoutCheckResult> Checks;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	bool bAllPassed = false;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	int32 FailedCount = 0;
};


// =========================================================================
// Landcover Grid (Stage 0 output)
// =========================================================================

/** A single named weight layer downsampled to an NxN grid (row 0 = south). */
USTRUCT(BlueprintType)
struct FMapBlockoutLayerMap
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FString LayerName;

	/** Grid resolution (square). */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	int32 GridN = 0;

	/** Row-major weights in [0,1]. Length = GridN * GridN. Row 0 = south edge of map. */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	TArray<float> Weights;
};

/**
 * Result of Stage 0: a square, origin-centred sample of every paint layer on the
 * source landscape, plus the world bounds. Drop-in replacement for the contributor's
 * landcover_grid.json.
 */
USTRUCT(BlueprintType)
struct FMapBlockoutLandcoverGrid
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	bool bSuccess = false;

	/** Source landscape actor label. */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FString LandscapeLabel;

	/** Grid resolution (square). 120 is the contributor's default. */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	int32 GridN = 120;

	/** World min (Unreal units). The world is treated as square and origin-centred. */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	float WorldLo = 0.0f;

	/** World max (Unreal units). */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	float WorldHi = 0.0f;

	/** Heightmap downsampled to GridN x GridN, normalized 0..1. Empty if export failed. */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	TArray<float> HeightNormalized;

	/** One entry per paint layer, in the order the source landscape returned them. */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	TArray<FMapBlockoutLayerMap> Layers;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FString ErrorMessage;
};


// =========================================================================
// River Centerlines (optional Stage 0 input for crisp rivers)
// =========================================================================

USTRUCT(BlueprintType)
struct FMapBlockoutRiverPoint
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FVector2D World = FVector2D::ZeroVector;

	/** Width in meters at this point (default 30). */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	float WidthM = 30.0f;
};

USTRUCT(BlueprintType)
struct FMapBlockoutRiver
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FString Name;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	TArray<FMapBlockoutRiverPoint> Points;
};


// =========================================================================
// Configuration
// =========================================================================

USTRUCT(BlueprintType)
struct FMapBlockoutRoadConfig
{
	GENERATED_BODY()

	/** Number of main (paved) vertical arteries. */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	int32 MainVerticals = 3;

	/** Number of main (paved) horizontal arteries. */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	int32 MainHorizontals = 2;

	/** Dirt-road grid pitch (km). Smaller = denser network = more intersections / more POIs. */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	float DirtSpacingKm = 1.7f;

	/** Extra non-grid diagonal connectors. */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	int32 Diagonals = 3;
};

USTRUCT(BlueprintType)
struct FMapBlockoutPOIConfig
{
	GENERATED_BODY()

	/** Target POI count for a 12 km reference map. 0 = auto-scale by area. */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	int32 TargetCount = 16;

	/** Minimum POI spacing as a fraction of map span. */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	float MinSpacingFrac = 0.085f;
};

/** Maps the contributor's design categories to source-landscape paint-layer names. */
USTRUCT(BlueprintType)
struct FMapBlockoutLayerKeyMap
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FString Crop;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FString Soil;

	/** Water / wet layer. May be empty — water can be synthesized from heightmap percentile. */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FString Flood;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FString Forest;
};

USTRUCT(BlueprintType)
struct FMapBlockoutConfig
{
	GENERATED_BODY()

	/** Output folder is named after this. */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FString LevelName;

	/** Absolute or project-relative output directory. Empty = <Project>/Saved/MapBlockout/<LevelName>/. */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FString OutputDir;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FMapBlockoutLayerKeyMap Layers;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	int32 Seed = 7;

	/** Required field coverage band (% of play area). MapDesigner default 50..60. */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FVector2D FieldCoverageBand = FVector2D(50.0f, 60.0f);

	/** Required tree coverage band (% of play area). MapDesigner default 30..40. */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FVector2D TreeCoverageBand = FVector2D(30.0f, 40.0f);

	/** Min crop-layer weight for a cell to be field-eligible. Lower = more fields. */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	float FieldCropThreshold = 0.12f;

	/** Width of the young-growth treeline ring around each wood (cells). */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	int32 ForestFringeIters = 9;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FMapBlockoutRoadConfig Road;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FMapBlockoutPOIConfig Pois;

	/** Optional precise river polylines (overrides flood-layer-derived water). */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	TArray<FMapBlockoutRiver> Rivers;
};


// =========================================================================
// Stage 1 — Roads
// =========================================================================

UENUM(BlueprintType)
enum class EMapBlockoutRoadType : uint8
{
	Main UMETA(DisplayName = "Main Road"),
	Dirt UMETA(DisplayName = "Dirt Road"),
	Railway UMETA(DisplayName = "Railway"),
};

USTRUCT(BlueprintType)
struct FMapBlockoutRoad
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	EMapBlockoutRoadType Type = EMapBlockoutRoadType::Dirt;

	/** Ordered world-XY points (Unreal units). Treat as a polyline. */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	TArray<FVector2D> Points;

	/** Road half-width (Unreal units) for materialization as a landscape spline. */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	float WidthCm = 500.0f;
};

USTRUCT(BlueprintType)
struct FMapBlockoutRoadNetworkResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	TArray<FMapBlockoutRoad> Roads;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FMapBlockoutGateResult Gate;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FString ErrorMessage;
};


// =========================================================================
// Stage 2 — POIs
// =========================================================================

UENUM(BlueprintType)
enum class EMapBlockoutPOIType : uint8
{
	Town UMETA(DisplayName = "Town"),
	Village UMETA(DisplayName = "Village"),
	Farmstead UMETA(DisplayName = "Farmstead"),
	Crossroads UMETA(DisplayName = "Crossroads Hamlet"),
	Industrial UMETA(DisplayName = "Industrial"),
	Strongpoint UMETA(DisplayName = "Forest-Surrounded Strongpoint"),
};

USTRUCT(BlueprintType)
struct FMapBlockoutBuilding
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FVector2D World = FVector2D::ZeroVector;

	/** Footprint half-extents (Unreal units). */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FVector2D HalfExtents = FVector2D(400.0f, 400.0f);

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	float YawDegrees = 0.0f;
};

USTRUCT(BlueprintType)
struct FMapBlockoutPOI
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FString Name;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	EMapBlockoutPOIType Type = EMapBlockoutPOIType::Village;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FVector2D Center = FVector2D::ZeroVector;

	/** POI zone radius (Unreal units). */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	float RadiusCm = 5000.0f;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	TArray<FMapBlockoutBuilding> Buildings;
};

USTRUCT(BlueprintType)
struct FMapBlockoutPOIResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	TArray<FMapBlockoutPOI> Pois;

	/** Any service roads added by this stage (appended to the Stage 1 network). */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	TArray<FMapBlockoutRoad> AddedServiceRoads;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FMapBlockoutGateResult Gate;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FString ErrorMessage;
};


// =========================================================================
// Stage 3 — Fields
// =========================================================================

/**
 * A binary mask sampled at MaskW x MaskH cells over the play area.
 * Cells are stored row-major. Row 0 = north edge (matches render output, not the
 * landcover grid which is row 0 = south).
 */
USTRUCT(BlueprintType)
struct FMapBlockoutMask
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	int32 Width = 0;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	int32 Height = 0;

	/** Row-major, 0 = off, 1 = on. Length = Width * Height. */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	TArray<uint8> Cells;
};

USTRUCT(BlueprintType)
struct FMapBlockoutFieldResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FMapBlockoutMask FieldMask;

	/** Coverage as a fraction of total play area (0..1). */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	float CoverageFraction = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FMapBlockoutGateResult Gate;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FString ErrorMessage;
};


// =========================================================================
// Stage 4 — Foliage (Trees, Treelines, Underbrush)
// =========================================================================

USTRUCT(BlueprintType)
struct FMapBlockoutFoliageResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FMapBlockoutMask ForestMask;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FMapBlockoutMask TreelineMask;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FMapBlockoutMask ScrubMask;

	/** Combined tree+forest coverage as a fraction of total play area (0..1). */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	float TreeCoverageFraction = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FMapBlockoutGateResult Gate;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FString ErrorMessage;
};


// =========================================================================
// Stage 5 — Railway and Bridges
// =========================================================================

USTRUCT(BlueprintType)
struct FMapBlockoutBridge
{
	GENERATED_BODY()

	/** Bridge midpoint (Unreal units). */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FVector2D World = FVector2D::ZeroVector;

	/** Span length (Unreal units). */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	float LengthCm = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	float YawDegrees = 0.0f;

	/** Which infrastructure crosses the bridge. */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	EMapBlockoutRoadType Carries = EMapBlockoutRoadType::Main;
};

USTRUCT(BlueprintType)
struct FMapBlockoutRailwayResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	bool bSuccess = false;

	/** Railway polylines (Type = Railway). */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	TArray<FMapBlockoutRoad> RailLines;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	TArray<FMapBlockoutBridge> Bridges;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FMapBlockoutGateResult Gate;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FString ErrorMessage;
};


// =========================================================================
// Accumulated Blockout State + Final Pipeline Result
// =========================================================================

/**
 * Carries every prior stage's output forward. Passed by const ref to each stage
 * so later stages can read (but not mutate) earlier results — matches the spec's
 * "snapshots are never overwritten" rule.
 */
USTRUCT(BlueprintType)
struct FMapBlockoutState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FMapBlockoutConfig Config;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FMapBlockoutLandcoverGrid Grid;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FMapBlockoutRoadNetworkResult Stage1Roads;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FMapBlockoutPOIResult Stage2Pois;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FMapBlockoutFieldResult Stage3Fields;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FMapBlockoutFoliageResult Stage4Foliage;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FMapBlockoutRailwayResult Stage5Railway;
};

USTRUCT(BlueprintType)
struct FMapBlockoutPipelineResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FMapBlockoutState FinalState;

	/** Final-pass gate (re-validates every prior stage). */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FMapBlockoutGateResult FinalGate;

	/** Absolute paths of every PNG/JSON written (8 files on success). */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	TArray<FString> OutputFiles;

	/** Resolved output directory (named after Config.LevelName). */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FString OutputDir;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FString ErrorMessage;
};


// =========================================================================
// Materialization Results
// =========================================================================

USTRUCT(BlueprintType)
struct FMapBlockoutMaterializeResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	bool bSuccess = false;

	/** How many primitives were created (splines, actors, painted cells, foliage instances). */
	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	int32 CreatedCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "MapBlockout")
	FString ErrorMessage;
};


// =========================================================================
// Service
// =========================================================================

/**
 * UMapBlockoutService — procedural FPS-map blockout from a VibeUE landscape.
 *
 * Workflow (matches docs/design/map-designer-spec.md):
 *   0. ExportLandcoverGrid        — pulls weight layers off the source landscape
 *   1. GenerateRoads              — Main + Dirt network, gated on connectivity
 *   2. PlacePOIs                  — towns/farms/villages on the road network
 *   3. PlaceFields                — agricultural fields, 50–60% coverage band
 *   4. PlaceFoliage               — forest + treelines + scrub, 30–40% trees
 *   5. PlaceRailway               — railway + water-crossing bridges
 *   6. RunFinalPass               — re-validate everything + write 8 deliverable files
 *
 * RunFullPipeline runs the whole sequence and writes the deliverables.
 * Each stage can also be called individually; consumers may inspect/repair the
 * intermediate state before advancing.
 *
 * After the plan is built, the Materialize* methods turn it into real engine
 * geometry: landscape splines, paint layers, spawned actors, foliage instances.
 *
 * Python usage:
 *   cfg = unreal.MapBlockoutConfig()
 *   cfg.level_name = "Verkhova"
 *   cfg.layers.crop, cfg.layers.forest = "Crop", "Forest"
 *   result = unreal.MapBlockoutService.run_full_pipeline_for_landscape("Landscape1", cfg)
 *   if result.success:
 *       unreal.MapBlockoutService.materialize_roads_as_splines(
 *           result.final_state.stage1_roads, "Landscape1")
 */
UCLASS(BlueprintType)
class VIBEUE_API UMapBlockoutService : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	// =================================================================
	// Stage 0 — Landcover grid export
	// =================================================================

	/**
	 * Sample every paint layer on the source landscape plus its heightmap into an
	 * NxN normalized grid (drop-in for the contributor's landcover_grid.json).
	 *
	 * @param LandscapeLabel             - actor label of the source landscape (must exist)
	 * @param GridN                      - grid resolution (default 120, MapDesigner spec default)
	 * @param bSynthesizeFloodFromHeight - add an extra layer named "FloodFromHeight"
	 *        derived from the lowest FloodPercentile% of the heightmap. Useful when
	 *        the source landscape has no painted water layer — map it as
	 *        `cfg.layers.flood = "FloodFromHeight"`. Mirrors the host-Python
	 *        `--flood-from-height` behaviour.
	 * @param FloodPercentile            - lowest %% of terrain treated as flood (default 8).
	 * @return Bounds + height + per-layer weights, or bSuccess=false + ErrorMessage
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|MapBlockout")
	static FMapBlockoutLandcoverGrid ExportLandcoverGrid(
		const FString& LandscapeLabel,
		int32 GridN = 120,
		bool bSynthesizeFloodFromHeight = false,
		float FloodPercentile = 8.0f);

	/**
	 * Serialize a landcover grid to disk as JSON (compatible with the host-Python
	 * reference implementation's landcover_grid.json).
	 *
	 * @return Absolute path of the written file, or empty string on failure.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|MapBlockout")
	static FString WriteLandcoverGridJson(
		const FMapBlockoutLandcoverGrid& Grid,
		const FString& OutputFilePath);

	/**
	 * Load a landcover_grid.json file (host-Python format) into a grid struct.
	 * Used by tests / by callers who already have a JSON fixture and want to
	 * skip the live-landscape export step.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|MapBlockout")
	static FMapBlockoutLandcoverGrid LoadLandcoverGridJson(
		const FString& FilePath);

	/**
	 * Extract river centerline polylines from a binary water mask. Picks the largest
	 * 8-connected water component (drops ponds/lakes), runs a Euclidean distance
	 * transform to recover the medial axis, finds the geodesic diameter endpoints
	 * via two BFS passes, threads a Dijkstra path that hugs the high-DT ridge from
	 * one endpoint to the other, then traces the longest tributary back to the
	 * spine. Per-point widths come from `2 * dt * meters_per_pixel` so wide rivers
	 * get wide-river bridges and narrow tributaries get narrow ones.
	 *
	 * Mirrors the host-Python river-centerline reference algorithm.
	 *
	 * @param WaterMask    - binary mask (1 = water)
	 * @param WorldLo      - landcover grid's WorldLo (sets world-space coords)
	 * @param WorldHi      - landcover grid's WorldHi
	 * @param MinLengthCm  - drop polylines shorter than this (Unreal units)
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|MapBlockout")
	static TArray<FMapBlockoutRiver> ExtractRiverCenterlines(
		const FMapBlockoutMask& WaterMask,
		float WorldLo,
		float WorldHi,
		float MinLengthCm = 50000.0f);


	// =================================================================
	// Stages 1–5 — per-stage generators (each runs its own gate)
	// =================================================================

	/** Stage 1 — Roadways. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|MapBlockout")
	static FMapBlockoutRoadNetworkResult GenerateRoads(
		const FMapBlockoutLandcoverGrid& Grid,
		const FMapBlockoutConfig& Config);

	/** Stage 2 — Points of Interest. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|MapBlockout")
	static FMapBlockoutPOIResult PlacePois(
		const FMapBlockoutLandcoverGrid& Grid,
		const FMapBlockoutRoadNetworkResult& Roads,
		const FMapBlockoutConfig& Config);

	/** Stage 3 — Fields. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|MapBlockout")
	static FMapBlockoutFieldResult PlaceFields(
		const FMapBlockoutLandcoverGrid& Grid,
		const FMapBlockoutRoadNetworkResult& Roads,
		const FMapBlockoutPOIResult& Pois,
		const FMapBlockoutConfig& Config);

	/** Stage 4 — Trees, Forests, Underbrush. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|MapBlockout")
	static FMapBlockoutFoliageResult PlaceFoliage(
		const FMapBlockoutLandcoverGrid& Grid,
		const FMapBlockoutRoadNetworkResult& Roads,
		const FMapBlockoutPOIResult& Pois,
		const FMapBlockoutFieldResult& Fields,
		const FMapBlockoutConfig& Config);

	/** Stage 5 — Railway + Bridges. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|MapBlockout")
	static FMapBlockoutRailwayResult PlaceRailway(
		const FMapBlockoutLandcoverGrid& Grid,
		const FMapBlockoutRoadNetworkResult& Roads,
		const FMapBlockoutPOIResult& Pois,
		const FMapBlockoutFieldResult& Fields,
		const FMapBlockoutFoliageResult& Foliage,
		const FMapBlockoutConfig& Config);

	/** Final Pass — re-run every prior gate against the cumulative state. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|MapBlockout")
	static FMapBlockoutGateResult RunFinalPass(const FMapBlockoutState& State);


	// =================================================================
	// Rendering
	// =================================================================

	/**
	 * Render the cumulative per-stage PNG snapshot (Stage 1..5) into OutputDir,
	 * following the MapDesigner color chart and including the top-right color key.
	 *
	 * @param Stage      - 1..5
	 * @param State      - state through the requested stage (later stages may be empty)
	 * @param OutputDir  - directory (created if missing)
	 * @return Absolute file path written, or empty string on failure.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|MapBlockout")
	static FString RenderStageSnapshot(
		int32 Stage,
		const FMapBlockoutState& State,
		const FString& OutputDir);

	/**
	 * Render the three final deliverables — CombinedFoliageAndMap.png,
	 * FoliageHeatMap.png, MapHeatMap.png — into OutputDir.
	 *
	 * @return Absolute paths of the three files written (empty array on failure).
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|MapBlockout")
	static TArray<FString> RenderFinalDeliverables(
		const FMapBlockoutState& State,
		const FString& OutputDir);


	// =================================================================
	// Orchestrator
	// =================================================================

	/**
	 * Run Stage 0..5 + Final Pass against an existing grid, writing all 8 PNGs.
	 * Stops at the first failing gate; consumers can read the failing stage's
	 * Gate field to know what to repair.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|MapBlockout")
	static FMapBlockoutPipelineResult RunFullPipeline(
		const FMapBlockoutLandcoverGrid& Grid,
		const FMapBlockoutConfig& Config);

	/**
	 * Convenience: ExportLandcoverGrid + RunFullPipeline in one call.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|MapBlockout")
	static FMapBlockoutPipelineResult RunFullPipelineForLandscape(
		const FString& LandscapeLabel,
		const FMapBlockoutConfig& Config);


	// =================================================================
	// Materialization — turn the plan into actual engine geometry
	// =================================================================

	/**
	 * Convert every road polyline into a landscape spline on the target landscape
	 * (uses ULandscapeService::CreateSplinePoint / ConnectSplinePoints internally).
	 * Main roads and dirt roads are placed on separate splines so they can carry
	 * different mesh entries.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|MapBlockout")
	static FMapBlockoutMaterializeResult MaterializeRoadsAsSplines(
		const FMapBlockoutRoadNetworkResult& Roads,
		const FString& LandscapeLabel);

	/**
	 * Paint the field mask into a landscape weight layer (creates the layer info
	 * object if it doesn't exist). Reuses ULandscapeService::PaintLayer.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|MapBlockout")
	static FMapBlockoutMaterializeResult MaterializeFieldsAsPaint(
		const FMapBlockoutFieldResult& Fields,
		const FString& LandscapeLabel,
		const FString& LayerName = TEXT("Crop"));

	/**
	 * Spawn one empty actor per POI as a labeled placeholder under FolderPath
	 * (e.g. "/MapBlockout/POIs/"). Each POI gets child static-mesh actors at every
	 * building location. Intended as a blockout — the contributor replaces these
	 * placeholders with finished assets later.
	 *
	 * @param BuildingMeshPath - optional default mesh for buildings (empty = empty actors)
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|MapBlockout")
	static FMapBlockoutMaterializeResult MaterializePoisAsActors(
		const FMapBlockoutPOIResult& Pois,
		const FString& FolderPath,
		const FString& BuildingMeshPath = TEXT(""));

	/**
	 * Scatter foliage instances over the forest/treeline/scrub masks using the
	 * existing UFoliageService scatter pipeline.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|MapBlockout")
	static FMapBlockoutMaterializeResult MaterializeForestAsFoliage(
		const FMapBlockoutFoliageResult& Foliage,
		const FString& ForestFoliageTypePath,
		const FString& TreelineFoliageTypePath,
		const FString& ScrubFoliageTypePath);

	/**
	 * Add railway splines and spawn bridge placeholder actors (one per bridge).
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|MapBlockout")
	static FMapBlockoutMaterializeResult MaterializeRailwayAndBridges(
		const FMapBlockoutRailwayResult& Railway,
		const FString& LandscapeLabel,
		const FString& BridgeMeshPath = TEXT(""));
};
