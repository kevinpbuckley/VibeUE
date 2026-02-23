// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UFoliageService.h"
#include "InstancedFoliageActor.h"
#include "FoliageType.h"
#include "FoliageType_InstancedStaticMesh.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "LandscapeInfo.h"
#include "LandscapeEdit.h"
#include "LandscapeLayerInfoObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "EngineUtils.h"
#include "ScopedTransaction.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

// =================================================================
// Helper Methods
// =================================================================

UWorld* UFoliageService::GetEditorWorld()
{
	if (GEditor)
	{
		return GEditor->GetEditorWorldContext().World();
	}
	return nullptr;
}

AInstancedFoliageActor* UFoliageService::GetOrCreateFoliageActor(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}

	// Find existing IFA
	for (TActorIterator<AInstancedFoliageActor> It(World); It; ++It)
	{
		return *It;
	}

	// Create one if none exists
	return AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(World, true);
}

UFoliageType* UFoliageService::FindFoliageTypeInIFA(
	const FString& MeshOrFoliageTypePath,
	AInstancedFoliageActor* IFA)
{
	if (!IFA)
	{
		return nullptr;
	}

	// First try to load as a UFoliageType directly
	UObject* LoadedAsset = StaticLoadObject(UObject::StaticClass(), nullptr, *MeshOrFoliageTypePath);
	if (UFoliageType* FT = Cast<UFoliageType>(LoadedAsset))
	{
		// Check if this foliage type is registered in the IFA
		const TMap<UFoliageType*, TUniqueObj<FFoliageInfo>>& FoliageInfos = IFA->GetFoliageInfos();
		if (FoliageInfos.Contains(FT))
		{
			return FT;
		}
	}

	// Try to find by mesh path — iterate all foliage types in IFA
	UStaticMesh* Mesh = Cast<UStaticMesh>(LoadedAsset);
	if (!Mesh)
	{
		Mesh = LoadObject<UStaticMesh>(nullptr, *MeshOrFoliageTypePath);
	}

	if (Mesh)
	{
		const TMap<UFoliageType*, TUniqueObj<FFoliageInfo>>& FoliageInfos = IFA->GetFoliageInfos();
		for (const auto& Pair : FoliageInfos)
		{
			UFoliageType_InstancedStaticMesh* ISMT = Cast<UFoliageType_InstancedStaticMesh>(Pair.Key);
			if (ISMT && ISMT->GetStaticMesh() == Mesh)
			{
				return Pair.Key;
			}
		}
	}

	return nullptr;
}

UFoliageType* UFoliageService::FindOrCreateFoliageTypeForMesh(
	const FString& MeshOrFoliageTypePath,
	AInstancedFoliageActor* IFA)
{
	if (!IFA)
	{
		return nullptr;
	}

	// Check if already registered
	UFoliageType* Existing = FindFoliageTypeInIFA(MeshOrFoliageTypePath, IFA);
	if (Existing)
	{
		return Existing;
	}

	// Try loading as UFoliageType asset
	UFoliageType* FoliageType = LoadObject<UFoliageType>(nullptr, *MeshOrFoliageTypePath);
	if (FoliageType)
	{
		IFA->AddFoliageType(FoliageType);
		return FoliageType;
	}

	// Try loading as UStaticMesh — create a transient foliage type
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshOrFoliageTypePath);
	if (!Mesh)
	{
		UE_LOG(LogTemp, Error, TEXT("UFoliageService: Could not load asset '%s' as StaticMesh or FoliageType"), *MeshOrFoliageTypePath);
		return nullptr;
	}

	// Create a new UFoliageType_InstancedStaticMesh in the transient package
	UFoliageType_InstancedStaticMesh* NewFT = NewObject<UFoliageType_InstancedStaticMesh>(
		GetTransientPackage(), NAME_None, RF_Transactional);
	NewFT->SetStaticMesh(Mesh);

	// Register it with the IFA
	FoliageType = IFA->AddFoliageType(NewFT);
	return FoliageType;
}

bool UFoliageService::TraceToSurface(
	UWorld* World, float X, float Y,
	FVector& OutLocation, FVector& OutNormal)
{
	if (!World)
	{
		return false;
	}

	// Trace from high above down to find surface
	FVector Start(X, Y, 100000.0f);
	FVector End(X, Y, -100000.0f);

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = false;
	QueryParams.bReturnPhysicalMaterial = false;

	if (World->LineTraceSingleByChannel(HitResult, Start, End, ECC_WorldStatic, QueryParams))
	{
		OutLocation = HitResult.ImpactPoint;
		OutNormal = HitResult.ImpactNormal;
		return true;
	}

	return false;
}

// =================================================================
// Internal Scatter Implementation
// =================================================================

