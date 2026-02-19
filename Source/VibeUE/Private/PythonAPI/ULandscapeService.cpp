// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/ULandscapeService.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "LandscapeInfo.h"
#include "LandscapeComponent.h"
#include "LandscapeEdit.h"
#include "LandscapeEditorUtils.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "LandscapeDataAccess.h"
#include "EditorAssetLibrary.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "ScopedTransaction.h"

// =================================================================
// Helper Methods
// =================================================================

UWorld* ULandscapeService::GetEditorWorld()
{
	if (GEditor)
	{
		return GEditor->GetEditorWorldContext().World();
	}
	return nullptr;
}

ALandscape* ULandscapeService::FindLandscapeByIdentifier(const FString& NameOrLabel)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<ALandscape> It(World); It; ++It)
	{
		ALandscape* Landscape = *It;
		if (Landscape->GetActorLabel().Equals(NameOrLabel, ESearchCase::IgnoreCase) ||
			Landscape->GetName().Equals(NameOrLabel, ESearchCase::IgnoreCase))
		{
			return Landscape;
		}
	}

	// Also check ALandscapeProxy in case it's a streaming proxy
	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		ALandscapeProxy* Proxy = *It;
		if (Proxy->GetActorLabel().Equals(NameOrLabel, ESearchCase::IgnoreCase) ||
			Proxy->GetName().Equals(NameOrLabel, ESearchCase::IgnoreCase))
		{
			ALandscape* AsLandscape = Cast<ALandscape>(Proxy);
			if (AsLandscape)
			{
				return AsLandscape;
			}
		}
	}

	return nullptr;
}

ULandscapeInfo* ULandscapeService::GetLandscapeInfoForActor(ALandscapeProxy* Landscape)
{
	if (!Landscape)
	{
		return nullptr;
	}
	return Landscape->GetLandscapeInfo();
}

void ULandscapeService::UpdateLandscapeAfterHeightEdit(ALandscapeProxy* Landscape)
{
	if (!Landscape)
	{
		return;
	}

	// Mark all landscape components dirty so they rebuild their collision and render data.
	// This is necessary after modifying heightmap data via FLandscapeEditDataInterface,
	// otherwise line traces, height queries via GetHeightAtLocation, and visual rendering
	// will still use the old data.
	for (ULandscapeComponent* Component : Landscape->LandscapeComponents)
	{
		if (Component)
		{
			// Recreate collision for this component
			ULandscapeHeightfieldCollisionComponent* CollisionComp = Component->GetCollisionComponent();
			if (CollisionComp)
			{
				CollisionComp->RecreateCollision();
			}
			Component->MarkRenderStateDirty();
			Component->UpdateComponentToWorld();
		}
	}
}

void ULandscapeService::PopulateLandscapeInfo(ALandscapeProxy* Landscape, FLandscapeInfo_Custom& OutInfo)
{
	if (!Landscape)
	{
		return;
	}

	OutInfo.ActorName = Landscape->GetName();
	OutInfo.ActorLabel = Landscape->GetActorLabel();
	OutInfo.Location = Landscape->GetActorLocation();
	OutInfo.Rotation = Landscape->GetActorRotation();
	OutInfo.Scale = Landscape->GetActorScale3D();
	OutInfo.ComponentSizeQuads = Landscape->ComponentSizeQuads;
	OutInfo.SubsectionSizeQuads = Landscape->SubsectionSizeQuads;
	OutInfo.NumSubsections = Landscape->NumSubsections;
	OutInfo.NumComponents = Landscape->LandscapeComponents.Num();

	// Calculate overall resolution
	int32 MinX = MAX_int32, MinY = MAX_int32, MaxX = MIN_int32, MaxY = MIN_int32;
	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	if (Info)
	{
		Info->GetLandscapeExtent(MinX, MinY, MaxX, MaxY);
		OutInfo.ResolutionX = MaxX - MinX + 1;
		OutInfo.ResolutionY = MaxY - MinY + 1;
	}

	// Material
	if (Landscape->GetLandscapeMaterial())
	{
		OutInfo.MaterialPath = Landscape->GetLandscapeMaterial()->GetPathName();
	}

	// Layer info
	if (Info)
	{
		for (const FLandscapeInfoLayerSettings& LayerSettings : Info->Layers)
		{
			FLandscapeLayerInfo_Custom LayerInfo;
			if (LayerSettings.LayerInfoObj)
			{
				LayerInfo.LayerName = LayerSettings.LayerInfoObj->GetLayerName().ToString();
				LayerInfo.LayerInfoPath = LayerSettings.LayerInfoObj->GetPathName();
				LayerInfo.bIsWeightBlended = LayerSettings.LayerInfoObj->GetBlendMethod() != ELandscapeTargetLayerBlendMethod::None;
			}
			else
			{
				LayerInfo.LayerName = LayerSettings.GetLayerName().ToString();
			}
			OutInfo.Layers.Add(LayerInfo);
		}
	}
}

// =================================================================
// Discovery Operations
// =================================================================

TArray<FLandscapeInfo_Custom> ULandscapeService::ListLandscapes()
{
	TArray<FLandscapeInfo_Custom> Result;

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::ListLandscapes: No editor world available"));
		return Result;
	}

	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		FLandscapeInfo_Custom Info;
		PopulateLandscapeInfo(*It, Info);
		Result.Add(Info);
	}

	return Result;
}

bool ULandscapeService::GetLandscapeInfo(const FString& LandscapeNameOrLabel, FLandscapeInfo_Custom& OutInfo)
{
	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::GetLandscapeInfo: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return false;
	}

	PopulateLandscapeInfo(Landscape, OutInfo);
	return true;
}

// =================================================================
// Lifecycle Operations
// =================================================================

FLandscapeCreateResult ULandscapeService::CreateLandscape(
	FVector Location,
	FRotator Rotation,
	FVector Scale,
	int32 SectionsPerComponent,
	int32 QuadsPerSection,
	int32 ComponentCountX,
	int32 ComponentCountY,
	const FString& LandscapeLabel)
{
	FLandscapeCreateResult Result;

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		Result.ErrorMessage = TEXT("No editor world available");
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::CreateLandscape: %s"), *Result.ErrorMessage);
		return Result;
	}

	// Validate parameters
	TArray<int32> ValidQuadSizes = { 7, 15, 31, 63, 127, 255 };
	if (!ValidQuadSizes.Contains(QuadsPerSection))
	{
		Result.ErrorMessage = FString::Printf(TEXT("Invalid QuadsPerSection: %d. Must be one of: 7, 15, 31, 63, 127, 255"), QuadsPerSection);
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::CreateLandscape: %s"), *Result.ErrorMessage);
		return Result;
	}

	if (SectionsPerComponent < 1 || SectionsPerComponent > 2)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Invalid SectionsPerComponent: %d. Must be 1 or 2"), SectionsPerComponent);
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::CreateLandscape: %s"), *Result.ErrorMessage);
		return Result;
	}

	if (ComponentCountX < 1 || ComponentCountY < 1)
	{
		Result.ErrorMessage = TEXT("ComponentCountX and ComponentCountY must be >= 1");
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::CreateLandscape: %s"), *Result.ErrorMessage);
		return Result;
	}

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeService", "CreateLandscape", "Create Landscape"));

	// Calculate total resolution
	int32 ComponentSizeQuads = QuadsPerSection * SectionsPerComponent;
	int32 SizeX = ComponentCountX * ComponentSizeQuads + 1;
	int32 SizeY = ComponentCountY * ComponentSizeQuads + 1;

	// Create flat heightmap data (mid-height = 32768 for uint16)
	TArray<uint16> HeightData;
	HeightData.SetNumZeroed(SizeX * SizeY);
	for (int32 i = 0; i < HeightData.Num(); i++)
	{
		HeightData[i] = 32768; // Mid-height (flat terrain)
	}

	// Create the landscape
	TMap<FGuid, TArray<uint16>> HeightDataPerLayers;
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayerDataPerLayers;

	// IMPORTANT: Import() internally looks up height data using FGuid() (default/empty GUID),
	// NOT the landscape GUID parameter. The InGuid param is only used for SetLandscapeGuid().
	FGuid LandscapeGuid = FGuid::NewGuid();
	HeightDataPerLayers.Add(FGuid(), MoveTemp(HeightData));
	MaterialLayerDataPerLayers.Add(FGuid(), TArray<FLandscapeImportLayerInfo>());

	ALandscape* NewLandscape = World->SpawnActor<ALandscape>(Location, Rotation);
	if (!NewLandscape)
	{
		Result.ErrorMessage = TEXT("Failed to spawn landscape actor");
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::CreateLandscape: %s"), *Result.ErrorMessage);
		return Result;
	}

	NewLandscape->SetActorScale3D(Scale);
	NewLandscape->SetLandscapeGuid(LandscapeGuid);

	TArrayView<const FLandscapeLayer> EmptyLayers;
	NewLandscape->Import(
		LandscapeGuid,
		0, 0,
		SizeX - 1, SizeY - 1,
		SectionsPerComponent, QuadsPerSection,
		HeightDataPerLayers,
		nullptr, // HeightmapFileName
		MaterialLayerDataPerLayers,
		ELandscapeImportAlphamapType::Additive,
		EmptyLayers
	);

	// Set label if provided
	if (!LandscapeLabel.IsEmpty())
	{
		NewLandscape->SetActorLabel(LandscapeLabel);
	}

	// Register landscape info
	ULandscapeInfo* LandscapeInfo = NewLandscape->GetLandscapeInfo();
	if (LandscapeInfo)
	{
		LandscapeInfo->UpdateComponentLayerAllowList();
	}

	Result.bSuccess = true;
	Result.ActorLabel = NewLandscape->GetActorLabel();

	UE_LOG(LogTemp, Log, TEXT("ULandscapeService::CreateLandscape: Created landscape '%s' (%dx%d vertices, %d components)"),
		*Result.ActorLabel, SizeX, SizeY, ComponentCountX * ComponentCountY);

	return Result;
}

