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
#include "LandscapeEditLayer.h"
#include "LandscapeEditorModule.h"
#include "LandscapeFileFormatInterface.h"
#include "EditorAssetLibrary.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ScopedTransaction.h"
#include "LandscapeSplinesComponent.h"
#include "LandscapeSplineControlPoint.h"
#include "LandscapeSplineSegment.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"

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

/**
 * Resolve a valid editing layer GUID for the given landscape.
 * GetEditingLayer() can return an invalid GUID on freshly created landscapes
 * because nothing has explicitly called SetEditingLayer(). The Landscape editor UI
 * always has a selected layer (GetCurrentEditLayerConst()), so we replicate that
 * by falling back to the first available edit layer.
 */
static FGuid ResolveEditLayerGuid(ALandscape* Landscape)
{
	FGuid LayerGuid = Landscape->GetEditingLayer();
	if (!LayerGuid.IsValid())
	{
		const TArray<ULandscapeEditLayerBase*> EditLayers = Landscape->GetEditLayers();
		if (EditLayers.Num() > 0 && EditLayers[0])
		{
			LayerGuid = EditLayers[0]->GetGuid();
			UE_LOG(LogTemp, Log, TEXT("ResolveEditLayerGuid: Falling back to first edit layer '%s' GUID=%s"),
				*EditLayers[0]->GetFName().ToString(), *LayerGuid.ToString());
		}
	}
	return LayerGuid;
}

void ULandscapeService::UpdateLandscapeAfterHeightEdit(ALandscapeProxy* Landscape)
{
	if (!Landscape)
	{
		return;
	}

	UWorld* World = Landscape->GetWorld();
	if (!World)
	{
		return;
	}

	// Update every proxy that belongs to this landscape GUID. In partitioned levels,
	// components can be distributed across proxies, so updating only one actor can leave
	// terrain in a partially refreshed state.
	const FGuid LandscapeGuid = Landscape->GetLandscapeGuid();
	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		ALandscapeProxy* Proxy = *It;
		if (!Proxy || Proxy->GetLandscapeGuid() != LandscapeGuid)
		{
			continue;
		}

		for (ULandscapeComponent* Component : Proxy->LandscapeComponents)
		{
			if (!Component)
			{
				continue;
			}

			ULandscapeHeightfieldCollisionComponent* CollisionComp = Component->GetCollisionComponent();
			if (CollisionComp)
			{
				CollisionComp->RecreateCollision();
			}

			// Refresh material instances so weight map textures stay valid
			// after height edits. Without this, layer queries may find 0 layers.
			Component->UpdateMaterialInstances();

			Component->MarkRenderStateDirty();
			Component->UpdateComponentToWorld();
		}

		Proxy->MarkPackageDirty();
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

	// Load file data through Unreal's native Landscape file format importer
	// (same import stack used by Landscape UI for PNG/RAW format handling).
	TArray<uint16> ImportedHeightData;
	const FString Extension = FPaths::GetExtension(FilePath, false).ToLower();
	ILandscapeEditorModule& LandscapeEditorModule = FModuleManager::LoadModuleChecked<ILandscapeEditorModule>(TEXT("LandscapeEditor"));
	const ILandscapeHeightmapFileFormat* HeightmapFormat = LandscapeEditorModule.GetHeightmapFormatByExtension(*FString::Printf(TEXT(".%s"), *Extension));

	if (HeightmapFormat)
	{
		const FLandscapeImportData<uint16> ImportData = HeightmapFormat->Import(*FilePath, FLandscapeFileResolution(SizeX, SizeY));
		if (ImportData.ResultCode == ELandscapeImportResult::Error)
		{
			UE_LOG(LogTemp, Error, TEXT("ULandscapeService::ImportHeightmap: Native import failed for '%s': %s"),
				*FilePath, *ImportData.ErrorMessage.ToString());
			return false;
		}

		if (ImportData.Data.Num() != SizeX * SizeY)
		{
			UE_LOG(LogTemp, Error, TEXT("ULandscapeService::ImportHeightmap: Native import size mismatch. Expected %d samples, got %d samples"),
				SizeX * SizeY, ImportData.Data.Num());
			return false;
		}

		ImportedHeightData = ImportData.Data;
	}
	else
	{
		TArray<uint8> FileData;
		if (!FFileHelper::LoadFileToArray(FileData, *FilePath))
		{
			UE_LOG(LogTemp, Error, TEXT("ULandscapeService::ImportHeightmap: Failed to load RAW file '%s'"), *FilePath);
			return false;
		}

		if (FileData.Num() != ExpectedBytes)
		{
			UE_LOG(LogTemp, Error, TEXT("ULandscapeService::ImportHeightmap: RAW file size mismatch. Expected %d bytes for %dx%d landscape, got %d bytes"),
				ExpectedBytes, SizeX, SizeY, FileData.Num());
			return false;
		}

		ImportedHeightData.SetNumUninitialized(SizeX * SizeY);
		FMemory::Memcpy(ImportedHeightData.GetData(), FileData.GetData(), ExpectedBytes);
	}

	// Write the uint16 data directly to the landscape via FHeightmapAccessor,
	// matching the exact path used by the Landscape editor UI Import button
	// (FEdModeLandscape::ImportHeightData → FHeightmapAccessor<false>::SetData).
	// We intentionally do NOT convert uint16→float→uint16 through SetHeightInRegion
	// because the round-trip introduces floating-point precision errors.
	{
		FScopedTransaction Transaction(NSLOCTEXT("LandscapeService", "ImportHeightmap", "Import Heightmap"));

		const FGuid EditLayerGuid = ResolveEditLayerGuid(Landscape);

		FScopedSetLandscapeEditingLayer EditLayerScope(
			Landscape,
			EditLayerGuid,
			[Landscape]()
			{
				if (Landscape)
				{
					Landscape->RequestLayersContentUpdate(ELandscapeLayerUpdateMode::Update_Heightmap_All);
				}
			});

		FHeightmapAccessor<false> HeightmapAccessor(LandscapeInfo);
		HeightmapAccessor.SetData(MinX, MinY, MaxX, MaxY, ImportedHeightData.GetData());
		HeightmapAccessor.Flush();
	}

	// Only update heightmap — do NOT call ForceLayersFullUpdate() which would
	// also resolve weightmap layers and potentially zero out paint weights.
	Landscape->RequestLayersContentUpdate(ELandscapeLayerUpdateMode::Update_Heightmap_All);
	UpdateLandscapeAfterHeightEdit(Landscape);

	UE_LOG(LogTemp, Log, TEXT("ULandscapeService::ImportHeightmap: Imported %s heightmap to '%s' (%dx%d)"),
		*Extension.ToUpper(), *LandscapeNameOrLabel, SizeX, SizeY);
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

	// Export as PNG by default (if no extension or .png), with RAW fallback based on extension.
	FString FinalOutputPath = OutputFilePath;
	const FString Extension = FPaths::GetExtension(OutputFilePath, false).ToLower();
	const bool bExportPng = Extension.IsEmpty() || Extension == TEXT("png");

	if (bExportPng && Extension.IsEmpty())
	{
		FinalOutputPath += TEXT(".png");
	}

	// Use the native Landscape export path (same core path used by the editor UI)
	LandscapeInfo->ExportHeightmap(FinalOutputPath);
	if (!FPaths::FileExists(FinalOutputPath))
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::ExportHeightmap: Native export did not produce file '%s'"), *FinalOutputPath);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("ULandscapeService::ExportHeightmap: Exported %s heightmap from '%s' (%dx%d) to '%s'"),
		bExportPng ? TEXT("PNG") : TEXT("RAW"), *LandscapeNameOrLabel, SizeX, SizeY, *FinalOutputPath);

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