FFoliageScatterResult UFoliageService::ScatterInternal(
	const FString& MeshOrFoliageTypePath,
	const TArray<FVector2D>& CandidatePositions,
	int32 Count,
	float MinScale, float MaxScale,
	bool bAlignToNormal, bool bRandomYaw,
	int32 Seed,
	const FString& LandscapeNameOrLabel,
	const FString& LayerName,
	float LayerWeightThreshold)
{
	FFoliageScatterResult Result;
	Result.InstancesRequested = Count;

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		Result.ErrorMessage = TEXT("No editor world available");
		return Result;
	}

	AInstancedFoliageActor* IFA = GetOrCreateFoliageActor(World);
	if (!IFA)
	{
		Result.ErrorMessage = TEXT("Failed to get or create InstancedFoliageActor");
		return Result;
	}

	UFoliageType* FoliageType = FindOrCreateFoliageTypeForMesh(MeshOrFoliageTypePath, IFA);
	if (!FoliageType)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Could not load or create foliage type for '%s'"), *MeshOrFoliageTypePath);
		return Result;
	}

	// Optionally find landscape for layer-aware placement
	ALandscape* Landscape = nullptr;
	ULandscapeInfo* LandscapeInfo = nullptr;
	if (!LandscapeNameOrLabel.IsEmpty())
	{
		for (TActorIterator<ALandscape> It(World); It; ++It)
		{
			ALandscape* L = *It;
			if (L->GetActorLabel().Equals(LandscapeNameOrLabel, ESearchCase::IgnoreCase) ||
				L->GetName().Equals(LandscapeNameOrLabel, ESearchCase::IgnoreCase))
			{
				Landscape = L;
				LandscapeInfo = L->GetLandscapeInfo();
				break;
			}
		}
	}

	// Check if layer-aware placement is requested but landscape not found
	bool bLayerAware = !LayerName.IsEmpty() && LayerWeightThreshold > 0.0f;
	if (bLayerAware && (!Landscape || !LandscapeInfo))
	{
		Result.ErrorMessage = FString::Printf(TEXT("Layer-aware placement requires a valid landscape. '%s' not found."), *LandscapeNameOrLabel);
		return Result;
	}

	// Find target layer info for layer-aware placement
	ULandscapeLayerInfoObject* TargetLayerInfo = nullptr;
	if (bLayerAware)
	{
		for (const FLandscapeInfoLayerSettings& LayerSettings : LandscapeInfo->Layers)
		{
			if (LayerSettings.LayerInfoObj &&
				LayerSettings.LayerInfoObj->GetLayerName().ToString().Equals(LayerName, ESearchCase::IgnoreCase))
			{
				TargetLayerInfo = LayerSettings.LayerInfoObj;
				break;
			}
		}
		if (!TargetLayerInfo)
		{
			Result.ErrorMessage = FString::Printf(TEXT("Layer '%s' not found on landscape '%s'"), *LayerName, *LandscapeNameOrLabel);
			return Result;
		}
	}

	FScopedTransaction Transaction(NSLOCTEXT("FoliageService", "ScatterFoliage", "Scatter Foliage"));
	IFA->Modify();

	FRandomStream RNG(Seed != 0 ? Seed : FMath::Rand());

	// Collect valid instances
	TArray<FFoliageInstance> NewInstances;
	NewInstances.Reserve(Count);

	for (const FVector2D& Pos : CandidatePositions)
	{
		if (NewInstances.Num() >= Count)
		{
			break;
		}

		// Trace to surface
		FVector HitLocation;
		FVector HitNormal;
		if (!TraceToSurface(World, Pos.X, Pos.Y, HitLocation, HitNormal))
		{
			Result.InstancesRejected++;
			continue;
		}

		// Layer weight check
		if (bLayerAware && Landscape && LandscapeInfo)
		{
			FVector LandscapeLocation = Landscape->GetActorLocation();
			FVector LandscapeScale = Landscape->GetActorScale3D();
			int32 LocalX = FMath::RoundToInt((Pos.X - LandscapeLocation.X) / LandscapeScale.X);
			int32 LocalY = FMath::RoundToInt((Pos.Y - LandscapeLocation.Y) / LandscapeScale.Y);

			FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
			TArray<uint8> WeightData;
			WeightData.SetNumZeroed(1);
			LandscapeEdit.GetWeightData(TargetLayerInfo, LocalX, LocalY, LocalX, LocalY, WeightData.GetData(), 0);

			float Weight = WeightData[0] / 255.0f;
			if (Weight < LayerWeightThreshold)
			{
				Result.InstancesRejected++;
				continue;
			}
		}

		// Build foliage instance
		FFoliageInstance Instance;
		Instance.Location = HitLocation;

		// Scale
		float Scale = RNG.FRandRange(MinScale, MaxScale);
		Instance.DrawScale3D = FVector3f(Scale, Scale, Scale);

		// Rotation
		FRotator Rot = FRotator::ZeroRotator;
		if (bRandomYaw)
		{
			Rot.Yaw = RNG.FRandRange(0.0f, 360.0f);
		}
		if (bAlignToNormal)
		{
			// Align Z axis to surface normal
			FQuat NormalQuat = FQuat::FindBetweenNormals(FVector::UpVector, HitNormal);
			FQuat YawQuat(FVector::UpVector, FMath::DegreesToRadians(Rot.Yaw));
			Rot = (NormalQuat * YawQuat).Rotator();
		}
		Instance.Rotation = Rot;

		Instance.Flags = 0;
		Instance.PreAlignRotation = Instance.Rotation;
		Instance.ZOffset = 0.0f;

		NewInstances.Add(Instance);
	}

	// Add all instances to the IFA in one batch
	if (NewInstances.Num() > 0)
	{
		FFoliageInfo* FoliageInfo = IFA->FindInfo(FoliageType);
		if (FoliageInfo)
		{
			TArray<const FFoliageInstance*> InstancePtrs;
			InstancePtrs.Reserve(NewInstances.Num());
			for (const FFoliageInstance& Inst : NewInstances)
			{
				InstancePtrs.Add(&Inst);
			}
			FoliageInfo->AddInstances(FoliageType, InstancePtrs);
			Result.InstancesAdded = NewInstances.Num();
		}
		else
		{
			Result.ErrorMessage = TEXT("Failed to find FoliageInfo after registering type");
			return Result;
		}
	}

	Result.bSuccess = true;
	UE_LOG(LogTemp, Log, TEXT("UFoliageService::ScatterInternal: Placed %d/%d instances (%d rejected) for '%s'"),
		Result.InstancesAdded, Result.InstancesRequested, Result.InstancesRejected, *MeshOrFoliageTypePath);

	return Result;
}

