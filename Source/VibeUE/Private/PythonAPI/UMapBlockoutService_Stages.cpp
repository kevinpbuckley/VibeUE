// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UMapBlockoutService.h"
#include "MapBlockout/MapBlockoutMath.h"
#include "MapBlockout/MapBlockoutImage.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

// Stage 1–5 generators, the Final Pass, the renderers, and the orchestrators.
// Every method is currently a stub returning bSuccess=false with a descriptive
// error message — signatures are locked so consumers (Python, the skill loader)
// compile against the final surface. The contributor ports the host-Python
// reference at Source/VibeUE/Tests/MapBlockout/reference/ into these bodies.
//
// Spec: docs/design/map-designer-spec.md
// Skill: Content/Skills/map-blockout/

namespace
{
	FMapBlockoutGateResult MakeStubGate(int32 Stage, const TCHAR* StageName)
	{
		FMapBlockoutGateResult Gate;
		Gate.Stage = Stage;
		FMapBlockoutCheckResult NotImpl;
		NotImpl.Name = TEXT("StageImplemented");
		NotImpl.bPassed = false;
		NotImpl.Message = FString::Printf(TEXT("%s not yet implemented — see docs/design/map-designer-spec.md"), StageName);
		Gate.Checks.Add(NotImpl);
		Gate.bAllPassed = false;
		Gate.FailedCount = 1;
		return Gate;
	}

	FString ResolveOutputDir(const FMapBlockoutConfig& Config)
	{
		FString Dir = Config.OutputDir;
		if (Dir.IsEmpty())
		{
			Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MapBlockout"), Config.LevelName);
		}
		FPaths::NormalizeDirectoryName(Dir);
		return FPaths::ConvertRelativePathToFull(Dir);
	}
}


// =========================================================================
// Stage 1 — Roads
// =========================================================================

FMapBlockoutRoadNetworkResult UMapBlockoutService::GenerateRoads(
	const FMapBlockoutLandcoverGrid& Grid, const FMapBlockoutConfig& Config)
{
	FMapBlockoutRoadNetworkResult Result;
	Result.bSuccess = false;
	Result.ErrorMessage = TEXT("Stage 1 (Roads) not yet implemented");
	Result.Gate = MakeStubGate(1, TEXT("Stage 1 — Roadways"));

	// Intended implementation (see Stage 1 in docs/design/map-designer-spec.md):
	//   1. From Config.Road: lay down MainVerticals + MainHorizontals paved arteries
	//      across the play area (terrain-aware: nudge each artery onto the nearest
	//      ridge/valley using the height-derived slope field from Grid.HeightNormalized).
	//   2. Lay a dirt-road sub-grid at Road.DirtSpacingKm pitch, prune segments
	//      that cross water (flood layer) or unbuildable cells.
	//   3. Add Road.Diagonals non-grid connectors.
	//   4. Connected-components on the rasterized network: keep only the largest
	//      component (orphan road fragments fail the gate).
	//   5. Convert polylines from grid coords to world coords using Grid.WorldLo/Hi.
	//   6. Run Stage 1 checks (Connectivity, Grid Sensibility, Pattern Realism,
	//      AAA Standard, Color Compliance, Color Key) and fill Result.Gate.
	return Result;
}


// =========================================================================
// Stage 2 — POIs
// =========================================================================

FMapBlockoutPOIResult UMapBlockoutService::PlacePOIs(
	const FMapBlockoutLandcoverGrid& Grid,
	const FMapBlockoutRoadNetworkResult& Roads,
	const FMapBlockoutConfig& Config)
{
	FMapBlockoutPOIResult Result;
	Result.bSuccess = false;
	Result.ErrorMessage = TEXT("Stage 2 (POIs) not yet implemented");
	Result.Gate = MakeStubGate(2, TEXT("Stage 2 — POIs"));

	// Intended implementation (see Stage 2 in docs/design/map-designer-spec.md):
	//   1. Sample candidate centers from road intersections + along main roads.
	//   2. Reject candidates inside water (flood layer) or on top of road surfaces.
	//   3. Poisson-disk filter with MinSpacingFrac for natural distribution.
	//   4. Type each POI by road context (town↔main, farm↔dirt, etc.).
	//   5. Place buildings inside each POI zone respecting the road clearance
	//      and zone radius; ensure no building sits on a road segment.
	//   6. Append any new service roads (POIs that needed dirt-track connections).
	//   7. Run Stage 2 checks (POI Count, On-Network, Type-Road Match,
	//      Distribution, Buildings Off Roads, No River Overlap, Layout Realism,
	//      AAA FPS Combat Design, Boundary Circle, Color Key).
	return Result;
}


// =========================================================================
// Stage 3 — Fields
// =========================================================================

