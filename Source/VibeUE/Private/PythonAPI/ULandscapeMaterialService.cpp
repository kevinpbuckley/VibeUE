// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/ULandscapeMaterialService.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionLandscapeLayerBlend.h"
#include "Materials/MaterialExpressionLandscapeLayerWeight.h"
#include "Materials/MaterialExpressionLandscapeLayerCoords.h"
#include "Materials/MaterialExpressionLandscapeLayerSample.h"
#include "Materials/MaterialExpressionLandscapeGrassOutput.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureObject.h"
#include "LandscapeGrassType.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialEditingLibrary.h"
#include "LandscapeLayerInfoObject.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeEdit.h"
#include "EditorAssetLibrary.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/MaterialFactoryNew.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "AssetRegistry/AssetRegistryModule.h"

// =================================================================
// Helper Methods
// =================================================================

UMaterial* ULandscapeMaterialService::LoadMaterialAsset(const FString& MaterialPath)
{
	UObject* LoadedObject = UEditorAssetLibrary::LoadAsset(MaterialPath);
	if (!LoadedObject)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeMaterialService: Failed to load material: %s"), *MaterialPath);
		return nullptr;
	}

	UMaterial* Material = Cast<UMaterial>(LoadedObject);
	if (!Material)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeMaterialService: Object is not a material: %s"), *MaterialPath);
		return nullptr;
	}

	return Material;
}

UMaterialExpression* ULandscapeMaterialService::FindExpressionById(UMaterial* Material, const FString& ExpressionId)
{
	if (!Material) return nullptr;

	TArray<UMaterialExpression*> Expressions;
	Material->GetAllExpressionsInMaterialAndFunctionsOfType<UMaterialExpression>(Expressions);

	for (UMaterialExpression* Expression : Expressions)
	{
		if (GetExpressionId(Expression) == ExpressionId)
		{
			return Expression;
		}
	}

	// Try matching by index
	int32 Index = FCString::Atoi(*ExpressionId);
	if (Index >= 0 && Index < Expressions.Num())
	{
		return Expressions[Index];
	}

	return nullptr;
}

FString ULandscapeMaterialService::GetExpressionId(UMaterialExpression* Expression)
{
	if (!Expression) return TEXT("");
	return FString::Printf(TEXT("%s_%p"), *Expression->GetClass()->GetName(), Expression);
}

UMaterialExpressionLandscapeLayerBlend* ULandscapeMaterialService::FindLayerBlendNode(UMaterial* Material, const FString& NodeId)
{
	UMaterialExpression* Expr = FindExpressionById(Material, NodeId);
	if (!Expr)
	{
		return nullptr;
	}
	return Cast<UMaterialExpressionLandscapeLayerBlend>(Expr);
}

void ULandscapeMaterialService::RefreshMaterialGraph(UMaterial* Material)
{
	if (!Material) return;

	if (!IsInGameThread())
	{
		return;
	}

	Material->MarkPackageDirty();

	if (IsValid(Material))
	{
		Material->PreEditChange(nullptr);
		Material->PostEditChange();
	}

	if (Material->MaterialGraph)
	{
		if (UMaterialGraph* MaterialGraph = Cast<UMaterialGraph>(Material->MaterialGraph))
		{
			if (IsValid(MaterialGraph))
			{
				MaterialGraph->LinkMaterialExpressionsFromGraph();
				MaterialGraph->RebuildGraph();
			}
		}
	}
}

// =================================================================
// Material Creation
// =================================================================

FLandscapeMaterialCreateResult ULandscapeMaterialService::CreateLandscapeMaterial(
	const FString& MaterialName,
	const FString& DestinationPath)
{
	FLandscapeMaterialCreateResult Result;

	if (MaterialName.IsEmpty())
	{
		Result.ErrorMessage = TEXT("MaterialName cannot be empty");
		UE_LOG(LogTemp, Error, TEXT("ULandscapeMaterialService::CreateLandscapeMaterial: %s"), *Result.ErrorMessage);
		return Result;
	}

	// Check if asset already exists to avoid blocking overwrite dialog
	FString FullAssetPath = DestinationPath / MaterialName;
	if (UEditorAssetLibrary::DoesAssetExist(FullAssetPath))
	{
		Result.ErrorMessage = FString::Printf(TEXT("Landscape material '%s' already exists at '%s'. Delete it first or use a different name."), *MaterialName, *FullAssetPath);
		UE_LOG(LogTemp, Error, TEXT("ULandscapeMaterialService::CreateLandscapeMaterial: %s"), *Result.ErrorMessage);
		return Result;
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	IAssetTools& AssetTools = AssetToolsModule.Get();

	UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
	UObject* NewAsset = AssetTools.CreateAsset(MaterialName, DestinationPath, UMaterial::StaticClass(), Factory);

	UMaterial* NewMaterial = Cast<UMaterial>(NewAsset);
	if (!NewMaterial)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Failed to create material '%s' at '%s'"), *MaterialName, *DestinationPath);
		UE_LOG(LogTemp, Error, TEXT("ULandscapeMaterialService::CreateLandscapeMaterial: %s"), *Result.ErrorMessage);
		return Result;
	}

	// Landscape materials use Surface domain (same as regular materials)
	// No special domain change needed - MD_Surface is the default

	Result.bSuccess = true;
	Result.AssetPath = NewMaterial->GetPathName();

	UE_LOG(LogTemp, Log, TEXT("ULandscapeMaterialService::CreateLandscapeMaterial: Created landscape material '%s'"), *Result.AssetPath);
	return Result;
}