// =================================================================
// Poisson Disk Sampling Helper
// =================================================================

static TArray<FVector2D> GeneratePoissonDiskSamples(
	float MinX, float MinY, float MaxX, float MaxY,
	int32 Count, FRandomStream& RNG,
	float MinDistance = 0.0f)
{
	TArray<FVector2D> Result;

	if (Count <= 0)
	{
		return Result;
	}

	float Width = MaxX - MinX;
	float Height = MaxY - MinY;

	if (Width <= 0.0f || Height <= 0.0f)
	{
		return Result;
	}

	// Calculate minimum distance between points based on area and count
	if (MinDistance <= 0.0f)
	{
		float Area = Width * Height;
		// Approximate: each point gets Area/Count space, min distance is ~0.7 of the side of that square
		MinDistance = FMath::Sqrt(Area / static_cast<float>(Count)) * 0.7f;
	}

	// Cell size for spatial grid
	float CellSize = MinDistance / FMath::Sqrt(2.0f);
	int32 GridW = FMath::CeilToInt(Width / CellSize);
	int32 GridH = FMath::CeilToInt(Height / CellSize);

	// Grid stores index into Result array, -1 = empty
	TArray<int32> Grid;
	Grid.SetNumUninitialized(GridW * GridH);
	for (int32 i = 0; i < Grid.Num(); i++)
	{
		Grid[i] = -1;
	}

	// Start with a random point
	FVector2D Initial(
		RNG.FRandRange(MinX, MaxX),
		RNG.FRandRange(MinY, MaxY));
	Result.Add(Initial);

	int32 GX = FMath::Clamp(FMath::FloorToInt((Initial.X - MinX) / CellSize), 0, GridW - 1);
	int32 GY = FMath::Clamp(FMath::FloorToInt((Initial.Y - MinY) / CellSize), 0, GridH - 1);
	Grid[GY * GridW + GX] = 0;

	TArray<int32> ActiveList;
	ActiveList.Add(0);

	int32 MaxAttempts = 30;

	while (ActiveList.Num() > 0 && Result.Num() < Count)
	{
		int32 ActiveIdx = RNG.RandRange(0, ActiveList.Num() - 1);
		int32 PointIdx = ActiveList[ActiveIdx];
		FVector2D Point = Result[PointIdx];

		bool bFound = false;
		for (int32 Attempt = 0; Attempt < MaxAttempts; Attempt++)
		{
			float Angle = RNG.FRandRange(0.0f, 2.0f * PI);
			float Dist = RNG.FRandRange(MinDistance, MinDistance * 2.0f);
			FVector2D Candidate(
				Point.X + Dist * FMath::Cos(Angle),
				Point.Y + Dist * FMath::Sin(Angle));

			// Bounds check
			if (Candidate.X < MinX || Candidate.X > MaxX ||
				Candidate.Y < MinY || Candidate.Y > MaxY)
			{
				continue;
			}

			int32 CandGX = FMath::Clamp(FMath::FloorToInt((Candidate.X - MinX) / CellSize), 0, GridW - 1);
			int32 CandGY = FMath::Clamp(FMath::FloorToInt((Candidate.Y - MinY) / CellSize), 0, GridH - 1);

			// Check neighbors in a 5x5 grid around the candidate
			bool bTooClose = false;
			for (int32 NY = FMath::Max(0, CandGY - 2); NY <= FMath::Min(GridH - 1, CandGY + 2) && !bTooClose; NY++)
			{
				for (int32 NX = FMath::Max(0, CandGX - 2); NX <= FMath::Min(GridW - 1, CandGX + 2) && !bTooClose; NX++)
				{
					int32 NeighborIdx = Grid[NY * GridW + NX];
					if (NeighborIdx >= 0)
					{
						float D = FVector2D::Distance(Candidate, Result[NeighborIdx]);
						if (D < MinDistance)
						{
							bTooClose = true;
						}
					}
				}
			}

			if (!bTooClose)
			{
				int32 NewIdx = Result.Num();
				Result.Add(Candidate);
				Grid[CandGY * GridW + CandGX] = NewIdx;
				ActiveList.Add(NewIdx);
				bFound = true;
				break;
			}
		}

		if (!bFound)
		{
			ActiveList.RemoveAtSwap(ActiveIdx);
		}
	}

	// If Poisson didn't generate enough points (can happen in tight spaces),
	// fill with random points
	int32 FallbackAttempts = 0;
	while (Result.Num() < Count && FallbackAttempts < Count * 10)
	{
		FallbackAttempts++;
		FVector2D Candidate(
			RNG.FRandRange(MinX, MaxX),
			RNG.FRandRange(MinY, MaxY));
		Result.Add(Candidate);
	}

	return Result;
}