TArray<float> ULandscapeService::GetHeightInRegion(
	const FString& LandscapeNameOrLabel,
	int32 StartX, int32 StartY,
	int32 SizeX, int32 SizeY)
{
	TArray<float> Result;

	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::GetHeightInRegion: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return Result;
	}

	if (SizeX <= 0 || SizeY <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::GetHeightInRegion: Invalid region size %dx%d"), SizeX, SizeY);
		return Result;
	}

	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
	if (!LandscapeInfo)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::GetHeightInRegion: No landscape info"));
		return Result;
	}

	int32 EndX = StartX + SizeX - 1;
	int32 EndY = StartY + SizeY - 1;

	// Read raw uint16 height data
	TArray<uint16> HeightData;
	HeightData.SetNumUninitialized(SizeX * SizeY);

	FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
	LandscapeEdit.GetHeightData(StartX, StartY, EndX, EndY, HeightData.GetData(), 0);

	// Convert uint16 to world-space float heights
	FVector LandscapeLocation = Landscape->GetActorLocation();
	float ZScale = Landscape->GetActorScale3D().Z;
	float LandscapeZScale = LANDSCAPE_ZSCALE;

	Result.SetNumUninitialized(SizeX * SizeY);
	for (int32 i = 0; i < HeightData.Num(); i++)
	{
		Result[i] = LandscapeLocation.Z + (static_cast<float>(HeightData[i]) - 32768.0f) * LandscapeZScale * ZScale;
	}

	UE_LOG(LogTemp, Log, TEXT("ULandscapeService::GetHeightInRegion: Read %d heights from region (%d,%d)-(%d,%d)"),
		Result.Num(), StartX, StartY, EndX, EndY);
	return Result;
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
		float NormalizedHeight = (Heights[i] - Landscape->GetActorLocation().Z) / (LandscapeZScale * ZScale);
		int32 UintHeight = FMath::RoundToInt(NormalizedHeight + 32768.0f);
		HeightData[i] = static_cast<uint16>(FMath::Clamp(UintHeight, 0, 65535));
	}

	const FGuid EditLayerGuid = ResolveEditLayerGuid(Landscape);

	// Scope the HeightmapAccessor so its destructor flushes and releases the
	// heightmap texture write lock before UpdateLandscapeAfterHeightEdit
	// triggers UpdateMaterialInstances / texture compression.
	{
		FScopedSetLandscapeEditingLayer EditLayerScope(
			Landscape,
			EditLayerGuid,
			[Landscape]()
			{
				if (Landscape)
				{
					Landscape->RequestLayersContentUpdate(ELandscapeLayerUpdateMode::Update_Heightmap_All);
				}
			});

		FHeightmapAccessor<false> HeightmapAccessor(LandscapeInfo);
		HeightmapAccessor.SetData(StartX, StartY, StartX + SizeX - 1, StartY + SizeY - 1, HeightData.GetData());
		HeightmapAccessor.Flush();
	} // ~FHeightmapAccessor: flushes and releases heightmap texture write lock

	// Only update heightmap — do NOT call ForceLayersFullUpdate() which would
	// also resolve weightmap layers. If the edit layer has no stored weight data,
	// a full resolve zeroes out all paint layer weights.
	Landscape->RequestLayersContentUpdate(ELandscapeLayerUpdateMode::Update_Heightmap_All);

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

	// Read current height data (merged view across all edit layers)
	TArray<uint16> HeightData;
	HeightData.SetNumUninitialized(SizeX * SizeY);

	int32 SaturatedCount = 0;

	{
		FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
		LandscapeEdit.GetHeightData(MinX, MinY, MaxX, MaxY, HeightData.GetData(), 0);
	} // ~FLandscapeEditDataInterface: release read lock

	// Apply brush
	// Convert world-space height delta to uint16 heightmap delta
	// UE mapping: WorldHeight = (HeightValue - 32768) * LANDSCAPE_ZSCALE * ActorScale.Z
	// So: HeightDelta_uint16 = WorldDelta / (LANDSCAPE_ZSCALE * ActorScale.Z)
	float ZScale = LandscapeScale.Z;
	float StrengthInUnits = Strength / (LANDSCAPE_ZSCALE * ZScale);

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

	// Write using edit-layer-aware path to preserve paint layer weights.
	// Using FLandscapeEditDataInterface::SetHeightData bypasses edit layers and
	// causes a full layer resolve that zeroes out all weightmap data.
	const FGuid EditLayerGuid = ResolveEditLayerGuid(Landscape);
	{
		FScopedSetLandscapeEditingLayer EditLayerScope(
			Landscape,
			EditLayerGuid,
			[Landscape]()
			{
				if (Landscape)
				{
					Landscape->RequestLayersContentUpdate(ELandscapeLayerUpdateMode::Update_Heightmap_All);
				}
			});

		FHeightmapAccessor<false> HeightmapAccessor(LandscapeInfo);
		HeightmapAccessor.SetData(MinX, MinY, MaxX, MaxY, HeightData.GetData());
		HeightmapAccessor.Flush();
	} // ~FHeightmapAccessor: flushes and releases heightmap texture write lock

	// Only update heightmap — do NOT call ForceLayersFullUpdate() which would
	// also resolve weightmap layers. If the edit layer has no stored weight data,
	// a full resolve zeroes out all paint layer weights.
	Landscape->RequestLayersContentUpdate(ELandscapeLayerUpdateMode::Update_Heightmap_All);

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

	// Read current heights (merged view across all edit layers)
	{
		FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
		LandscapeEdit.GetHeightData(MinX, MinY, MaxX, MaxY, HeightData.GetData(), 0);
	} // ~FLandscapeEditDataInterface: release read lock

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

	// Write using edit-layer-aware path to preserve paint layer weights.
	const FGuid EditLayerGuid = ResolveEditLayerGuid(Landscape);
	{
		FScopedSetLandscapeEditingLayer EditLayerScope(
			Landscape,
			EditLayerGuid,
			[Landscape]()
			{
				if (Landscape)
				{
					Landscape->RequestLayersContentUpdate(ELandscapeLayerUpdateMode::Update_Heightmap_All);
				}
			});

		FHeightmapAccessor<false> HeightmapAccessor(LandscapeInfo);
		HeightmapAccessor.SetData(MinX, MinY, MaxX, MaxY, HeightData.GetData());
		HeightmapAccessor.Flush();
	} // ~FHeightmapAccessor: flushes and releases heightmap texture write lock

	Landscape->RequestLayersContentUpdate(ELandscapeLayerUpdateMode::Update_Heightmap_All);

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

	// Read current heights (merged view across all edit layers)
	{
		FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
		LandscapeEdit.GetHeightData(MinX, MinY, MaxX, MaxY, HeightData.GetData(), 0);
	} // ~FLandscapeEditDataInterface: release read lock

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

	// Write using edit-layer-aware path to preserve paint layer weights.
	const FGuid EditLayerGuid = ResolveEditLayerGuid(Landscape);
	{
		FScopedSetLandscapeEditingLayer EditLayerScope(
			Landscape,
			EditLayerGuid,
			[Landscape]()
			{
				if (Landscape)
				{
					Landscape->RequestLayersContentUpdate(ELandscapeLayerUpdateMode::Update_Heightmap_All);
				}
			});

		FHeightmapAccessor<false> HeightmapAccessor(LandscapeInfo);
		HeightmapAccessor.SetData(MinX, MinY, MaxX, MaxY, SmoothedData.GetData());
		HeightmapAccessor.Flush();
	} // ~FHeightmapAccessor: flushes and releases heightmap texture write lock

	Landscape->RequestLayersContentUpdate(ELandscapeLayerUpdateMode::Update_Heightmap_All);

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

	int32 SaturatedCount = 0;

	// Read current heights (merged view across all edit layers)
	{
		FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
		LandscapeEdit.GetHeightData(MinX, MinY, MaxX, MaxY, HeightData.GetData(), 0);
	} // ~FLandscapeEditDataInterface: release read lock

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

	// Write using edit-layer-aware path to preserve paint layer weights.
	const FGuid EditLayerGuid = ResolveEditLayerGuid(Landscape);
	{
		FScopedSetLandscapeEditingLayer EditLayerScope(
			Landscape,
			EditLayerGuid,
			[Landscape]()
			{
				if (Landscape)
				{
					Landscape->RequestLayersContentUpdate(ELandscapeLayerUpdateMode::Update_Heightmap_All);
				}
			});

		FHeightmapAccessor<false> HeightmapAccessor(LandscapeInfo);
		HeightmapAccessor.SetData(MinX, MinY, MaxX, MaxY, HeightData.GetData());
		HeightmapAccessor.Flush();
	} // ~FHeightmapAccessor: flushes and releases heightmap texture write lock

	Landscape->RequestLayersContentUpdate(ELandscapeLayerUpdateMode::Update_Heightmap_All);

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

	// Scope the edit interface so its destructor flushes and releases the
	// heightmap texture write lock before UpdateLandscapeAfterHeightEdit
	// triggers UpdateMaterialInstances / texture compression.
	{
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
	} // ~FLandscapeEditDataInterface: flushes and releases heightmap texture write lock

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

	// Read from the edit layer (matching paint/set write paths) so that
	// weight changes are visible immediately without waiting for deferred layer resolution.
	FGuid LayerGuid = ResolveEditLayerGuid(Landscape);
	FScopedSetLandscapeEditingLayer EditLayerScope(Landscape, LayerGuid);

	for (const FLandscapeInfoLayerSettings& LayerSettings : Info->Layers)
	{
		if (!LayerSettings.LayerInfoObj)
		{
			continue;
		}

		// Read a single pixel of weight data from the edit layer
		TArray<uint8> WeightData;
		WeightData.SetNumZeroed(1);
		TAlphamapAccessor<true> AlphaAccessor(Info, LayerSettings.LayerInfoObj);
		AlphaAccessor.GetData(LocalX, LocalY, LocalX, LocalY, WeightData.GetData());

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

	// Use edit layer system (same fix as heightmap writing)
	FGuid LayerGuid = ResolveEditLayerGuid(Landscape);
	FScopedSetLandscapeEditingLayer EditLayerScope(Landscape, LayerGuid);

	// Scope the TAlphamapAccessor so its destructor releases the texture write
	// lock before any subsequent layer resolve / texture compression.
	{
		// Use TAlphamapAccessor which properly handles edit layers (mirrors FHeightmapAccessor pattern)
		TAlphamapAccessor<false> AlphaAccessor(Info, TargetLayer);

		// Read current weight data for the target layer
		TArray<uint8> WeightData;
		WeightData.SetNumZeroed(SizeX * SizeY);
		AlphaAccessor.GetData(MinX, MinY, MaxX, MaxY, WeightData.GetData());

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

		// Write weight data through the edit layer system
		AlphaAccessor.SetData(MinX, MinY, MaxX, MaxY, WeightData.GetData(), ELandscapeLayerPaintingRestriction::None);
		AlphaAccessor.Flush();
	} // ~TAlphamapAccessor: releases texture write lock

	// NOTE: ForceLayersFullUpdate() is intentionally NOT called here to allow
	// batching multiple paint strokes. Call UpdateLandscapeAfterHeightEdit()
	// or trigger a layer update after completing all paint operations.

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

// =================================================================
// Internal Helpers (file-local)
// =================================================================

/** Find the ULandscapeLayerInfoObject for the given layer name, or nullptr */
static ULandscapeLayerInfoObject* FindLayerInfoByName(ULandscapeInfo* Info, const FString& LayerName)
{
	if (!Info)
	{
		return nullptr;
	}
	for (const FLandscapeInfoLayerSettings& LayerSettings : Info->Layers)
	{
		if (LayerSettings.LayerInfoObj &&
			LayerSettings.LayerInfoObj->GetLayerName().ToString().Equals(LayerName, ESearchCase::IgnoreCase))
		{
			return LayerSettings.LayerInfoObj;
		}
	}
	return nullptr;
}

/** Bilinear sample from a flat float array */
static float BilinearSampleFloat(const TArray<float>& Data, int32 Width, int32 Height, float U, float V)
{
	float X = U * static_cast<float>(Width - 1);
	float Y = V * static_cast<float>(Height - 1);
	int32 X0 = FMath::FloorToInt(X);
	int32 Y0 = FMath::FloorToInt(Y);
	int32 X1 = FMath::Min(X0 + 1, Width - 1);
	int32 Y1 = FMath::Min(Y0 + 1, Height - 1);
	float Fx = X - static_cast<float>(X0);
	float Fy = Y - static_cast<float>(Y0);

	float TL = Data[Y0 * Width + X0];
	float TR = Data[Y0 * Width + X1];
	float BL = Data[Y1 * Width + X0];
	float BR = Data[Y1 * Width + X1];
	return FMath::Lerp(FMath::Lerp(TL, TR, Fx), FMath::Lerp(BL, BR, Fx), Fy);
}

/** Get or create the ULandscapeSplinesComponent on a landscape */
static ULandscapeSplinesComponent* GetOrCreateSplinesComponent(ALandscape* Landscape)
{
	if (!Landscape)
	{
		return nullptr;
	}

	ULandscapeSplinesComponent* SplinesComp = Landscape->GetSplinesComponent();
	if (!SplinesComp)
	{
		Landscape->Modify();
		Landscape->CreateSplineComponent();
		SplinesComp = Landscape->GetSplinesComponent();
	}

	return SplinesComp;
}

// =================================================================
// Batch Painting Operations
// =================================================================

bool ULandscapeService::PaintLayerInRegion(
	const FString& LandscapeNameOrLabel,
	const FString& LayerName,
	int32 StartX, int32 StartY,
	int32 SizeX, int32 SizeY,
	float Strength)
{
	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::PaintLayerInRegion: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return false;
	}

	if (SizeX <= 0 || SizeY <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::PaintLayerInRegion: Invalid region size %dx%d"), SizeX, SizeY);
		return false;
	}

	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	if (!Info)
	{
		return false;
	}

	ULandscapeLayerInfoObject* TargetLayer = FindLayerInfoByName(Info, LayerName);
	if (!TargetLayer)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::PaintLayerInRegion: Layer '%s' not found"), *LayerName);
		return false;
	}

	int32 EndX = StartX + SizeX - 1;
	int32 EndY = StartY + SizeY - 1;

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeService", "PaintLayerInRegion", "Paint Layer In Region"));

	FGuid LayerGuid = ResolveEditLayerGuid(Landscape);
	FScopedSetLandscapeEditingLayer EditLayerScope(Landscape, LayerGuid);

	// Scope the TAlphamapAccessor so its destructor releases the texture write
	// lock before ForceLayersFullUpdate() triggers texture compression.
	{
		TAlphamapAccessor<false> AlphaAccessor(Info, TargetLayer);

		// Build flat array at the requested strength
		uint8 WeightVal = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Strength * 255.0f), 0, 255));
		TArray<uint8> WeightData;
		WeightData.Init(WeightVal, SizeX * SizeY);

		AlphaAccessor.SetData(StartX, StartY, EndX, EndY, WeightData.GetData(), ELandscapeLayerPaintingRestriction::None);
		AlphaAccessor.Flush();
	} // ~TAlphamapAccessor: releases texture write lock

	Info->ForceLayersFullUpdate();

	UE_LOG(LogTemp, Log, TEXT("ULandscapeService::PaintLayerInRegion: Painted '%s' in region (%d,%d)-(%d,%d) strength=%.2f"),
		*LayerName, StartX, StartY, EndX, EndY, Strength);
	return true;
}