bool ULandscapeService::DeleteLandscape(const FString& LandscapeNameOrLabel)
{
	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::DeleteLandscape: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return false;
	}

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return false;
	}

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeService", "DeleteLandscape", "Delete Landscape"));

	bool bDestroyed = World->DestroyActor(Landscape);
	if (bDestroyed)
	{
		UE_LOG(LogTemp, Log, TEXT("ULandscapeService::DeleteLandscape: Destroyed landscape '%s'"), *LandscapeNameOrLabel);
	}

	return bDestroyed;
}

// =================================================================
// Heightmap Operations
// =================================================================

bool ULandscapeService::ImportHeightmap(
	const FString& LandscapeNameOrLabel,
	const FString& FilePath)
{
	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::ImportHeightmap: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return false;
	}

	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
	if (!LandscapeInfo)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::ImportHeightmap: No landscape info for '%s'"), *LandscapeNameOrLabel);
		return false;
	}

	// Load file data
	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *FilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::ImportHeightmap: Failed to load file '%s'"), *FilePath);
		return false;
	}

	// Get landscape extent
	int32 MinX, MinY, MaxX, MaxY;
	if (!LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::ImportHeightmap: Failed to get landscape extent"));
		return false;
	}

	int32 SizeX = MaxX - MinX + 1;
	int32 SizeY = MaxY - MinY + 1;
	int32 ExpectedBytes = SizeX * SizeY * sizeof(uint16);

	if (FileData.Num() != ExpectedBytes)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::ImportHeightmap: File size mismatch. Expected %d bytes for %dx%d landscape, got %d bytes"),
			ExpectedBytes, SizeX, SizeY, FileData.Num());
		return false;
	}

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeService", "ImportHeightmap", "Import Heightmap"));

	// Write height data using the edit interface
	FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
	const uint16* HeightData = reinterpret_cast<const uint16*>(FileData.GetData());
	LandscapeEdit.SetHeightData(MinX, MinY, MaxX, MaxY, HeightData, 0, true);

	UE_LOG(LogTemp, Log, TEXT("ULandscapeService::ImportHeightmap: Imported heightmap to '%s' (%dx%d)"), *LandscapeNameOrLabel, SizeX, SizeY);
	return true;
}

bool ULandscapeService::ExportHeightmap(
	const FString& LandscapeNameOrLabel,
	const FString& OutputFilePath)
{
	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::ExportHeightmap: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return false;
	}

	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
	if (!LandscapeInfo)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::ExportHeightmap: No landscape info"));
		return false;
	}

	int32 MinX, MinY, MaxX, MaxY;
	if (!LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::ExportHeightmap: Failed to get landscape extent"));
		return false;
	}

	int32 SizeX = MaxX - MinX + 1;
	int32 SizeY = MaxY - MinY + 1;

	// Read height data
	TArray<uint16> HeightData;
	HeightData.SetNumUninitialized(SizeX * SizeY);

	FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
	LandscapeEdit.GetHeightData(MinX, MinY, MaxX, MaxY, HeightData.GetData(), 0);

	// Save to file
	TArray<uint8> FileData;
	FileData.SetNumUninitialized(HeightData.Num() * sizeof(uint16));
	FMemory::Memcpy(FileData.GetData(), HeightData.GetData(), FileData.Num());

	if (!FFileHelper::SaveArrayToFile(FileData, *OutputFilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::ExportHeightmap: Failed to save file '%s'"), *OutputFilePath);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("ULandscapeService::ExportHeightmap: Exported heightmap from '%s' (%dx%d) to '%s'"),
		*LandscapeNameOrLabel, SizeX, SizeY, *OutputFilePath);
	return true;
}

FLandscapeHeightSample ULandscapeService::GetHeightAtLocation(
	const FString& LandscapeNameOrLabel,
	float WorldX, float WorldY)
{
	FLandscapeHeightSample Sample;

	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::GetHeightAtLocation: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return Sample;
	}

	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
	if (!LandscapeInfo)
	{
		return Sample;
	}

	// Primary method: Read directly from heightmap data via FLandscapeEditDataInterface.
	// This is more reliable than line traces, which depend on collision being rebuilt.
	FVector LandscapeLocation = Landscape->GetActorLocation();
	FVector LandscapeScale = Landscape->GetActorScale3D();

	// Convert world coords to landscape-local vertex coords
	float LocalX = (WorldX - LandscapeLocation.X) / LandscapeScale.X;
	float LocalY = (WorldY - LandscapeLocation.Y) / LandscapeScale.Y;

	// Get the 4 surrounding vertices for bilinear interpolation
	int32 X0 = FMath::FloorToInt(LocalX);
	int32 Y0 = FMath::FloorToInt(LocalY);
	int32 X1 = X0 + 1;
	int32 Y1 = Y0 + 1;

	// Clamp to landscape extent
	int32 LandMinX, LandMinY, LandMaxX, LandMaxY;
	if (LandscapeInfo->GetLandscapeExtent(LandMinX, LandMinY, LandMaxX, LandMaxY))
	{
		X0 = FMath::Clamp(X0, LandMinX, LandMaxX);
		Y0 = FMath::Clamp(Y0, LandMinY, LandMaxY);
		X1 = FMath::Clamp(X1, LandMinX, LandMaxX);
		Y1 = FMath::Clamp(Y1, LandMinY, LandMaxY);

		// Read the 2x2 region
		int32 SizeX = X1 - X0 + 1;
		int32 SizeY = Y1 - Y0 + 1;
		TArray<uint16> HeightData;
		HeightData.SetNumUninitialized(SizeX * SizeY);

		FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
		LandscapeEdit.GetHeightData(X0, Y0, X1, Y1, HeightData.GetData(), 0);

		// Bilinear interpolation
		float FracX = LocalX - FMath::FloorToFloat(LocalX);
		float FracY = LocalY - FMath::FloorToFloat(LocalY);

		float H00 = static_cast<float>(HeightData[0]);
		float H10 = (SizeX > 1) ? static_cast<float>(HeightData[1]) : H00;
		float H01 = (SizeY > 1) ? static_cast<float>(HeightData[SizeX]) : H00;
		float H11 = (SizeX > 1 && SizeY > 1) ? static_cast<float>(HeightData[SizeX + 1]) : H00;

		float InterpolatedHeight = FMath::Lerp(
			FMath::Lerp(H00, H10, FracX),
			FMath::Lerp(H01, H11, FracX),
			FracY);

		// Convert uint16 height to world-space Z
		// UE mapping: WorldZ = LandscapeZ + (HeightValue - 32768) * LANDSCAPE_ZSCALE * ActorScale.Z
		float WorldZ = LandscapeLocation.Z + (InterpolatedHeight - 32768.0f) * LANDSCAPE_ZSCALE * LandscapeScale.Z;

		Sample.Height = WorldZ;
		Sample.WorldLocation = FVector(WorldX, WorldY, WorldZ);
		Sample.bValid = true;
	}

	// Fallback: try the landscape's built-in height query
	if (!Sample.bValid)
	{
		TOptional<float> Height = Landscape->GetHeightAtLocation(FVector(WorldX, WorldY, 0.0f));
		if (Height.IsSet())
		{
			Sample.Height = Height.GetValue();
			Sample.WorldLocation = FVector(WorldX, WorldY, Height.GetValue());
			Sample.bValid = true;
		}
	}

	return Sample;
}