// =================================================================
// Discovery
// =================================================================

TArray<FVibeUEFoliageTypeInfo> UFoliageService::ListFoliageTypes()
{
	TArray<FVibeUEFoliageTypeInfo> Result;

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("UFoliageService::ListFoliageTypes: No editor world available"));
		return Result;
	}

	for (TActorIterator<AInstancedFoliageActor> It(World); It; ++It)
	{
		AInstancedFoliageActor* IFA = *It;
		const TMap<UFoliageType*, TUniqueObj<FFoliageInfo>>& FoliageInfos = IFA->GetFoliageInfos();

		for (const auto& Pair : FoliageInfos)
		{
			UFoliageType* FT = Pair.Key;
			const FFoliageInfo& Info = Pair.Value.Get();

			FVibeUEFoliageTypeInfo TypeInfo;
			TypeInfo.FoliageTypeName = FT->GetName();
			TypeInfo.InstanceCount = Info.Instances.Num();

			if (UFoliageType_InstancedStaticMesh* ISMT = Cast<UFoliageType_InstancedStaticMesh>(FT))
			{
				if (ISMT->GetStaticMesh())
				{
					TypeInfo.MeshPath = ISMT->GetStaticMesh()->GetPathName();
				}
			}

			TypeInfo.FoliageTypePath = FT->GetPathName();
			Result.Add(TypeInfo);
		}
	}

	return Result;
}

int32 UFoliageService::GetInstanceCount(const FString& MeshOrFoliageTypePath)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return -1;
	}

	for (TActorIterator<AInstancedFoliageActor> It(World); It; ++It)
	{
		AInstancedFoliageActor* IFA = *It;
		UFoliageType* FT = FindFoliageTypeInIFA(MeshOrFoliageTypePath, IFA);
		if (FT)
		{
			const TMap<UFoliageType*, TUniqueObj<FFoliageInfo>>& FoliageInfos = IFA->GetFoliageInfos();
			const TUniqueObj<FFoliageInfo>* FoundInfo = FoliageInfos.Find(FT);
			if (FoundInfo)
			{
				return FoundInfo->Get().Instances.Num();
			}
		}
	}

	return -1;
}

// =================================================================
// Foliage Type Management
// =================================================================

FFoliageTypeCreateResult UFoliageService::CreateFoliageType(
	const FString& MeshPath,
	const FString& SavePath,
	const FString& AssetName,
	float MinScale,
	float MaxScale,
	bool bAlignToNormal,
	float AlignToNormalMaxAngle,
	float GroundSlopeMaxAngle,
	float CullDistanceMax)
{
	FFoliageTypeCreateResult Result;

	// Load the static mesh
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
	if (!Mesh)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Could not load static mesh '%s'"), *MeshPath);
		UE_LOG(LogTemp, Error, TEXT("UFoliageService::CreateFoliageType: %s"), *Result.ErrorMessage);
		return Result;
	}

	// Create the package
	FString FullPath = SavePath / AssetName;
	FString PackageName = FullPath;
	if (!PackageName.StartsWith(TEXT("/")))
	{
		PackageName = TEXT("/") + PackageName;
	}

	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Failed to create package at '%s'"), *PackageName);
		return Result;
	}

	// Create the foliage type asset
	UFoliageType_InstancedStaticMesh* FoliageType = NewObject<UFoliageType_InstancedStaticMesh>(
		Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);

	if (!FoliageType)
	{
		Result.ErrorMessage = TEXT("Failed to create UFoliageType_InstancedStaticMesh");
		return Result;
	}

	FoliageType->SetStaticMesh(Mesh);

	// Configure properties
	FoliageType->Scaling = EFoliageScaling::Uniform;
	FoliageType->ScaleX = FFloatInterval(MinScale, MaxScale);
	FoliageType->ScaleY = FFloatInterval(MinScale, MaxScale);
	FoliageType->ScaleZ = FFloatInterval(MinScale, MaxScale);
	FoliageType->AlignToNormal = bAlignToNormal;
	FoliageType->AlignMaxAngle = AlignToNormalMaxAngle;
	FoliageType->GroundSlopeAngle = FFloatInterval(0.0f, GroundSlopeMaxAngle);
	FoliageType->CullDistance = FInt32Interval(0, FMath::RoundToInt(CullDistanceMax));
	FoliageType->RandomYaw = true;

	// Mark dirty and save
	FoliageType->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(FoliageType);

	// Save the package
	FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(Package, FoliageType, *PackageFileName, SaveArgs);

	Result.bSuccess = true;
	Result.AssetPath = FoliageType->GetPathName();

	UE_LOG(LogTemp, Log, TEXT("UFoliageService::CreateFoliageType: Created '%s' with mesh '%s'"),
		*Result.AssetPath, *MeshPath);

	return Result;
}