// =================================================================
// Layer Blend Node Management
// =================================================================

FLandscapeLayerBlendInfo ULandscapeMaterialService::CreateLayerBlendNode(
	const FString& MaterialPath,
	int32 PosX,
	int32 PosY)
{
	FLandscapeLayerBlendInfo Result;

	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return Result;
	}

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeMaterialService", "CreateLayerBlend", "Create Landscape Layer Blend"));
	Material->Modify();

	UMaterialExpression* NewExpression = UMaterialEditingLibrary::CreateMaterialExpression(
		Material, UMaterialExpressionLandscapeLayerBlend::StaticClass(), PosX, PosY);

	if (!NewExpression)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeMaterialService::CreateLayerBlendNode: Failed to create expression"));
		return Result;
	}

	RefreshMaterialGraph(Material);

	Result.NodeId = GetExpressionId(NewExpression);

	UE_LOG(LogTemp, Log, TEXT("ULandscapeMaterialService::CreateLayerBlendNode: Created LandscapeLayerBlend node"));
	return Result;
}

FLandscapeLayerBlendInfo ULandscapeMaterialService::CreateLayerBlendNodeWithLayers(
	const FString& MaterialPath,
	const TArray<FLandscapeMaterialLayerConfig>& Layers,
	int32 PosX,
	int32 PosY)
{
	FLandscapeLayerBlendInfo Result;

	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return Result;
	}

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeMaterialService", "CreateLayerBlendWithLayers", "Create Landscape Layer Blend With Layers"));
	Material->Modify();

	UMaterialExpression* NewExpression = UMaterialEditingLibrary::CreateMaterialExpression(
		Material, UMaterialExpressionLandscapeLayerBlend::StaticClass(), PosX, PosY);

	if (!NewExpression)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeMaterialService::CreateLayerBlendNodeWithLayers: Failed to create expression"));
		return Result;
	}

	UMaterialExpressionLandscapeLayerBlend* BlendNode = Cast<UMaterialExpressionLandscapeLayerBlend>(NewExpression);
	if (!BlendNode)
	{
		return Result;
	}

	BlendNode->Modify();

	// Add all layers in one go
	for (const FLandscapeMaterialLayerConfig& LayerConfig : Layers)
	{
		ELandscapeLayerBlendType BlendTypeEnum = LB_WeightBlend;
		if (LayerConfig.BlendType.Equals(TEXT("LB_AlphaBlend"), ESearchCase::IgnoreCase))
		{
			BlendTypeEnum = LB_AlphaBlend;
		}
		else if (LayerConfig.BlendType.Equals(TEXT("LB_HeightBlend"), ESearchCase::IgnoreCase))
		{
			BlendTypeEnum = LB_HeightBlend;
		}

		FLayerBlendInput NewLayer;
		NewLayer.LayerName = FName(*LayerConfig.LayerName);
		NewLayer.BlendType = BlendTypeEnum;
		NewLayer.PreviewWeight = LayerConfig.PreviewWeight;
		BlendNode->Layers.Add(NewLayer);
	}

	RefreshMaterialGraph(Material);

	// Build result with all layer info
	Result.NodeId = GetExpressionId(NewExpression);
	for (const FLayerBlendInput& Layer : BlendNode->Layers)
	{
		FLandscapeMaterialLayerConfig Config;
		Config.LayerName = Layer.LayerName.ToString();
		Config.BlendType = UEnum::GetValueAsString(Layer.BlendType);
		Config.PreviewWeight = Layer.PreviewWeight;
		Result.Layers.Add(Config);
	}

	UE_LOG(LogTemp, Log, TEXT("ULandscapeMaterialService::CreateLayerBlendNodeWithLayers: Created node with %d layers"),
		Layers.Num());
	return Result;
}

