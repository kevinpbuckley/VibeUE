// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "PythonAPI/UAssetDiscoveryService.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "EditorAssetLibrary.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"
#include "Application/ThrottleManager.h"
#include "Async/TaskGraphInterfaces.h"
#include "Engine/StaticMesh.h"
#include "Curves/CurveFloat.h"
#include "Editor.h"
#include "Editor/Transactor.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"
#include "ScopedTransaction.h"

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
		// P16: a saved asset's package file must really leave the disk, and the result must say so.
		TestEqual(TEXT("a saved asset reports file_status 'removed'"), Res.FileStatus, FString(TEXT("removed")));
		TestTrue(TEXT("a saved asset reports file_removed"), Res.bFileRemoved);
		FString DeletedFilename;
		if (FPackageName::TryConvertLongPackageNameToFilename(AssetPackagePath, DeletedFilename, FPackageName::GetAssetPackageExtension()))
		{
			TestFalse(TEXT("the package file is really gone from disk"), IFileManager::Get().FileExists(*DeletedFilename));
		}
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

// Interchange syncs the Content Browser to what it imported (Interchange.FeatureFlags.Import.SyncToBrowser, on by
// default; outside commandlets it overrides the bSyncToBrowser = false that ImportAssetTasks passes). With no
// Content Browser tab open that opens the status bar's drawer, which holds a Slate responsive-mode request until
// its open animation finishes. Headless nothing runs that animation, so the request is never released, and while
// one is held UEditorEngine::Tick ticks no PIE world: every PIE test that runs after this one sees a frozen
// world (VibeUE.Input.HoldInPIE failed in the full suite and passed alone). This test has no browser to sync,
// so the sync is off for the import and the old value comes back after it.
namespace
{
	class FScopedInterchangeBrowserSyncOff
	{
	public:
		FScopedInterchangeBrowserSyncOff()
			: Var(IConsoleManager::Get().FindConsoleVariable(TEXT("Interchange.FeatureFlags.Import.SyncToBrowser")))
		{
			if (Var)
			{
				bWasOn = Var->GetBool();
				Var->SetWithCurrentPriority(false);
			}
		}
		~FScopedInterchangeBrowserSyncOff()
		{
			if (Var)
			{
				Var->SetWithCurrentPriority(bWasOn);
			}
		}

	private:
		IConsoleVariable* Var = nullptr;
		bool bWasOn = false;
	};
}