bool ULandscapeService::SetHeightInRegion(
	const FString& LandscapeNameOrLabel,
	int32 StartX, int32 StartY,
	int32 SizeX, int32 SizeY,
	const TArray<float>& Heights)
{
	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::SetHeightInRegion: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return false;
	}

	if (Heights.Num() != SizeX * SizeY)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::SetHeightInRegion: Heights array size %d doesn't match %d x %d = %d"),
			Heights.Num(), SizeX, SizeY, SizeX * SizeY);
		return false;
	}

	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
	if (!LandscapeInfo)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::SetHeightInRegion: No landscape info"));
		return false;
	}

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeService", "SetHeightInRegion", "Set Height In Region"));

	// Convert float heights to uint16
	// UE landscape height range: 0-65535, where 32768 = zero (mid-height)
	// The mapping is: WorldHeight = (HeightValue - 32768) * LANDSCAPE_ZSCALE * ActorScale.Z
	float ZScale = Landscape->GetActorScale3D().Z;
	float LandscapeZScale = LANDSCAPE_ZSCALE;

	TArray<uint16> HeightData;
	HeightData.SetNumUninitialized(SizeX * SizeY);

	for (int32 i = 0; i < Heights.Num(); i++)
	{
		// Convert world-space height to uint16
		float NormalizedHeight = Heights[i] / (LandscapeZScale * ZScale);
		int32 UintHeight = FMath::RoundToInt(NormalizedHeight + 32768.0f);
		HeightData[i] = static_cast<uint16>(FMath::Clamp(UintHeight, 0, 65535));
	}

	FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
	LandscapeEdit.SetHeightData(StartX, StartY, StartX + SizeX - 1, StartY + SizeY - 1, HeightData.GetData(), 0, true);

	UpdateLandscapeAfterHeightEdit(Landscape);

	UE_LOG(LogTemp, Log, TEXT("ULandscapeService::SetHeightInRegion: Set heights in region (%d,%d)-(%d,%d)"),
		StartX, StartY, StartX + SizeX - 1, StartY + SizeY - 1);
	return true;
}

// =================================================================
// Sculpting Operations
// =================================================================

static float CalculateBrushFalloff(float Distance, float Radius, const FString& FalloffType)
{
	if (Radius <= 0.0f || Distance >= Radius)
	{
		return 0.0f;
	}

	float Ratio = Distance / Radius;

	if (FalloffType.Equals(TEXT("Smooth"), ESearchCase::IgnoreCase))
	{
		// Cosine falloff
		return 0.5f * (FMath::Cos(Ratio * PI) + 1.0f);
	}
	else if (FalloffType.Equals(TEXT("Spherical"), ESearchCase::IgnoreCase))
	{
		return FMath::Sqrt(1.0f - Ratio * Ratio);
	}
	else if (FalloffType.Equals(TEXT("Tip"), ESearchCase::IgnoreCase))
	{
		return 1.0f - Ratio * Ratio;
	}
	else // Linear (default)
	{
		return 1.0f - Ratio;
	}
}

bool ULandscapeService::SculptAtLocation(
	const FString& LandscapeNameOrLabel,
	float WorldX, float WorldY,
	float BrushRadius,
	float Strength,
	const FString& BrushFalloffType)
{
	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::SculptAtLocation: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return false;
	}

	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
	if (!LandscapeInfo)
	{
		return false;
	}

	// Convert world coordinates to landscape-local coordinates
	FVector LandscapeLocation = Landscape->GetActorLocation();
	FVector LandscapeScale = Landscape->GetActorScale3D();

	float LocalX = (WorldX - LandscapeLocation.X) / LandscapeScale.X;
	float LocalY = (WorldY - LandscapeLocation.Y) / LandscapeScale.Y;
	float LocalRadius = BrushRadius / LandscapeScale.X;

	// Get the region to modify
	int32 MinX = FMath::FloorToInt(LocalX - LocalRadius);
	int32 MinY = FMath::FloorToInt(LocalY - LocalRadius);
	int32 MaxX = FMath::CeilToInt(LocalX + LocalRadius);
	int32 MaxY = FMath::CeilToInt(LocalY + LocalRadius);

	// Clamp to landscape extent
	int32 LandMinX, LandMinY, LandMaxX, LandMaxY;
	if (!LandscapeInfo->GetLandscapeExtent(LandMinX, LandMinY, LandMaxX, LandMaxY))
	{
		return false;
	}

	MinX = FMath::Max(MinX, LandMinX);
	MinY = FMath::Max(MinY, LandMinY);
	MaxX = FMath::Min(MaxX, LandMaxX);
	MaxY = FMath::Min(MaxY, LandMaxY);

	if (MinX > MaxX || MinY > MaxY)
	{
		return false;
	}

	int32 SizeX = MaxX - MinX + 1;
	int32 SizeY = MaxY - MinY + 1;

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeService", "SculptAtLocation", "Sculpt Landscape"));

	// Read current height data
	TArray<uint16> HeightData;
	HeightData.SetNumUninitialized(SizeX * SizeY);

	FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
	LandscapeEdit.GetHeightData(MinX, MinY, MaxX, MaxY, HeightData.GetData(), 0);

	// Apply brush
	// Convert world-space height delta to uint16 heightmap delta
	// UE mapping: WorldHeight = (HeightValue - 32768) * LANDSCAPE_ZSCALE * ActorScale.Z
	// So: HeightDelta_uint16 = WorldDelta / (LANDSCAPE_ZSCALE * ActorScale.Z)
	float ZScale = LandscapeScale.Z;
	float StrengthInUnits = Strength / (LANDSCAPE_ZSCALE * ZScale);

	int32 SaturatedCount = 0;
	for (int32 Y = 0; Y < SizeY; Y++)
	{
		for (int32 X = 0; X < SizeX; X++)
		{
			float VertX = static_cast<float>(MinX + X);
			float VertY = static_cast<float>(MinY + Y);
			float Distance = FMath::Sqrt(FMath::Square(VertX - LocalX) + FMath::Square(VertY - LocalY));

			float Falloff = CalculateBrushFalloff(Distance, LocalRadius, BrushFalloffType);
			if (Falloff > 0.0f)
			{
				int32 Index = Y * SizeX + X;
				float CurrentHeight = static_cast<float>(HeightData[Index]);
				float Delta = StrengthInUnits * Falloff;
				float NewHeight = FMath::Clamp(CurrentHeight + Delta, 0.0f, 65535.0f);
				if (NewHeight == 0.0f || NewHeight == 65535.0f)
				{
					SaturatedCount++;
				}
				HeightData[Index] = static_cast<uint16>(FMath::RoundToInt(NewHeight));
			}
		}
	}

	// Write modified height data
	LandscapeEdit.SetHeightData(MinX, MinY, MaxX, MaxY, HeightData.GetData(), 0, true);

	UpdateLandscapeAfterHeightEdit(Landscape);

	if (SaturatedCount > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::SculptAtLocation: %d vertices hit height limit. Consider using landscape Z offset or higher Z scale."), SaturatedCount);
	}

	UE_LOG(LogTemp, Log, TEXT("ULandscapeService::SculptAtLocation: Sculpted at (%.0f, %.0f) with radius %.0f, strength %.2f"),
		WorldX, WorldY, BrushRadius, Strength);
	return true;
}

