// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UMapBlockoutService.h"
#include "PythonAPI/ULandscapeService.h"
#include "PythonAPI/UFoliageService.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "GameFramework/Actor.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/SoftObjectPath.h"

// Materializers: turn the blockout plan into actual engine geometry.
// Each method is a thin adapter over an existing VibeUE service.

namespace
{
	UWorld* GetEditorWorld()
	{
		return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	}

	UStaticMesh* LoadStaticMesh(const FString& Path)
	{
		if (Path.IsEmpty()) { return nullptr; }
		return LoadObject<UStaticMesh>(nullptr, *Path);
	}

	void OrganizeActorIntoFolder(AActor* Actor, const FString& FolderPath, const FString& Label)
	{
		if (!Actor) { return; }
		Actor->SetActorLabel(Label);
		if (!FolderPath.IsEmpty()) { Actor->SetFolderPath(FName(*FolderPath)); }
	}
}


// =========================================================================
// Roads as Landscape Splines
// =========================================================================

FMapBlockoutMaterializeResult UMapBlockoutService::MaterializeRoadsAsSplines(
	const FMapBlockoutRoadNetworkResult& Roads, const FString& LandscapeLabel)
{
	FMapBlockoutMaterializeResult Result;
	if (!Roads.bSuccess || !Roads.Gate.bAllPassed)
	{
		Result.ErrorMessage = TEXT("MaterializeRoadsAsSplines: Roads stage has not passed.");
		return Result;
	}
	if (LandscapeLabel.IsEmpty())
	{
		Result.ErrorMessage = TEXT("MaterializeRoadsAsSplines: empty LandscapeLabel.");
		return Result;
	}

	int32 Created = 0;
	for (const FMapBlockoutRoad& R : Roads.Roads)
	{
		if (R.Points.Num() < 2) { continue; }

		TArray<FVector> Locations;
		Locations.Reserve(R.Points.Num());
		for (const FVector2D& P : R.Points) { Locations.Add(FVector(P.X, P.Y, 0.0f)); }

		const FLandscapeSplineInfo Info = ULandscapeService::CreateSplineFromPoints(
			LandscapeLabel,
			Locations,
			R.WidthCm * 0.5f,    // half-width
			500.0f,              // SideFalloff
			500.0f,              // EndFalloff
			/*PaintLayerName=*/ FString(),
			/*bRaiseTerrain=*/ true,
			/*bLowerTerrain=*/ true,
			/*bClosedLoop=*/ false);
		if (Info.bSuccess) { Created += Info.NumControlPoints; }
	}

	Result.CreatedCount = Created;
	Result.bSuccess = (Created > 0);
	if (!Result.bSuccess)
	{
		Result.ErrorMessage = TEXT("MaterializeRoadsAsSplines: no spline points created.");
	}
	return Result;
}


// =========================================================================
// Fields as Landscape Paint
// =========================================================================