// import_asset takes a mesh through AssetImportTask, and refuses (instead of asserting on TaskGraph's
// RecursionGuard) when it is called from inside a game-thread task.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeAssetImportMeshTest, "VibeUE.Assets.ImportMesh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeAssetImportMeshTest::RunTest(const FString&)
{
	const FScopedInterchangeBrowserSyncOff NoBrowserSync;
	// Only a request this import left is this test's to report; one already held on entry belongs to whoever took it.
	const bool bThrottleFreeOnEntry = FSlateThrottleManager::Get().IsAllowingExpensiveTasks();
	const FString TestDirectory = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("VibeUE/ImportMeshTest"));
	const FString ObjSource = FPaths::Combine(TestDirectory, TEXT("SM_ImportMeshTest.obj"));
	const FString AssetPackagePath = TEXT("/Game/VibeUETests/SM_ImportMeshTest");

	IFileManager::Get().MakeDirectory(*TestDirectory, true);
	if (UEditorAssetLibrary::DoesAssetExist(AssetPackagePath))
	{
		UEditorAssetLibrary::DeleteAsset(AssetPackagePath);
	}

	// one 1 m quad as two triangles
	const FString Obj = TEXT("v 0 0 0\nv 100 0 0\nv 100 100 0\nv 0 100 0\nvt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\nvn 0 0 1\n")
		TEXT("f 1/1/1 2/2/1 3/3/1\nf 1/1/1 3/3/1 4/4/1\n");
	TestTrue(TEXT("OBJ fixture written"), FFileHelper::SaveStringToFile(Obj, *ObjSource));

	// Run the call as a task taken from the game thread's own queue, the context where Interchange's wait would assert
	// on TaskGraph's RecursionGuard: it must refuse and import nothing. WaitUntilTaskCompletes alone does not make that
	// context: it retracts the task and runs it inline, outside the queue (measured 2026-09-29: the import ran).
	bool bGuardHeld = false;
	FString TaskPath;
	FString TaskError;
	const FGraphEventRef Event = FFunctionGraphTask::CreateAndDispatchWhenReady([&ObjSource, &bGuardHeld, &TaskPath, &TaskError]()
	{
		bGuardHeld = FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::GameThread);
		if (bGuardHeld) // unrefused, an import here would assert, so it is only tried where the refusal must fire
		{
			TaskPath = UAssetDiscoveryService::ImportAsset(ObjSource, TEXT("/Game/VibeUETests"), TEXT("SM_ImportMeshTest"), TaskError);
		}
	}, TStatId(), nullptr, ENamedThreads::GameThread);
	if (FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::GameThread))
	{
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(Event, ENamedThreads::GameThread); // already inside the queue
	}
	else
	{
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	}
	TestTrue(TEXT("the task ran with the game thread's queue guard held"), Event->IsComplete() && bGuardHeld);
	TestTrue(FString::Printf(TEXT("there the import is refused ('%s')"), *TaskError), TaskPath.IsEmpty() && TaskError.Contains(TEXT("RecursionGuard")));
	TestFalse(TEXT("the refused import created nothing"), UEditorAssetLibrary::DoesAssetExist(AssetPackagePath));

	FString Error;
	const FString ImportedPath = UAssetDiscoveryService::ImportAsset(ObjSource, TEXT("/Game/VibeUETests"), TEXT("SM_ImportMeshTest"), Error);
	TestFalse(FString::Printf(TEXT("OBJ mesh imported (%s)"), *Error), ImportedPath.IsEmpty());

	const UObject* Imported = ImportedPath.IsEmpty() ? nullptr : StaticLoadObject(UObject::StaticClass(), nullptr, *ImportedPath);
	TestTrue(TEXT("the returned asset is a static mesh"), Imported && Imported->IsA<UStaticMesh>());
	TestTrue(TEXT("the mesh was saved to disk"), FPackageName::DoesPackageExist(AssetPackagePath));

	if (UEditorAssetLibrary::DoesAssetExist(AssetPackagePath))
	{
		UEditorAssetLibrary::DeleteAsset(AssetPackagePath);
	}
	IFileManager::Get().Delete(*ObjSource, false, true);
	IFileManager::Get().DeleteDirectory(*TestDirectory, false, true);
	// the test folder goes only if it is empty (not a tree delete): other VibeUE tests may keep assets there
	IFileManager::Get().DeleteDirectory(*FPaths::Combine(FPaths::ProjectContentDir(), TEXT("VibeUETests")), false, false);

	// The request an import leaves held shows up a frame after the call returns, so look a few frames on: at least
	// three polls as well as 0.5 s, or one long first frame (a save, the DDC) would end the look before the Slate tick
	// that opens the drawer. A held request that is still there a few seconds later is not an editor busy compiling
	// the mesh: it is a leak.
	const double LookStarted = FPlatformTime::Seconds();
	const TSharedRef<int32> Polls = MakeShared<int32>(0);
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, LookStarted, bThrottleFreeOnEntry, Polls]()
	{
		++*Polls;
		if (!bThrottleFreeOnEntry || FSlateThrottleManager::Get().IsAllowingExpensiveTasks())
		{
			return *Polls >= 3 && FPlatformTime::Seconds() - LookStarted >= 0.5;
		}
		if (FPlatformTime::Seconds() - LookStarted > 5.0)
		{
			AddError(TEXT("The mesh import left a Slate responsive-mode request held: the editor ticks no PIE world while one is."));
			return true;
		}
		return false;
	}));

	return true;
}