bool ULandscapeService::FlattenAtLocation(
	const FString& LandscapeNameOrLabel,
	float WorldX, float WorldY,
	float BrushRadius,
	float TargetHeight,
	float Strength,
	const FString& BrushFalloffType)
{
	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::FlattenAtLocation: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return false;
	}

	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
	if (!LandscapeInfo)
	{
		return false;
	}

	FVector LandscapeLocation = Landscape->GetActorLocation();
	FVector LandscapeScale = Landscape->GetActorScale3D();

	float LocalX = (WorldX - LandscapeLocation.X) / LandscapeScale.X;
	float LocalY = (WorldY - LandscapeLocation.Y) / LandscapeScale.Y;
	float LocalRadius = BrushRadius / LandscapeScale.X;

	// Convert target height to uint16
	float ZScale = LandscapeScale.Z;
	float TargetLocal = (TargetHeight - LandscapeLocation.Z) / (LANDSCAPE_ZSCALE * ZScale);
	float TargetUint = TargetLocal + 32768.0f;

	int32 MinX = FMath::FloorToInt(LocalX - LocalRadius);
	int32 MinY = FMath::FloorToInt(LocalY - LocalRadius);
	int32 MaxX = FMath::CeilToInt(LocalX + LocalRadius);
	int32 MaxY = FMath::CeilToInt(LocalY + LocalRadius);

	int32 LandMinX, LandMinY, LandMaxX, LandMaxY;
	if (!LandscapeInfo->GetLandscapeExtent(LandMinX, LandMinY, LandMaxX, LandMaxY))
	{
		return false;
	}

	MinX = FMath::Max(MinX, LandMinX);
	MinY = FMath::Max(MinY, LandMinY);
	MaxX = FMath::Min(MaxX, LandMaxX);
	MaxY = FMath::Min(MaxY, LandMaxY);

	if (MinX > MaxX || MinY > MaxY)
	{
		return false;
	}

	int32 SizeX = MaxX - MinX + 1;
	int32 SizeY = MaxY - MinY + 1;

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeService", "FlattenAtLocation", "Flatten Landscape"));

	TArray<uint16> HeightData;
	HeightData.SetNumUninitialized(SizeX * SizeY);

	FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
	LandscapeEdit.GetHeightData(MinX, MinY, MaxX, MaxY, HeightData.GetData(), 0);

	for (int32 Y = 0; Y < SizeY; Y++)
	{
		for (int32 X = 0; X < SizeX; X++)
		{
			float VertX = static_cast<float>(MinX + X);
			float VertY = static_cast<float>(MinY + Y);
			float Distance = FMath::Sqrt(FMath::Square(VertX - LocalX) + FMath::Square(VertY - LocalY));

			float Falloff = CalculateBrushFalloff(Distance, LocalRadius, BrushFalloffType);
			if (Falloff > 0.0f)
			{
				int32 Index = Y * SizeX + X;
				float CurrentHeight = static_cast<float>(HeightData[Index]);
				float NewHeight = FMath::Lerp(CurrentHeight, TargetUint, Strength * Falloff);
				HeightData[Index] = static_cast<uint16>(FMath::Clamp(FMath::RoundToInt(NewHeight), 0, 65535));
			}
		}
	}

	LandscapeEdit.SetHeightData(MinX, MinY, MaxX, MaxY, HeightData.GetData(), 0, true);

	UpdateLandscapeAfterHeightEdit(Landscape);

	UE_LOG(LogTemp, Log, TEXT("ULandscapeService::FlattenAtLocation: Flattened at (%.0f, %.0f) to height %.0f"),
		WorldX, WorldY, TargetHeight);
	return true;
}

bool ULandscapeService::SmoothAtLocation(
	const FString& LandscapeNameOrLabel,
	float WorldX, float WorldY,
	float BrushRadius,
	float Strength,
	const FString& BrushFalloffType)
{
	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::SmoothAtLocation: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return false;
	}

	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
	if (!LandscapeInfo)
	{
		return false;
	}

	FVector LandscapeLocation = Landscape->GetActorLocation();
	FVector LandscapeScale = Landscape->GetActorScale3D();

	float LocalX = (WorldX - LandscapeLocation.X) / LandscapeScale.X;
	float LocalY = (WorldY - LandscapeLocation.Y) / LandscapeScale.Y;
	float LocalRadius = BrushRadius / LandscapeScale.X;

	// Adaptive kernel radius: scales with brush radius and strength
	// At Strength=1.0, kernel covers ~10% of the brush radius in vertex space
	// Clamped to [1, 32] to balance effectiveness vs performance
	int32 KernelRadius = FMath::Max(1, FMath::RoundToInt(LocalRadius * Strength * 0.1f));
	KernelRadius = FMath::Min(KernelRadius, 32);

	// Read a larger region to accommodate the kernel sampling
	int32 MinX = FMath::FloorToInt(LocalX - LocalRadius) - KernelRadius;
	int32 MinY = FMath::FloorToInt(LocalY - LocalRadius) - KernelRadius;
	int32 MaxX = FMath::CeilToInt(LocalX + LocalRadius) + KernelRadius;
	int32 MaxY = FMath::CeilToInt(LocalY + LocalRadius) + KernelRadius;

	int32 LandMinX, LandMinY, LandMaxX, LandMaxY;
	if (!LandscapeInfo->GetLandscapeExtent(LandMinX, LandMinY, LandMaxX, LandMaxY))
	{
		return false;
	}

	MinX = FMath::Max(MinX, LandMinX);
	MinY = FMath::Max(MinY, LandMinY);
	MaxX = FMath::Min(MaxX, LandMaxX);
	MaxY = FMath::Min(MaxY, LandMaxY);

	if (MinX > MaxX || MinY > MaxY)
	{
		return false;
	}

	int32 SizeX = MaxX - MinX + 1;
	int32 SizeY = MaxY - MinY + 1;

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeService", "SmoothAtLocation", "Smooth Landscape"));

	TArray<uint16> HeightData;
	HeightData.SetNumUninitialized(SizeX * SizeY);

	FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
	LandscapeEdit.GetHeightData(MinX, MinY, MaxX, MaxY, HeightData.GetData(), 0);

	// Pre-compute Gaussian weights for the kernel
	float Sigma = static_cast<float>(KernelRadius) / 2.0f;
	float SigmaSq2 = 2.0f * Sigma * Sigma;

	// Create output copy
	TArray<uint16> SmoothedData = HeightData;

	// Apply adaptive Gaussian blur kernel
	for (int32 Y = KernelRadius; Y < SizeY - KernelRadius; Y++)
	{
		for (int32 X = KernelRadius; X < SizeX - KernelRadius; X++)
		{
			float VertX = static_cast<float>(MinX + X);
			float VertY = static_cast<float>(MinY + Y);
			float Distance = FMath::Sqrt(FMath::Square(VertX - LocalX) + FMath::Square(VertY - LocalY));

			float Falloff = CalculateBrushFalloff(Distance, LocalRadius, BrushFalloffType);
			if (Falloff > 0.0f)
			{
				// Gaussian-weighted average over KernelRadius neighborhood
				float Sum = 0.0f;
				float WeightSum = 0.0f;
				for (int32 DY = -KernelRadius; DY <= KernelRadius; DY++)
				{
					for (int32 DX = -KernelRadius; DX <= KernelRadius; DX++)
					{
						float Dist = FMath::Sqrt(static_cast<float>(DX * DX + DY * DY));
						float Weight = FMath::Exp(-(Dist * Dist) / SigmaSq2);
						Sum += static_cast<float>(HeightData[(Y + DY) * SizeX + (X + DX)]) * Weight;
						WeightSum += Weight;
					}
				}
				float Average = Sum / WeightSum;

				int32 Index = Y * SizeX + X;
				float Current = static_cast<float>(HeightData[Index]);
				float NewHeight = FMath::Lerp(Current, Average, Strength * Falloff);
				SmoothedData[Index] = static_cast<uint16>(FMath::Clamp(FMath::RoundToInt(NewHeight), 0, 65535));
			}
		}
	}

	LandscapeEdit.SetHeightData(MinX, MinY, MaxX, MaxY, SmoothedData.GetData(), 0, true);

	UpdateLandscapeAfterHeightEdit(Landscape);

	UE_LOG(LogTemp, Log, TEXT("ULandscapeService::SmoothAtLocation: Smoothed at (%.0f, %.0f) with radius %.0f, kernel %d"),
		WorldX, WorldY, BrushRadius, KernelRadius);
	return true;
}