bool ULandscapeService::PaintLayerInWorldRect(
	const FString& LandscapeNameOrLabel,
	const FString& LayerName,
	float WorldMinX, float WorldMinY,
	float WorldMaxX, float WorldMaxY,
	float Strength)
{
	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::PaintLayerInWorldRect: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return false;
	}

	FVector LandscapeLocation = Landscape->GetActorLocation();
	FVector LandscapeScale = Landscape->GetActorScale3D();

	int32 StartX = FMath::FloorToInt((WorldMinX - LandscapeLocation.X) / LandscapeScale.X);
	int32 StartY = FMath::FloorToInt((WorldMinY - LandscapeLocation.Y) / LandscapeScale.Y);
	int32 EndX   = FMath::CeilToInt((WorldMaxX - LandscapeLocation.X) / LandscapeScale.X);
	int32 EndY   = FMath::CeilToInt((WorldMaxY - LandscapeLocation.Y) / LandscapeScale.Y);

	// Clamp to landscape extent
	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	if (Info)
	{
		int32 LandMinX, LandMinY, LandMaxX, LandMaxY;
		if (Info->GetLandscapeExtent(LandMinX, LandMinY, LandMaxX, LandMaxY))
		{
			StartX = FMath::Max(StartX, LandMinX);
			StartY = FMath::Max(StartY, LandMinY);
			EndX   = FMath::Min(EndX,   LandMaxX);
			EndY   = FMath::Min(EndY,   LandMaxY);
		}
	}

	int32 SizeX = EndX - StartX + 1;
	int32 SizeY = EndY - StartY + 1;

	if (SizeX <= 0 || SizeY <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::PaintLayerInWorldRect: World rect is outside landscape bounds"));
		return false;
	}

	return PaintLayerInRegion(LandscapeNameOrLabel, LayerName, StartX, StartY, SizeX, SizeY, Strength);
}

// =================================================================
// Weight Map Import / Export
// =================================================================

TArray<float> ULandscapeService::GetWeightsInRegion(
	const FString& LandscapeNameOrLabel,
	const FString& LayerName,
	int32 StartX, int32 StartY,
	int32 SizeX, int32 SizeY)
{
	TArray<float> Result;

	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::GetWeightsInRegion: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return Result;
	}

	if (SizeX <= 0 || SizeY <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::GetWeightsInRegion: Invalid region size %dx%d"), SizeX, SizeY);
		return Result;
	}

	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	if (!Info)
	{
		return Result;
	}

	ULandscapeLayerInfoObject* TargetLayer = FindLayerInfoByName(Info, LayerName);
	if (!TargetLayer)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::GetWeightsInRegion: Layer '%s' not found"), *LayerName);
		return Result;
	}

	int32 EndX = StartX + SizeX - 1;
	int32 EndY = StartY + SizeY - 1;

	TArray<uint8> WeightData;
	WeightData.SetNumZeroed(SizeX * SizeY);

	// Read from the edit layer (matching SetWeightsInRegion's write path) so that
	// weight changes are visible immediately without waiting for deferred layer resolution.
	FGuid LayerGuid = ResolveEditLayerGuid(Landscape);
	FScopedSetLandscapeEditingLayer EditLayerScope(Landscape, LayerGuid);

	TAlphamapAccessor<true> AlphaAccessor(Info, TargetLayer);
	AlphaAccessor.GetData(StartX, StartY, EndX, EndY, WeightData.GetData());

	Result.SetNumUninitialized(SizeX * SizeY);
	for (int32 i = 0; i < WeightData.Num(); i++)
	{
		Result[i] = WeightData[i] / 255.0f;
	}

	return Result;
}