bool ULandscapeMaterialService::AddLayerToBlendNode(
	const FString& MaterialPath,
	const FString& BlendNodeId,
	const FString& LayerName,
	const FString& BlendType)
{
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return false;
	}

	UMaterialExpressionLandscapeLayerBlend* BlendNode = FindLayerBlendNode(Material, BlendNodeId);
	if (!BlendNode)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeMaterialService::AddLayerToBlendNode: Blend node '%s' not found"), *BlendNodeId);
		return false;
	}

	// Check if layer already exists
	for (const FLayerBlendInput& Existing : BlendNode->Layers)
	{
		if (Existing.LayerName.ToString().Equals(LayerName, ESearchCase::IgnoreCase))
		{
			UE_LOG(LogTemp, Warning, TEXT("ULandscapeMaterialService::AddLayerToBlendNode: Layer '%s' already exists"), *LayerName);
			return false;
		}
	}

	// Determine blend type
	ELandscapeLayerBlendType BlendTypeEnum = LB_WeightBlend;
	if (BlendType.Equals(TEXT("LB_AlphaBlend"), ESearchCase::IgnoreCase))
	{
		BlendTypeEnum = LB_AlphaBlend;
	}
	else if (BlendType.Equals(TEXT("LB_HeightBlend"), ESearchCase::IgnoreCase))
	{
		BlendTypeEnum = LB_HeightBlend;
	}

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeMaterialService", "AddLayer", "Add Layer to Blend Node"));
	Material->Modify();
	BlendNode->Modify();

	FLayerBlendInput NewLayer;
	NewLayer.LayerName = FName(*LayerName);
	NewLayer.BlendType = BlendTypeEnum;
	NewLayer.PreviewWeight = 1.0f;
	BlendNode->Layers.Add(NewLayer);

	RefreshMaterialGraph(Material);

	UE_LOG(LogTemp, Log, TEXT("ULandscapeMaterialService::AddLayerToBlendNode: Added layer '%s' (%s)"),
		*LayerName, *BlendType);
	return true;
}

bool ULandscapeMaterialService::RemoveLayerFromBlendNode(
	const FString& MaterialPath,
	const FString& BlendNodeId,
	const FString& LayerName)
{
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return false;
	}

	UMaterialExpressionLandscapeLayerBlend* BlendNode = FindLayerBlendNode(Material, BlendNodeId);
	if (!BlendNode)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeMaterialService::RemoveLayerFromBlendNode: Blend node '%s' not found"), *BlendNodeId);
		return false;
	}

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeMaterialService", "RemoveLayer", "Remove Layer from Blend Node"));
	Material->Modify();
	BlendNode->Modify();

	bool bFound = false;
	for (int32 i = BlendNode->Layers.Num() - 1; i >= 0; i--)
	{
		if (BlendNode->Layers[i].LayerName.ToString().Equals(LayerName, ESearchCase::IgnoreCase))
		{
			BlendNode->Layers.RemoveAt(i);
			bFound = true;
			break;
		}
	}

	if (bFound)
	{
		RefreshMaterialGraph(Material);
		UE_LOG(LogTemp, Log, TEXT("ULandscapeMaterialService::RemoveLayerFromBlendNode: Removed layer '%s'"), *LayerName);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeMaterialService::RemoveLayerFromBlendNode: Layer '%s' not found"), *LayerName);
	}

	return bFound;
}

FLandscapeLayerBlendInfo ULandscapeMaterialService::GetLayerBlendInfo(
	const FString& MaterialPath,
	const FString& BlendNodeId)
{
	FLandscapeLayerBlendInfo Result;

	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return Result;
	}

	UMaterialExpressionLandscapeLayerBlend* BlendNode = FindLayerBlendNode(Material, BlendNodeId);
	if (!BlendNode)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeMaterialService::GetLayerBlendInfo: Blend node '%s' not found"), *BlendNodeId);
		return Result;
	}

	Result.NodeId = BlendNodeId;

	for (const FLayerBlendInput& Layer : BlendNode->Layers)
	{
		FLandscapeMaterialLayerConfig Config;
		Config.LayerName = Layer.LayerName.ToString();

		switch (Layer.BlendType)
		{
		case LB_WeightBlend:
			Config.BlendType = TEXT("LB_WeightBlend");
			break;
		case LB_AlphaBlend:
			Config.BlendType = TEXT("LB_AlphaBlend");
			break;
		case LB_HeightBlend:
			Config.BlendType = TEXT("LB_HeightBlend");
			Config.bUseHeightBlend = true;
			break;
		default:
			Config.BlendType = TEXT("Unknown");
			break;
		}

		Config.PreviewWeight = Layer.PreviewWeight;
		Result.Layers.Add(Config);
	}

	return Result;
}