bool ULandscapeService::RaiseLowerRegion(
	const FString& LandscapeNameOrLabel,
	float WorldCenterX, float WorldCenterY,
	float WorldWidth, float WorldHeight,
	float HeightDelta,
	float FalloffWidth)
{
	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::RaiseLowerRegion: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return false;
	}

	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
	if (!LandscapeInfo)
	{
		return false;
	}

	FVector LandscapeLocation = Landscape->GetActorLocation();
	FVector LandscapeScale = Landscape->GetActorScale3D();

	// Inner rectangle (full strength)
	float HalfW = WorldWidth * 0.5f;
	float HalfH = WorldHeight * 0.5f;

	// Outer rectangle expands by FalloffWidth
	float OuterHalfW = HalfW + FalloffWidth;
	float OuterHalfH = HalfH + FalloffWidth;

	int32 MinX = FMath::FloorToInt((WorldCenterX - OuterHalfW - LandscapeLocation.X) / LandscapeScale.X);
	int32 MinY = FMath::FloorToInt((WorldCenterY - OuterHalfH - LandscapeLocation.Y) / LandscapeScale.Y);
	int32 MaxX = FMath::CeilToInt((WorldCenterX + OuterHalfW - LandscapeLocation.X) / LandscapeScale.X);
	int32 MaxY = FMath::CeilToInt((WorldCenterY + OuterHalfH - LandscapeLocation.Y) / LandscapeScale.Y);

	// Clamp to landscape extent
	int32 LandMinX, LandMinY, LandMaxX, LandMaxY;
	if (!LandscapeInfo->GetLandscapeExtent(LandMinX, LandMinY, LandMaxX, LandMaxY))
	{
		return false;
	}

	MinX = FMath::Max(MinX, LandMinX);
	MinY = FMath::Max(MinY, LandMinY);
	MaxX = FMath::Min(MaxX, LandMaxX);
	MaxY = FMath::Min(MaxY, LandMaxY);

	if (MinX > MaxX || MinY > MaxY)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::RaiseLowerRegion: Region outside landscape bounds"));
		return false;
	}

	int32 SizeX = MaxX - MinX + 1;
	int32 SizeY = MaxY - MinY + 1;

	// Convert world-space height delta to uint16 heightmap delta
	float ZScale = LandscapeScale.Z;
	float DeltaUint16 = HeightDelta / (LANDSCAPE_ZSCALE * ZScale);

	// Inner rectangle edges in world coords
	float InnerMinWX = WorldCenterX - HalfW;
	float InnerMaxWX = WorldCenterX + HalfW;
	float InnerMinWY = WorldCenterY - HalfH;
	float InnerMaxWY = WorldCenterY + HalfH;

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeService", "RaiseLowerRegion", "Raise/Lower Landscape Region"));

	TArray<uint16> HeightData;
	HeightData.SetNumUninitialized(SizeX * SizeY);

	FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
	LandscapeEdit.GetHeightData(MinX, MinY, MaxX, MaxY, HeightData.GetData(), 0);

	int32 SaturatedCount = 0;
	for (int32 Y = 0; Y < SizeY; Y++)
	{
		for (int32 X = 0; X < SizeX; X++)
		{
			// Convert back to world coords for falloff calculation
			float VertWorldX = LandscapeLocation.X + static_cast<float>(MinX + X) * LandscapeScale.X;
			float VertWorldY = LandscapeLocation.Y + static_cast<float>(MinY + Y) * LandscapeScale.Y;

			// Calculate distance from vertex to the inner rectangle edge
			// Negative = inside inner rect, Positive = in falloff band
			float DistX = 0.0f;
			if (VertWorldX < InnerMinWX) DistX = InnerMinWX - VertWorldX;
			else if (VertWorldX > InnerMaxWX) DistX = VertWorldX - InnerMaxWX;

			float DistY = 0.0f;
			if (VertWorldY < InnerMinWY) DistY = InnerMinWY - VertWorldY;
			else if (VertWorldY > InnerMaxWY) DistY = VertWorldY - InnerMaxWY;

			float DistToEdge = FMath::Sqrt(DistX * DistX + DistY * DistY);

			// Compute falloff strength
			float FalloffStrength = 1.0f;
			if (FalloffWidth > 0.0f && DistToEdge > 0.0f)
			{
				if (DistToEdge >= FalloffWidth)
				{
					continue; // Outside the falloff band entirely
				}
				// Cosine falloff for smooth transition
				float NormDist = DistToEdge / FalloffWidth;
				FalloffStrength = 0.5f * (FMath::Cos(NormDist * PI) + 1.0f);
			}
			else if (FalloffWidth <= 0.0f && DistToEdge > 0.0f)
			{
				continue; // No falloff and outside inner rect
			}

			int32 Index = Y * SizeX + X;
			float CurrentHeight = static_cast<float>(HeightData[Index]);
			float NewHeight = FMath::Clamp(CurrentHeight + DeltaUint16 * FalloffStrength, 0.0f, 65535.0f);
			if (NewHeight == 0.0f || NewHeight == 65535.0f)
			{
				SaturatedCount++;
			}
			HeightData[Index] = static_cast<uint16>(FMath::RoundToInt(NewHeight));
		}
	}

	LandscapeEdit.SetHeightData(MinX, MinY, MaxX, MaxY, HeightData.GetData(), 0, true);

	UpdateLandscapeAfterHeightEdit(Landscape);

	if (SaturatedCount > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::RaiseLowerRegion: %d vertices hit height limit. Consider using landscape Z offset or higher Z scale."), SaturatedCount);
	}

	UE_LOG(LogTemp, Log, TEXT("ULandscapeService::RaiseLowerRegion: Raised/lowered region (%.0f,%.0f)-(%.0f,%.0f) by %.0f world units, falloff %.0f"),
		WorldCenterX - HalfW, WorldCenterY - HalfH, WorldCenterX + HalfW, WorldCenterY + HalfH, HeightDelta, FalloffWidth);
	return true;
}