bool ULandscapeService::SetWeightsInRegion(
	const FString& LandscapeNameOrLabel,
	const FString& LayerName,
	int32 StartX, int32 StartY,
	int32 SizeX, int32 SizeY,
	const TArray<float>& Weights)
{
	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::SetWeightsInRegion: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return false;
	}

	if (Weights.Num() != SizeX * SizeY)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::SetWeightsInRegion: Array size %d doesn't match %dx%d=%d"),
			Weights.Num(), SizeX, SizeY, SizeX * SizeY);
		return false;
	}

	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	if (!Info)
	{
		return false;
	}

	ULandscapeLayerInfoObject* TargetLayer = FindLayerInfoByName(Info, LayerName);
	if (!TargetLayer)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::SetWeightsInRegion: Layer '%s' not found"), *LayerName);
		return false;
	}

	int32 EndX = StartX + SizeX - 1;
	int32 EndY = StartY + SizeY - 1;

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeService", "SetWeightsInRegion", "Set Weights In Region"));

	FGuid LayerGuid = ResolveEditLayerGuid(Landscape);
	FScopedSetLandscapeEditingLayer EditLayerScope(Landscape, LayerGuid);

	// Scope the TAlphamapAccessor so its destructor releases the texture write
	// lock before ForceLayersFullUpdate() triggers texture compression.
	{
		TAlphamapAccessor<false> AlphaAccessor(Info, TargetLayer);

		TArray<uint8> WeightData;
		WeightData.SetNumUninitialized(SizeX * SizeY);
		for (int32 i = 0; i < Weights.Num(); i++)
		{
			WeightData[i] = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Weights[i] * 255.0f), 0, 255));
		}

		AlphaAccessor.SetData(StartX, StartY, EndX, EndY, WeightData.GetData(), ELandscapeLayerPaintingRestriction::None);
		AlphaAccessor.Flush();
	} // ~TAlphamapAccessor: releases texture write lock

	Info->ForceLayersFullUpdate();

	UE_LOG(LogTemp, Log, TEXT("ULandscapeService::SetWeightsInRegion: Set %d weights in region (%d,%d)-(%d,%d)"),
		Weights.Num(), StartX, StartY, EndX, EndY);
	return true;
}

FWeightMapExportResult ULandscapeService::ExportWeightMap(
	const FString& LandscapeNameOrLabel,
	const FString& LayerName,
	const FString& OutputFilePath)
{
	FWeightMapExportResult ExportResult;

	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		ExportResult.ErrorMessage = FString::Printf(TEXT("Landscape '%s' not found"), *LandscapeNameOrLabel);
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::ExportWeightMap: %s"), *ExportResult.ErrorMessage);
		return ExportResult;
	}

	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	if (!Info)
	{
		ExportResult.ErrorMessage = TEXT("No landscape info");
		return ExportResult;
	}

	ULandscapeLayerInfoObject* TargetLayer = FindLayerInfoByName(Info, LayerName);
	if (!TargetLayer)
	{
		ExportResult.ErrorMessage = FString::Printf(TEXT("Layer '%s' not found"), *LayerName);
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::ExportWeightMap: %s"), *ExportResult.ErrorMessage);
		return ExportResult;
	}

	int32 MinX, MinY, MaxX, MaxY;
	if (!Info->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
	{
		ExportResult.ErrorMessage = TEXT("Failed to get landscape extent");
		return ExportResult;
	}

	int32 SizeX = MaxX - MinX + 1;
	int32 SizeY = MaxY - MinY + 1;

	TArray<uint8> WeightData;
	WeightData.SetNumZeroed(SizeX * SizeY);

	FLandscapeEditDataInterface LandscapeEdit(Info);
	LandscapeEdit.GetWeightData(TargetLayer, MinX, MinY, MaxX, MaxY, WeightData.GetData(), 0);

	// Write as 8-bit grayscale PNG using IImageWrapper
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	if (!ImageWrapper.IsValid())
	{
		ExportResult.ErrorMessage = TEXT("Failed to create PNG image wrapper");
		return ExportResult;
	}

	ImageWrapper->SetRaw(WeightData.GetData(), WeightData.Num(), SizeX, SizeY, ERGBFormat::Gray, 8);

	const TArray<uint8, FDefaultAllocator64>& CompressedData64 = ImageWrapper->GetCompressed(0);
	TArray<uint8> CompressedData;
	CompressedData.Append(CompressedData64.GetData(), (int32)CompressedData64.Num());
	if (!FFileHelper::SaveArrayToFile(CompressedData, *OutputFilePath))
	{
		ExportResult.ErrorMessage = FString::Printf(TEXT("Failed to write file '%s'"), *OutputFilePath);
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::ExportWeightMap: %s"), *ExportResult.ErrorMessage);
		return ExportResult;
	}

	ExportResult.bSuccess = true;
	ExportResult.FilePath = OutputFilePath;
	ExportResult.Width = SizeX;
	ExportResult.Height = SizeY;

	UE_LOG(LogTemp, Log, TEXT("ULandscapeService::ExportWeightMap: Exported layer '%s' to '%s' (%dx%d)"),
		*LayerName, *OutputFilePath, SizeX, SizeY);
	return ExportResult;
}

FWeightMapImportResult ULandscapeService::ImportWeightMap(
	const FString& LandscapeNameOrLabel,
	const FString& LayerName,
	const FString& FilePath)
{
	FWeightMapImportResult ImportResult;

	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		ImportResult.ErrorMessage = FString::Printf(TEXT("Landscape '%s' not found"), *LandscapeNameOrLabel);
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::ImportWeightMap: %s"), *ImportResult.ErrorMessage);
		return ImportResult;
	}

	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	if (!Info)
	{
		ImportResult.ErrorMessage = TEXT("No landscape info");
		return ImportResult;
	}

	ULandscapeLayerInfoObject* TargetLayer = FindLayerInfoByName(Info, LayerName);
	if (!TargetLayer)
	{
		ImportResult.ErrorMessage = FString::Printf(TEXT("Layer '%s' not found"), *LayerName);
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::ImportWeightMap: %s"), *ImportResult.ErrorMessage);
		return ImportResult;
	}

	int32 MinX, MinY, MaxX, MaxY;
	if (!Info->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
	{
		ImportResult.ErrorMessage = TEXT("Failed to get landscape extent");
		return ImportResult;
	}

	int32 SizeX = MaxX - MinX + 1;
	int32 SizeY = MaxY - MinY + 1;

	// Load file
	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *FilePath))
	{
		ImportResult.ErrorMessage = FString::Printf(TEXT("Failed to load file '%s'"), *FilePath);
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::ImportWeightMap: %s"), *ImportResult.ErrorMessage);
		return ImportResult;
	}

	// Decode PNG
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(FileData.GetData(), FileData.Num()))
	{
		ImportResult.ErrorMessage = TEXT("Failed to decode PNG file");
		return ImportResult;
	}

	int32 ImgWidth  = ImageWrapper->GetWidth();
	int32 ImgHeight = ImageWrapper->GetHeight();
	if (ImgWidth != SizeX || ImgHeight != SizeY)
	{
		ImportResult.ErrorMessage = FString::Printf(TEXT("Image size %dx%d does not match landscape size %dx%d"),
			ImgWidth, ImgHeight, SizeX, SizeY);
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::ImportWeightMap: %s"), *ImportResult.ErrorMessage);
		return ImportResult;
	}

	TArray<uint8> RawData;
	if (!ImageWrapper->GetRaw(ERGBFormat::Gray, 8, RawData))
	{
		ImportResult.ErrorMessage = TEXT("Failed to extract raw pixels from PNG");
		return ImportResult;
	}

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeService", "ImportWeightMap", "Import Weight Map"));

	FGuid LayerGuid = ResolveEditLayerGuid(Landscape);
	FScopedSetLandscapeEditingLayer EditLayerScope(Landscape, LayerGuid);

	// Scope the TAlphamapAccessor so its destructor releases the texture write
	// lock before ForceLayersFullUpdate() triggers texture compression.
	{
		TAlphamapAccessor<false> AlphaAccessor(Info, TargetLayer);
		AlphaAccessor.SetData(MinX, MinY, MaxX, MaxY, RawData.GetData(), ELandscapeLayerPaintingRestriction::None);
		AlphaAccessor.Flush();
	} // ~TAlphamapAccessor: releases texture write lock

	Info->ForceLayersFullUpdate();

	ImportResult.bSuccess = true;
	ImportResult.VerticesModified = SizeX * SizeY;

	UE_LOG(LogTemp, Log, TEXT("ULandscapeService::ImportWeightMap: Imported '%s' from '%s' (%dx%d = %d vertices)"),
		*LayerName, *FilePath, SizeX, SizeY, ImportResult.VerticesModified);
	return ImportResult;
}

// =================================================================
// Landscape Holes (Visibility Mask)
// =================================================================

bool ULandscapeService::SetHoleAtLocation(
	const FString& LandscapeNameOrLabel,
	float WorldX, float WorldY,
	float BrushRadius,
	bool bCreateHole)
{
	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::SetHoleAtLocation: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return false;
	}

	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	if (!Info)
	{
		return false;
	}

	ULandscapeLayerInfoObject* VisLayer = ALandscapeProxy::VisibilityLayer;
	if (!VisLayer)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::SetHoleAtLocation: VisibilityLayer not initialized"));
		return false;
	}

	FVector LandscapeLocation = Landscape->GetActorLocation();
	FVector LandscapeScale    = Landscape->GetActorScale3D();

	float LocalX      = (WorldX - LandscapeLocation.X) / LandscapeScale.X;
	float LocalY      = (WorldY - LandscapeLocation.Y) / LandscapeScale.Y;
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

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeService", "SetHoleAtLocation", "Set Landscape Hole"));

	FGuid LayerGuid = ResolveEditLayerGuid(Landscape);
	FScopedSetLandscapeEditingLayer EditLayerScope(Landscape, LayerGuid);

	// Scope the TAlphamapAccessor so its destructor releases the texture write
	// lock before ForceLayersFullUpdate() triggers texture compression.
	{
		TAlphamapAccessor<false> AlphaAccessor(Info, VisLayer);

		TArray<uint8> WeightData;
		WeightData.SetNumZeroed(SizeX * SizeY);
		AlphaAccessor.GetData(MinX, MinY, MaxX, MaxY, WeightData.GetData());

		uint8 HoleWeight = bCreateHole ? 255 : 0;

		for (int32 Y = 0; Y < SizeY; Y++)
		{
			for (int32 X = 0; X < SizeX; X++)
			{
				float VertX    = static_cast<float>(MinX + X);
				float VertY    = static_cast<float>(MinY + Y);
				float Distance = FMath::Sqrt(FMath::Square(VertX - LocalX) + FMath::Square(VertY - LocalY));
				if (Distance <= LocalRadius)
				{
					WeightData[Y * SizeX + X] = HoleWeight;
				}
			}
		}

		AlphaAccessor.SetData(MinX, MinY, MaxX, MaxY, WeightData.GetData(), ELandscapeLayerPaintingRestriction::None);
		AlphaAccessor.Flush();
	} // ~TAlphamapAccessor: releases texture write lock

	Info->ForceLayersFullUpdate();

	UE_LOG(LogTemp, Log, TEXT("ULandscapeService::SetHoleAtLocation: %s hole at (%.0f, %.0f) r=%.0f"),
		bCreateHole ? TEXT("Created") : TEXT("Filled"), WorldX, WorldY, BrushRadius);
	return true;
}

