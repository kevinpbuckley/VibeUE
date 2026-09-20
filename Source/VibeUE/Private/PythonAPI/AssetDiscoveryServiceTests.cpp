// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "PythonAPI/UAssetDiscoveryService.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "EditorAssetLibrary.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"

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

// Regression guard for the A13 follow-up (DeleteAssetUnattended false-refusal + lost-reason fix).
//
// SCOPE NOTE: the false-refusal half of the bug (a Blueprint refused because FBlueprintActionDatabase
// roots transient UBlueprintNodeSpawner objects) cannot be reproduced under a commandlet — the
// automation harness runs headless, and FBlueprintActionDatabase::RefreshAssetActions early-returns
// on IsRunningCommandlet() (BlueprintActionDatabase.cpp:1628), so no node spawners are ever built to
// refuse over. That half was verified in the live editor. What this test locks in headlessly is the
// API-shape half: the function returns an FUnattendedDeleteResult struct (so Python never loses the
// reason to a false->None collapse), the struct's fields are populated on both refusal and success,
// and an unreferenced asset still deletes cleanly through the new clear-actions + gather path.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeDeleteAssetUnattendedResultStructTest, "VibeUE.Assets.DeleteAssetUnattendedResultStruct",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeDeleteAssetUnattendedResultStructTest::RunTest(const FString&)
{
	UFunction* Function = UAssetDiscoveryService::StaticClass()->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(UAssetDiscoveryService, DeleteAssetUnattended));
	TestNotNull(TEXT("DeleteAssetUnattended is reflected"), Function);
	if (Function)
	{
		TestTrue(TEXT("DeleteAssetUnattended is AICallable"), Function->HasMetaData(TEXT("AICallable")));
		// The whole point of the fix: it returns a struct, not a bool, so the reason survives to Python.
		const FStructProperty* ReturnProp = CastField<FStructProperty>(Function->GetReturnProperty());
		TestNotNull(TEXT("DeleteAssetUnattended returns a struct (not a bool)"), ReturnProp);
		if (ReturnProp)
		{
			TestTrue(TEXT("return type is FUnattendedDeleteResult"),
				ReturnProp->Struct == FUnattendedDeleteResult::StaticStruct());
		}
	}

	// Empty path: refused, reason survives in the struct.
	{
		const FUnattendedDeleteResult Res = UAssetDiscoveryService::DeleteAssetUnattended(TEXT(""), true);
		TestFalse(TEXT("empty path is refused"), Res.bSuccess);
		TestTrue(TEXT("empty-path reason survives"), Res.ErrorMessage.Contains(TEXT("empty")));
	}

	// Missing asset: refused, reason survives in the struct.
	{
		const FUnattendedDeleteResult Res = UAssetDiscoveryService::DeleteAssetUnattended(
			TEXT("/Game/VibeUETests/T_MissingDeleteAsset"), true);
		TestFalse(TEXT("missing asset is refused"), Res.bSuccess);
		TestTrue(TEXT("missing-asset reason survives"), Res.ErrorMessage.Contains(TEXT("not found")));
	}

	// Unreferenced asset: deletes cleanly through the new clear-actions + gather path.
	const FString TestDirectory = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("VibeUE/DeleteAssetUnattendedTest"));
	const FString SourcePng = FPaths::Combine(TestDirectory, TEXT("pixel.png"));
	const FString AssetPackagePath = TEXT("/Game/VibeUETests/T_DeleteAssetUnattendedTest");

	IFileManager::Get().MakeDirectory(*TestDirectory, true);
	if (UEditorAssetLibrary::DoesAssetExist(AssetPackagePath))
	{
		UEditorAssetLibrary::DeleteAsset(AssetPackagePath);
	}

	TArray<uint8> PngBytes;
	const bool bDecoded = FBase64::Decode(
		TEXT("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII="),
		PngBytes);
	TestTrue(TEXT("PNG fixture decoded"), bDecoded);
	TestTrue(TEXT("PNG fixture written"), bDecoded && FFileHelper::SaveArrayToFile(PngBytes, *SourcePng));

	FString ImportError;
	const FString ImportedPath = UAssetDiscoveryService::ImportAsset(
		SourcePng, TEXT("/Game/VibeUETests"), TEXT("T_DeleteAssetUnattendedTest"), ImportError);
	TestFalse(TEXT("texture fixture imported"), ImportedPath.IsEmpty());
	if (!ImportedPath.IsEmpty())
	{
		FString PackageFilename;
		const bool bResolvedPackage = FPackageName::TryConvertLongPackageNameToFilename(
			AssetPackagePath, PackageFilename, FPackageName::GetAssetPackageExtension());
		TestTrue(TEXT("fixture package filename resolves"), bResolvedPackage);
		if (bResolvedPackage)
		{
			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			TestTrue(TEXT("fixture can be marked read-only"), PlatformFile.SetReadOnly(*PackageFilename, true));
			const FUnattendedDeleteResult ReadOnlyRes =
				UAssetDiscoveryService::DeleteAssetUnattended(AssetPackagePath, true);
			TestFalse(TEXT("read-only package is refused without entering the engine delete path"), ReadOnlyRes.bSuccess);
			TestTrue(TEXT("read-only refusal explains the recovery action"),
				ReadOnlyRes.ErrorMessage.Contains(TEXT("read-only")));
			TestTrue(TEXT("read-only refusal leaves the asset intact"),
				UEditorAssetLibrary::DoesAssetExist(AssetPackagePath));
			TestTrue(TEXT("fixture read-only state can be cleared"), PlatformFile.SetReadOnly(*PackageFilename, false));
		}

		const FUnattendedDeleteResult Res = UAssetDiscoveryService::DeleteAssetUnattended(AssetPackagePath, false);
		TestTrue(TEXT("unreferenced asset deletes and reports success in the struct"), Res.bSuccess);
		TestTrue(TEXT("success carries no error message"), Res.ErrorMessage.IsEmpty());
		TestFalse(TEXT("asset is actually gone after delete"), UEditorAssetLibrary::DoesAssetExist(AssetPackagePath));
	}

	if (UEditorAssetLibrary::DoesAssetExist(AssetPackagePath))
	{
		UEditorAssetLibrary::DeleteAsset(AssetPackagePath);
	}
	IFileManager::Get().Delete(*SourcePng, false, true);
	IFileManager::Get().DeleteDirectory(*TestDirectory, false, true);

	return true;
}