// Simple hash-based noise function (no external dependencies)
static float HashNoise2D(int32 X, int32 Y, int32 Seed)
{
	// Simple integer hash
	int32 N = X + Y * 57 + Seed * 131;
	N = (N << 13) ^ N;
	return (1.0f - ((N * (N * N * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
}

static float SmoothNoise2D(int32 X, int32 Y, int32 Seed)
{
	float Corners = (HashNoise2D(X - 1, Y - 1, Seed) + HashNoise2D(X + 1, Y - 1, Seed)
		+ HashNoise2D(X - 1, Y + 1, Seed) + HashNoise2D(X + 1, Y + 1, Seed)) / 16.0f;
	float Sides = (HashNoise2D(X - 1, Y, Seed) + HashNoise2D(X + 1, Y, Seed)
		+ HashNoise2D(X, Y - 1, Seed) + HashNoise2D(X, Y + 1, Seed)) / 8.0f;
	float Center = HashNoise2D(X, Y, Seed) / 4.0f;
	return Corners + Sides + Center;
}

static float CosineInterpolate(float A, float B, float X)
{
	float Ft = X * PI;
	float F = (1.0f - FMath::Cos(Ft)) * 0.5f;
	return A * (1.0f - F) + B * F;
}

static float InterpolatedNoise2D(float X, float Y, int32 Seed)
{
	int32 IntX = FMath::FloorToInt(X);
	int32 IntY = FMath::FloorToInt(Y);
	float FracX = X - IntX;
	float FracY = Y - IntY;

	float V1 = SmoothNoise2D(IntX, IntY, Seed);
	float V2 = SmoothNoise2D(IntX + 1, IntY, Seed);
	float V3 = SmoothNoise2D(IntX, IntY + 1, Seed);
	float V4 = SmoothNoise2D(IntX + 1, IntY + 1, Seed);

	float I1 = CosineInterpolate(V1, V2, FracX);
	float I2 = CosineInterpolate(V3, V4, FracX);

	return CosineInterpolate(I1, I2, FracY);
}

static float PerlinNoise2D(float X, float Y, float Frequency, int32 Octaves, int32 Seed)
{
	float Total = 0.0f;
	float Amplitude = 1.0f;
	float MaxAmplitude = 0.0f;

	for (int32 i = 0; i < Octaves; i++)
	{
		Total += InterpolatedNoise2D(X * Frequency, Y * Frequency, Seed + i * 1000) * Amplitude;
		MaxAmplitude += Amplitude;
		Frequency *= 2.0f;
		Amplitude *= 0.5f;
	}

	return Total / MaxAmplitude; // Normalize to [-1, 1]
}

FLandscapeNoiseResult ULandscapeService::ApplyNoise(
	const FString& LandscapeNameOrLabel,
	float WorldCenterX, float WorldCenterY,
	float WorldRadius,
	float Amplitude,
	float Frequency,
	int32 Seed,
	int32 Octaves)
{
	FLandscapeNoiseResult Result;

	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::ApplyNoise: Landscape '%s' not found"), *LandscapeNameOrLabel);
		Result.ErrorMessage = FString::Printf(TEXT("Landscape '%s' not found"), *LandscapeNameOrLabel);
		return Result;
	}

	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
	if (!LandscapeInfo)
	{
		Result.ErrorMessage = TEXT("Could not get landscape info");
		return Result;
	}

	FVector LandscapeLocation = Landscape->GetActorLocation();
	FVector LandscapeScale = Landscape->GetActorScale3D();

	float LocalCenterX = (WorldCenterX - LandscapeLocation.X) / LandscapeScale.X;
	float LocalCenterY = (WorldCenterY - LandscapeLocation.Y) / LandscapeScale.Y;
	float LocalRadius = WorldRadius / LandscapeScale.X;

	int32 MinX = FMath::FloorToInt(LocalCenterX - LocalRadius);
	int32 MinY = FMath::FloorToInt(LocalCenterY - LocalRadius);
	int32 MaxX = FMath::CeilToInt(LocalCenterX + LocalRadius);
	int32 MaxY = FMath::CeilToInt(LocalCenterY + LocalRadius);

	// Clamp to landscape extent
	int32 LandMinX, LandMinY, LandMaxX, LandMaxY;
	if (!LandscapeInfo->GetLandscapeExtent(LandMinX, LandMinY, LandMaxX, LandMaxY))
	{
		Result.ErrorMessage = TEXT("Failed to get landscape extent");
		return Result;
	}

	MinX = FMath::Max(MinX, LandMinX);
	MinY = FMath::Max(MinY, LandMinY);
	MaxX = FMath::Min(MaxX, LandMaxX);
	MaxY = FMath::Min(MaxY, LandMaxY);

	if (MinX > MaxX || MinY > MaxY)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::ApplyNoise: Region outside landscape bounds"));
		Result.ErrorMessage = TEXT("Region outside landscape bounds");
		return Result;
	}

	int32 SizeX = MaxX - MinX + 1;
	int32 SizeY = MaxY - MinY + 1;

	// Convert amplitude to uint16 heightmap units
	float ZScale = LandscapeScale.Z;
	float AmplitudeUint16 = Amplitude / (LANDSCAPE_ZSCALE * ZScale);

	// Clamp octaves to reasonable range
	Octaves = FMath::Clamp(Octaves, 1, 8);

	float MinDelta = 0.0f;
	float MaxDelta = 0.0f;
	int32 VerticesModified = 0;
	int32 SaturatedCount = 0;

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeService", "ApplyNoise", "Apply Noise to Landscape"));

	TArray<uint16> HeightData;
	HeightData.SetNumUninitialized(SizeX * SizeY);

	FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
	LandscapeEdit.GetHeightData(MinX, MinY, MaxX, MaxY, HeightData.GetData(), 0);

	for (int32 Y = 0; Y < SizeY; Y++)
	{
		for (int32 X = 0; X < SizeX; X++)
		{
			float VertX = static_cast<float>(MinX + X);
			float VertY = static_cast<float>(MinY + Y);

			// Distance from center for circular falloff
			float Distance = FMath::Sqrt(FMath::Square(VertX - LocalCenterX) + FMath::Square(VertY - LocalCenterY));
			if (Distance >= LocalRadius)
			{
				continue;
			}

			// Smooth falloff at edges
			float Falloff = 0.5f * (FMath::Cos(Distance / LocalRadius * PI) + 1.0f);

			// Generate noise using world coordinates for consistency across calls
			float WorldVertX = LandscapeLocation.X + VertX * LandscapeScale.X;
			float WorldVertY = LandscapeLocation.Y + VertY * LandscapeScale.Y;
			float NoiseValue = PerlinNoise2D(WorldVertX, WorldVertY, Frequency, Octaves, Seed);

			int32 Index = Y * SizeX + X;
			float CurrentHeight = static_cast<float>(HeightData[Index]);
			float Delta = NoiseValue * AmplitudeUint16 * Falloff;

			// Track delta statistics in world units
			float DeltaWorld = Delta * LANDSCAPE_ZSCALE * ZScale;
			MinDelta = FMath::Min(MinDelta, DeltaWorld);
			MaxDelta = FMath::Max(MaxDelta, DeltaWorld);
			VerticesModified++;

			float NewHeight = FMath::Clamp(CurrentHeight + Delta, 0.0f, 65535.0f);
			if (NewHeight == 0.0f || NewHeight == 65535.0f)
			{
				SaturatedCount++;
			}
			HeightData[Index] = static_cast<uint16>(FMath::RoundToInt(NewHeight));
		}
	}

	LandscapeEdit.SetHeightData(MinX, MinY, MaxX, MaxY, HeightData.GetData(), 0, true);

	UpdateLandscapeAfterHeightEdit(Landscape);

	if (SaturatedCount > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::ApplyNoise: %d vertices hit height limit."), SaturatedCount);
	}

	Result.bSuccess = true;
	Result.MinDeltaApplied = MinDelta;
	Result.MaxDeltaApplied = MaxDelta;
	Result.VerticesModified = VerticesModified;
	Result.SaturatedVertices = SaturatedCount;

	UE_LOG(LogTemp, Log, TEXT("ULandscapeService::ApplyNoise: Applied noise at (%.0f, %.0f) radius %.0f, amplitude %.0f, freq %.4f, octaves %d. Delta range [%.1f, %.1f], %d vertices modified, %d saturated"),
		WorldCenterX, WorldCenterY, WorldRadius, Amplitude, Frequency, Octaves, MinDelta, MaxDelta, VerticesModified, SaturatedCount);
	return Result;
}

// =================================================================
// Paint Layer Operations
// =================================================================

TArray<FLandscapeLayerInfo_Custom> ULandscapeService::ListLayers(const FString& LandscapeNameOrLabel)
{
	TArray<FLandscapeLayerInfo_Custom> Result;

	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::ListLayers: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return Result;
	}

	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	if (!Info)
	{
		return Result;
	}

	for (const FLandscapeInfoLayerSettings& LayerSettings : Info->Layers)
	{
		FLandscapeLayerInfo_Custom LayerInfo;
		if (LayerSettings.LayerInfoObj)
		{
			LayerInfo.LayerName = LayerSettings.LayerInfoObj->GetLayerName().ToString();
			LayerInfo.LayerInfoPath = LayerSettings.LayerInfoObj->GetPathName();
			LayerInfo.bIsWeightBlended = LayerSettings.LayerInfoObj->GetBlendMethod() != ELandscapeTargetLayerBlendMethod::None;
		}
		else
		{
			LayerInfo.LayerName = LayerSettings.GetLayerName().ToString();
		}
		Result.Add(LayerInfo);
	}

	return Result;
}

bool ULandscapeService::AddLayer(
	const FString& LandscapeNameOrLabel,
	const FString& LayerInfoAssetPath)
{
	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::AddLayer: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return false;
	}

	UObject* LoadedObj = UEditorAssetLibrary::LoadAsset(LayerInfoAssetPath);
	if (!LoadedObj)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::AddLayer: Failed to load layer info asset '%s'"), *LayerInfoAssetPath);
		return false;
	}

	ULandscapeLayerInfoObject* LayerInfoObj = Cast<ULandscapeLayerInfoObject>(LoadedObj);
	if (!LayerInfoObj)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::AddLayer: Asset is not a ULandscapeLayerInfoObject: '%s'"), *LayerInfoAssetPath);
		return false;
	}

	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	if (!Info)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::AddLayer: No landscape info"));
		return false;
	}

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeService", "AddLayer", "Add Landscape Layer"));

	// Add layer info to landscape
	int32 LayerIndex = Info->Layers.Num();
	FLandscapeInfoLayerSettings NewLayerSettings(LayerInfoObj, Landscape);
	Info->Layers.Add(NewLayerSettings);

	// Update the component layer allowlist
	Info->UpdateComponentLayerAllowList();

	UE_LOG(LogTemp, Log, TEXT("ULandscapeService::AddLayer: Added layer '%s' to landscape '%s'"),
		*LayerInfoObj->GetLayerName().ToString(), *LandscapeNameOrLabel);
	return true;
}