bool ULandscapeService::SetHoleInRegion(
	const FString& LandscapeNameOrLabel,
	int32 StartX, int32 StartY,
	int32 SizeX, int32 SizeY,
	bool bCreateHole)
{
	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::SetHoleInRegion: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return false;
	}

	if (SizeX <= 0 || SizeY <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::SetHoleInRegion: Invalid region size %dx%d"), SizeX, SizeY);
		return false;
	}

	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	if (!Info)
	{
		return false;
	}

	ULandscapeLayerInfoObject* VisLayer = ALandscapeProxy::VisibilityLayer;
	if (!VisLayer)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::SetHoleInRegion: VisibilityLayer not initialized"));
		return false;
	}

	int32 EndX = StartX + SizeX - 1;
	int32 EndY = StartY + SizeY - 1;

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeService", "SetHoleInRegion", "Set Landscape Hole In Region"));

	FGuid LayerGuid = ResolveEditLayerGuid(Landscape);
	FScopedSetLandscapeEditingLayer EditLayerScope(Landscape, LayerGuid);

	// Scope the TAlphamapAccessor so its destructor releases the texture write
	// lock before ForceLayersFullUpdate() triggers texture compression.
	{
		TAlphamapAccessor<false> AlphaAccessor(Info, VisLayer);

		uint8 HoleWeight = bCreateHole ? 255 : 0;
		TArray<uint8> WeightData;
		WeightData.Init(HoleWeight, SizeX * SizeY);

		AlphaAccessor.SetData(StartX, StartY, EndX, EndY, WeightData.GetData(), ELandscapeLayerPaintingRestriction::None);
		AlphaAccessor.Flush();
	} // ~TAlphamapAccessor: releases texture write lock

	Info->ForceLayersFullUpdate();

	UE_LOG(LogTemp, Log, TEXT("ULandscapeService::SetHoleInRegion: %s hole in region (%d,%d)-(%d,%d)"),
		bCreateHole ? TEXT("Created") : TEXT("Filled"), StartX, StartY, EndX, EndY);
	return true;
}

bool ULandscapeService::GetHoleAtLocation(
	const FString& LandscapeNameOrLabel,
	float WorldX, float WorldY)
{
	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::GetHoleAtLocation: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return false;
	}

	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	if (!Info)
	{
		return false;
	}

	ULandscapeLayerInfoObject* VisLayer = ALandscapeProxy::VisibilityLayer;
	if (!VisLayer)
	{
		return false;
	}

	FVector LandscapeLocation = Landscape->GetActorLocation();
	FVector LandscapeScale    = Landscape->GetActorScale3D();

	int32 LocalX = FMath::RoundToInt((WorldX - LandscapeLocation.X) / LandscapeScale.X);
	int32 LocalY = FMath::RoundToInt((WorldY - LandscapeLocation.Y) / LandscapeScale.Y);

	TArray<uint8> WeightData;
	WeightData.SetNumZeroed(1);

	// Read from the edit layer (matching SetHoleAtLocation's write path) so that
	// holes are visible immediately without waiting for deferred layer resolution.
	FGuid LayerGuid = ResolveEditLayerGuid(Landscape);
	FScopedSetLandscapeEditingLayer EditLayerScope(Landscape, LayerGuid);

	TAlphamapAccessor<true> AlphaAccessor(Info, VisLayer);
	AlphaAccessor.GetData(LocalX, LocalY, LocalX, LocalY, WeightData.GetData());

	return WeightData[0] > 128;
}

// =================================================================
// Landscape Splines
// =================================================================

FSplineCreateResult ULandscapeService::CreateSplinePoint(
	const FString& LandscapeNameOrLabel,
	FVector WorldLocation,
	float Width,
	float SideFalloff,
	float EndFalloff,
	const FString& PaintLayerName,
	bool bRaiseTerrain,
	bool bLowerTerrain)
{
	FSplineCreateResult CreateResult;

	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		CreateResult.ErrorMessage = FString::Printf(TEXT("Landscape '%s' not found"), *LandscapeNameOrLabel);
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::CreateSplinePoint: %s"), *CreateResult.ErrorMessage);
		return CreateResult;
	}

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeService", "CreateSplinePoint", "Create Spline Point"));
	Landscape->Modify();

	ULandscapeSplinesComponent* SplinesComp = GetOrCreateSplinesComponent(Landscape);
	if (!SplinesComp)
	{
		CreateResult.ErrorMessage = TEXT("Failed to get/create spline component");
		return CreateResult;
	}

	SplinesComp->Modify();

	// Convert world location to landscape-local space
	FTransform LandscapeTransform = Landscape->GetActorTransform();
	FVector LocalLocation = LandscapeTransform.InverseTransformPosition(WorldLocation);

	ULandscapeSplineControlPoint* NewCP = NewObject<ULandscapeSplineControlPoint>(SplinesComp, NAME_None, RF_Transactional);
	NewCP->Location     = LocalLocation;
	NewCP->Width        = Width;
	NewCP->SideFalloff  = SideFalloff;
	NewCP->EndFalloff   = EndFalloff;
	NewCP->LayerName    = FName(*PaintLayerName);
	NewCP->bRaiseTerrain = bRaiseTerrain;
	NewCP->bLowerTerrain = bLowerTerrain;

	CreateResult.PointIndex = SplinesComp->GetControlPoints().Num();
	SplinesComp->GetControlPoints().Add(NewCP);

	// Note: Do NOT call UpdateSplinePoints() here - the point has no connected
	// segments yet, and the update is unnecessary. The editor only calls it for
	// visible mesh rendering. UpdateSplinePoints will be called properly when
	// the point gets connected via ConnectSplinePoints or CreateSplineFromPoints.

	SplinesComp->MarkRenderStateDirty();
	Landscape->MarkPackageDirty();

	CreateResult.bSuccess = true;
	UE_LOG(LogTemp, Log, TEXT("ULandscapeService::CreateSplinePoint: Created point %d at (%.0f,%.0f,%.0f)"),
		CreateResult.PointIndex, WorldLocation.X, WorldLocation.Y, WorldLocation.Z);
	return CreateResult;
}

