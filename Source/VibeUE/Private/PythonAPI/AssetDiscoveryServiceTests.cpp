// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "PythonAPI/UAssetDiscoveryService.h"
#include "EditorAssetLibrary.h"
#include "HAL/FileManager.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeAssetReimportTest, "VibeUE.Assets.ReimportAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeAssetReimportTest::RunTest(const FString&)
{
	UFunction* Function = UAssetDiscoveryService::StaticClass()->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(UAssetDiscoveryService, ReimportAsset));
	TestNotNull(TEXT("ReimportAsset is reflected"), Function);
	if (Function)
	{
		TestTrue(TEXT("ReimportAsset is AICallable"), Function->HasMetaData(TEXT("AICallable")));
	}

	FString SourceFileUsed;
	FString Error;
	TestFalse(TEXT("missing asset is rejected"), UAssetDiscoveryService::ReimportAsset(
		TEXT("/Game/VibeUETests/T_MissingReimportAsset"), TEXT(""), SourceFileUsed, Error));
	TestTrue(TEXT("missing asset reports a useful error"), Error.Contains(TEXT("not found")));

	const FString TestDirectory = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("VibeUE/ReimportAssetTest"));
	const FString OriginalSource = FPaths::Combine(TestDirectory, TEXT("original.png"));
	const FString ReplacementSource = FPaths::Combine(TestDirectory, TEXT("replacement.png"));
	const FString AssetPackagePath = TEXT("/Game/VibeUETests/T_ReimportAssetTest");

	IFileManager::Get().MakeDirectory(*TestDirectory, true);
	if (UEditorAssetLibrary::DoesAssetExist(AssetPackagePath))
	{
		UEditorAssetLibrary::DeleteAsset(AssetPackagePath);
	}

	// Valid one-pixel PNG used only as a deterministic legacy texture-import fixture.
	TArray<uint8> PngBytes;
	const bool bDecoded = FBase64::Decode(
		TEXT("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII="),
		PngBytes);
	TestTrue(TEXT("PNG fixture decoded"), bDecoded);
	TestTrue(TEXT("original PNG fixture written"), bDecoded && FFileHelper::SaveArrayToFile(PngBytes, *OriginalSource));
	TestTrue(TEXT("replacement PNG fixture written"), bDecoded && FFileHelper::SaveArrayToFile(PngBytes, *ReplacementSource));

	const FString ImportedPath = UAssetDiscoveryService::ImportAsset(
		OriginalSource, TEXT("/Game/VibeUETests"), TEXT("T_ReimportAssetTest"), Error);
	TestFalse(TEXT("texture fixture imported"), ImportedPath.IsEmpty());
	if (!ImportedPath.IsEmpty())
	{
		SourceFileUsed.Empty();
		Error.Empty();
		TestTrue(TEXT("legacy texture reimports with an explicit source"),
			UAssetDiscoveryService::ReimportAsset(ImportedPath, OriginalSource, SourceFileUsed, Error));
		TestTrue(TEXT("explicit source is reported"), FPaths::IsSamePath(SourceFileUsed, OriginalSource));

		SourceFileUsed.Empty();
		Error.Empty();
		TestTrue(TEXT("legacy texture reimports from its stored source"),
			UAssetDiscoveryService::ReimportAsset(ImportedPath, TEXT(""), SourceFileUsed, Error));
		TestTrue(TEXT("stored source is reported"), FPaths::IsSamePath(SourceFileUsed, OriginalSource));

		IFileManager::Get().Delete(*OriginalSource, false, true);
		SourceFileUsed.Empty();
		Error.Empty();
		TestFalse(TEXT("missing stored source is rejected without prompting"),
			UAssetDiscoveryService::ReimportAsset(ImportedPath, TEXT(""), SourceFileUsed, Error));
		TestTrue(TEXT("missing source error names the problem"), Error.Contains(TEXT("does not exist")));

		SourceFileUsed.Empty();
		Error.Empty();
		TestTrue(TEXT("replacement source retargets and reimports the asset"),
			UAssetDiscoveryService::ReimportAsset(ImportedPath, ReplacementSource, SourceFileUsed, Error));
		TestTrue(TEXT("replacement source is reported"), FPaths::IsSamePath(SourceFileUsed, ReplacementSource));
	}

	if (UEditorAssetLibrary::DoesAssetExist(AssetPackagePath))
	{
		UEditorAssetLibrary::SaveAsset(AssetPackagePath, false);
		UEditorAssetLibrary::DeleteAsset(AssetPackagePath);
	}
	IFileManager::Get().Delete(*OriginalSource, false, true);
	IFileManager::Get().Delete(*ReplacementSource, false, true);
	IFileManager::Get().DeleteDirectory(*TestDirectory, false, true);

	return true;
}

#endif // WITH_AUTOMATION_TESTS