bool ULandscapeMaterialService::ConnectToLayerInput(
	const FString& MaterialPath,
	const FString& SourceExpressionId,
	const FString& SourceOutput,
	const FString& BlendNodeId,
	const FString& LayerName,
	const FString& InputType)
{
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return false;
	}

	UMaterialExpression* SourceExpr = FindExpressionById(Material, SourceExpressionId);
	if (!SourceExpr)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeMaterialService::ConnectToLayerInput: Source expression '%s' not found"), *SourceExpressionId);
		return false;
	}

	UMaterialExpressionLandscapeLayerBlend* BlendNode = FindLayerBlendNode(Material, BlendNodeId);
	if (!BlendNode)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeMaterialService::ConnectToLayerInput: Blend node '%s' not found"), *BlendNodeId);
		return false;
	}

	// Find the layer index
	int32 LayerIndex = INDEX_NONE;
	for (int32 i = 0; i < BlendNode->Layers.Num(); i++)
	{
		if (BlendNode->Layers[i].LayerName.ToString().Equals(LayerName, ESearchCase::IgnoreCase))
		{
			LayerIndex = i;
			break;
		}
	}

	if (LayerIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeMaterialService::ConnectToLayerInput: Layer '%s' not found in blend node"), *LayerName);
		return false;
	}

	// Determine the source output index
	int32 SourceOutputIndex = 0;
	if (!SourceOutput.IsEmpty())
	{
		TArray<FExpressionOutput>& Outputs = SourceExpr->GetOutputs();
		for (int32 i = 0; i < Outputs.Num(); i++)
		{
			if (Outputs[i].OutputName.ToString().Equals(SourceOutput, ESearchCase::IgnoreCase))
			{
				SourceOutputIndex = i;
				break;
			}
		}
	}

	// Calculate the input index on the blend node
	// The LandscapeLayerBlend node has inputs per layer:
	// Each layer has: Layer (color input) and optionally Height (for height blend)
	// Input index: LayerIndex * InputsPerLayer + InputOffset
	int32 InputsPerLayer = 1;
	int32 InputOffset = 0;

	// Check if any layer uses height blend (which adds height inputs)
	bool bHasHeightInputs = false;
	for (const FLayerBlendInput& Layer : BlendNode->Layers)
	{
		if (Layer.BlendType == LB_HeightBlend)
		{
			bHasHeightInputs = true;
			break;
		}
	}

	if (bHasHeightInputs)
	{
		InputsPerLayer = 2; // Layer + Height
	}

	if (InputType.Equals(TEXT("Height"), ESearchCase::IgnoreCase))
	{
		InputOffset = 1;
	}

	int32 TargetInputIndex = LayerIndex * InputsPerLayer + InputOffset;

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeMaterialService", "ConnectToLayer", "Connect to Layer Input"));
	Material->Modify();
	BlendNode->Modify();

	FExpressionInput* TargetInput = BlendNode->GetInput(TargetInputIndex);
	if (TargetInputIndex >= 0 && TargetInput)
	{
		TargetInput->Connect(SourceOutputIndex, SourceExpr);
		RefreshMaterialGraph(Material);

		UE_LOG(LogTemp, Log, TEXT("ULandscapeMaterialService::ConnectToLayerInput: Connected to layer '%s' %s input"),
			*LayerName, *InputType);
		return true;
	}

	UE_LOG(LogTemp, Error, TEXT("ULandscapeMaterialService::ConnectToLayerInput: Input index %d out of range"),
		TargetInputIndex);
	return false;
}

// =================================================================
// Landscape Layer Coordinates
// =================================================================

FString ULandscapeMaterialService::CreateLayerCoordsNode(
	const FString& MaterialPath,
	float MappingScale,
	int32 PosX,
	int32 PosY)
{
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return FString();
	}

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeMaterialService", "CreateLayerCoords", "Create Landscape Layer Coords"));
	Material->Modify();

	UMaterialExpression* NewExpression = UMaterialEditingLibrary::CreateMaterialExpression(
		Material, UMaterialExpressionLandscapeLayerCoords::StaticClass(), PosX, PosY);

	if (!NewExpression)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeMaterialService::CreateLayerCoordsNode: Failed to create expression"));
		return FString();
	}

	UMaterialExpressionLandscapeLayerCoords* CoordsNode = Cast<UMaterialExpressionLandscapeLayerCoords>(NewExpression);
	if (CoordsNode)
	{
		CoordsNode->MappingScale = MappingScale;
	}

	RefreshMaterialGraph(Material);

	FString NodeId = GetExpressionId(NewExpression);
	UE_LOG(LogTemp, Log, TEXT("ULandscapeMaterialService::CreateLayerCoordsNode: Created with scale %.4f"), MappingScale);
	return NodeId;
}

// =================================================================
// Landscape Layer Sample Expression
// =================================================================

FString ULandscapeMaterialService::CreateLayerSampleNode(
	const FString& MaterialPath,
	const FString& LayerName,
	int32 PosX,
	int32 PosY)
{
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return FString();
	}

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeMaterialService", "Create Layer Sample", "Create Layer Sample"));
	Material->Modify();

	UMaterialExpression* NewExpression = UMaterialEditingLibrary::CreateMaterialExpression(
		Material, UMaterialExpressionLandscapeLayerSample::StaticClass(), PosX, PosY);

	if (!NewExpression)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeMaterialService::CreateLayerSampleNode: Failed to create expression"));
		return FString();
	}

	UMaterialExpressionLandscapeLayerSample* SampleNode = Cast<UMaterialExpressionLandscapeLayerSample>(NewExpression);
	if (SampleNode)
	{
		SampleNode->ParameterName = FName(*LayerName);
	}

	RefreshMaterialGraph(Material);

	FString NodeId = GetExpressionId(NewExpression);
	UE_LOG(LogTemp, Log, TEXT("ULandscapeMaterialService::CreateLayerSampleNode: Created for layer '%s'"), *LayerName);
	return NodeId;
}

