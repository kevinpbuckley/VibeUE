// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UEnvQueryService.h"

#if WITH_VIBEUE_EQS

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EnvQueryServiceInternal.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryOption.h"
#include "EnvironmentQueryGraph.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

TArray<FString> UEnvQueryService::ListQueries(const FString& DirectoryPath)
{
	const FAssetRegistryModule& ARM =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	FARFilter Filter;
	Filter.ClassPaths.Add(UEnvQuery::StaticClass()->GetClassPathName());
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

FString UEnvQueryService::CreateQuery(const FString& AssetPath)
{
	// CreateQuery is the one write that cannot go through OpenWriteGuard -- there is no asset to
	// load yet -- so it applies the same path rules here.
	const FString PathError = VibeEQS::CheckWritableAssetPath(AssetPath);
	if (!PathError.IsEmpty())
	{
		return PathError;
	}

	FString PackagePath;
	FString AssetName;
	if (!AssetPath.Split(TEXT("/"), &PackagePath, &AssetName, ESearchCase::IgnoreCase, ESearchDir::FromEnd)
		|| AssetName.IsEmpty())
	{
		return FString::Printf(TEXT("Not a valid asset path: %s"), *AssetPath);
	}

	// Creating over an existing asset replaces someone's query with an empty one, so it is an error
	// rather than a silent overwrite. Both halves matter: the file check covers an asset on disk
	// this process has never loaded, FindPackage covers one created earlier this session and not
	// yet saved.
	//
	// The question is put to the filesystem rather than to FPackageName::DoesPackageExist, which
	// answers from a package-path index built when the process started: a .uasset that existed at
	// startup and has since been deleted out of band still reads as present there, and creation is
	// then refused for an asset that is not actually there. That is not a corner case — it is the
	// state every rerun of this plugin's own test suite starts in.
	FString ExistingFilename;
	const bool bAssetFileOnDisk =
		FPackageName::TryConvertLongPackageNameToFilename(
			AssetPath, ExistingFilename, FPackageName::GetAssetPackageExtension())
		&& IFileManager::Get().FileExists(*ExistingFilename);
	if (bAssetFileOnDisk || FindPackage(nullptr, *AssetPath))
	{
		return FString::Printf(TEXT("Asset already exists: %s"), *AssetPath);
	}

	UPackage* Package = CreatePackage(*AssetPath);
	if (!Package)
	{
		return FString::Printf(TEXT("Failed to create package: %s"), *AssetPath);
	}

	UEnvQuery* Query = NewObject<UEnvQuery>(
		Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
	if (!Query)
	{
		return FString::Printf(TEXT("Failed to create Environment Query: %s"), *AssetPath);
	}

	// The factory-equivalent object above has a null EdGraph; without this it has nothing to write
	// to and nothing to show when a human opens it.
	const FString GraphError = VibeEQS::EnsureGraph(Query);
	if (!GraphError.IsEmpty())
	{
		return GraphError;
	}

	UEnvironmentQueryGraph* Graph = Cast<UEnvironmentQueryGraph>(Query->EdGraph);
	if (!Graph)
	{
		return FString::Printf(TEXT("%s has no Environment Query graph"), *AssetPath);
	}

	FAssetRegistryModule::AssetCreated(Query);

	return VibeEQS::CommitGraph(Query, Graph);
}

bool UEnvQueryService::GetQueryInfo(const FString& AssetPath, FEQSQueryInfo& OutInfo)
{
	OutInfo = FEQSQueryInfo();

	if (AssetPath.IsEmpty())
	{
		OutInfo.Error = TEXT("AssetPath is empty");
		return false;
	}

	// Read-only, so deliberately not OpenWriteGuard: that path calls EnsureGraph, which creates a
	// graph, spawns nodes and regenerates the option list. Reporting on an asset must not change it
	// — bHasGraph == false is a fact worth returning, not a defect to repair behind the caller's
	// back.
	UEnvQuery* Query = LoadObject<UEnvQuery>(nullptr, *AssetPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Query)
	{
		OutInfo.Error = FString::Printf(TEXT("Environment Query not found: %s"), *AssetPath);
		return false;
	}

	const TArray<UEnvQueryOption*>& Options = Query->GetOptions();
	OutInfo.OptionCount = Options.Num();
	for (const UEnvQueryOption* Option : Options)
	{
		if (Option)
		{
			OutInfo.TestCount += Option->Tests.Num();
		}
	}

	OutInfo.bHasGraph = Query->EdGraph != nullptr;
	if (UEnvironmentQueryGraph* Graph = Cast<UEnvironmentQueryGraph>(Query->EdGraph))
	{
		OutInfo.bHasRootNode = VibeEQS::FindRootNode(Graph) != nullptr;
	}

	return true;
}

FString UEnvQueryService::CompileAndSave(const FString& AssetPath)
{
	UEnvQuery* Query = nullptr;
	UEnvironmentQueryGraph* Graph = nullptr;
	const FString Error = VibeEQS::OpenWriteGuard(AssetPath, Query, Graph);
	if (!Error.IsEmpty())
	{
		return Error;
	}

	return VibeEQS::CommitGraph(Query, Graph);
}

#else  // WITH_VIBEUE_EQS

namespace
{
	const TCHAR* const GEQSUnavailable =
		TEXT("EQS authoring is unavailable: the EnvironmentQueryEditor plugin is not enabled in this "
			 "build (WITH_VIBEUE_EQS=0).");
}

TArray<FString> UEnvQueryService::ListQueries(const FString&)
{
	return TArray<FString>();
}

FString UEnvQueryService::CreateQuery(const FString&)
{
	return GEQSUnavailable;
}

bool UEnvQueryService::GetQueryInfo(const FString&, FEQSQueryInfo& OutInfo)
{
	OutInfo = FEQSQueryInfo();
	OutInfo.Error = GEQSUnavailable;
	return false;
}

FString UEnvQueryService::CompileAndSave(const FString&)
{
	return GEQSUnavailable;
}

#endif // WITH_VIBEUE_EQS
