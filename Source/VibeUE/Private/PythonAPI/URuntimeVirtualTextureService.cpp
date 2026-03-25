// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/URuntimeVirtualTextureService.h"
#include "VT/RuntimeVirtualTexture.h"
#include "Components/RuntimeVirtualTextureComponent.h"
#include "VT/VirtualTextureBuilder.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "EditorAssetLibrary.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

// =================================================================
// Helper Methods
// =================================================================

URuntimeVirtualTexture* URuntimeVirtualTextureService::LoadRVTAsset(const FString& AssetPath)
{
	UObject* LoadedObj = UEditorAssetLibrary::LoadAsset(AssetPath);
	if (!LoadedObj)
	{
		UE_LOG(LogTemp, Warning, TEXT("URuntimeVirtualTextureService: Failed to load RVT: %s"), *AssetPath);
		return nullptr;
	}

	URuntimeVirtualTexture* RVT = Cast<URuntimeVirtualTexture>(LoadedObj);
	if (!RVT)
	{
		UE_LOG(LogTemp, Warning, TEXT("URuntimeVirtualTextureService: Object is not an RVT: %s (is %s)"),
			*AssetPath, *LoadedObj->GetClass()->GetName());
		return nullptr;
	}

	return RVT;
}

// =================================================================
// Asset Creation
// =================================================================

FRVTCreateResult URuntimeVirtualTextureService::CreateRuntimeVirtualTexture(
	const FString& AssetName,
	const FString& DirectoryPath,
	const FString& MaterialType,
	int32 TileCount,
	int32 TileSize,
	int32 TileBorderSize,
	bool bContinuousUpdate,
	bool bSinglePhysicalSpace)
{
	FRVTCreateResult Result;

	if (AssetName.IsEmpty())
	{
		Result.ErrorMessage = TEXT("AssetName cannot be empty");
		return Result;
	}

	FString PackagePath = DirectoryPath;
	if (!PackagePath.EndsWith(TEXT("/")))
	{
		PackagePath += TEXT("/");
	}

	// Check if already exists
	FString FullAssetPath = PackagePath + AssetName;
	if (UEditorAssetLibrary::DoesAssetExist(FullAssetPath))
	{
		Result.ErrorMessage = FString::Printf(TEXT("RVT '%s' already exists at '%s'"), *AssetName, *FullAssetPath);
		return Result;
	}

	// Create package for the RVT
	FString PackageName = FPackageName::ObjectPathToPackageName(FullAssetPath);
	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Failed to create package for '%s'"), *FullAssetPath);
		return Result;
	}

	URuntimeVirtualTexture* NewRVT = NewObject<URuntimeVirtualTexture>(
		Package, FName(*AssetName), RF_Public | RF_Standalone);

	if (!NewRVT)
	{
		Result.ErrorMessage = TEXT("Failed to create URuntimeVirtualTexture object");
		return Result;
	}

	// Parse material type
	FString TypeUpper = MaterialType.ToUpper().Replace(TEXT(" "), TEXT("_"));
	ERuntimeVirtualTextureMaterialType MatType = ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Roughness;

	if (TypeUpper.Contains(TEXT("BASECOLOR_NORMAL_ROUGHNESS")) || TypeUpper.Contains(TEXT("BASE_COLOR_NORMAL_ROUGHNESS")))
	{
		MatType = ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Roughness;
	}
	else if (TypeUpper.Contains(TEXT("BASECOLOR_NORMAL_SPECULAR")) || TypeUpper.Contains(TEXT("BASE_COLOR_NORMAL_SPECULAR")))
	{
		MatType = ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular;
	}
	else if (TypeUpper == TEXT("BASECOLOR") || TypeUpper == TEXT("BASE_COLOR"))
	{
		MatType = ERuntimeVirtualTextureMaterialType::BaseColor;
	}
	else if (TypeUpper.Contains(TEXT("WORLDHEIGHT")) || TypeUpper.Contains(TEXT("WORLD_HEIGHT")))
	{
		MatType = ERuntimeVirtualTextureMaterialType::WorldHeight;
	}

	// URuntimeVirtualTexture properties are protected with only public getters in UE 5.7.
	// Use UObject property reflection to set them from editor tooling.
	{
		auto* RVTClass = NewRVT->GetClass();

		// Set MaterialType enum
		if (FEnumProperty* EnumProp = CastField<FEnumProperty>(RVTClass->FindPropertyByName(TEXT("MaterialType"))))
		{
			FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
			void* ValuePtr = EnumProp->ContainerPtrToValuePtr<void>(NewRVT);
			UnderlyingProp->SetIntPropertyValue(ValuePtr, (int64)MatType);
		}
		else if (FByteProperty* ByteProp = CastField<FByteProperty>(RVTClass->FindPropertyByName(TEXT("MaterialType"))))
		{
			ByteProp->SetPropertyValue_InContainer(NewRVT, (uint8)MatType);
		}

		// Set TileCount (int32)
		if (FIntProperty* IntProp = CastField<FIntProperty>(RVTClass->FindPropertyByName(TEXT("TileCount"))))
		{
			IntProp->SetPropertyValue_InContainer(NewRVT, TileCount);
		}

		// Set TileSize (int32)
		if (FIntProperty* IntProp = CastField<FIntProperty>(RVTClass->FindPropertyByName(TEXT("TileSize"))))
		{
			IntProp->SetPropertyValue_InContainer(NewRVT, TileSize);
		}

		// Set TileBorderSize (int32)
		if (FIntProperty* IntProp = CastField<FIntProperty>(RVTClass->FindPropertyByName(TEXT("TileBorderSize"))))
		{
			IntProp->SetPropertyValue_InContainer(NewRVT, TileBorderSize);
		}

		// Set bContinuousUpdate (bool)
		if (FBoolProperty* BoolProp = CastField<FBoolProperty>(RVTClass->FindPropertyByName(TEXT("bContinuousUpdate"))))
		{
			BoolProp->SetPropertyValue_InContainer(NewRVT, bContinuousUpdate);
		}

		// Set bSinglePhysicalSpace (bool)
		if (FBoolProperty* BoolProp = CastField<FBoolProperty>(RVTClass->FindPropertyByName(TEXT("bSinglePhysicalSpace"))))
		{
			BoolProp->SetPropertyValue_InContainer(NewRVT, bSinglePhysicalSpace);
		}
	}

	// Notify asset registry
	FAssetRegistryModule::AssetCreated(NewRVT);
	NewRVT->MarkPackageDirty();

	// Save the asset
	UEditorAssetLibrary::SaveAsset(FullAssetPath, false);

	Result.bSuccess = true;
	Result.AssetPath = FullAssetPath;

	UE_LOG(LogTemp, Log, TEXT("URuntimeVirtualTextureService::CreateRuntimeVirtualTexture: Created '%s' (type=%s)"),
		*Result.AssetPath, *MaterialType);

	return Result;
}