// =================================================================
// Landscape Grass Output
// =================================================================

FString ULandscapeMaterialService::CreateGrassOutput(
	const FString& MaterialPath,
	const TMap<FString, FString>& GrassTypeNames,
	int32 PosX,
	int32 PosY)
{
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return FString();
	}

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeMaterialService", "Create Grass Output", "Create Grass Output"));
	Material->Modify();

	UMaterialExpression* NewExpression = UMaterialEditingLibrary::CreateMaterialExpression(
		Material, UMaterialExpressionLandscapeGrassOutput::StaticClass(), PosX, PosY);

	if (!NewExpression)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeMaterialService::CreateGrassOutput: Failed to create expression"));
		return FString();
	}

	UMaterialExpressionLandscapeGrassOutput* GrassNode = Cast<UMaterialExpressionLandscapeGrassOutput>(NewExpression);
	if (GrassNode && GrassTypeNames.Num() > 0)
	{
		GrassNode->GrassTypes.Empty();

		for (const auto& Pair : GrassTypeNames)
		{
			FGrassInput NewInput;
			NewInput.Name = FName(*Pair.Key);

			// Load grass type asset
			UObject* LoadedObj = UEditorAssetLibrary::LoadAsset(Pair.Value);
			ULandscapeGrassType* GrassType = Cast<ULandscapeGrassType>(LoadedObj);
			if (GrassType)
			{
				NewInput.GrassType = GrassType;
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("ULandscapeMaterialService::CreateGrassOutput: Failed to load grass type: %s"), *Pair.Value);
			}

			GrassNode->GrassTypes.Add(NewInput);
		}
	}

	RefreshMaterialGraph(Material);

	FString NodeId = GetExpressionId(NewExpression);
	UE_LOG(LogTemp, Log, TEXT("ULandscapeMaterialService::CreateGrassOutput: Created with %d grass types"), GrassTypeNames.Num());
	return NodeId;
}

// =================================================================
// Layer Info Object Management
// =================================================================

FLandscapeLayerInfoCreateResult ULandscapeMaterialService::CreateLayerInfoObject(
	const FString& LayerName,
	const FString& DestinationPath,
	bool bIsWeightBlended)
{
	FLandscapeLayerInfoCreateResult Result;

	if (LayerName.IsEmpty())
	{
		Result.ErrorMessage = TEXT("LayerName cannot be empty");
		UE_LOG(LogTemp, Error, TEXT("ULandscapeMaterialService::CreateLayerInfoObject: %s"), *Result.ErrorMessage);
		return Result;
	}

	// Create the layer info object asset
	FString AssetName = FString::Printf(TEXT("LI_%s"), *LayerName);

	// Check if asset already exists to avoid blocking overwrite dialog
	FString FullAssetPath = DestinationPath / AssetName;
	if (UEditorAssetLibrary::DoesAssetExist(FullAssetPath))
	{
		// Layer info already exists - return it as success
		UObject* ExistingObj = UEditorAssetLibrary::LoadAsset(FullAssetPath);
		ULandscapeLayerInfoObject* ExistingInfo = Cast<ULandscapeLayerInfoObject>(ExistingObj);
		if (ExistingInfo)
		{
			Result.bSuccess = true;
			Result.AssetPath = ExistingInfo->GetPathName();
			Result.LayerName = LayerName;
			UE_LOG(LogTemp, Log, TEXT("ULandscapeMaterialService::CreateLayerInfoObject: Layer info '%s' already exists at '%s', returning existing"),
				*LayerName, *Result.AssetPath);
			return Result;
		}
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	IAssetTools& AssetTools = AssetToolsModule.Get();

	// Create package and object directly since there's no standard factory
	FString PackagePath = DestinationPath / AssetName;
	FString PackageName = FPackageName::ObjectPathToPackageName(PackagePath);

	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Failed to create package for '%s'"), *PackagePath);
		UE_LOG(LogTemp, Error, TEXT("ULandscapeMaterialService::CreateLayerInfoObject: %s"), *Result.ErrorMessage);
		return Result;
	}

	ULandscapeLayerInfoObject* LayerInfoObj = NewObject<ULandscapeLayerInfoObject>(
		Package, FName(*AssetName), RF_Public | RF_Standalone);

	if (!LayerInfoObj)
	{
		Result.ErrorMessage = TEXT("Failed to create ULandscapeLayerInfoObject");
		UE_LOG(LogTemp, Error, TEXT("ULandscapeMaterialService::CreateLayerInfoObject: %s"), *Result.ErrorMessage);
		return Result;
	}

	LayerInfoObj->SetLayerName(FName(*LayerName), false);
	LayerInfoObj->SetBlendMethod(
		bIsWeightBlended ? ELandscapeTargetLayerBlendMethod::FinalWeightBlending : ELandscapeTargetLayerBlendMethod::None,
		false);

	// Notify asset registry
	FAssetRegistryModule::AssetCreated(LayerInfoObj);
	LayerInfoObj->MarkPackageDirty();

	// Save the asset
	UEditorAssetLibrary::SaveAsset(LayerInfoObj->GetPathName(), false);

	Result.bSuccess = true;
	Result.AssetPath = LayerInfoObj->GetPathName();
	Result.LayerName = LayerName;

	UE_LOG(LogTemp, Log, TEXT("ULandscapeMaterialService::CreateLayerInfoObject: Created layer info '%s' at '%s'"),
		*LayerName, *Result.AssetPath);
	return Result;
}