bool ULandscapeService::RemoveLayer(
	const FString& LandscapeNameOrLabel,
	const FString& LayerName)
{
	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::RemoveLayer: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return false;
	}

	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	if (!Info)
	{
		return false;
	}

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeService", "RemoveLayer", "Remove Landscape Layer"));

	bool bFound = false;
	for (int32 i = Info->Layers.Num() - 1; i >= 0; i--)
	{
		FName CurrentLayerName = Info->Layers[i].GetLayerName();
		if (CurrentLayerName.ToString().Equals(LayerName, ESearchCase::IgnoreCase))
		{
			Info->Layers.RemoveAt(i);
			bFound = true;
			break;
		}
	}

	if (bFound)
	{
		Info->UpdateComponentLayerAllowList();
		UE_LOG(LogTemp, Log, TEXT("ULandscapeService::RemoveLayer: Removed layer '%s' from '%s'"), *LayerName, *LandscapeNameOrLabel);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::RemoveLayer: Layer '%s' not found on '%s'"), *LayerName, *LandscapeNameOrLabel);
	}

	return bFound;
}

TArray<FLandscapeLayerWeightSample> ULandscapeService::GetLayerWeightsAtLocation(
	const FString& LandscapeNameOrLabel,
	float WorldX, float WorldY)
{
	TArray<FLandscapeLayerWeightSample> Result;

	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::GetLayerWeightsAtLocation: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return Result;
	}

	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	if (!Info)
	{
		return Result;
	}

	// Convert world to landscape local
	FVector LandscapeLocation = Landscape->GetActorLocation();
	FVector LandscapeScale = Landscape->GetActorScale3D();

	int32 LocalX = FMath::RoundToInt((WorldX - LandscapeLocation.X) / LandscapeScale.X);
	int32 LocalY = FMath::RoundToInt((WorldY - LandscapeLocation.Y) / LandscapeScale.Y);

	FLandscapeEditDataInterface LandscapeEdit(Info);

	for (const FLandscapeInfoLayerSettings& LayerSettings : Info->Layers)
	{
		if (!LayerSettings.LayerInfoObj)
		{
			continue;
		}

		// Read a single pixel of weight data
		TArray<uint8> WeightData;
		WeightData.SetNumZeroed(1);
		LandscapeEdit.GetWeightData(LayerSettings.LayerInfoObj, LocalX, LocalY, LocalX, LocalY, WeightData.GetData(), 0);

		FLandscapeLayerWeightSample Sample;
		Sample.LayerName = LayerSettings.LayerInfoObj->GetLayerName().ToString();
		Sample.Weight = WeightData[0] / 255.0f;
		Result.Add(Sample);
	}

	return Result;
}

bool ULandscapeService::PaintLayerAtLocation(
	const FString& LandscapeNameOrLabel,
	const FString& LayerName,
	float WorldX, float WorldY,
	float BrushRadius,
	float Strength)
{
	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::PaintLayerAtLocation: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return false;
	}

	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	if (!Info)
	{
		return false;
	}

	// Find the target layer info
	ULandscapeLayerInfoObject* TargetLayer = nullptr;
	for (const FLandscapeInfoLayerSettings& LayerSettings : Info->Layers)
	{
		if (LayerSettings.LayerInfoObj &&
			LayerSettings.LayerInfoObj->GetLayerName().ToString().Equals(LayerName, ESearchCase::IgnoreCase))
		{
			TargetLayer = LayerSettings.LayerInfoObj;
			break;
		}
	}

	if (!TargetLayer)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::PaintLayerAtLocation: Layer '%s' not found on landscape"), *LayerName);
		return false;
	}

	FVector LandscapeLocation = Landscape->GetActorLocation();
	FVector LandscapeScale = Landscape->GetActorScale3D();

	float LocalX = (WorldX - LandscapeLocation.X) / LandscapeScale.X;
	float LocalY = (WorldY - LandscapeLocation.Y) / LandscapeScale.Y;
	float LocalRadius = BrushRadius / LandscapeScale.X;

	int32 MinX = FMath::FloorToInt(LocalX - LocalRadius);
	int32 MinY = FMath::FloorToInt(LocalY - LocalRadius);
	int32 MaxX = FMath::CeilToInt(LocalX + LocalRadius);
	int32 MaxY = FMath::CeilToInt(LocalY + LocalRadius);

	int32 LandMinX, LandMinY, LandMaxX, LandMaxY;
	if (!Info->GetLandscapeExtent(LandMinX, LandMinY, LandMaxX, LandMaxY))
	{
		return false;
	}

	MinX = FMath::Max(MinX, LandMinX);
	MinY = FMath::Max(MinY, LandMinY);
	MaxX = FMath::Min(MaxX, LandMaxX);
	MaxY = FMath::Min(MaxY, LandMaxY);

	if (MinX > MaxX || MinY > MaxY)
	{
		return false;
	}

	int32 SizeX = MaxX - MinX + 1;
	int32 SizeY = MaxY - MinY + 1;

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeService", "PaintLayer", "Paint Landscape Layer"));

	FLandscapeEditDataInterface LandscapeEdit(Info);

	// Read current weight data for the target layer
	TArray<uint8> WeightData;
	WeightData.SetNumZeroed(SizeX * SizeY);
	LandscapeEdit.GetWeightData(TargetLayer, MinX, MinY, MaxX, MaxY, WeightData.GetData(), 0);

	// Apply brush to weight data
	for (int32 Y = 0; Y < SizeY; Y++)
	{
		for (int32 X = 0; X < SizeX; X++)
		{
			float VertX = static_cast<float>(MinX + X);
			float VertY = static_cast<float>(MinY + Y);
			float Distance = FMath::Sqrt(FMath::Square(VertX - LocalX) + FMath::Square(VertY - LocalY));

			float Falloff = CalculateBrushFalloff(Distance, LocalRadius, TEXT("Smooth"));
			if (Falloff > 0.0f)
			{
				int32 Index = Y * SizeX + X;
				float Current = WeightData[Index] / 255.0f;
				float NewWeight = FMath::Clamp(Current + Strength * Falloff, 0.0f, 1.0f);
				WeightData[Index] = static_cast<uint8>(FMath::RoundToInt(NewWeight * 255.0f));
			}
		}
	}

	// Write weight data
	LandscapeEdit.SetAlphaData(TargetLayer, MinX, MinY, MaxX, MaxY, WeightData.GetData(), 0);

	UE_LOG(LogTemp, Log, TEXT("ULandscapeService::PaintLayerAtLocation: Painted '%s' at (%.0f, %.0f)"),
		*LayerName, WorldX, WorldY);
	return true;
}

