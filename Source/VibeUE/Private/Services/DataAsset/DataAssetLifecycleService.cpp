// Copyright Kevin Buckley 2025 All Rights Reserved.

#include "Services/DataAsset/DataAssetLifecycleService.h"
#include "Services/DataAsset/DataAssetPropertyService.h"
#include "Core/ServiceContext.h"
#include "Core/ErrorCodes.h"
#include "Engine/DataAsset.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/DataAssetFactory.h"

DEFINE_LOG_CATEGORY_STATIC(LogDataAssetLifecycleService, Log, All);

FDataAssetLifecycleService::FDataAssetLifecycleService(TSharedPtr<FServiceContext> InServiceContext)
	: ServiceContext(InServiceContext)
{
}

TResult<FDataAssetCreateResult> FDataAssetLifecycleService::CreateDataAsset(
	UClass* DataAssetClass,
	const FString& AssetPath,
	const FString& AssetName,
	const TSharedPtr<FJsonObject>& InitialProperties)
{
	if (!DataAssetClass)
	{
		return TResult<FDataAssetCreateResult>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("DataAssetClass is required"));
	}
	
	if (AssetName.IsEmpty())
	{
		return TResult<FDataAssetCreateResult>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("AssetName is required"));
	}
	
	// Verify it's a data asset class
	if (!DataAssetClass->IsChildOf(UDataAsset::StaticClass()))
	{
		return TResult<FDataAssetCreateResult>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			FString::Printf(TEXT("%s is not a DataAsset class"), *DataAssetClass->GetName()));
	}
	
	// Use default path if not provided
	FString FinalPath = AssetPath.IsEmpty() ? TEXT("/Game/Data") : AssetPath;
	
	// Create the asset using asset tools
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	
	// Create factory
	UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
	Factory->DataAssetClass = DataAssetClass;
	
	UObject* NewAsset = AssetTools.CreateAsset(AssetName, FinalPath, DataAssetClass, Factory);
	
	if (!NewAsset)
	{
		return TResult<FDataAssetCreateResult>::Error(
			VibeUE::ErrorCodes::ASSET_CREATE_FAILED,
			FString::Printf(TEXT("Failed to create data asset at %s/%s"), *FinalPath, *AssetName));
	}
	
	UDataAsset* DataAsset = Cast<UDataAsset>(NewAsset);
	
	// Apply initial properties if provided
	if (InitialProperties.IsValid() && InitialProperties->Values.Num() > 0)
	{
		FDataAssetPropertyService PropertyService(ServiceContext);
		
		for (auto& Pair : InitialProperties->Values)
		{
			FProperty* Property = DataAssetClass->FindPropertyByName(*Pair.Key);
			if (Property && PropertyService.ShouldExposeProperty(Property))
			{
				FString Error;
				if (!PropertyService.JsonToProperty(Property, DataAsset, Pair.Value, Error))
				{
					UE_LOG(LogDataAssetLifecycleService, Warning, TEXT("Failed to set initial property %s: %s"), *Pair.Key, *Error);
				}
			}
		}
	}
	
	// Mark dirty
	NewAsset->MarkPackageDirty();
	
	FDataAssetCreateResult Result;
	Result.AssetPath = NewAsset->GetPathName();
	Result.AssetName = AssetName;
	Result.ClassName = DataAssetClass->GetName();
	
	return TResult<FDataAssetCreateResult>::Success(MoveTemp(Result));
}
