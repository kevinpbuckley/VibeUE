// Copyright Kevin Buckley 2025 All Rights Reserved.

#include "Services/DataAsset/DataAssetDiscoveryService.h"
#include "Core/ServiceContext.h"
#include "Core/ErrorCodes.h"
#include "Engine/DataAsset.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "UObject/UObjectIterator.h"
#include "Misc/PackageName.h"

DEFINE_LOG_CATEGORY_STATIC(LogDataAssetDiscoveryService, Log, All);

FDataAssetDiscoveryService::FDataAssetDiscoveryService(TSharedPtr<FServiceContext> InServiceContext)
	: ServiceContext(InServiceContext)
{
}

// ========== Type Discovery ==========

TResult<TArray<FDataAssetTypeInfo>> FDataAssetDiscoveryService::SearchTypes(const FString& SearchFilter)
{
	TArray<FDataAssetTypeInfo> Types;
	
	// Get all classes derived from UDataAsset
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		
		// Must be a subclass of UDataAsset but not UDataAsset itself
		if (!Class->IsChildOf(UDataAsset::StaticClass()))
		{
			continue;
		}
		
		// Skip abstract classes
		if (Class->HasAnyClassFlags(CLASS_Abstract))
		{
			continue;
		}
		
		// Skip deprecated classes
		if (Class->HasAnyClassFlags(CLASS_Deprecated))
		{
			continue;
		}
		
		FString ClassName = Class->GetName();
		FString ClassPath = Class->GetPathName();
		
		// Apply search filter if provided
		if (!SearchFilter.IsEmpty())
		{
			if (!ClassName.Contains(SearchFilter, ESearchCase::IgnoreCase) &&
				!ClassPath.Contains(SearchFilter, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}
		
		FDataAssetTypeInfo TypeInfo;
		TypeInfo.Name = ClassName;
		TypeInfo.Path = ClassPath;
		TypeInfo.bIsNative = !Class->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
		
		// Get module info
		UPackage* Package = Class->GetOutermost();
		if (Package)
		{
			TypeInfo.Module = Package->GetName();
		}
		
		// Get parent class
		if (UClass* Super = Class->GetSuperClass())
		{
			TypeInfo.ParentClass = Super->GetName();
		}
		
		Types.Add(MoveTemp(TypeInfo));
	}
	
	return TResult<TArray<FDataAssetTypeInfo>>::Success(MoveTemp(Types));
}

// ========== Asset Discovery ==========

TResult<TArray<FDataAssetInfo>> FDataAssetDiscoveryService::ListAssets(
	const FString& AssetType,
	const FString& SearchPath)
{
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	
	TArray<FAssetData> AssetDataList;
	
	// If specific type provided, filter by it
	if (!AssetType.IsEmpty())
	{
		auto ClassResult = FindDataAssetClass(AssetType);
		if (ClassResult.IsError())
		{
			return TResult<TArray<FDataAssetInfo>>::Error(
				ClassResult.GetErrorCode(),
				ClassResult.GetErrorMessage());
		}
		
		UClass* FilterClass = ClassResult.GetValue();
		FTopLevelAssetPath ClassPath(FilterClass->GetClassPathName());
		AssetRegistry.GetAssetsByClass(ClassPath, AssetDataList, true);
	}
	else
	{
		// Get all UDataAsset subclasses
		FTopLevelAssetPath DataAssetPath(UDataAsset::StaticClass()->GetClassPathName());
		AssetRegistry.GetAssetsByClass(DataAssetPath, AssetDataList, true);
	}
	
	TArray<FDataAssetInfo> Assets;
	
	for (const FAssetData& AssetData : AssetDataList)
	{
		// Filter by path if specified
		FString AssetPath = AssetData.GetObjectPathString();
		if (!SearchPath.IsEmpty() && !AssetPath.StartsWith(SearchPath))
		{
			continue;
		}
		
		FDataAssetInfo Info;
		Info.Name = AssetData.AssetName.ToString();
		Info.Path = AssetPath;
		Info.ClassName = AssetData.AssetClassPath.GetAssetName().ToString();
		
		Assets.Add(MoveTemp(Info));
	}
	
	return TResult<TArray<FDataAssetInfo>>::Success(MoveTemp(Assets));
}

// ========== Asset Loading ==========

TResult<UDataAsset*> FDataAssetDiscoveryService::LoadDataAsset(const FString& AssetPath)
{
	// Try direct load first
	UDataAsset* DataAsset = Cast<UDataAsset>(StaticLoadObject(UDataAsset::StaticClass(), nullptr, *AssetPath));
	
	if (!DataAsset)
	{
		// Try adding .AssetName suffix
		FString FullPath = AssetPath;
		if (!FullPath.Contains(TEXT(".")))
		{
			int32 LastSlash;
			if (FullPath.FindLastChar('/', LastSlash))
			{
				FString AssetName = FullPath.Mid(LastSlash + 1);
				FullPath = FullPath + TEXT(".") + AssetName;
				DataAsset = Cast<UDataAsset>(StaticLoadObject(UDataAsset::StaticClass(), nullptr, *FullPath));
			}
		}
	}
	
	if (!DataAsset)
	{
		// Search by name in asset registry
		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		TArray<FAssetData> AssetDataList;
		AssetRegistry.GetAssetsByClass(FTopLevelAssetPath(UDataAsset::StaticClass()->GetClassPathName()), AssetDataList, true);
		
		FString SearchName = FPackageName::GetShortName(AssetPath);
		
		for (const FAssetData& AssetData : AssetDataList)
		{
			if (AssetData.AssetName.ToString().Equals(SearchName, ESearchCase::IgnoreCase))
			{
				DataAsset = Cast<UDataAsset>(AssetData.GetAsset());
				break;
			}
		}
	}
	
	if (!DataAsset)
	{
		return TResult<UDataAsset*>::Error(
			VibeUE::ErrorCodes::ASSET_NOT_FOUND,
			FString::Printf(TEXT("Could not find data asset: %s"), *AssetPath));
	}
	
	return TResult<UDataAsset*>::Success(DataAsset);
}

TResult<UClass*> FDataAssetDiscoveryService::FindDataAssetClass(const FString& ClassNameOrPath)
{
	FString SearchName = ClassNameOrPath;
	
	// Try direct find
	UClass* FoundClass = FindObject<UClass>(nullptr, *SearchName);
	
	if (!FoundClass)
	{
		// Try with /Script/ prefix - search all classes
		if (!SearchName.StartsWith(TEXT("/")))
		{
			for (TObjectIterator<UClass> It; It; ++It)
			{
				UClass* Class = *It;
				if (Class->IsChildOf(UDataAsset::StaticClass()))
				{
					if (Class->GetName().Equals(SearchName, ESearchCase::IgnoreCase))
					{
						FoundClass = Class;
						break;
					}
				}
			}
		}
	}
	
	if (!FoundClass)
	{
		// Try loading
		FoundClass = LoadObject<UClass>(nullptr, *SearchName);
	}
	
	if (!FoundClass)
	{
		return TResult<UClass*>::Error(
			VibeUE::ErrorCodes::TYPE_NOT_FOUND,
			FString::Printf(TEXT("Could not find data asset class: %s"), *ClassNameOrPath));
	}
	
	// Verify it's a data asset class
	if (!FoundClass->IsChildOf(UDataAsset::StaticClass()))
	{
		return TResult<UClass*>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			FString::Printf(TEXT("%s is not a DataAsset class"), *ClassNameOrPath));
	}
	
	return TResult<UClass*>::Success(FoundClass);
}