// =================================================================
// Property Operations
// =================================================================

bool ULandscapeService::SetLandscapeMaterial(
	const FString& LandscapeNameOrLabel,
	const FString& MaterialPath)
{
	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::SetLandscapeMaterial: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return false;
	}

	UObject* LoadedObj = UEditorAssetLibrary::LoadAsset(MaterialPath);
	UMaterialInterface* Material = Cast<UMaterialInterface>(LoadedObj);
	if (!Material)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::SetLandscapeMaterial: Failed to load material '%s'"), *MaterialPath);
		return false;
	}

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeService", "SetMaterial", "Set Landscape Material"));

	Landscape->Modify();
	Landscape->LandscapeMaterial = Material;
	Landscape->PostEditChange();

	// Refresh components
	for (ULandscapeComponent* Component : Landscape->LandscapeComponents)
	{
		if (Component)
		{
			Component->MarkRenderStateDirty();
		}
	}

	UE_LOG(LogTemp, Log, TEXT("ULandscapeService::SetLandscapeMaterial: Set material '%s' on landscape '%s'"),
		*MaterialPath, *LandscapeNameOrLabel);
	return true;
}

FString ULandscapeService::GetLandscapeProperty(
	const FString& LandscapeNameOrLabel,
	const FString& PropertyName)
{
	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::GetLandscapeProperty: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return FString();
	}

	// Handle common transform properties via getter methods (these live on USceneComponent, not AActor)
	if (PropertyName.Equals(TEXT("RelativeScale3D"), ESearchCase::IgnoreCase) ||
		PropertyName.Equals(TEXT("Scale"), ESearchCase::IgnoreCase) ||
		PropertyName.Equals(TEXT("ActorScale3D"), ESearchCase::IgnoreCase))
	{
		FVector Scale = Landscape->GetActorScale3D();
		return FString::Printf(TEXT("X=%f Y=%f Z=%f"), Scale.X, Scale.Y, Scale.Z);
	}
	if (PropertyName.Equals(TEXT("RelativeLocation"), ESearchCase::IgnoreCase) ||
		PropertyName.Equals(TEXT("Location"), ESearchCase::IgnoreCase) ||
		PropertyName.Equals(TEXT("ActorLocation"), ESearchCase::IgnoreCase))
	{
		FVector Loc = Landscape->GetActorLocation();
		return FString::Printf(TEXT("X=%f Y=%f Z=%f"), Loc.X, Loc.Y, Loc.Z);
	}
	if (PropertyName.Equals(TEXT("RelativeRotation"), ESearchCase::IgnoreCase) ||
		PropertyName.Equals(TEXT("Rotation"), ESearchCase::IgnoreCase) ||
		PropertyName.Equals(TEXT("ActorRotation"), ESearchCase::IgnoreCase))
	{
		FRotator Rot = Landscape->GetActorRotation();
		return FString::Printf(TEXT("Pitch=%f Yaw=%f Roll=%f"), Rot.Pitch, Rot.Yaw, Rot.Roll);
	}

	// Search on the actor class
	FProperty* Property = Landscape->GetClass()->FindPropertyByName(FName(*PropertyName));
	UObject* Container = Landscape;

	// If not found on actor, also check the root component
	if (!Property && Landscape->GetRootComponent())
	{
		Property = Landscape->GetRootComponent()->GetClass()->FindPropertyByName(FName(*PropertyName));
		if (Property)
		{
			Container = Landscape->GetRootComponent();
		}
	}

	if (!Property)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::GetLandscapeProperty: Property '%s' not found"), *PropertyName);
		return FString();
	}

	FString Value;
	Property->ExportTextItem_Direct(Value, Property->ContainerPtrToValuePtr<void>(Container), nullptr, Cast<UObject>(Container), PPF_None);
	return Value;
}

bool ULandscapeService::SetLandscapeProperty(
	const FString& LandscapeNameOrLabel,
	const FString& PropertyName,
	const FString& Value)
{
	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::SetLandscapeProperty: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return false;
	}

	FProperty* Property = Landscape->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Property)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::SetLandscapeProperty: Property '%s' not found"), *PropertyName);
		return false;
	}

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeService", "SetProperty", "Set Landscape Property"));
	Landscape->Modify();

	const TCHAR* ValuePtr = *Value;
	Property->ImportText_Direct(ValuePtr, Property->ContainerPtrToValuePtr<void>(Landscape), Landscape, PPF_None);
	Landscape->PostEditChange();

	return true;
}

// =================================================================
// Visibility & Collision
// =================================================================

bool ULandscapeService::SetLandscapeVisibility(
	const FString& LandscapeNameOrLabel,
	bool bVisible)
{
	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::SetLandscapeVisibility: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return false;
	}

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeService", "SetVisibility", "Set Landscape Visibility"));
	Landscape->Modify();
	Landscape->SetIsTemporarilyHiddenInEditor(!bVisible);

	return true;
}

bool ULandscapeService::SetLandscapeCollision(
	const FString& LandscapeNameOrLabel,
	bool bEnableCollision)
{
	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::SetLandscapeCollision: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return false;
	}

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeService", "SetCollision", "Set Landscape Collision"));
	Landscape->Modify();

	Landscape->SetActorEnableCollision(bEnableCollision);
	Landscape->PostEditChange();

	UE_LOG(LogTemp, Log, TEXT("ULandscapeService::SetLandscapeCollision: Set collision %s on '%s'"),
		bEnableCollision ? TEXT("enabled") : TEXT("disabled"), *LandscapeNameOrLabel);
	return true;
}

// =================================================================
// Existence Checks
// =================================================================

bool ULandscapeService::LandscapeExists(const FString& LandscapeNameOrLabel)
{
	return FindLandscapeByIdentifier(LandscapeNameOrLabel) != nullptr;
}

bool ULandscapeService::LayerExists(
	const FString& LandscapeNameOrLabel,
	const FString& LayerName)
{
	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		return false;
	}

	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	if (!Info)
	{
		return false;
	}

	for (const FLandscapeInfoLayerSettings& LayerSettings : Info->Layers)
	{
		if (LayerSettings.GetLayerName().ToString().Equals(LayerName, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}