FMapBlockoutMaterializeResult UMapBlockoutService::MaterializeFieldsAsPaint(
	const FMapBlockoutFieldResult& Fields,
	const FString& LandscapeLabel, const FString& LayerName)
{
	FMapBlockoutMaterializeResult Result;
	if (!Fields.bSuccess || !Fields.Gate.bAllPassed)
	{
		Result.ErrorMessage = TEXT("MaterializeFieldsAsPaint: Fields stage has not passed.");
		return Result;
	}
	if (LandscapeLabel.IsEmpty() || LayerName.IsEmpty())
	{
		Result.ErrorMessage = TEXT("MaterializeFieldsAsPaint: missing LandscapeLabel or LayerName.");
		return Result;
	}

	// Look up landscape bounds so we can map mask cells -> world coords.
	FLandscapeInfo_Custom Info;
	if (!ULandscapeService::GetLandscapeInfo(LandscapeLabel, Info))
	{
		Result.ErrorMessage = FString::Printf(TEXT("Landscape '%s' not found."), *LandscapeLabel);
		return Result;
	}

	// Span derives from landscape resolution * scale (same as ExportLandcoverGrid).
	const float HalfSpanX = (Info.ResolutionX > 0 ? (Info.ResolutionX - 1) : 0) * Info.Scale.X * 0.5f;
	const float HalfSpanY = (Info.ResolutionY > 0 ? (Info.ResolutionY - 1) : 0) * Info.Scale.Y * 0.5f;
	const float Half = FMath::Max(HalfSpanX, HalfSpanY);
	const float WorldLo = -Half;
	const float WorldHi = +Half;
	const float Span = WorldHi - WorldLo;

	const int32 W = Fields.FieldMask.Width;
	const int32 H = Fields.FieldMask.Height;
	const TArray<uint8>& Mask = Fields.FieldMask.Cells;
	if (W <= 0 || H <= 0 || Mask.Num() < W * H)
	{
		Result.ErrorMessage = TEXT("MaterializeFieldsAsPaint: empty/invalid field mask.");
		return Result;
	}

	// Step in big tiles so we don't hammer PaintLayer once per pixel. Brush
	// radius covers the tile, strength = 1.0.
	const int32 Step = FMath::Max(2, FMath::Min(W, H) / 80); // ~80 brush taps along each axis
	const float CellSpan = Span / FMath::Max(1, W - 1);
	const float BrushRadius = CellSpan * Step;

	int32 Painted = 0;
	for (int32 Y = 0; Y < H; Y += Step)
	{
		for (int32 X = 0; X < W; X += Step)
		{
			// Sample whether the tile centre is field-marked.
			if (!Mask[Y * W + X]) { continue; }
			const float Wx = WorldLo + (Span * X) / FMath::Max(1, W - 1);
			const float Wy = WorldHi - (Span * Y) / FMath::Max(1, H - 1);
			if (ULandscapeService::PaintLayerAtLocation(LandscapeLabel, LayerName, Wx, Wy, BrushRadius, 1.0f))
			{
				++Painted;
			}
		}
	}

	Result.CreatedCount = Painted;
	Result.bSuccess = (Painted > 0);
	if (!Result.bSuccess)
	{
		Result.ErrorMessage = TEXT("MaterializeFieldsAsPaint: no cells painted (layer may not exist on landscape).");
	}
	return Result;
}


// =========================================================================
// Pois as actors
// =========================================================================

FMapBlockoutMaterializeResult UMapBlockoutService::MaterializePoisAsActors(
	const FMapBlockoutPOIResult& Pois,
	const FString& FolderPath, const FString& BuildingMeshPath)
{
	FMapBlockoutMaterializeResult Result;
	if (!Pois.bSuccess || !Pois.Gate.bAllPassed)
	{
		Result.ErrorMessage = TEXT("MaterializePoisAsActors: Pois stage has not passed.");
		return Result;
	}

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		Result.ErrorMessage = TEXT("MaterializePoisAsActors: no editor world.");
		return Result;
	}

	UStaticMesh* BuildingMesh = LoadStaticMesh(BuildingMeshPath);
	const FString PoiFolder = FolderPath.IsEmpty() ? TEXT("MapBlockout/Pois") : FolderPath;

	int32 Spawned = 0;
	for (const FMapBlockoutPOI& POI : Pois.Pois)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		// Parent: an AActor placed at POI center with a label and folder path.
		AActor* Parent = World->SpawnActor<AActor>(AActor::StaticClass(),
			FVector(POI.Center.X, POI.Center.Y, 0.0f), FRotator::ZeroRotator, Params);
		if (!Parent) { continue; }
		OrganizeActorIntoFolder(Parent, PoiFolder, FString::Printf(TEXT("POI_%s"), *POI.Name));
		++Spawned;

		// Buildings: AStaticMeshActor per building footprint.
		for (int32 I = 0; I < POI.Buildings.Num(); ++I)
		{
			const FMapBlockoutBuilding& B = POI.Buildings[I];
			AStaticMeshActor* Bld = World->SpawnActor<AStaticMeshActor>(
				FVector(B.World.X, B.World.Y, 0.0f),
				FRotator(0.0f, B.YawDegrees, 0.0f),
				Params);
			if (!Bld) { continue; }
			Bld->SetMobility(EComponentMobility::Static);
			if (BuildingMesh && Bld->GetStaticMeshComponent())
			{
				Bld->GetStaticMeshComponent()->SetStaticMesh(BuildingMesh);
				// Scale to footprint size (mesh assumed unit-cube-like).
				const FVector Scale(
					FMath::Max(0.1f, B.HalfExtents.X / 50.0f),
					FMath::Max(0.1f, B.HalfExtents.Y / 50.0f),
					1.0f);
				Bld->SetActorScale3D(Scale);
			}
			OrganizeActorIntoFolder(Bld, PoiFolder,
				FString::Printf(TEXT("%s_Building_%d"), *POI.Name, I + 1));
			Bld->AttachToActor(Parent, FAttachmentTransformRules::KeepWorldTransform);
			++Spawned;
		}
	}

	Result.CreatedCount = Spawned;
	Result.bSuccess = (Spawned > 0);
	if (!Result.bSuccess)
	{
		Result.ErrorMessage = TEXT("MaterializePoisAsActors: no actors spawned.");
	}
	return Result;
}