FMapBlockoutFieldResult UMapBlockoutService::PlaceFields(
	const FMapBlockoutLandcoverGrid& Grid,
	const FMapBlockoutRoadNetworkResult& Roads,
	const FMapBlockoutPOIResult& POIs,
	const FMapBlockoutConfig& Config)
{
	FMapBlockoutFieldResult Result;
	Result.bSuccess = false;
	Result.ErrorMessage = TEXT("Stage 3 (Fields) not yet implemented");
	Result.Gate = MakeStubGate(3, TEXT("Stage 3 — Fields"));

	// Intended implementation (see Stage 3 in docs/design/map-designer-spec.md):
	//   1. Start mask = crop_layer >= FieldCropThreshold.
	//   2. Subtract roads (dilated by road half-width), POI footprints, water.
	//   3. Run connected components and drop polygons smaller than a sensible floor.
	//   4. Iterate FieldCropThreshold (or grow the mask) until coverage lands
	//      inside FieldCoverageBand. If unreachable, fail the gate.
	//   5. Stage 3 checks: Placement Sense, No Road/Building/POI/River Overlap,
	//      Coverage Bounds, Foliage Headroom, Color Compliance, Color Key.
	return Result;
}


// =========================================================================
// Stage 4 — Trees, Forests, Underbrush
// =========================================================================

FMapBlockoutFoliageResult UMapBlockoutService::PlaceFoliage(
	const FMapBlockoutLandcoverGrid& Grid,
	const FMapBlockoutRoadNetworkResult& Roads,
	const FMapBlockoutPOIResult& POIs,
	const FMapBlockoutFieldResult& Fields,
	const FMapBlockoutConfig& Config)
{
	FMapBlockoutFoliageResult Result;
	Result.bSuccess = false;
	Result.ErrorMessage = TEXT("Stage 4 (Foliage) not yet implemented");
	Result.Gate = MakeStubGate(4, TEXT("Stage 4 — Trees / Forests / Underbrush"));

	// Intended implementation (see Stage 4 in docs/design/map-designer-spec.md):
	//   1. Forest seeds from forest_layer >= threshold, subtract roads/POIs/fields/water.
	//   2. Erode + label to get irregular, organic forest polygons (no perfect circles).
	//   3. Dilate forests by ForestFringeIters to grow treelines along the edge.
	//   4. Subtract roads from treelines so road corridors stay clear.
	//   5. Scrub mask = unused area near treelines.
	//   6. At least one POI must be ringed by forest (Forest-Surrounded POI check).
	//   7. Stage 4 checks: No Forbidden Overlap, Treeline Logic, Forest-Surrounded
	//      POI, Road Corridors Clear, Coverage Bounds (30..40%), In-POI Trees Valid,
	//      AAA FPS Standard, Color Compliance, Color Key.
	return Result;
}


// =========================================================================
// Stage 5 — Railway + Bridges
// =========================================================================

FMapBlockoutRailwayResult UMapBlockoutService::PlaceRailway(
	const FMapBlockoutLandcoverGrid& Grid,
	const FMapBlockoutRoadNetworkResult& Roads,
	const FMapBlockoutPOIResult& POIs,
	const FMapBlockoutFieldResult& Fields,
	const FMapBlockoutFoliageResult& Foliage,
	const FMapBlockoutConfig& Config)
{
	FMapBlockoutRailwayResult Result;
	Result.bSuccess = false;
	Result.ErrorMessage = TEXT("Stage 5 (Railway + Bridges) not yet implemented");
	Result.Gate = MakeStubGate(5, TEXT("Stage 5 — Railway and Bridges"));

	// Intended implementation (see Stage 5 in docs/design/map-designer-spec.md):
	//   1. Lay one or two railway polylines connecting major POIs through fields
	//      (railway may cross fields, but never sits on buildings).
	//   2. For every water crossing of a road or rail, place a Bridge entry
	//      (midpoint, length, yaw, carries which infrastructure).
	//   3. Where rail crosses treelines, register the affected cells in the
	//      Foliage state as "remove" — the Final Pass re-validates.
	//   4. Stage 5 checks: Railway Placed, No Building Overlap (Rail), No Building
	//      Overlap (Bridge), Bridges Over Water Only, Treeline Clearance,
	//      Field Crossing Allowed, Color Compliance, Color Key.
	return Result;
}


// =========================================================================
// Final Pass
// =========================================================================

FMapBlockoutGateResult UMapBlockoutService::RunFinalPass(const FMapBlockoutState& State)
{
	// Intended impl: re-execute every stage's checks against the cumulative state
	// (catches cross-stage regressions like Stage 5 dropping a tree onto a Stage 1
	// road corridor that had been clean). Run cross-layer integrity pass last.
	return MakeStubGate(6, TEXT("Final Pass"));
}


// =========================================================================
// Rendering
// =========================================================================

FString UMapBlockoutService::RenderStageSnapshot(
	int32 Stage, const FMapBlockoutState& State, const FString& OutputDir)
{
	if (Stage < 1 || Stage > 5 || OutputDir.IsEmpty()) { return FString(); }

	// Intended impl:
	//   1. Allocate a 1600x1600+panel RGBA8 bitmap via MapBlockoutImage::NewBitmap.
	//   2. Render each stage's primitives in cumulative order using the MapDesigner
	//      color chart constants in MapBlockoutImage::Colors::*.
	//   3. Stamp the top-right Color Key with MapBlockoutImage::DrawColorKey
	//      (panel must NOT overlap the map area).
	//   4. Write PNG via MapBlockoutImage::WritePNG.

	// Stub: emit nothing.
	return FString();
}

