// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UMapBlockoutService.h"
#include "PythonAPI/ULandscapeService.h"
#include "MapBlockout/MapBlockoutMath.h"
#include "MapBlockout/MapBlockoutImage.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"

// Skeleton: every method compiles and returns a well-formed "not implemented"
// result. Stage 1-5 algorithms and renderers live next door in stage .cpp files.
// Spec: docs/design/map-designer-spec.md. Reference impl:
// Source/VibeUE/Tests/MapBlockout/reference/.

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
}


// =========================================================================
// Stage 0 — Landcover grid export
// =========================================================================

FMapBlockoutLandcoverGrid UMapBlockoutService::ExportLandcoverGrid(
	const FString& LandscapeLabel, int32 GridN)
{
	FMapBlockoutLandcoverGrid Result;
	Result.LandscapeLabel = LandscapeLabel;
	Result.GridN = GridN;

	// Intended implementation:
	//   1. Resolve the landscape via ULandscapeService::FindLandscapeByIdentifier
	//      (note: that helper is private; we expose ListLandscapes/GetLandscapeInfo instead).
	//   2. ListLandscapes() / GetLandscapeInfo() to confirm it exists + read world bounds.
	//   3. For each layer reported by ListLandscapes layers field:
	//        - call ExportWeightMap to a temp PNG, decode via MapBlockoutImage::LoadGrayscalePNG,
	//          downsample with MapBlockoutMath::ResampleBilinear to GridN x GridN,
	//          (optionally FlipVertical so row 0 = south, matching landcover_grid.json convention),
	//          push into Result.Layers
	//   4. ExportHeightmap to a temp PNG, decode + downsample into Result.HeightNormalized.
	//   5. Set WorldLo/WorldHi to the max symmetric half-span of the landscape's AABB.

	Result.bSuccess = false;
	Result.ErrorMessage = TEXT("ExportLandcoverGrid not yet implemented");
	return Result;
}

FString UMapBlockoutService::WriteLandcoverGridJson(
	const FMapBlockoutLandcoverGrid& Grid, const FString& OutputFilePath)
{
	if (OutputFilePath.IsEmpty()) { return FString(); }

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("N"),  Grid.GridN);
	Root->SetNumberField(TEXT("lo"), Grid.WorldLo);
	Root->SetNumberField(TEXT("hi"), Grid.WorldHi);
	Root->SetStringField(TEXT("landscape"), Grid.LandscapeLabel);

	// Each layer becomes a top-level key holding a flat array (matches the
	// contributor's landcover_grid.json layout: arrays of arrays. Stub writes a
	// flat array — port to nested when implementing for parity).
	for (const FMapBlockoutLayerMap& Layer : Grid.Layers)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		Values.Reserve(Layer.Weights.Num());
		for (float W : Layer.Weights) { Values.Add(MakeShared<FJsonValueNumber>(W)); }
		Root->SetArrayField(Layer.LayerName, Values);
	}

	FString Text;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	if (!FJsonSerializer::Serialize(Root, Writer)) { return FString(); }

	const FString Dir = FPaths::GetPath(OutputFilePath);
	if (!Dir.IsEmpty())
	{
		FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*Dir);
	}
	if (!FFileHelper::SaveStringToFile(Text, *OutputFilePath)) { return FString(); }
	return FPaths::ConvertRelativePathToFull(OutputFilePath);
}

TArray<FMapBlockoutRiver> UMapBlockoutService::ExtractRiverCenterlines(
	const FMapBlockoutMask& WaterMask, float WorldLo, float WorldHi, float MinLengthCm)
{
	// Intended impl: copy WaterMask.Cells, call MapBlockoutMath::Skeletonize, walk
	// the skeleton into polylines, simplify (Ramer-Douglas-Peucker), map cell coords
	// to world coords via [WorldLo, WorldHi], drop polylines shorter than MinLengthCm.
	return TArray<FMapBlockoutRiver>();
}


// =========================================================================
// Stage 1–5 — see UMapBlockoutService_Stages.cpp
// =========================================================================
//
// Each per-stage generator (GenerateRoads, PlacePOIs, PlaceFields, PlaceFoliage,
// PlaceRailway), the final-pass validator, the renderers, and the orchestrators
// are implemented in UMapBlockoutService_Stages.cpp to keep this file focused on
// the Stage-0 surface. Materialization lives in UMapBlockoutService_Materialize.cpp.
//
// MakeStubGate (above) is the shared placeholder used until each stage's algorithm
// lands; signatures are locked, so the contributor only needs to fill bodies.