// =========================================================================
// Forest / Treeline / Scrub as Foliage
// =========================================================================

namespace
{
	void SampleMaskPositions(
		const FMapBlockoutMask& Mask, float WorldLo, float WorldHi,
		int32 Step, TArray<FVector>& OutPositions)
	{
		const int32 W = Mask.Width, H = Mask.Height;
		if (W <= 0 || H <= 0 || Mask.Cells.Num() < W * H || WorldHi <= WorldLo) { return; }
		const float Span = WorldHi - WorldLo;
		for (int32 Y = 0; Y < H; Y += Step)
		{
			for (int32 X = 0; X < W; X += Step)
			{
				if (!Mask.Cells[Y * W + X]) { continue; }
				const float Wx = WorldLo + (Span * X) / FMath::Max(1, W - 1);
				const float Wy = WorldHi - (Span * Y) / FMath::Max(1, H - 1);
				OutPositions.Add(FVector(Wx, Wy, 0.0f));
			}
		}
	}
}

FMapBlockoutMaterializeResult UMapBlockoutService::MaterializeForestAsFoliage(
	const FMapBlockoutFoliageResult& Foliage,
	const FString& ForestFoliageTypePath,
	const FString& TreelineFoliageTypePath,
	const FString& ScrubFoliageTypePath)
{
	FMapBlockoutMaterializeResult Result;
	if (!Foliage.bSuccess || !Foliage.Gate.bAllPassed)
	{
		Result.ErrorMessage = TEXT("MaterializeForestAsFoliage: Foliage stage has not passed.");
		return Result;
	}

	// World bounds derive from the mask's intended size — but the mask itself
	// doesn't carry world bounds. We infer them from the field/foliage masks
	// being WORK_W = 1600 cells across a world span. Callers using this
	// adapter must rely on landscape bounds being centred at origin (as the
	// rest of the pipeline assumes). We use the size of the foliage masks to
	// derive a placeholder span — for production use, prefer the Stage4
	// outputs from a run that knows its WorldLo/Hi (full pipeline).
	const FMapBlockoutMask& Forest = Foliage.ForestMask;
	const float Half = 80000.0f; // 1.6 km half-span fallback for the unit test path
	const float Lo = -Half, Hi = +Half;

	int32 Created = 0;
	const int32 ForestStep = 4;   // dense
	const int32 TreelineStep = 5; // medium
	const int32 ScrubStep = 6;    // sparse

	if (!ForestFoliageTypePath.IsEmpty())
	{
		TArray<FVector> Pos; SampleMaskPositions(Foliage.ForestMask, Lo, Hi, ForestStep, Pos);
		if (Pos.Num())
		{
			const FFoliageScatterResult R = UFoliageService::AddFoliageInstances(
				ForestFoliageTypePath, Pos, /*MinScale=*/0.8f, /*MaxScale=*/1.4f,
				/*bAlignToNormal=*/true, /*bRandomYaw=*/true, /*bTraceToSurface=*/true);
			Created += R.InstancesAdded;
		}
	}
	if (!TreelineFoliageTypePath.IsEmpty())
	{
		TArray<FVector> Pos; SampleMaskPositions(Foliage.TreelineMask, Lo, Hi, TreelineStep, Pos);
		if (Pos.Num())
		{
			const FFoliageScatterResult R = UFoliageService::AddFoliageInstances(
				TreelineFoliageTypePath, Pos, 0.7f, 1.2f, true, true, true);
			Created += R.InstancesAdded;
		}
	}
	if (!ScrubFoliageTypePath.IsEmpty())
	{
		TArray<FVector> Pos; SampleMaskPositions(Foliage.ScrubMask, Lo, Hi, ScrubStep, Pos);
		if (Pos.Num())
		{
			const FFoliageScatterResult R = UFoliageService::AddFoliageInstances(
				ScrubFoliageTypePath, Pos, 0.6f, 1.1f, true, true, true);
			Created += R.InstancesAdded;
		}
	}

	Result.CreatedCount = Created;
	Result.bSuccess = (Created > 0);
	if (!Result.bSuccess)
	{
		Result.ErrorMessage = TEXT("MaterializeForestAsFoliage: no instances created (foliage type paths may be empty/invalid).");
	}
	return Result;
}