bool UFoliageService::SetFoliageTypeProperty(
	const FString& FoliageTypePath,
	const FString& PropertyName,
	const FString& Value)
{
	UFoliageType* FoliageType = LoadObject<UFoliageType>(nullptr, *FoliageTypePath);
	if (!FoliageType)
	{
		UE_LOG(LogTemp, Error, TEXT("UFoliageService::SetFoliageTypeProperty: Could not load '%s'"), *FoliageTypePath);
		return false;
	}

	FProperty* Property = FoliageType->GetClass()->FindPropertyByName(*PropertyName);
	if (!Property)
	{
		UE_LOG(LogTemp, Error, TEXT("UFoliageService::SetFoliageTypeProperty: Property '%s' not found on UFoliageType"), *PropertyName);
		return false;
	}

	void* PropertyAddr = Property->ContainerPtrToValuePtr<void>(FoliageType);
	if (!Property->ImportText_Direct(*Value, PropertyAddr, FoliageType, PPF_None))
	{
		UE_LOG(LogTemp, Error, TEXT("UFoliageService::SetFoliageTypeProperty: Failed to set '%s' to '%s'"), *PropertyName, *Value);
		return false;
	}

	FoliageType->MarkPackageDirty();
	return true;
}

FString UFoliageService::GetFoliageTypeProperty(
	const FString& FoliageTypePath,
	const FString& PropertyName)
{
	UFoliageType* FoliageType = LoadObject<UFoliageType>(nullptr, *FoliageTypePath);
	if (!FoliageType)
	{
		UE_LOG(LogTemp, Error, TEXT("UFoliageService::GetFoliageTypeProperty: Could not load '%s'"), *FoliageTypePath);
		return FString();
	}

	FProperty* Property = FoliageType->GetClass()->FindPropertyByName(*PropertyName);
	if (!Property)
	{
		UE_LOG(LogTemp, Error, TEXT("UFoliageService::GetFoliageTypeProperty: Property '%s' not found"), *PropertyName);
		return FString();
	}

	FString Result;
	void* PropertyAddr = Property->ContainerPtrToValuePtr<void>(FoliageType);
	Property->ExportTextItem_Direct(Result, PropertyAddr, nullptr, FoliageType, PPF_None);
	return Result;
}

// =================================================================
// Placement
// =================================================================

FFoliageScatterResult UFoliageService::ScatterFoliage(
	const FString& MeshOrFoliageTypePath,
	float WorldCenterX,
	float WorldCenterY,
	float Radius,
	int32 Count,
	float MinScale,
	float MaxScale,
	bool bAlignToNormal,
	bool bRandomYaw,
	int32 Seed,
	const FString& LandscapeNameOrLabel)
{
	if (Count <= 0)
	{
		FFoliageScatterResult Result;
		Result.bSuccess = true;
		Result.InstancesRequested = 0;
		return Result;
	}

	if (Radius <= 0.0f)
	{
		FFoliageScatterResult Result;
		Result.ErrorMessage = TEXT("Radius must be > 0");
		return Result;
	}

	FRandomStream RNG(Seed != 0 ? Seed : FMath::Rand());

	// Generate Poisson disk samples within the bounding box, then filter to circle
	TArray<FVector2D> AllSamples = GeneratePoissonDiskSamples(
		WorldCenterX - Radius, WorldCenterY - Radius,
		WorldCenterX + Radius, WorldCenterY + Radius,
		Count * 2, // Over-generate since we'll filter to circle
		RNG);

	// Filter to circular region
	TArray<FVector2D> CircleSamples;
	CircleSamples.Reserve(Count);
	float RadiusSq = Radius * Radius;
	for (const FVector2D& Sample : AllSamples)
	{
		float DX = Sample.X - WorldCenterX;
		float DY = Sample.Y - WorldCenterY;
		if (DX * DX + DY * DY <= RadiusSq)
		{
			CircleSamples.Add(Sample);
		}
	}

	return ScatterInternal(MeshOrFoliageTypePath, CircleSamples, Count,
		MinScale, MaxScale, bAlignToNormal, bRandomYaw, Seed, LandscapeNameOrLabel);
}

