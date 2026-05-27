// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UMapBlockoutService.h"
#include "PythonAPI/ULandscapeService.h"

// Materializers — turn the blockout plan into real engine geometry.
// These are thin adapters over services that already exist in VibeUE:
//   - Roads / Railway  -> ULandscapeService::CreateSplinePoint + ConnectSplinePoints
//   - Fields           -> ULandscapeService::PaintLayer (creates LayerInfo if missing)
//   - POIs / Bridges   -> spawn AStaticMeshActor / AEmptyActor via UActorService
//   - Forests          -> existing UFoliageService scatter pipeline
//
// All stubs return bSuccess=false; the contributor wires the calls in.


FMapBlockoutMaterializeResult UMapBlockoutService::MaterializeRoadsAsSplines(
	const FMapBlockoutRoadNetworkResult& Roads, const FString& LandscapeLabel)
{
	FMapBlockoutMaterializeResult Result;
	Result.bSuccess = false;
	Result.ErrorMessage = TEXT("MaterializeRoadsAsSplines not yet implemented");

	// Intended impl:
	//   1. Validate Roads.bSuccess && Roads.Gate.bAllPassed.
	//   2. Group roads by Type. For each polyline:
	//        a. For each consecutive pair of points, call
	//           ULandscapeService::CreateSplinePoint(LandscapeLabel, P, Rotator::Zero,
	//                                                Road.WidthCm, ...) twice (start + end).
	//        b. Call ULandscapeService::ConnectSplinePoints(start_idx, end_idx, ...).
	//   3. Apply different SplineMeshEntry sets per type (paved mesh for Main,
	//      gravel mesh for Dirt). MeshPath provided via Config in a future iteration.
	//   4. Result.CreatedCount = total spline points emitted.
	return Result;
}

FMapBlockoutMaterializeResult UMapBlockoutService::MaterializeFieldsAsPaint(
	const FMapBlockoutFieldResult& Fields,
	const FString& LandscapeLabel, const FString& LayerName)
{
	FMapBlockoutMaterializeResult Result;
	Result.bSuccess = false;
	Result.ErrorMessage = TEXT("MaterializeFieldsAsPaint not yet implemented");

	// Intended impl:
	//   1. Confirm the target paint layer exists; if not, create it via
	//      ULandscapeMaterialService::CreateLayerInfoObject + AddLayer.
	//   2. Walk Fields.FieldMask: for every cell == 1, compute the world XY and
	//      call ULandscapeService::PaintLayer(LandscapeLabel, LayerName, X, Y,
	//                                         BrushRadiusCm, /*Strength=*/1.0f).
	//   3. Batch by row/column to minimize call count (or expose a bulk paint API
	//      on ULandscapeService — separate ticket).
	return Result;
}

FMapBlockoutMaterializeResult UMapBlockoutService::MaterializePOIsAsActors(
	const FMapBlockoutPOIResult& POIs,
	const FString& FolderPath, const FString& BuildingMeshPath)
{
	FMapBlockoutMaterializeResult Result;
	Result.bSuccess = false;
	Result.ErrorMessage = TEXT("MaterializePOIsAsActors not yet implemented");

	// Intended impl:
	//   1. For each POI: spawn an AEmptyActor at POI.Center, set folder + label
	//      ("POI_<Type>_<N>"), attach a debug sphere component sized to POI.RadiusCm.
	//   2. For each Building in the POI: spawn AStaticMeshActor under the parent.
	//      If BuildingMeshPath is empty, use AEmptyActor placeholders.
	//   3. World folder = FolderPath ("/MapBlockout/POIs/" by convention).
	return Result;
}

FMapBlockoutMaterializeResult UMapBlockoutService::MaterializeForestAsFoliage(
	const FMapBlockoutFoliageResult& Foliage,
	const FString& ForestFoliageTypePath,
	const FString& TreelineFoliageTypePath,
	const FString& ScrubFoliageTypePath)
{
	FMapBlockoutMaterializeResult Result;
	Result.bSuccess = false;
	Result.ErrorMessage = TEXT("MaterializeForestAsFoliage not yet implemented");

	// Intended impl: for each mask (Forest, Treeline, Scrub) sample instance
	// positions (Poisson-disk inside cells == 1), then call UFoliageService::
	// AddInstances(FoliageTypePath, Positions) for the matching FoliageType.
	return Result;
}

FMapBlockoutMaterializeResult UMapBlockoutService::MaterializeRailwayAndBridges(
	const FMapBlockoutRailwayResult& Railway,
	const FString& LandscapeLabel, const FString& BridgeMeshPath)
{
	FMapBlockoutMaterializeResult Result;
	Result.bSuccess = false;
	Result.ErrorMessage = TEXT("MaterializeRailwayAndBridges not yet implemented");

	// Intended impl:
	//   1. Same spline pipeline as MaterializeRoadsAsSplines but with the railway
	//      tier (Type = Railway, rail-tie mesh).
	//   2. For each Bridge: spawn AStaticMeshActor at Bridge.World, rotation =
	//      (0, YawDegrees, 0), scale.X derived from LengthCm. Use BridgeMeshPath
	//      (empty = placeholder cube).
	return Result;
}