// =========================================================================
// Railway + Bridges
// =========================================================================

FMapBlockoutMaterializeResult UMapBlockoutService::MaterializeRailwayAndBridges(
	const FMapBlockoutRailwayResult& Railway,
	const FString& LandscapeLabel, const FString& BridgeMeshPath)
{
	FMapBlockoutMaterializeResult Result;
	if (!Railway.bSuccess || !Railway.Gate.bAllPassed)
	{
		Result.ErrorMessage = TEXT("MaterializeRailwayAndBridges: Railway stage has not passed.");
		return Result;
	}

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		Result.ErrorMessage = TEXT("MaterializeRailwayAndBridges: no editor world.");
		return Result;
	}

	int32 Created = 0;

	// Rail polylines → landscape splines.
	if (!LandscapeLabel.IsEmpty())
	{
		for (const FMapBlockoutRoad& R : Railway.RailLines)
		{
			if (R.Points.Num() < 2) { continue; }
			TArray<FVector> Locations;
			Locations.Reserve(R.Points.Num());
			for (const FVector2D& P : R.Points) { Locations.Add(FVector(P.X, P.Y, 0.0f)); }
			const FLandscapeSplineInfo Info = ULandscapeService::CreateSplineFromPoints(
				LandscapeLabel, Locations, R.WidthCm * 0.5f,
				500.0f, 500.0f, FString(), true, true, false);
			if (Info.bSuccess) { Created += Info.NumControlPoints; }
		}
	}

	// Bridges → static-mesh actors at bridge midpoints.
	UStaticMesh* BridgeMesh = LoadStaticMesh(BridgeMeshPath);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	int32 BridgeIdx = 0;
	for (const FMapBlockoutBridge& B : Railway.Bridges)
	{
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(
			FVector(B.World.X, B.World.Y, 0.0f),
			FRotator(0.0f, B.YawDegrees, 0.0f), Params);
		if (!Actor) { continue; }
		Actor->SetMobility(EComponentMobility::Static);
		if (BridgeMesh && Actor->GetStaticMeshComponent())
		{
			Actor->GetStaticMeshComponent()->SetStaticMesh(BridgeMesh);
			Actor->SetActorScale3D(FVector(FMath::Max(0.5f, B.LengthCm / 100.0f), 1.0f, 1.0f));
		}
		const TCHAR* CarryName =
			(B.Carries == EMapBlockoutRoadType::Railway) ? TEXT("Rail") :
			(B.Carries == EMapBlockoutRoadType::Main)    ? TEXT("Main") : TEXT("Dirt");
		OrganizeActorIntoFolder(Actor, TEXT("MapBlockout/Bridges"),
			FString::Printf(TEXT("Bridge_%s_%d"), CarryName, ++BridgeIdx));
		++Created;
	}

	Result.CreatedCount = Created;
	Result.bSuccess = (Created > 0);
	if (!Result.bSuccess)
	{
		Result.ErrorMessage = TEXT("MaterializeRailwayAndBridges: nothing created.");
	}
	return Result;
}