FFoliageScatterResult UFoliageService::ScatterFoliageRect(
	const FString& MeshOrFoliageTypePath,
	float WorldMinX,
	float WorldMinY,
	float WorldMaxX,
	float WorldMaxY,
	int32 Count,
	float MinScale,
	float MaxScale,
	bool bAlignToNormal,
	bool bRandomYaw,
	int32 Seed,
	const FString& LandscapeNameOrLabel)
{
	if (Count <= 0)
	{
		FFoliageScatterResult Result;
		Result.bSuccess = true;
		Result.InstancesRequested = 0;
		return Result;
	}

	FRandomStream RNG(Seed != 0 ? Seed : FMath::Rand());

	TArray<FVector2D> Samples = GeneratePoissonDiskSamples(
		WorldMinX, WorldMinY, WorldMaxX, WorldMaxY,
		Count, RNG);

	return ScatterInternal(MeshOrFoliageTypePath, Samples, Count,
		MinScale, MaxScale, bAlignToNormal, bRandomYaw, Seed, LandscapeNameOrLabel);
}

FFoliageScatterResult UFoliageService::AddFoliageInstances(
	const FString& MeshOrFoliageTypePath,
	const TArray<FVector>& Locations,
	float MinScale,
	float MaxScale,
	bool bAlignToNormal,
	bool bRandomYaw,
	bool bTraceToSurface)
{
	FFoliageScatterResult Result;
	Result.InstancesRequested = Locations.Num();

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		Result.ErrorMessage = TEXT("No editor world available");
		return Result;
	}

	AInstancedFoliageActor* IFA = GetOrCreateFoliageActor(World);
	if (!IFA)
	{
		Result.ErrorMessage = TEXT("Failed to get or create InstancedFoliageActor");
		return Result;
	}

	UFoliageType* FoliageType = FindOrCreateFoliageTypeForMesh(MeshOrFoliageTypePath, IFA);
	if (!FoliageType)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Could not load or create foliage type for '%s'"), *MeshOrFoliageTypePath);
		return Result;
	}

	FScopedTransaction Transaction(NSLOCTEXT("FoliageService", "AddFoliageInstances", "Add Foliage Instances"));
	IFA->Modify();

	FRandomStream RNG(FMath::Rand());
	TArray<FFoliageInstance> NewInstances;
	NewInstances.Reserve(Locations.Num());

	for (const FVector& Location : Locations)
	{
		FVector FinalLocation = Location;
		FVector SurfaceNormal = FVector::UpVector;

		if (bTraceToSurface)
		{
			FVector HitLocation, HitNormal;
			if (TraceToSurface(World, Location.X, Location.Y, HitLocation, HitNormal))
			{
				FinalLocation = HitLocation;
				SurfaceNormal = HitNormal;
			}
			else
			{
				Result.InstancesRejected++;
				continue;
			}
		}

		FFoliageInstance Instance;
		Instance.Location = FinalLocation;

		float Scale = RNG.FRandRange(MinScale, MaxScale);
		Instance.DrawScale3D = FVector3f(Scale, Scale, Scale);

		FRotator Rot = FRotator::ZeroRotator;
		if (bRandomYaw)
		{
			Rot.Yaw = RNG.FRandRange(0.0f, 360.0f);
		}
		if (bAlignToNormal)
		{
			FQuat NormalQuat = FQuat::FindBetweenNormals(FVector::UpVector, SurfaceNormal);
			FQuat YawQuat(FVector::UpVector, FMath::DegreesToRadians(Rot.Yaw));
			Rot = (NormalQuat * YawQuat).Rotator();
		}
		Instance.Rotation = Rot;
		Instance.PreAlignRotation = Instance.Rotation;
		Instance.Flags = 0;
		Instance.ZOffset = 0.0f;

		NewInstances.Add(Instance);
	}

	if (NewInstances.Num() > 0)
	{
		FFoliageInfo* FoliageInfo = IFA->FindInfo(FoliageType);
		if (FoliageInfo)
		{
			TArray<const FFoliageInstance*> InstancePtrs;
			InstancePtrs.Reserve(NewInstances.Num());
			for (const FFoliageInstance& Inst : NewInstances)
			{
				InstancePtrs.Add(&Inst);
			}
			FoliageInfo->AddInstances(FoliageType, InstancePtrs);
			Result.InstancesAdded = NewInstances.Num();
		}
	}

	Result.bSuccess = true;
	UE_LOG(LogTemp, Log, TEXT("UFoliageService::AddFoliageInstances: Placed %d/%d instances for '%s'"),
		Result.InstancesAdded, Result.InstancesRequested, *MeshOrFoliageTypePath);

	return Result;
}

// =================================================================
// Layer-Aware Placement
// =================================================================