bool ULandscapeService::ConnectSplinePoints(
	const FString& LandscapeNameOrLabel,
	int32 StartPointIndex,
	int32 EndPointIndex,
	float TangentLength,
	float EndTangentLength,
	const FString& PaintLayerName,
	bool bRaiseTerrain,
	bool bLowerTerrain)
{
	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::ConnectSplinePoints: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return false;
	}

	ULandscapeSplinesComponent* SplinesComp = Landscape->GetSplinesComponent();
	if (!SplinesComp)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::ConnectSplinePoints: No spline component on landscape"));
		return false;
	}

	if (!SplinesComp->GetControlPoints().IsValidIndex(StartPointIndex) ||
		!SplinesComp->GetControlPoints().IsValidIndex(EndPointIndex))
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::ConnectSplinePoints: Invalid point indices %d, %d (have %d points)"),
			StartPointIndex, EndPointIndex, SplinesComp->GetControlPoints().Num());
		return false;
	}

	ULandscapeSplineControlPoint* StartCP = SplinesComp->GetControlPoints()[StartPointIndex];
	ULandscapeSplineControlPoint* EndCP   = SplinesComp->GetControlPoints()[EndPointIndex];

	if (!StartCP || !EndCP)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::ConnectSplinePoints: Null control point"));
		return false;
	}

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeService", "ConnectSplinePoints", "Connect Spline Points"));
	SplinesComp->Modify();
	StartCP->Modify();
	EndCP->Modify();

	// Auto-calculate tangent from distance if not specified (0.0 = sentinel for auto).
	// Non-zero values — including NEGATIVE — are used as-is. Negative tangent lengths
	// are valid in UE and reverse the spline mesh flow direction along the segment.
	float UsedStartTangent = TangentLength;
	if (UsedStartTangent == 0.0f)
	{
		UsedStartTangent = (StartCP->Location - EndCP->Location).Size() * 0.5f;
	}

	// End tangent: 0.0 (default) = negate start tangent, which is the standard UE
	// convention (end tangent points back toward start for proper mesh flow).
	// Non-zero values are used as-is for explicit control.
	float UsedEndTangent = EndTangentLength;
	if (UsedEndTangent == 0.0f)
	{
		UsedEndTangent = -UsedStartTangent;
	}

	ULandscapeSplineSegment* NewSeg = NewObject<ULandscapeSplineSegment>(SplinesComp, NAME_None, RF_Transactional);
	NewSeg->Connections[0].ControlPoint = StartCP;
	NewSeg->Connections[0].TangentLen   = UsedStartTangent;
	NewSeg->Connections[1].ControlPoint = EndCP;
	NewSeg->Connections[1].TangentLen   = UsedEndTangent;
	NewSeg->LayerName    = FName(*PaintLayerName);
	NewSeg->bRaiseTerrain = bRaiseTerrain;
	NewSeg->bLowerTerrain = bLowerTerrain;

	SplinesComp->GetSegments().Add(NewSeg);

	// Add back-references from control points to the segment
	StartCP->ConnectedSegments.Add(FLandscapeSplineConnection(NewSeg, 0));
	EndCP->ConnectedSegments.Add(FLandscapeSplineConnection(NewSeg, 1));

	// Auto-calculate rotations for smooth tangents
	StartCP->AutoCalcRotation(false);
	EndCP->AutoCalcRotation(false);

	// Update control points (which cascades to connected segments automatically).
	// No need to separately call NewSeg->UpdateSplinePoints() since the CP updates
	// propagate to all connected segments. This matches the UE editor's pattern
	// in LandscapeEdModeSplineTools.cpp.
	StartCP->UpdateSplinePoints();
	EndCP->UpdateSplinePoints();

	SplinesComp->MarkRenderStateDirty();
	Landscape->MarkPackageDirty();

	UE_LOG(LogTemp, Log, TEXT("ULandscapeService::ConnectSplinePoints: Connected points %d → %d (start_tan=%.0f, end_tan=%.0f)"),
		StartPointIndex, EndPointIndex, UsedStartTangent, UsedEndTangent);
	return true;
}

FLandscapeSplineInfo ULandscapeService::CreateSplineFromPoints(
	const FString& LandscapeNameOrLabel,
	const TArray<FVector>& WorldLocations,
	float Width,
	float SideFalloff,
	float EndFalloff,
	const FString& PaintLayerName,
	bool bRaiseTerrain,
	bool bLowerTerrain,
	bool bClosedLoop)
{
	FLandscapeSplineInfo SplineInfo;

	if (WorldLocations.Num() < 2)
	{
		SplineInfo.ErrorMessage = TEXT("Need at least 2 points to create a spline");
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::CreateSplineFromPoints: %s"), *SplineInfo.ErrorMessage);
		return SplineInfo;
	}

	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		SplineInfo.ErrorMessage = FString::Printf(TEXT("Landscape '%s' not found"), *LandscapeNameOrLabel);
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::CreateSplineFromPoints: %s"), *SplineInfo.ErrorMessage);
		return SplineInfo;
	}

	// Remember the starting index so we can connect relative indices
	ULandscapeSplinesComponent* SplinesComp = GetOrCreateSplinesComponent(Landscape);
	if (!SplinesComp)
	{
		SplineInfo.ErrorMessage = TEXT("Failed to get/create spline component");
		return SplineInfo;
	}

	int32 BaseIndex = SplinesComp->GetControlPoints().Num();

	// Create all control points
	for (const FVector& Loc : WorldLocations)
	{
		FSplineCreateResult PointResult = CreateSplinePoint(
			LandscapeNameOrLabel, Loc, Width, SideFalloff, EndFalloff, PaintLayerName, bRaiseTerrain, bLowerTerrain);
		if (!PointResult.bSuccess)
		{
			SplineInfo.ErrorMessage = FString::Printf(TEXT("Failed to create control point: %s"), *PointResult.ErrorMessage);
			return SplineInfo;
		}
	}

	// Connect them sequentially
	int32 NumPoints = WorldLocations.Num();
	for (int32 i = 0; i < NumPoints - 1; i++)
	{
		if (!ConnectSplinePoints(LandscapeNameOrLabel, BaseIndex + i, BaseIndex + i + 1, 0.0f, 0.0f, PaintLayerName, bRaiseTerrain, bLowerTerrain))
		{
			SplineInfo.ErrorMessage = FString::Printf(TEXT("Failed to connect points %d → %d"), i, i + 1);
			return SplineInfo;
		}
	}

	// Close loop if requested
	if (bClosedLoop && NumPoints >= 2)
	{
		ConnectSplinePoints(LandscapeNameOrLabel, BaseIndex + NumPoints - 1, BaseIndex, 0.0f, 0.0f, PaintLayerName, bRaiseTerrain, bLowerTerrain);
	}

	// Return the current spline state
	return GetSplineInfo(LandscapeNameOrLabel);
}

FLandscapeSplineInfo ULandscapeService::GetSplineInfo(const FString& LandscapeNameOrLabel)
{
	FLandscapeSplineInfo SplineInfo;

	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		SplineInfo.ErrorMessage = FString::Printf(TEXT("Landscape '%s' not found"), *LandscapeNameOrLabel);
		return SplineInfo;
	}

	ULandscapeSplinesComponent* SplinesComp = Landscape->GetSplinesComponent();
	if (!SplinesComp)
	{
		// No splines yet — return empty but success
		SplineInfo.bSuccess = true;
		return SplineInfo;
	}

	FTransform LandscapeTransform = Landscape->GetActorTransform();

	// Enumerate control points
	for (int32 i = 0; i < SplinesComp->GetControlPoints().Num(); i++)
	{
		ULandscapeSplineControlPoint* CP = SplinesComp->GetControlPoints()[i];
		if (!CP)
		{
			continue;
		}

		FLandscapeSplinePointInfo PointInfo;
		PointInfo.PointIndex    = i;
		PointInfo.Location      = LandscapeTransform.TransformPosition(CP->Location);
		PointInfo.Rotation      = CP->Rotation;
		PointInfo.Width         = CP->Width;
		PointInfo.SideFalloff   = CP->SideFalloff;
		PointInfo.EndFalloff    = CP->EndFalloff;
		PointInfo.LayerName     = CP->LayerName.ToString();
		PointInfo.bRaiseTerrain = CP->bRaiseTerrain;
		PointInfo.bLowerTerrain = CP->bLowerTerrain;

		// Mesh properties on control point
		if (CP->Mesh)
		{
			PointInfo.MeshPath = CP->Mesh->GetPathName();
		}
		PointInfo.MeshScale = CP->MeshScale;
		PointInfo.SegmentMeshOffset = CP->SegmentMeshOffset;

		SplineInfo.ControlPoints.Add(PointInfo);
	}

	// Enumerate segments
	for (int32 i = 0; i < SplinesComp->GetSegments().Num(); i++)
	{
		ULandscapeSplineSegment* Seg = SplinesComp->GetSegments()[i];
		if (!Seg)
		{
			continue;
		}

		// Find start/end point indices
		auto FindCPIndex = [&](ULandscapeSplineControlPoint* CP) -> int32
		{
			for (int32 j = 0; j < SplinesComp->GetControlPoints().Num(); j++)
			{
				if (SplinesComp->GetControlPoints()[j] == CP)
				{
					return j;
				}
			}
			return -1;
		};

		FLandscapeSplineSegmentInfo SegInfo;
		SegInfo.SegmentIndex      = i;
		SegInfo.StartPointIndex   = FindCPIndex(Seg->Connections[0].ControlPoint.Get());
		SegInfo.EndPointIndex     = FindCPIndex(Seg->Connections[1].ControlPoint.Get());
		SegInfo.StartTangentLength = Seg->Connections[0].TangentLen;
		SegInfo.EndTangentLength   = Seg->Connections[1].TangentLen;
		SegInfo.LayerName         = Seg->LayerName.ToString();
		SegInfo.bRaiseTerrain     = Seg->bRaiseTerrain;
		SegInfo.bLowerTerrain     = Seg->bLowerTerrain;

		// Populate spline mesh entries
		for (const FLandscapeSplineMeshEntry& MeshEntry : Seg->SplineMeshes)
		{
			FLandscapeSplineMeshEntryInfo EntryInfo;
			if (MeshEntry.Mesh)
			{
				EntryInfo.MeshPath = MeshEntry.Mesh->GetPathName();
			}
			EntryInfo.Scale = MeshEntry.Scale;
			EntryInfo.bScaleToWidth = MeshEntry.bScaleToWidth;
			EntryInfo.CenterAdjust = MeshEntry.CenterAdjust;
			EntryInfo.ForwardAxis = static_cast<int32>(MeshEntry.ForwardAxis);
			EntryInfo.UpAxis = static_cast<int32>(MeshEntry.UpAxis);

			for (UMaterialInterface* Mat : MeshEntry.MaterialOverrides)
			{
				EntryInfo.MaterialOverridePaths.Add(Mat ? Mat->GetPathName() : TEXT(""));
			}

			SegInfo.SplineMeshes.Add(EntryInfo);
		}

		SplineInfo.Segments.Add(SegInfo);
	}

	SplineInfo.NumControlPoints = SplineInfo.ControlPoints.Num();
	SplineInfo.NumSegments      = SplineInfo.Segments.Num();
	SplineInfo.bSuccess         = true;
	return SplineInfo;
}