// Regression guard for item 18: after an unattended delete, the asset registry must no longer know
// about the object, so does_asset_exist / GetAssetByObjectPath return false/invalid. (At UE 5.8 the
// notification is performed by the engine: ObjectTools::ForceDeleteObjects -> DeleteSingleObject calls
// FAssetRegistryModule::AssetDeleted for every deleted object. This test locks that behaviour in so a
// future rewrite of the delete path that stops going through ForceDeleteObjects is caught here.)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeDeleteAssetUnattendedNotifiesRegistryTest, "VibeUE.Assets.DeleteAssetUnattendedNotifiesRegistry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeDeleteAssetUnattendedNotifiesRegistryTest::RunTest(const FString&)
{
	IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();

	const FString TestDirectory = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("VibeUE/DeleteAssetRegistryTest"));
	const FString SourcePng = FPaths::Combine(TestDirectory, TEXT("pixel.png"));
	const FString AssetPackagePath = TEXT("/Game/VibeUETests/T_DeleteAssetRegistryTest");

	IFileManager::Get().MakeDirectory(*TestDirectory, true);
	if (UEditorAssetLibrary::DoesAssetExist(AssetPackagePath))
	{
		UEditorAssetLibrary::DeleteAsset(AssetPackagePath);
	}

	TArray<uint8> PngBytes;
	const bool bDecoded = FBase64::Decode(
		TEXT("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII="),
		PngBytes);
	TestTrue(TEXT("PNG fixture decoded"), bDecoded);
	TestTrue(TEXT("PNG fixture written"), bDecoded && FFileHelper::SaveArrayToFile(PngBytes, *SourcePng));

	FString ImportError;
	const FString ImportedObjectPath = UAssetDiscoveryService::ImportAsset(
		SourcePng, TEXT("/Game/VibeUETests"), TEXT("T_DeleteAssetRegistryTest"), ImportError);
	TestFalse(TEXT("texture fixture imported"), ImportedObjectPath.IsEmpty());

	if (!ImportedObjectPath.IsEmpty())
	{
		const FSoftObjectPath ObjectPath(ImportedObjectPath);

		// Sanity: the create path registered it (ImportAsset calls FAssetRegistryModule::AssetCreated).
		TestTrue(TEXT("registry knows the asset before the delete"),
			AssetRegistry.GetAssetByObjectPath(ObjectPath).IsValid());

		const FUnattendedDeleteResult Res = UAssetDiscoveryService::DeleteAssetUnattended(AssetPackagePath, false);
		TestTrue(TEXT("unreferenced asset deletes and reports success"), Res.bSuccess);

		// The whole point of item 18: the registry entry is gone, so does_asset_exist-equivalent lookups
		// return false/invalid rather than reporting a phantom asset.
		TestFalse(TEXT("registry no longer resolves the object path after the unattended delete"),
			AssetRegistry.GetAssetByObjectPath(ObjectPath).IsValid());
		TestFalse(TEXT("does_asset_exist is false after the unattended delete"),
			UEditorAssetLibrary::DoesAssetExist(AssetPackagePath));
	}

	if (UEditorAssetLibrary::DoesAssetExist(AssetPackagePath))
	{
		UEditorAssetLibrary::DeleteAsset(AssetPackagePath);
	}
	IFileManager::Get().Delete(*SourcePng, false, true);
	IFileManager::Get().DeleteDirectory(*TestDirectory, false, true);

	return true;
}

#endif // WITH_AUTOMATION_TESTS