FFoliageScatterResult UFoliageService::ScatterFoliageOnLayer(
	const FString& MeshOrFoliageTypePath,
	const FString& LandscapeNameOrLabel,
	const FString& LayerName,
	int32 Count,
	float MinScale,
	float MaxScale,
	float LayerWeightThreshold,
	bool bAlignToNormal,
	bool bRandomYaw,
	int32 Seed)
{
	if (Count <= 0)
	{
		FFoliageScatterResult Result;
		Result.bSuccess = true;
		Result.InstancesRequested = 0;
		return Result;
	}

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		FFoliageScatterResult Result;
		Result.ErrorMessage = TEXT("No editor world available");
		return Result;
	}

	// Find the landscape to determine scatter bounds
	ALandscape* Landscape = nullptr;
	for (TActorIterator<ALandscape> It(World); It; ++It)
	{
		ALandscape* L = *It;
		if (L->GetActorLabel().Equals(LandscapeNameOrLabel, ESearchCase::IgnoreCase) ||
			L->GetName().Equals(LandscapeNameOrLabel, ESearchCase::IgnoreCase))
		{
			Landscape = L;
			break;
		}
	}

	if (!Landscape)
	{
		FFoliageScatterResult Result;
		Result.ErrorMessage = FString::Printf(TEXT("Landscape '%s' not found"), *LandscapeNameOrLabel);
		return Result;
	}

	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
	if (!LandscapeInfo)
	{
		FFoliageScatterResult Result;
		Result.ErrorMessage = TEXT("No landscape info available");
		return Result;
	}

	// Get landscape bounds in world space
	int32 MinLX, MinLY, MaxLX, MaxLY;
	if (!LandscapeInfo->GetLandscapeExtent(MinLX, MinLY, MaxLX, MaxLY))
	{
		FFoliageScatterResult Result;
		Result.ErrorMessage = TEXT("Could not get landscape extent");
		return Result;
	}

	FVector LandscapeLocation = Landscape->GetActorLocation();
	FVector LandscapeScale = Landscape->GetActorScale3D();

	float WorldMinX = LandscapeLocation.X + MinLX * LandscapeScale.X;
	float WorldMinY = LandscapeLocation.Y + MinLY * LandscapeScale.Y;
	float WorldMaxX = LandscapeLocation.X + MaxLX * LandscapeScale.X;
	float WorldMaxY = LandscapeLocation.Y + MaxLY * LandscapeScale.Y;

	FRandomStream RNG(Seed != 0 ? Seed : FMath::Rand());

	// Over-generate candidates since many will be rejected by layer weight check
	int32 OverGenerateCount = Count * 4;
	TArray<FVector2D> Samples = GeneratePoissonDiskSamples(
		WorldMinX, WorldMinY, WorldMaxX, WorldMaxY,
		OverGenerateCount, RNG);

	return ScatterInternal(MeshOrFoliageTypePath, Samples, Count,
		MinScale, MaxScale, bAlignToNormal, bRandomYaw, Seed, LandscapeNameOrLabel,
		LayerName, LayerWeightThreshold);
}

// =================================================================
// Removal
// =================================================================

FFoliageRemoveResult UFoliageService::RemoveFoliageInRadius(
	const FString& MeshOrFoliageTypePath,
	float WorldCenterX,
	float WorldCenterY,
	float Radius)
{
	FFoliageRemoveResult Result;

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		Result.ErrorMessage = TEXT("No editor world available");
		return Result;
	}

	float RadiusSq = Radius * Radius;

	for (TActorIterator<AInstancedFoliageActor> It(World); It; ++It)
	{
		AInstancedFoliageActor* IFA = *It;
		UFoliageType* FT = FindFoliageTypeInIFA(MeshOrFoliageTypePath, IFA);
		if (!FT)
		{
			continue;
		}

		FFoliageInfo* InfoPtr = IFA->FindInfo(FT);
		if (!InfoPtr)
		{
			continue;
		}

		FScopedTransaction Transaction(NSLOCTEXT("FoliageService", "RemoveFoliageInRadius", "Remove Foliage In Radius"));
		IFA->Modify();

		FFoliageInfo& Info = *InfoPtr;
		TArray<int32> IndicesToRemove;

		for (int32 i = 0; i < Info.Instances.Num(); i++)
		{
			const FFoliageInstance& Instance = Info.Instances[i];
			float DX = Instance.Location.X - WorldCenterX;
			float DY = Instance.Location.Y - WorldCenterY;
			if (DX * DX + DY * DY <= RadiusSq)
			{
				IndicesToRemove.Add(i);
			}
		}

		if (IndicesToRemove.Num() > 0)
		{
			Info.RemoveInstances(IndicesToRemove, true);
			Result.InstancesRemoved += IndicesToRemove.Num();
		}
	}

	Result.bSuccess = true;
	UE_LOG(LogTemp, Log, TEXT("UFoliageService::RemoveFoliageInRadius: Removed %d instances of '%s' in radius %.0f at (%.0f, %.0f)"),
		Result.InstancesRemoved, *MeshOrFoliageTypePath, Radius, WorldCenterX, WorldCenterY);

	return Result;
}