bool ULandscapeService::ModifySplinePoint(
	const FString& LandscapeNameOrLabel,
	int32 PointIndex,
	FVector WorldLocation,
	float Width,
	float SideFalloff,
	float EndFalloff,
	const FString& PaintLayerName,
	FRotator Rotation,
	bool bAutoCalcRotation)
{
	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::ModifySplinePoint: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return false;
	}

	ULandscapeSplinesComponent* SplinesComp = Landscape->GetSplinesComponent();
	if (!SplinesComp || !SplinesComp->GetControlPoints().IsValidIndex(PointIndex))
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::ModifySplinePoint: Invalid index %d"), PointIndex);
		return false;
	}

	ULandscapeSplineControlPoint* CP = SplinesComp->GetControlPoints()[PointIndex];
	if (!CP)
	{
		return false;
	}

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeService", "ModifySplinePoint", "Modify Spline Point"));
	CP->Modify();

	FTransform LandscapeTransform = Landscape->GetActorTransform();
	CP->Location = LandscapeTransform.InverseTransformPosition(WorldLocation);

	if (Width >= 0.0f)       CP->Width       = Width;
	if (SideFalloff >= 0.0f) CP->SideFalloff = SideFalloff;
	if (EndFalloff >= 0.0f)  CP->EndFalloff  = EndFalloff;

	if (!PaintLayerName.Equals(TEXT("__unchanged__"), ESearchCase::CaseSensitive))
	{
		CP->LayerName = FName(*PaintLayerName);
	}

	if (bAutoCalcRotation)
	{
		CP->AutoCalcRotation(false);
	}
	else
	{
		// Apply explicit rotation supplied by caller
		CP->Rotation = Rotation;
	}
	CP->UpdateSplinePoints();

	SplinesComp->MarkRenderStateDirty();
	Landscape->MarkPackageDirty();

	UE_LOG(LogTemp, Log, TEXT("ULandscapeService::ModifySplinePoint: Modified point %d (rotation=%s)"),
		PointIndex, bAutoCalcRotation ? TEXT("auto") : *Rotation.ToString());
	return true;
}

bool ULandscapeService::DeleteSplinePoint(
	const FString& LandscapeNameOrLabel,
	int32 PointIndex)
{
	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::DeleteSplinePoint: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return false;
	}

	ULandscapeSplinesComponent* SplinesComp = Landscape->GetSplinesComponent();
	if (!SplinesComp || !SplinesComp->GetControlPoints().IsValidIndex(PointIndex))
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::DeleteSplinePoint: Invalid index %d"), PointIndex);
		return false;
	}

	ULandscapeSplineControlPoint* CP = SplinesComp->GetControlPoints()[PointIndex];
	if (!CP)
	{
		SplinesComp->GetControlPoints().RemoveAt(PointIndex);
		return true;
	}

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeService", "DeleteSplinePoint", "Delete Spline Point"));
	SplinesComp->Modify();
	CP->Modify();

	// Collect connected segments to delete
	TArray<ULandscapeSplineSegment*> SegsToDelete;
	for (const FLandscapeSplineConnection& Conn : CP->ConnectedSegments)
	{
		if (Conn.Segment)
		{
			SegsToDelete.Add(Conn.Segment);
		}
	}

	// Remove each connected segment and its references from the other control point
	for (ULandscapeSplineSegment* Seg : SegsToDelete)
	{
		Seg->Modify();

		// Remove back-reference from the OTHER control point
		for (int32 ConnIdx = 0; ConnIdx < 2; ConnIdx++)
		{
			ULandscapeSplineControlPoint* OtherCP = Seg->Connections[ConnIdx].ControlPoint.Get();
			if (OtherCP && OtherCP != CP)
			{
				OtherCP->Modify();
				OtherCP->ConnectedSegments.RemoveAll([Seg](const FLandscapeSplineConnection& C)
				{
					return C.Segment == Seg;
				});
				OtherCP->UpdateSplinePoints();
			}
		}

		SplinesComp->GetSegments().Remove(Seg);
	}

	SplinesComp->GetControlPoints().RemoveAt(PointIndex);

	SplinesComp->MarkRenderStateDirty();
	Landscape->MarkPackageDirty();

	UE_LOG(LogTemp, Log, TEXT("ULandscapeService::DeleteSplinePoint: Deleted point %d and %d connected segments"),
		PointIndex, SegsToDelete.Num());
	return true;
}

bool ULandscapeService::DeleteAllSplines(const FString& LandscapeNameOrLabel)
{
	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::DeleteAllSplines: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return false;
	}

	ULandscapeSplinesComponent* SplinesComp = Landscape->GetSplinesComponent();
	if (!SplinesComp)
	{
		// Nothing to delete
		return true;
	}

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeService", "DeleteAllSplines", "Delete All Splines"));
	SplinesComp->Modify();

	int32 NumPoints   = SplinesComp->GetControlPoints().Num();
	int32 NumSegments = SplinesComp->GetSegments().Num();

	SplinesComp->GetControlPoints().Empty();
	SplinesComp->GetSegments().Empty();

	SplinesComp->MarkRenderStateDirty();
	Landscape->MarkPackageDirty();

	UE_LOG(LogTemp, Log, TEXT("ULandscapeService::DeleteAllSplines: Cleared %d points and %d segments from '%s'"),
		NumPoints, NumSegments, *LandscapeNameOrLabel);
	return true;
}

bool ULandscapeService::ApplySplinesToLandscape(const FString& LandscapeNameOrLabel)
{
	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::ApplySplinesToLandscape: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return false;
	}

	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
	if (!LandscapeInfo)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::ApplySplinesToLandscape: No landscape info"));
		return false;
	}

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeService", "ApplySplines", "Apply Splines to Landscape"));

	// Set the editing layer so that ApplySplines() can rasterize into the
	// correct heightmap/weightmap layer. Without this, UE5.7 asserts
	// "EditingLayer != nullptr" in LandscapeSplineRaster.cpp:335 and the
	// SEH-caught exception leaves heightmap textures permanently locked.
	const FGuid EditLayerGuid = ResolveEditLayerGuid(Landscape);
	FScopedSetLandscapeEditingLayer EditLayerScope(
		Landscape,
		EditLayerGuid,
		[Landscape]()
		{
			if (Landscape)
			{
				Landscape->RequestLayersContentUpdate(ELandscapeLayerUpdateMode::Update_Heightmap_All);
			}
		});

	// ApplySplines rasterizes terrain deformation and layer painting for all splines
	LandscapeInfo->ApplySplines(nullptr, true);

	LandscapeInfo->ForceLayersFullUpdate();
	UpdateLandscapeAfterHeightEdit(Landscape);

	UE_LOG(LogTemp, Log, TEXT("ULandscapeService::ApplySplinesToLandscape: Applied splines to '%s'"), *LandscapeNameOrLabel);
	return true;
}

bool ULandscapeService::SetSplineSegmentMeshes(
	const FString& LandscapeNameOrLabel,
	int32 SegmentIndex,
	const TArray<FLandscapeSplineMeshEntryInfo>& MeshEntries)
{
	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::SetSplineSegmentMeshes: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return false;
	}

	ULandscapeSplinesComponent* SplinesComp = Landscape->GetSplinesComponent();
	if (!SplinesComp || !SplinesComp->GetSegments().IsValidIndex(SegmentIndex))
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::SetSplineSegmentMeshes: Invalid segment index %d"), SegmentIndex);
		return false;
	}

	ULandscapeSplineSegment* Seg = SplinesComp->GetSegments()[SegmentIndex];
	if (!Seg)
	{
		return false;
	}

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeService", "SetSegmentMeshes", "Set Spline Segment Meshes"));
	Seg->Modify();

	// Clear existing mesh entries and rebuild
	Seg->SplineMeshes.Empty();

	for (const FLandscapeSplineMeshEntryInfo& EntryInfo : MeshEntries)
	{
		FLandscapeSplineMeshEntry NewEntry;

		if (!EntryInfo.MeshPath.IsEmpty())
		{
			UStaticMesh* Mesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *EntryInfo.MeshPath));
			if (Mesh)
			{
				NewEntry.Mesh = Mesh;
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::SetSplineSegmentMeshes: Could not load mesh '%s'"), *EntryInfo.MeshPath);
			}
		}

		NewEntry.Scale = EntryInfo.Scale;
		NewEntry.bScaleToWidth = EntryInfo.bScaleToWidth;
		NewEntry.CenterAdjust = EntryInfo.CenterAdjust;
		NewEntry.ForwardAxis = static_cast<ESplineMeshAxis::Type>(FMath::Clamp(EntryInfo.ForwardAxis, 0, 2));
		NewEntry.UpAxis = static_cast<ESplineMeshAxis::Type>(FMath::Clamp(EntryInfo.UpAxis, 0, 2));

		// Load material overrides
		for (const FString& MatPath : EntryInfo.MaterialOverridePaths)
		{
			if (!MatPath.IsEmpty())
			{
				UMaterialInterface* Mat = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *MatPath));
				NewEntry.MaterialOverrides.Add(Mat);
			}
			else
			{
				NewEntry.MaterialOverrides.Add(nullptr);
			}
		}

		Seg->SplineMeshes.Add(NewEntry);
	}

	// Update the spline mesh components
	Seg->UpdateSplinePoints();

	SplinesComp->MarkRenderStateDirty();
	Landscape->MarkPackageDirty();

	UE_LOG(LogTemp, Log, TEXT("ULandscapeService::SetSplineSegmentMeshes: Set %d mesh entries on segment %d"),
		MeshEntries.Num(), SegmentIndex);
	return true;
}

