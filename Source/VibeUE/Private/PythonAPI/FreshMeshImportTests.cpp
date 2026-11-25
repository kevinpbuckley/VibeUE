// Copyright Buckley Builds LLC 2026 All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "EditorAssetLibrary.h"
#include "PythonAPI/UAssetDiscoveryService.h"

#if WITH_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeFreshMeshImportTest, "VibeUE.Assets.FreshMeshImport",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeFreshMeshImportTest::RunTest(const FString&)
{
	const FString Folder = TEXT("/Game/VibeUETests/Fresh_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString Source = FPaths::EngineContentDir() / TEXT("FbxEditorAutomation/BlenderCube.fbx");
	const FString ExistingFolder = Folder + TEXT("_Existing");
	const FString ExistingPath = ExistingFolder / TEXT("BackRed");
	UPackage* ExistingPackage = CreatePackage(*ExistingPath);
	UMaterial* Existing = NewObject<UMaterial>(ExistingPackage, TEXT("BackRed"), RF_Public | RF_Standalone);
	Existing->TwoSided = true;
	FAssetRegistryModule::AssetCreated(Existing);
	ExistingPackage->MarkPackageDirty();
	ON_SCOPE_EXIT { UEditorAssetLibrary::DeleteDirectory(ExistingFolder); };
	if (!TestTrue(TEXT("saved a same-named existing material"), UEditorAssetLibrary::SaveLoadedAsset(Existing, false))) return false;
	const FString MaterialFile = FPackageName::LongPackageNameToFilename(ExistingPath, FPackageName::GetAssetPackageExtension());
	TArray<uint8> BeforeBytes;
	if (!TestTrue(TEXT("read material before import"), FFileHelper::LoadFileToArray(BeforeBytes, *MaterialFile))) return false;
	const FString DiskFolder = FPackageName::LongPackageNameToFilename(Folder);
	const FString Marker = DiskFolder / TEXT("untracked.txt");
	ON_SCOPE_EXIT
	{
		IFileManager::Get().Delete(*Marker);
		if (UEditorAssetLibrary::DoesDirectoryExist(Folder))
		{
			UEditorAssetLibrary::DeleteDirectory(Folder);
		}
	};
	if (!TestTrue(TEXT("FBX fixture available"), FPaths::FileExists(Source)) ||
		!TestTrue(TEXT("created private folder"), IFileManager::Get().MakeDirectory(*DiskFolder, true)) ||
		!TestTrue(TEXT("wrote unregistered marker"), FFileHelper::SaveStringToFile(TEXT("untouched"), *Marker)))
	{
		return false;
	}
	const FFreshMeshImportResult Occupied = UAssetDiscoveryService::ImportMeshIntoFreshFolder(Source, Folder);
	TestFalse(TEXT("occupied disk folder refused"), Occupied.bSuccess);
	TestTrue(TEXT("no partial output on preflight refusal"), Occupied.CreatedAssets.IsEmpty());
	FString Original;
	FFileHelper::LoadFileToString(Original, *Marker);
	TestEqual(TEXT("pre-existing file unchanged"), Original, FString(TEXT("untouched")));
	IFileManager::Get().Delete(*Marker);
	const FFreshMeshImportResult Fresh = UAssetDiscoveryService::ImportMeshIntoFreshFolder(Source, Folder);
	if (!TestTrue(*Fresh.ErrorMessage, Fresh.bSuccess))
	{
		return false;
	}
	TestTrue(TEXT("main mesh included in manifest"), Fresh.CreatedAssets.Contains(Fresh.MainAsset));
	for (const FString& Path : Fresh.CreatedAssets)
	{
		TestTrue(TEXT("outputs are in the destination and saved"), Path.StartsWith(Folder + TEXT("/")) && FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(Path)));
	}
	TArray<uint8> AfterBytes;
	TestTrue(TEXT("existing material is byte-identical"), FFileHelper::LoadFileToArray(AfterBytes, *MaterialFile) && BeforeBytes == AfterBytes);
	TestTrue(TEXT("existing material retains its marker"), Existing->TwoSided && !ExistingPackage->IsDirty());
	const UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Fresh.MainAsset);
	if (!TestNotNull(TEXT("mesh remains inspectable"), Mesh)) return false;
	for (const FStaticMaterial& Slot : Mesh->GetStaticMaterials())
	{
		if (const UMaterialInterface* Material = Slot.MaterialInterface)
		{
			const FString Path = Material->GetPathName();
			TestTrue(TEXT("each material binding is accounted for"), Fresh.CreatedAssets.Contains(Path) != Fresh.ExternalMaterials.Contains(Path));
		}
	}
	TestTrue(TEXT("fixture produces material side assets or reports their reuse"), Fresh.CreatedAssets.Num() > 1 || !Fresh.ExternalMaterials.IsEmpty());
	const FFreshMeshImportResult Retry = UAssetDiscoveryService::ImportMeshIntoFreshFolder(Source, Folder);
	TestFalse(TEXT("second import is refused"), Retry.bSuccess);
	TestTrue(TEXT("retry creates no new objects"), Retry.CreatedAssets.IsEmpty());
	return true;
}
#endif