bool ULandscapeMaterialService::GetLayerInfoDetails(
	const FString& LayerInfoAssetPath,
	FString& OutLayerName,
	bool& bOutIsWeightBlended)
{
	UObject* LoadedObj = UEditorAssetLibrary::LoadAsset(LayerInfoAssetPath);
	if (!LoadedObj)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeMaterialService::GetLayerInfoDetails: Failed to load '%s'"), *LayerInfoAssetPath);
		return false;
	}

	ULandscapeLayerInfoObject* LayerInfo = Cast<ULandscapeLayerInfoObject>(LoadedObj);
	if (!LayerInfo)
	{
		UE_LOG(LogTemp, Warning, TEXT("ULandscapeMaterialService::GetLayerInfoDetails: Not a ULandscapeLayerInfoObject: '%s'"), *LayerInfoAssetPath);
		return false;
	}

	OutLayerName = LayerInfo->GetLayerName().ToString();
	bOutIsWeightBlended = LayerInfo->GetBlendMethod() != ELandscapeTargetLayerBlendMethod::None;

	return true;
}

// =================================================================
// Material Assignment
// =================================================================

bool ULandscapeMaterialService::AssignMaterialToLandscape(
	const FString& LandscapeNameOrLabel,
	const FString& MaterialPath,
	const TMap<FString, FString>& LayerInfoPaths)
{
	UWorld* World = nullptr;
	if (GEditor)
	{
		World = GEditor->GetEditorWorldContext().World();
	}

	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeMaterialService::AssignMaterialToLandscape: No editor world available"));
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
		UE_LOG(LogTemp, Error, TEXT("ULandscapeMaterialService::AssignMaterialToLandscape: Landscape '%s' not found"), *LandscapeNameOrLabel);
		return false;
	}

	// Load material
	UObject* MatObj = UEditorAssetLibrary::LoadAsset(MaterialPath);
	UMaterialInterface* Material = Cast<UMaterialInterface>(MatObj);
	if (!Material)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeMaterialService::AssignMaterialToLandscape: Failed to load material '%s'"), *MaterialPath);
		return false;
	}

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeMaterialService", "AssignMaterial", "Assign Material to Landscape"));
	LandscapeProxy->Modify();

	// Set material
	LandscapeProxy->LandscapeMaterial = Material;

	// PostEditChange first — this triggers material instance creation and may rebuild
	// the landscape info layer list. We MUST do this before adding our layer info objects,
	// otherwise PostEditChange can clear/rebuild them from the material's layers.
	LandscapeProxy->PostEditChange();

	// Now register layer info objects AFTER PostEditChange has rebuilt internals
	ULandscapeInfo* Info = LandscapeProxy->GetLandscapeInfo();
	if (!Info)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeMaterialService::AssignMaterialToLandscape: Failed to get LandscapeInfo for '%s' - material was set but layers could not be configured"),
			*LandscapeNameOrLabel);
		return false;
	}

	int32 SuccessfulLayers = 0;
	int32 FailedLayers = 0;
	for (const auto& Pair : LayerInfoPaths)
	{
		UObject* LayerObj = UEditorAssetLibrary::LoadAsset(Pair.Value);
		ULandscapeLayerInfoObject* LayerInfoObj = Cast<ULandscapeLayerInfoObject>(LayerObj);
		if (LayerInfoObj)
		{
			// Check if layer already exists (PostEditChange may have created entries without LayerInfoObj)
			bool bExists = false;
			for (FLandscapeInfoLayerSettings& LayerSettings : Info->Layers)
			{
				if (LayerSettings.GetLayerName().ToString().Equals(Pair.Key, ESearchCase::IgnoreCase))
				{
					LayerSettings.LayerInfoObj = LayerInfoObj;
					bExists = true;
					break;
				}
			}

			if (!bExists)
			{
				FLandscapeInfoLayerSettings NewLayerSettings(LayerInfoObj, LandscapeProxy);
				Info->Layers.Add(NewLayerSettings);
			}
			SuccessfulLayers++;
		}
		else
		{
			FailedLayers++;
			UE_LOG(LogTemp, Error, TEXT("ULandscapeMaterialService::AssignMaterialToLandscape: Failed to load layer info '%s' for layer '%s'. "
				"Layer info naming convention is LI_<LayerName> (e.g., LI_Grass). Use create_layer_info_object().asset_path for correct paths."),
				*Pair.Value, *Pair.Key);
		}
	}

	Info->UpdateComponentLayerAllowList();

	// Allocate weight maps by painting the first layer to 100% across the entire
	// landscape. FLandscapeEditDataInterface::SetAlphaData() internally creates
	// WeightmapLayerAllocation entries and reallocates weight maps as needed —
	// the same mechanism that PaintLayerAtLocation uses.
	{
		// Find the first valid layer info to use as the "fill" layer
		ULandscapeLayerInfoObject* FillLayer = nullptr;
		for (const FLandscapeInfoLayerSettings& LayerSettings : Info->Layers)
		{
			if (LayerSettings.LayerInfoObj)
			{
				FillLayer = LayerSettings.LayerInfoObj;
				break;
			}
		}

		if (FillLayer)
		{
			int32 MinX, MinY, MaxX, MaxY;
			if (Info->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
			{
				int32 SizeX = MaxX - MinX + 1;
				int32 SizeY = MaxY - MinY + 1;

				// Fill with 255 (full weight) for the first layer
				TArray<uint8> FillData;
				FillData.SetNumUninitialized(SizeX * SizeY);
				FMemory::Memset(FillData.GetData(), 255, FillData.Num());

				FLandscapeEditDataInterface LandscapeEdit(Info);
				LandscapeEdit.SetAlphaData(FillLayer, MinX, MinY, MaxX, MaxY, FillData.GetData(), 0);

				UE_LOG(LogTemp, Log, TEXT("ULandscapeMaterialService::AssignMaterialToLandscape: Initialized fill layer '%s' across %dx%d extent"),
					*FillLayer->GetLayerName().ToString(), SizeX, SizeY);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("ULandscapeMaterialService::AssignMaterialToLandscape: Could not get landscape extent for weight map initialization"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ULandscapeMaterialService::AssignMaterialToLandscape: No valid layer info found for weight map initialization"));
		}
	}

	if (FailedLayers > 0)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeMaterialService::AssignMaterialToLandscape: Assigned material '%s' to '%s' but %d of %d layer infos failed to load. "
			"Use create_layer_info_object() and pass .asset_path - do NOT guess paths."),
			*MaterialPath, *LandscapeNameOrLabel, FailedLayers, LayerInfoPaths.Num());
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("ULandscapeMaterialService::AssignMaterialToLandscape: Assigned '%s' to '%s' with %d/%d layers successfully"),
		*MaterialPath, *LandscapeNameOrLabel, SuccessfulLayers, LayerInfoPaths.Num());
	return true;
}