// =================================================================
// Introspection
// =================================================================

FRVTInfo URuntimeVirtualTextureService::GetRuntimeVirtualTextureInfo(const FString& AssetPath)
{
	FRVTInfo Info;

	URuntimeVirtualTexture* RVT = LoadRVTAsset(AssetPath);
	if (!RVT)
	{
		Info.ErrorMessage = FString::Printf(TEXT("Failed to load RVT: %s"), *AssetPath);
		return Info;
	}

	Info.AssetPath = RVT->GetPathName();

	// Get material type as string
	ERuntimeVirtualTextureMaterialType MatType = RVT->GetMaterialType();
	switch (MatType)
	{
	case ERuntimeVirtualTextureMaterialType::BaseColor:
		Info.MaterialType = TEXT("BaseColor");
		break;
	case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Roughness:
		Info.MaterialType = TEXT("BaseColor_Normal_Roughness");
		break;
	case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular:
		Info.MaterialType = TEXT("BaseColor_Normal_Specular");
		break;
	case ERuntimeVirtualTextureMaterialType::WorldHeight:
		Info.MaterialType = TEXT("WorldHeight");
		break;
	default:
		Info.MaterialType = TEXT("Unknown");
		break;
	}

	Info.TileCount = RVT->GetTileCount();
	Info.TileSize = RVT->GetTileSize();
	Info.TileBorderSize = RVT->GetTileBorderSize();
	Info.bContinuousUpdate = RVT->GetContinuousUpdate();
	Info.bSinglePhysicalSpace = RVT->GetSinglePhysicalSpace();

	return Info;
}

// =================================================================
// Level Integration
// =================================================================