TArray<FString> UMapBlockoutService::RenderFinalDeliverables(
	const FMapBlockoutState& State, const FString& OutputDir)
{
	// Intended impl: produces CombinedFoliageAndMap.png (with color key),
	// FoliageHeatMap.png (no key, trees+fields+scrub only) and MapHeatMap.png
	// (no key, roads+buildings+rail+bridges only) via MapBlockoutImage::RenderHeatmap.
	return TArray<FString>();
}


// =========================================================================
// Orchestrators
// =========================================================================

FMapBlockoutPipelineResult UMapBlockoutService::RunFullPipeline(
	const FMapBlockoutLandcoverGrid& Grid, const FMapBlockoutConfig& Config)
{
	FMapBlockoutPipelineResult Result;
	Result.OutputDir = ResolveOutputDir(Config);

	if (!Grid.bSuccess)
	{
		Result.ErrorMessage = TEXT("RunFullPipeline: input Grid is invalid (run ExportLandcoverGrid first).");
		return Result;
	}

	// Seed accumulated state with config + Stage 0.
	Result.FinalState.Config = Config;
	Result.FinalState.Grid = Grid;

	// Stage 1
	Result.FinalState.Stage1Roads = GenerateRoads(Grid, Config);
	if (!Result.FinalState.Stage1Roads.Gate.bAllPassed)
	{
		Result.ErrorMessage = TEXT("Stage 1 gate failed (see FinalState.Stage1Roads.Gate.Checks).");
		return Result;
	}

	// Stage 2
	Result.FinalState.Stage2POIs = PlacePOIs(Grid, Result.FinalState.Stage1Roads, Config);
	if (!Result.FinalState.Stage2POIs.Gate.bAllPassed)
	{
		Result.ErrorMessage = TEXT("Stage 2 gate failed (see FinalState.Stage2POIs.Gate.Checks).");
		return Result;
	}

	// Stage 3
	Result.FinalState.Stage3Fields = PlaceFields(
		Grid, Result.FinalState.Stage1Roads, Result.FinalState.Stage2POIs, Config);
	if (!Result.FinalState.Stage3Fields.Gate.bAllPassed)
	{
		Result.ErrorMessage = TEXT("Stage 3 gate failed (see FinalState.Stage3Fields.Gate.Checks).");
		return Result;
	}

	// Stage 4
	Result.FinalState.Stage4Foliage = PlaceFoliage(
		Grid, Result.FinalState.Stage1Roads, Result.FinalState.Stage2POIs,
		Result.FinalState.Stage3Fields, Config);
	if (!Result.FinalState.Stage4Foliage.Gate.bAllPassed)
	{
		Result.ErrorMessage = TEXT("Stage 4 gate failed (see FinalState.Stage4Foliage.Gate.Checks).");
		return Result;
	}

	// Stage 5
	Result.FinalState.Stage5Railway = PlaceRailway(
		Grid, Result.FinalState.Stage1Roads, Result.FinalState.Stage2POIs,
		Result.FinalState.Stage3Fields, Result.FinalState.Stage4Foliage, Config);
	if (!Result.FinalState.Stage5Railway.Gate.bAllPassed)
	{
		Result.ErrorMessage = TEXT("Stage 5 gate failed (see FinalState.Stage5Railway.Gate.Checks).");
		return Result;
	}

	// Final Pass
	Result.FinalGate = RunFinalPass(Result.FinalState);
	if (!Result.FinalGate.bAllPassed)
	{
		Result.ErrorMessage = TEXT("Final Pass failed (see FinalGate.Checks).");
		return Result;
	}

	// Render deliverables. Each per-stage snapshot is cumulative.
	FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*Result.OutputDir);
	for (int32 S = 1; S <= 5; ++S)
	{
		const FString Path = RenderStageSnapshot(S, Result.FinalState, Result.OutputDir);
		if (!Path.IsEmpty()) { Result.OutputFiles.Add(Path); }
	}
	for (const FString& Path : RenderFinalDeliverables(Result.FinalState, Result.OutputDir))
	{
		if (!Path.IsEmpty()) { Result.OutputFiles.Add(Path); }
	}

	Result.bSuccess = (Result.OutputFiles.Num() > 0);
	if (!Result.bSuccess)
	{
		Result.ErrorMessage = TEXT("Pipeline gates passed but renderers produced no output (stubs).");
	}
	return Result;
}

FMapBlockoutPipelineResult UMapBlockoutService::RunFullPipelineForLandscape(
	const FString& LandscapeLabel, const FMapBlockoutConfig& Config)
{
	const FMapBlockoutLandcoverGrid Grid = ExportLandcoverGrid(LandscapeLabel, 120);
	if (!Grid.bSuccess)
	{
		FMapBlockoutPipelineResult Result;
		Result.ErrorMessage = FString::Printf(
			TEXT("ExportLandcoverGrid failed for '%s': %s"),
			*LandscapeLabel, *Grid.ErrorMessage);
		return Result;
	}
	return RunFullPipeline(Grid, Config);
}
