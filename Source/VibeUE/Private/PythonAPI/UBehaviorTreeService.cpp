// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UBehaviorTreeService.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTreeServiceInternal.h"

namespace VibeBT
{
	/** Shared asset-registry sweep used by both AI services. */
	TArray<FString> ListAssetsOfClass(const UClass* Class, const FString& DirectoryPath)
	{
		const FAssetRegistryModule& ARM =
			FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

		FARFilter Filter;
		Filter.ClassPaths.Add(Class->GetClassPathName());
		Filter.PackagePaths.Add(*DirectoryPath);
		Filter.bRecursivePaths = true;

		TArray<FAssetData> Assets;
		ARM.Get().GetAssets(Filter, Assets);

		TArray<FString> Paths;
		Paths.Reserve(Assets.Num());
		for (const FAssetData& Asset : Assets)
		{
			Paths.Add(Asset.PackageName.ToString());
		}
		Paths.Sort();
		return Paths;
	}
}

TArray<FString> UBehaviorTreeService::ListBehaviorTrees(const FString& DirectoryPath)
{
	return VibeBT::ListAssetsOfClass(UBehaviorTree::StaticClass(), DirectoryPath);
}