FFoliageRemoveResult UFoliageService::RemoveAllFoliageOfType(
	const FString& MeshOrFoliageTypePath)
{
	FFoliageRemoveResult Result;

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		Result.ErrorMessage = TEXT("No editor world available");
		return Result;
	}

	for (TActorIterator<AInstancedFoliageActor> It(World); It; ++It)
	{
		AInstancedFoliageActor* IFA = *It;
		UFoliageType* FT = FindFoliageTypeInIFA(MeshOrFoliageTypePath, IFA);
		if (!FT)
		{
			continue;
		}

		FFoliageInfo* InfoPtr = IFA->FindInfo(FT);
		if (!InfoPtr)
		{
			continue;
		}

		FScopedTransaction Transaction(NSLOCTEXT("FoliageService", "RemoveAllOfType", "Remove All Foliage Of Type"));
		IFA->Modify();

		FFoliageInfo& Info = *InfoPtr;
		int32 Count = Info.Instances.Num();

		TArray<int32> AllIndices;
		AllIndices.Reserve(Count);
		for (int32 i = 0; i < Count; i++)
		{
			AllIndices.Add(i);
		}

		if (AllIndices.Num() > 0)
		{
			Info.RemoveInstances(AllIndices, true);
			Result.InstancesRemoved += Count;
		}
	}

	Result.bSuccess = true;
	UE_LOG(LogTemp, Log, TEXT("UFoliageService::RemoveAllFoliageOfType: Removed %d instances of '%s'"),
		Result.InstancesRemoved, *MeshOrFoliageTypePath);

	return Result;
}

FFoliageRemoveResult UFoliageService::ClearAllFoliage()
{
	FFoliageRemoveResult Result;

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		Result.ErrorMessage = TEXT("No editor world available");
		return Result;
	}

	for (TActorIterator<AInstancedFoliageActor> It(World); It; ++It)
	{
		AInstancedFoliageActor* IFA = *It;

		FScopedTransaction Transaction(NSLOCTEXT("FoliageService", "ClearAllFoliage", "Clear All Foliage"));
		IFA->Modify();

		// Collect all types first (can't modify while iterating const map)
		TArray<UFoliageType*> TypesToClear;
		{
			const TMap<UFoliageType*, TUniqueObj<FFoliageInfo>>& FoliageInfos = IFA->GetFoliageInfos();
			for (const auto& Pair : FoliageInfos)
			{
				TypesToClear.Add(Pair.Key);
			}
		}

		for (UFoliageType* FT : TypesToClear)
		{
			FFoliageInfo* InfoPtr = IFA->FindInfo(FT);
			if (InfoPtr)
			{
				int32 Count = InfoPtr->Instances.Num();

				TArray<int32> AllIndices;
				AllIndices.Reserve(Count);
				for (int32 i = 0; i < Count; i++)
				{
					AllIndices.Add(i);
				}

				if (AllIndices.Num() > 0)
				{
					InfoPtr->RemoveInstances(AllIndices, true);
					Result.InstancesRemoved += Count;
				}
			}
		}
	}

	Result.bSuccess = true;
	UE_LOG(LogTemp, Log, TEXT("UFoliageService::ClearAllFoliage: Removed %d total instances"),
		Result.InstancesRemoved);

	return Result;
}

// =================================================================
// Query
// =================================================================

FFoliageQueryResult UFoliageService::GetFoliageInRadius(
	const FString& MeshOrFoliageTypePath,
	float WorldCenterX,
	float WorldCenterY,
	float Radius,
	int32 MaxResults)
{
	FFoliageQueryResult Result;

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		Result.ErrorMessage = TEXT("No editor world available");
		return Result;
	}

	float RadiusSq = Radius * Radius;

	for (TActorIterator<AInstancedFoliageActor> It(World); It; ++It)
	{
		AInstancedFoliageActor* IFA = *It;
		UFoliageType* FT = FindFoliageTypeInIFA(MeshOrFoliageTypePath, IFA);
		if (!FT)
		{
			continue;
		}

		const TMap<UFoliageType*, TUniqueObj<FFoliageInfo>>& FoliageInfos = IFA->GetFoliageInfos();
		const TUniqueObj<FFoliageInfo>* FoundInfo = FoliageInfos.Find(FT);
		if (!FoundInfo)
		{
			continue;
		}

		const FFoliageInfo& Info = FoundInfo->Get();
		Result.TotalInstances = 0;

		for (int32 i = 0; i < Info.Instances.Num(); i++)
		{
			const FFoliageInstance& Instance = Info.Instances[i];
			float DX = Instance.Location.X - WorldCenterX;
			float DY = Instance.Location.Y - WorldCenterY;
			if (DX * DX + DY * DY <= RadiusSq)
			{
				Result.TotalInstances++;
				if (Result.Instances.Num() < MaxResults)
				{
					FFoliageInstanceInfo InstInfo;
					InstInfo.Location = Instance.Location;
					InstInfo.Rotation = Instance.Rotation;
					InstInfo.Scale = FVector(Instance.DrawScale3D.X, Instance.DrawScale3D.Y, Instance.DrawScale3D.Z);
					InstInfo.InstanceIndex = i;
					Result.Instances.Add(InstInfo);
				}
			}
		}
	}

	Result.bSuccess = true;
	return Result;
}

// =================================================================
// Existence Checks
// =================================================================

bool UFoliageService::FoliageTypeExists(const FString& AssetPath)
{
	UObject* Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
	if (!Asset)
	{
		return false;
	}
	return Cast<UFoliageType>(Asset) != nullptr || Cast<UStaticMesh>(Asset) != nullptr;
}

bool UFoliageService::HasFoliageInstances(const FString& MeshOrFoliageTypePath)
{
	int32 Count = GetInstanceCount(MeshOrFoliageTypePath);
	return Count > 0;
}