FRVTVolumeResult URuntimeVirtualTextureService::CreateRVTVolume(
	const FString& LandscapeNameOrLabel,
	const FString& RVTAssetPath,
	const FString& VolumeName)
{
	FRVTVolumeResult Result;

	// Get world
	UWorld* World = nullptr;
	if (GEditor)
	{
		World = GEditor->GetEditorWorldContext().World();
	}
	if (!World)
	{
		Result.ErrorMessage = TEXT("No editor world available");
		return Result;
	}

	// Find landscape
	ALandscapeProxy* LandscapeProxy = nullptr;
	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel().Equals(LandscapeNameOrLabel, ESearchCase::IgnoreCase) ||
			(*It)->GetName().Equals(LandscapeNameOrLabel, ESearchCase::IgnoreCase))
		{
			LandscapeProxy = *It;
			break;
		}
	}

	if (!LandscapeProxy)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Landscape '%s' not found"), *LandscapeNameOrLabel);
		return Result;
	}

	// Load RVT asset
	URuntimeVirtualTexture* RVT = LoadRVTAsset(RVTAssetPath);
	if (!RVT)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Failed to load RVT asset: %s"), *RVTAssetPath);
		return Result;
	}

	// Get landscape bounds for volume sizing
	FVector Origin, Extent;
	LandscapeProxy->GetActorBounds(false, Origin, Extent);

	// Spawn the RuntimeVirtualTextureComponent on a new actor
	// We create the actor with a RuntimeVirtualTextureComponent
	FString ActorLabel = VolumeName.IsEmpty()
		? FString::Printf(TEXT("RVT_Volume_%s"), *LandscapeProxy->GetActorLabel())
		: VolumeName;

	FScopedTransaction Transaction(NSLOCTEXT("RVTService", "CreateRVTVolume", "Create RVT Volume"));

	// Spawn a generic actor with RVT component
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* VolumeActor = World->SpawnActor<AActor>(AActor::StaticClass(), Origin, FRotator::ZeroRotator, SpawnParams);
	if (!VolumeActor)
	{
		Result.ErrorMessage = TEXT("Failed to spawn RVT volume actor");
		return Result;
	}

	VolumeActor->SetActorLabel(ActorLabel);

	// Add RuntimeVirtualTextureComponent
	URuntimeVirtualTextureComponent* RVTComponent = NewObject<URuntimeVirtualTextureComponent>(
		VolumeActor, URuntimeVirtualTextureComponent::StaticClass(), TEXT("RuntimeVirtualTexture"));

	if (RVTComponent)
	{
		RVTComponent->SetVirtualTexture(RVT);

		// Size the component to cover the landscape
		RVTComponent->SetWorldTransform(FTransform(FRotator::ZeroRotator, Origin, FVector::OneVector));

		VolumeActor->AddInstanceComponent(RVTComponent);
		RVTComponent->RegisterComponent();
	}

	Result.bSuccess = true;
	Result.VolumeName = VolumeActor->GetName();
	Result.VolumeLabel = VolumeActor->GetActorLabel();

	UE_LOG(LogTemp, Log, TEXT("URuntimeVirtualTextureService::CreateRVTVolume: Created volume '%s' covering landscape '%s'"),
		*Result.VolumeLabel, *LandscapeNameOrLabel);

	return Result;
}

bool URuntimeVirtualTextureService::AssignRVTToLandscape(
	const FString& LandscapeNameOrLabel,
	const FString& RVTAssetPath,
	int32 SlotIndex)
{
	// Get world
	UWorld* World = nullptr;
	if (GEditor)
	{
		World = GEditor->GetEditorWorldContext().World();
	}
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("URuntimeVirtualTextureService::AssignRVTToLandscape: No editor world available"));
		return false;
	}

	// Find landscape
	ALandscapeProxy* LandscapeProxy = nullptr;
	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel().Equals(LandscapeNameOrLabel, ESearchCase::IgnoreCase) ||
			(*It)->GetName().Equals(LandscapeNameOrLabel, ESearchCase::IgnoreCase))
		{
			LandscapeProxy = *It;
			break;
		}
	}

	if (!LandscapeProxy)
	{
		UE_LOG(LogTemp, Error, TEXT("URuntimeVirtualTextureService::AssignRVTToLandscape: Landscape '%s' not found"),
			*LandscapeNameOrLabel);
		return false;
	}

	// Load RVT
	URuntimeVirtualTexture* RVT = LoadRVTAsset(RVTAssetPath);
	if (!RVT)
	{
		return false;
	}

	FScopedTransaction Transaction(NSLOCTEXT("RVTService", "AssignRVT", "Assign RVT to Landscape"));
	LandscapeProxy->Modify();

	// Ensure the RuntimeVirtualTextures array is large enough
	while (LandscapeProxy->RuntimeVirtualTextures.Num() <= SlotIndex)
	{
		LandscapeProxy->RuntimeVirtualTextures.Add(nullptr);
	}

	LandscapeProxy->RuntimeVirtualTextures[SlotIndex] = RVT;
	LandscapeProxy->PostEditChange();

	UE_LOG(LogTemp, Log, TEXT("URuntimeVirtualTextureService::AssignRVTToLandscape: Assigned '%s' to '%s' at slot %d"),
		*RVTAssetPath, *LandscapeNameOrLabel, SlotIndex);

	return true;
}
