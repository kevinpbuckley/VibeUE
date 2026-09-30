// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "PythonAPI/UInputService.h"
#include "EditorAssetLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeInputCreateFoldersTest, "VibeUE.Input.CreateFolders",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeInputCreateFoldersTest::RunTest(const FString&)
{
	const FString Folder = TEXT("/Game/VibeUETests/InputFolders");
	const FString BareFolder = TEXT("VibeUETests/InputFolders");
	const FString UnmountedFolder = TEXT("/Temp/VibeUETests/InputFolders");
	// Where an unmounted folder used to land once /Game/ was put in front of it.
	const FString PrefixedFolder = TEXT("/Game/Temp/VibeUETests/InputFolders");

	const TArray<FString> Created = {
		Folder / TEXT("IA_VibeFolderBare"), Folder / TEXT("IA_VibeFolderFull"), Folder / TEXT("IMC_VibeFolderBare"),
		PrefixedFolder / TEXT("IA_VibeFolderUnmounted"), PrefixedFolder / TEXT("IMC_VibeFolderUnmounted")};
	auto DeleteCreated = [&Created]()
	{
		for (const FString& Path : Created)
		{
			if (UEditorAssetLibrary::DoesAssetExist(Path))
			{
				UEditorAssetLibrary::DeleteAsset(Path);
			}
		}
	};
	DeleteCreated();

	// A bare folder goes under /Game.
	const FInputCreateResult Bare = UInputService::CreateAction(TEXT("IA_VibeFolderBare"), BareFolder, TEXT("Boolean"));
	TestTrue(TEXT("create_action takes a bare folder"), Bare.bSuccess);
	TestEqual(TEXT("a bare folder goes under /Game"), Bare.AssetPath,
		Folder / TEXT("IA_VibeFolderBare.IA_VibeFolderBare"));

	// A path that starts with its mount point is kept as it is.
	const FInputCreateResult Full = UInputService::CreateAction(TEXT("IA_VibeFolderFull"), Folder, TEXT("Boolean"));
	TestTrue(TEXT("create_action takes a full path"), Full.bSuccess);
	TestEqual(TEXT("a full path is kept"), Full.AssetPath, Folder / TEXT("IA_VibeFolderFull.IA_VibeFolderFull"));

	const FInputCreateResult ContextBare = UInputService::CreateMappingContext(TEXT("IMC_VibeFolderBare"), BareFolder, 0);
	TestTrue(TEXT("create_mapping_context takes a bare folder"), ContextBare.bSuccess);
	TestEqual(TEXT("a bare context folder goes under /Game"), ContextBare.AssetPath,
		Folder / TEXT("IMC_VibeFolderBare.IMC_VibeFolderBare"));

	// A folder under no mounted content root is refused with its reason, and nothing is created anywhere.
	const FInputCreateResult Unmounted =
		UInputService::CreateAction(TEXT("IA_VibeFolderUnmounted"), UnmountedFolder, TEXT("Boolean"));
	TestFalse(TEXT("create_action refuses an unmounted folder"), Unmounted.bSuccess);
	TestTrue(TEXT("the refusal names the folder"), Unmounted.ErrorMessage.Contains(UnmountedFolder));
	TestFalse(TEXT("no action was created under /Game"),
		UEditorAssetLibrary::DoesAssetExist(PrefixedFolder / TEXT("IA_VibeFolderUnmounted")));

	const FInputCreateResult ContextUnmounted =
		UInputService::CreateMappingContext(TEXT("IMC_VibeFolderUnmounted"), UnmountedFolder, 0);
	TestFalse(TEXT("create_mapping_context refuses an unmounted folder"), ContextUnmounted.bSuccess);
	TestTrue(TEXT("the context refusal names the folder"), ContextUnmounted.ErrorMessage.Contains(UnmountedFolder));
	TestFalse(TEXT("no context was created under /Game"),
		UEditorAssetLibrary::DoesAssetExist(PrefixedFolder / TEXT("IMC_VibeFolderUnmounted")));

	DeleteCreated();
	return true;
}

#endif // WITH_AUTOMATION_TESTS