// P16: delete_asset_unattended and the undo buffer.
//  - Inside an OPEN editor transaction every delete is refused (TRANSACTION_OPEN): the engine delete can
//    reset the undo buffer, and UTransBuffer::Reset with a transaction open cancels it.
//  - An asset a COMMITTED transaction holds deletes as the engine always did: with undo as its only
//    holder, ObjectTools resets the undo history and removes it completely. The result reports the
//    reset (b_undo_buffer_reset, message) and a Warning is logged.
//
// The fixture is an in-memory curve in a scratch /Game package: it is never saved, so nothing reaches
// disk, and the delete reports file_status "never_on_disk". The reset is measured around ForceDeleteObjects
// and reported on failure returns too (ForceDeleteObjects returning 0, FILE_NOT_REMOVED); those are not
// exercised here: every engine failure after the delete call either opens a modal FMessageDialog (the
// OnAssetsCanDelete veto, DeleteSingleObject's "is in use") or needs the package saved to disk
// (FILE_NOT_REMOVED), so none can be forced headless without writing to disk. NOTE: step 2 resets the editor's undo
// buffer (that is the engine behaviour under test) - run this suite in a disposable host, not over work.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeDeleteAssetUnattendedUndoBufferTest, "VibeUE.Assets.DeleteAssetUnattendedUndoBuffer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeDeleteAssetUnattendedUndoBufferTest::RunTest(const FString&)
{
	if (!GEditor || !GEditor->Trans)
	{
		AddInfo(TEXT("No editor transaction buffer; skipping."));
		return true;
	}
	if (!TestFalse(TEXT("no transaction is open when the test starts"), GEditor->IsTransactionActive()))
	{
		return false;
	}

	const FString Leaf = FString::Printf(TEXT("Curve_UndoDelete_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const FString AssetPath = FString(TEXT("/Game/__VibeUETest/")) + Leaf;
	UPackage* Package = CreatePackage(*AssetPath);
	if (!TestNotNull(TEXT("created the scratch package"), Package))
	{
		return false;
	}
	UCurveFloat* Curve = NewObject<UCurveFloat>(Package, *Leaf, RF_Public | RF_Standalone | RF_Transactional);
	if (!TestNotNull(TEXT("created the scratch curve"), Curve))
	{
		return false;
	}
	FAssetRegistryModule::AssetCreated(Curve);
	const TWeakObjectPtr<UPackage> WeakPackage(Package);
	const TWeakObjectPtr<UCurveFloat> WeakCurve(Curve);
	ON_SCOPE_EXIT
	{
		// Whatever step failed: leave nothing dirty for a later auto-save sweep, and nothing registered.
		if (UCurveFloat* Leftover = WeakCurve.Get())
		{
			FAssetRegistryModule::AssetDeleted(Leftover);
			Leftover->ClearFlags(RF_Public | RF_Standalone);
			Leftover->MarkAsGarbage();
		}
		if (UPackage* Leftover = WeakPackage.Get())
		{
			Leftover->SetDirtyFlag(false);
		}
	};
	TestTrue(TEXT("the scratch asset is visible to the delete path"), UEditorAssetLibrary::DoesAssetExist(AssetPath));

	AddExpectedMessage(TEXT("while an editor transaction is open"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, 1, /*IsRegex=*/false);

	// 1. Inside a still-open transaction: refused even when forced, and nothing is touched.
	{
		FScopedTransaction Transaction(NSLOCTEXT("VibeUETests", "UndoDeleteProbe", "VibeUE undo-delete probe"));
		if (!TestTrue(TEXT("the probe transaction opened"), Transaction.IsOutstanding()))
		{
			return false;
		}
		// A REAL change, not a bare Modify(): UTransBuffer::End drops a transaction whose records hold no
		// change (FTransaction::IsTransient -> UndoBuffer.Pop, EditorTransaction.cpp), so a snapshot of an
		// unchanged curve vanishes at commit and step 2 would have nothing to test.
		Curve->Modify();
		Curve->FloatCurve.AddKey(0.0f, 1.0f);
		Curve->FloatCurve.AddKey(1.0f, 2.0f);

		const FUnattendedDeleteResult Res = UAssetDiscoveryService::DeleteAssetUnattended(AssetPath, /*bForceEvenIfReferenced=*/true);
		TestFalse(TEXT("open transaction: refused even when forced"), Res.bSuccess);
		TestEqual(TEXT("open transaction: error_code is TRANSACTION_OPEN"), Res.ErrorCode, FString(TEXT("TRANSACTION_OPEN")));
		TestTrue(TEXT("open transaction: nothing about the file or undo is reported"),
			Res.FileStatus.IsEmpty() && !Res.bFileRemoved && !Res.bUndoBufferReset);
		TestTrue(TEXT("open transaction: the transaction is still open afterwards"), GEditor->IsTransactionActive());
		TestTrue(TEXT("open transaction: the asset still exists"), WeakCurve.IsValid() && UEditorAssetLibrary::DoesAssetExist(AssetPath));
	}
	TestFalse(TEXT("the probe transaction is closed"), GEditor->IsTransactionActive());

	// Fixture precondition, not the product check: the committed transaction must still hold the curve.
	// Compared directly (not by queue length, which can stay flat when the buffer trims at its size limit).
	if (!GEditor->IsObjectInTransactionBuffer(Curve))
	{
		AddWarning(TEXT("The editor transactor did not keep the committed probe transaction; skipping the undo-held step. ")
			TEXT("This is a fixture gap, not a delete_asset_unattended result."));
		return true;
	}

	AddExpectedMessage(TEXT("is in the undo buffer; the engine resets the undo history to delete it"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, 1, /*IsRegex=*/false);

	// 2. Committed and undo-held, NOT forced: deletes as the engine always did, and says the history went.
	{
		const FUnattendedDeleteResult Res = UAssetDiscoveryService::DeleteAssetUnattended(AssetPath, /*bForceEvenIfReferenced=*/false);
		TestTrue(FString::Printf(TEXT("undo-held: the delete succeeds ('%s')"), *Res.ErrorMessage), Res.bSuccess);
		TestTrue(TEXT("undo-held: no error code"), Res.ErrorCode.IsEmpty());
		TestTrue(TEXT("undo-held: b_undo_buffer_reset is reported"), Res.bUndoBufferReset);
		TestTrue(FString::Printf(TEXT("undo-held: message names the reset ('%s')"), *Res.Message), Res.Message.Contains(TEXT("reset the undo history")));
		TestFalse(FString::Printf(TEXT("undo-held: message does not claim the history survived ('%s')"), *Res.Message), Res.Message.Contains(TEXT("did NOT reset")));
		TestEqual(TEXT("undo-held: file_status reports an asset that was never on disk"), Res.FileStatus, FString(TEXT("never_on_disk")));
		TestFalse(TEXT("undo-held: file_removed is false when there was no file"), Res.bFileRemoved);
		TestFalse(TEXT("undo-held: the asset is gone from the registry"), UEditorAssetLibrary::DoesAssetExist(AssetPath));
		TestEqual(TEXT("undo-held: the engine reset the undo history"), GEditor->Trans->GetQueueLength(), 0);
	}

	return true;
}

#endif // WITH_AUTOMATION_TESTS