bool ULandscapeService::SetSplinePointMesh(
	const FString& LandscapeNameOrLabel,
	int32 PointIndex,
	const FString& MeshPath,
	FVector MeshScale,
	float SegmentMeshOffset)
{
	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::SetSplinePointMesh: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return false;
	}

	ULandscapeSplinesComponent* SplinesComp = Landscape->GetSplinesComponent();
	if (!SplinesComp || !SplinesComp->GetControlPoints().IsValidIndex(PointIndex))
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeService::SetSplinePointMesh: Invalid point index %d"), PointIndex);
		return false;
	}

	ULandscapeSplineControlPoint* CP = SplinesComp->GetControlPoints()[PointIndex];
	if (!CP)
	{
		return false;
	}

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeService", "SetPointMesh", "Set Spline Point Mesh"));
	CP->Modify();

	if (MeshPath.IsEmpty())
	{
		CP->Mesh = nullptr;
	}
	else
	{
		UStaticMesh* Mesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *MeshPath));
		if (Mesh)
		{
			CP->Mesh = Mesh;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::SetSplinePointMesh: Could not load mesh '%s'"), *MeshPath);
		}
	}

	CP->MeshScale = MeshScale;
	CP->SegmentMeshOffset = SegmentMeshOffset;

	CP->UpdateSplinePoints();

	SplinesComp->MarkRenderStateDirty();
	Landscape->MarkPackageDirty();

	UE_LOG(LogTemp, Log, TEXT("ULandscapeService::SetSplinePointMesh: Set mesh on point %d (mesh=%s, offset=%.1f)"),
		PointIndex, *MeshPath, SegmentMeshOffset);
	return true;
}

// =================================================================
// Landscape Resize
// =================================================================

FLandscapeCreateResult ULandscapeService::ResizeLandscape(
	const FString& LandscapeNameOrLabel,
	int32 NewComponentCountX,
	int32 NewComponentCountY,
	int32 NewQuadsPerSection,
	int32 NewSectionsPerComponent)
{
	FLandscapeCreateResult FinalResult;

	ALandscape* Landscape = FindLandscapeByIdentifier(LandscapeNameOrLabel);
	if (!Landscape)
	{
		FinalResult.ErrorMessage = FString::Printf(TEXT("Landscape '%s' not found"), *LandscapeNameOrLabel);
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeService::ResizeLandscape: %s"), *FinalResult.ErrorMessage);
		return FinalResult;
	}

	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	if (!Info)
	{
		FinalResult.ErrorMessage = TEXT("No landscape info");
		return FinalResult;
	}

	// --- Snapshot old landscape properties ---
	FVector OldLocation = Landscape->GetActorLocation();
	FRotator OldRotation = Landscape->GetActorRotation();
	FVector OldScale = Landscape->GetActorScale3D();
	FString OldLabel = Landscape->GetActorLabel();
	int32 OldSectionsPerComp = Landscape->NumSubsections;
	int32 OldQuadsPerSection = Landscape->SubsectionSizeQuads;
	FString MaterialPath;
	if (Landscape->GetLandscapeMaterial())
	{
		MaterialPath = Landscape->GetLandscapeMaterial()->GetPathName();
	}

	// Resolve new params (keep old values when -1)
	int32 UsedQuadsPerSection      = (NewQuadsPerSection > 0)      ? NewQuadsPerSection      : OldQuadsPerSection;
	int32 UsedSectionsPerComponent = (NewSectionsPerComponent > 0) ? NewSectionsPerComponent : OldSectionsPerComp;

	// Get current full extent
	int32 MinX, MinY, MaxX, MaxY;
	if (!Info->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
	{
		FinalResult.ErrorMessage = TEXT("Failed to get landscape extent");
		return FinalResult;
	}

	int32 OldSizeX = MaxX - MinX + 1;
	int32 OldSizeY = MaxY - MinY + 1;

	int32 NewComponentSizeQuads = UsedQuadsPerSection * UsedSectionsPerComponent;
	int32 NewSizeX = NewComponentCountX * NewComponentSizeQuads + 1;
	int32 NewSizeY = NewComponentCountY * NewComponentSizeQuads + 1;

	UE_LOG(LogTemp, Log, TEXT("ULandscapeService::ResizeLandscape: Resizing '%s' from %dx%d to %dx%d vertices"),
		*LandscapeNameOrLabel, OldSizeX, OldSizeY, NewSizeX, NewSizeY);

	// --- Export all height data ---
	TArray<float> OldHeights = GetHeightInRegion(LandscapeNameOrLabel, MinX, MinY, OldSizeX, OldSizeY);
	if (OldHeights.Num() != OldSizeX * OldSizeY)
	{
		FinalResult.ErrorMessage = TEXT("Failed to read height data from old landscape");
		return FinalResult;
	}

	// --- Export all layer weight data ---
	TArray<TPair<FString, TArray<float>>> LayerWeights;
	TArray<FString> LayerPaths;
	for (const FLandscapeInfoLayerSettings& LayerSettings : Info->Layers)
	{
		if (!LayerSettings.LayerInfoObj)
		{
			continue;
		}
		FString LName = LayerSettings.LayerInfoObj->GetLayerName().ToString();
		LayerPaths.Add(LayerSettings.LayerInfoObj->GetPathName());
		TArray<float> Weights = GetWeightsInRegion(LandscapeNameOrLabel, LName, MinX, MinY, OldSizeX, OldSizeY);
		LayerWeights.Add(TPair<FString, TArray<float>>(LName, MoveTemp(Weights)));
	}

	// --- Delete old landscape ---
	if (!DeleteLandscape(LandscapeNameOrLabel))
	{
		FinalResult.ErrorMessage = TEXT("Failed to delete old landscape");
		return FinalResult;
	}

	// --- Create new landscape ---
	FLandscapeCreateResult CreateResult = CreateLandscape(
		OldLocation, OldRotation, OldScale,
		UsedSectionsPerComponent, UsedQuadsPerSection,
		NewComponentCountX, NewComponentCountY,
		OldLabel);

	if (!CreateResult.bSuccess)
	{
		FinalResult.ErrorMessage = FString::Printf(TEXT("Failed to create new landscape: %s"), *CreateResult.ErrorMessage);
		return FinalResult;
	}

	FString NewLabel = CreateResult.ActorLabel;

	// Restore material
	if (!MaterialPath.IsEmpty())
	{
		SetLandscapeMaterial(NewLabel, MaterialPath);
	}

	// Restore layers
	for (const FString& LayerPath : LayerPaths)
	{
		AddLayer(NewLabel, LayerPath);
	}

	// --- Bilinear resample and import heights ---
	TArray<float> NewHeights;
	NewHeights.SetNumUninitialized(NewSizeX * NewSizeY);
	for (int32 Y = 0; Y < NewSizeY; Y++)
	{
		for (int32 X = 0; X < NewSizeX; X++)
		{
			float U = (NewSizeX > 1) ? static_cast<float>(X) / static_cast<float>(NewSizeX - 1) : 0.5f;
			float V = (NewSizeY > 1) ? static_cast<float>(Y) / static_cast<float>(NewSizeY - 1) : 0.5f;
			NewHeights[Y * NewSizeX + X] = BilinearSampleFloat(OldHeights, OldSizeX, OldSizeY, U, V);
		}
	}

	SetHeightInRegion(NewLabel, 0, 0, NewSizeX, NewSizeY, NewHeights);

	// --- Bilinear resample and import weights per layer ---
	for (const TPair<FString, TArray<float>>& LayerEntry : LayerWeights)
	{
		const FString& LName     = LayerEntry.Key;
		const TArray<float>& Old = LayerEntry.Value;

		TArray<float> NewWeights;
		NewWeights.SetNumUninitialized(NewSizeX * NewSizeY);
		for (int32 Y = 0; Y < NewSizeY; Y++)
		{
			for (int32 X = 0; X < NewSizeX; X++)
			{
				float U = (NewSizeX > 1) ? static_cast<float>(X) / static_cast<float>(NewSizeX - 1) : 0.5f;
				float V = (NewSizeY > 1) ? static_cast<float>(Y) / static_cast<float>(NewSizeY - 1) : 0.5f;
				NewWeights[Y * NewSizeX + X] = BilinearSampleFloat(Old, OldSizeX, OldSizeY, U, V);
			}
		}

		SetWeightsInRegion(NewLabel, LName, 0, 0, NewSizeX, NewSizeY, NewWeights);
	}

	FinalResult.bSuccess  = true;
	FinalResult.ActorLabel = NewLabel;

	UE_LOG(LogTemp, Log, TEXT("ULandscapeService::ResizeLandscape: Successfully resized to '%s' (%dx%d vertices, %d layers)"),
		*NewLabel, NewSizeX, NewSizeY, LayerWeights.Num());
	return FinalResult;
}