// =================================================================
// Convenience Methods
// =================================================================

bool ULandscapeMaterialService::SetupLayerTextures(
	const FString& MaterialPath,
	const FString& BlendNodeId,
	const FString& LayerName,
	const FString& DiffuseTexturePath,
	const FString& NormalTexturePath,
	const FString& RoughnessTexturePath,
	float TextureTilingScale)
{
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return false;
	}

	UMaterialExpressionLandscapeLayerBlend* BlendNode = FindLayerBlendNode(Material, BlendNodeId);
	if (!BlendNode)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeMaterialService::SetupLayerTextures: Blend node '%s' not found"), *BlendNodeId);
		return false;
	}

	// Find layer index
	int32 LayerIndex = INDEX_NONE;
	for (int32 i = 0; i < BlendNode->Layers.Num(); i++)
	{
		if (BlendNode->Layers[i].LayerName.ToString().Equals(LayerName, ESearchCase::IgnoreCase))
		{
			LayerIndex = i;
			break;
		}
	}

	if (LayerIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeMaterialService::SetupLayerTextures: Layer '%s' not found"), *LayerName);
		return false;
	}

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeMaterialService", "SetupLayerTextures", "Setup Layer Textures"));
	Material->Modify();

	// Calculate positions for nodes
	int32 BaseX = -800;
	int32 BaseY = LayerIndex * 300;

	// Create landscape layer coords for UV tiling
	UMaterialExpression* CoordsExpr = UMaterialEditingLibrary::CreateMaterialExpression(
		Material, UMaterialExpressionLandscapeLayerCoords::StaticClass(), BaseX - 200, BaseY);
	if (CoordsExpr)
	{
		UMaterialExpressionLandscapeLayerCoords* CoordsNode = Cast<UMaterialExpressionLandscapeLayerCoords>(CoordsExpr);
		if (CoordsNode)
		{
			CoordsNode->MappingScale = TextureTilingScale;
		}
	}

	// Load and create diffuse texture sample
	UObject* DiffuseObj = UEditorAssetLibrary::LoadAsset(DiffuseTexturePath);
	UTexture* DiffuseTexture = Cast<UTexture>(DiffuseObj);
	if (!DiffuseTexture)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeMaterialService::SetupLayerTextures: Failed to load diffuse texture '%s'"), *DiffuseTexturePath);
		return false;
	}

	UMaterialExpression* DiffuseExpr = UMaterialEditingLibrary::CreateMaterialExpression(
		Material, UMaterialExpressionTextureSample::StaticClass(), BaseX, BaseY);
	if (DiffuseExpr)
	{
		UMaterialExpressionTextureSample* TexSample = Cast<UMaterialExpressionTextureSample>(DiffuseExpr);
		if (TexSample)
		{
			TexSample->Texture = DiffuseTexture;

			// Connect UV coords to texture sample
			if (CoordsExpr)
			{
				FExpressionInput* UVInput = TexSample->GetInput(0);
				if (UVInput)
				{
					UVInput->Connect(0, CoordsExpr); // UVs input
				}
			}

			// Connect diffuse to blend node layer input

			// Calculate input index for this layer
			int32 InputsPerLayer = 1;
			for (const FLayerBlendInput& Layer : BlendNode->Layers)
			{
				if (Layer.BlendType == LB_HeightBlend)
				{
					InputsPerLayer = 2;
					break;
				}
			}

			int32 TargetInput = LayerIndex * InputsPerLayer;
			FExpressionInput* BlendInput = BlendNode->GetInput(TargetInput);
			if (BlendInput)
			{
				BlendInput->Connect(0, TexSample);
			}
		}
	}

	// Optionally create normal map texture sample
	if (!NormalTexturePath.IsEmpty())
	{
		UObject* NormalObj = UEditorAssetLibrary::LoadAsset(NormalTexturePath);
		UTexture* NormalTexture = Cast<UTexture>(NormalObj);
		if (NormalTexture)
		{
			UMaterialExpression* NormalExpr = UMaterialEditingLibrary::CreateMaterialExpression(
				Material, UMaterialExpressionTextureSample::StaticClass(), BaseX, BaseY + 200);
			if (NormalExpr)
			{
				UMaterialExpressionTextureSample* NormalSample = Cast<UMaterialExpressionTextureSample>(NormalExpr);
				if (NormalSample)
				{
					NormalSample->Texture = NormalTexture;
					NormalSample->SamplerType = SAMPLERTYPE_Normal;

					// Connect UV coords
					if (CoordsExpr)
					{
						FExpressionInput* NormalUVInput = NormalSample->GetInput(0);
						if (NormalUVInput)
						{
							NormalUVInput->Connect(0, CoordsExpr);
						}
					}
				}
			}
		}
	}

	RefreshMaterialGraph(Material);

	UE_LOG(LogTemp, Log, TEXT("ULandscapeMaterialService::SetupLayerTextures: Setup textures for layer '%s'"), *LayerName);
	return true;
}

// =================================================================
// Landscape Layer Weight Expression
// =================================================================

FString ULandscapeMaterialService::CreateLayerWeightNode(
	const FString& MaterialPath,
	const FString& LayerName,
	float PreviewWeight,
	int32 PosX,
	int32 PosY)
{
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return FString();
	}

	FScopedTransaction Transaction(NSLOCTEXT("LandscapeMaterialService", "CreateLayerWeight", "Create Landscape Layer Weight"));
	Material->Modify();

	UMaterialExpression* NewExpression = UMaterialEditingLibrary::CreateMaterialExpression(
		Material, UMaterialExpressionLandscapeLayerWeight::StaticClass(), PosX, PosY);

	if (!NewExpression)
	{
		UE_LOG(LogTemp, Error, TEXT("ULandscapeMaterialService::CreateLayerWeightNode: Failed to create expression"));
		return FString();
	}

	UMaterialExpressionLandscapeLayerWeight* WeightNode = Cast<UMaterialExpressionLandscapeLayerWeight>(NewExpression);
	if (WeightNode)
	{
		WeightNode->ParameterName = FName(*LayerName);
		WeightNode->PreviewWeight = PreviewWeight;
	}

	RefreshMaterialGraph(Material);

	FString NodeId = GetExpressionId(NewExpression);
	UE_LOG(LogTemp, Log, TEXT("ULandscapeMaterialService::CreateLayerWeightNode: Created for layer '%s'"), *LayerName);
	return NodeId;
}

// =================================================================
// Existence Checks
// =================================================================

bool ULandscapeMaterialService::LandscapeMaterialExists(const FString& MaterialPath)
{
	return UEditorAssetLibrary::DoesAssetExist(MaterialPath);
}

bool ULandscapeMaterialService::LayerInfoExists(const FString& LayerInfoAssetPath)
{
	return UEditorAssetLibrary::DoesAssetExist(LayerInfoAssetPath);
}
