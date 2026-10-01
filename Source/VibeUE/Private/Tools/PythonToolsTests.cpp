// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "Tools/PythonTools.h"
#include "Engine/World.h"
#include "UObject/Package.h"
#include "Misc/ScopeExit.h"
#include "Editor.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// The execute_python_code reply must describe what the pre-run auto-save sweep ACTUALLY DID, not
// echo the argument back. A caller already knows what it passed; the only thing it cannot otherwise
// tell is whether its unsaved editor edits were flushed to disk. So `auto_save` is the outcome and
// `auto_save_note` names the reason whenever it is false — which is what keeps "opted out" from
// being indistinguishable from "swept, nothing was dirty" (both used to report auto_save:true with
// an empty saved_packages).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibePythonAutoSaveReportTest, "VibeUE.Python.AutoSaveReport",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibePythonAutoSaveReportTest::RunTest(const FString&)
{
	auto Run = [](bool bAutoSave)
	{
		const FString Json = UPythonTools::ExecutePythonCode(TEXT("x = 1\n"), bAutoSave);
		TSharedPtr<FJsonObject> Obj;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Obj);
		return Obj;
	};

	// auto_save=false: the sweep must be reported as NOT run, with the reason named, and nothing
	// may be listed as written.
	{
		const TSharedPtr<FJsonObject> Obj = Run(/*bAutoSave=*/false);
		if (!TestTrue(TEXT("auto_save=false reply parses as JSON"), Obj.IsValid()))
		{
			return false;
		}
		TestFalse(TEXT("auto_save reports the sweep did not run"), Obj->GetBoolField(TEXT("auto_save")));
		TestEqual(TEXT("auto_save_note names the opt-out"), Obj->GetStringField(TEXT("auto_save_note")), FString(TEXT("opted_out")));

		const TArray<TSharedPtr<FJsonValue>>* Saved = nullptr;
		if (TestTrue(TEXT("saved_packages present"), Obj->TryGetArrayField(TEXT("saved_packages"), Saved)))
		{
			TestEqual(TEXT("nothing written when the caller opted out"), Saved->Num(), 0);
		}
	}

	// auto_save=true in a normal editor: the sweep runs, so the note must be empty. (Whether any
	// package was dirty is environment-dependent and deliberately not asserted.)
	{
		const TSharedPtr<FJsonObject> Obj = Run(/*bAutoSave=*/true);
		if (!TestTrue(TEXT("auto_save=true reply parses as JSON"), Obj.IsValid()))
		{
			return false;
		}
		const bool bRan = Obj->GetBoolField(TEXT("auto_save"));
		const FString Note = Obj->GetStringField(TEXT("auto_save_note"));

		// Outcome and note must agree: ran <=> empty note, and a skip must never be silent.
		TestEqual(TEXT("auto_save and auto_save_note agree"), bRan, Note.IsEmpty());
		if (!bRan)
		{
			// A legitimate skip is fine in an automation run, but it must say which one.
			const bool bKnownReason = Note == TEXT("previous_run_crashed") || Note == TEXT("editor_unavailable")
				|| Note == TEXT("pie_active") || Note == TEXT("save_failed");
			TestTrue(FString::Printf(TEXT("skip reason is a documented code (got '%s')"), *Note), bKnownReason);
		}
		// Either way it must never claim the caller opted out — the caller asked for the sweep.
		TestNotEqual(TEXT("auto_save=true never reports opted_out"), Note, FString(TEXT("opted_out")));
	}

	return true;
}

// The pre-run auto-save sweep must skip during a PIE / Simulate session. It used to test only
// GIsPlayInEditorWorld, which the engine sets only while the PIE world itself ticks — an MCP call
// dispatched from editor scope during PIE saw it false and flushed every dirty package through
// UEditorLoadingAndSavingUtils::SavePackages (which has no PIE guard of its own). GEditor->PlayWorld
// is what is non-null for the whole session.
//
// A real PIE session needs latent commands and a renderer; the suite runs headless under -nullrhi.
// So PIE is simulated by pointing GEditor->PlayWorld at a transient PIE-type world that the engine
// is never told about (bInformEngineOfWorld=false: no world context, nothing ticks it). The guard
// restores the previous PlayWorld on every exit path, then the world is destroyed and un-rooted.
//
// Deliberately, NO sweep runs outside simulated PIE: a real auto_save=true run writes whatever other
// tests left dirty (it once saved a project's startup map mid-suite). The non-PIE branch is covered
// by VibeUE.Python.AutoSaveReport, and this test failing before the fix is what shows it is not
// vacuous (that pre-fix run does sweep - it is the bug being proven).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibePythonAutoSaveSkippedDuringPIETest, "VibeUE.Python.AutoSaveSkippedDuringPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibePythonAutoSaveSkippedDuringPIETest::RunTest(const FString&)
{
	if (!GEditor)
	{
		AddError(TEXT("No GEditor; cannot simulate a PIE session."));
		return false;
	}
	if (!TestNull(TEXT("no real PIE session is running"), GEditor->PlayWorld.Get()))
	{
		return false;
	}

	// CreateWorld roots the world by default (bAddToRoot=true), hence the RemoveFromRoot below.
	UWorld* FakePlayWorld = UWorld::CreateWorld(EWorldType::PIE, /*bInformEngineOfWorld=*/false);
	if (!TestNotNull(TEXT("created the transient PIE world"), FakePlayWorld))
	{
		return false;
	}
	// Declared before the guard so it runs AFTER it: PlayWorld is restored first, then the world dies.
	ON_SCOPE_EXIT
	{
		FakePlayWorld->DestroyWorld(/*bInformEngineOfWorld=*/false);
		FakePlayWorld->RemoveFromRoot();
	};
	TGuardValue<TObjectPtr<UWorld>> PlayWorldGuard(GEditor->PlayWorld, FakePlayWorld);

	// The skip is logged as a Warning; it is the expected outcome here.
	AddExpectedMessage(TEXT("Cannot auto-save: Currently in PIE mode"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, 1, /*IsRegex=*/false);

	const FString Json = UPythonTools::ExecutePythonCode(TEXT("x = 1\n"), /*bAutoSave=*/true);
	TSharedPtr<FJsonObject> Obj;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Obj);
	if (!TestTrue(TEXT("PIE reply parses as JSON"), Obj.IsValid()))
	{
		return false;
	}
	TestTrue(TEXT("the script itself still runs during PIE"), Obj->GetBoolField(TEXT("success")));
	TestFalse(TEXT("auto_save reports the sweep did NOT run during PIE"), Obj->GetBoolField(TEXT("auto_save")));
	// A set crash latch would report "previous_run_crashed" here and fail this check - correctly.
	TestEqual(TEXT("auto_save_note names PIE"), Obj->GetStringField(TEXT("auto_save_note")), FString(TEXT("pie_active")));

	const TArray<TSharedPtr<FJsonValue>>* Saved = nullptr;
	if (TestTrue(TEXT("saved_packages present"), Obj->TryGetArrayField(TEXT("saved_packages"), Saved)))
	{
		TestEqual(TEXT("nothing written during PIE"), Saved->Num(), 0);
	}

	return true;
}

// Issue #608: an ordinary Python exception must NOT be treated as an editor crash.
// UPythonTools suppresses the next run's auto-save sweep after a "crash", on the reasoning that the
// editor may hold half-mutated objects. That latch used to be set for any PYTHON_RUNTIME_ERROR —
// which is also what a plain traceback returns — so a trivial AttributeError silently disabled
// auto-save for the following call. Only a real SEH crash (PYTHON_EDITOR_CRASH) should do that.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibePythonExceptionIsNotACrashTest, "VibeUE.Python.ExceptionIsNotACrash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibePythonExceptionIsNotACrashTest::RunTest(const FString&)
{
	auto Run = [](const TCHAR* Code)
	{
		const FString Json = UPythonTools::ExecutePythonCode(Code, /*bAutoSave=*/true);
		TSharedPtr<FJsonObject> Obj;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Obj);
		return Obj;
	};

	// Start from a known-good state so the latch is definitely clear.
	if (!TestTrue(TEXT("baseline run parses"), Run(TEXT("x = 1\n")).IsValid()))
	{
		return false;
	}

	// The interpreter prints the traceback through LogPython at Error verbosity (several lines:
	// "Traceback (most recent call last):", the File/line frames, ...). That output is the POINT of
	// this test, not a failure of it, so expect it. Matching on the category covers every line.
	AddExpectedError(TEXT("LogPython"), EAutomationExpectedErrorFlags::Contains, 0);

	// A script that raises: an ordinary, interpreter-caught exception.
	{
		const TSharedPtr<FJsonObject> Obj = Run(TEXT("raise RuntimeError('VibeUEIntentionalTestException')\n"));
		if (!TestTrue(TEXT("failing run still returns JSON"), Obj.IsValid()))
		{
			return false;
		}
		TestFalse(TEXT("the raising script is reported as failed"), Obj->GetBoolField(TEXT("success")));
		// It must be classified as a runtime error, NOT as an editor crash.
		TestEqual(TEXT("an ordinary exception is PYTHON_RUNTIME_ERROR"),
			Obj->GetStringField(TEXT("error_code")), FString(TEXT("PYTHON_RUNTIME_ERROR")));
		TestNotEqual(TEXT("an ordinary exception is never PYTHON_EDITOR_CRASH"),
			Obj->GetStringField(TEXT("error_code")), FString(TEXT("PYTHON_EDITOR_CRASH")));
	}

	// The next run must still sweep: the exception above must not have latched "crashed".
	{
		const TSharedPtr<FJsonObject> Obj = Run(TEXT("y = 2\n"));
		if (!TestTrue(TEXT("follow-up run parses"), Obj.IsValid()))
		{
			return false;
		}
		TestTrue(TEXT("follow-up run succeeds"), Obj->GetBoolField(TEXT("success")));
		TestNotEqual(TEXT("auto-save is NOT suppressed after an ordinary exception"),
			Obj->GetStringField(TEXT("auto_save_note")), FString(TEXT("previous_run_crashed")));
		TestTrue(TEXT("the auto-save sweep ran on the follow-up call"), Obj->GetBoolField(TEXT("auto_save")));
	}

	return true;
}

// A map opened as an asset stays resident, and the NEXT level load then fails the engine's
// stale-world check and FATALS the editor ("World Memory Leaks", EditorServer.cpp) - naming
// neither the script nor the tool that left it behind. GetResidentMapWorlds surfaces those
// stragglers in the reply of the run that created them.
//
// The fixture is a scratch UWorld in its own /Game package, NOT a real project map: loading one of
// those pulls in World Partition, and releasing it again trips
// `InitState == EWorldPartitionInitState::Uninitialized`. A bare NewObject<UWorld> reproduces
// exactly what the detector looks at (a non-transient package, an inactive world) with nothing to
// tear down.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibePythonResidentMapsTest, "VibeUE.Python.ResidentMapDetection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibePythonResidentMapsTest::RunTest(const FString&)
{
	if (!GEditor)
	{
		AddWarning(TEXT("No GEditor; skipping the resident-map test."));
		return true;
	}

	UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();

	// The open level must never be flagged - it is not a straggler, and reporting it would fire a
	// scary warning on every single execute_python_code call.
	if (EditorWorld)
	{
		TestFalse(TEXT("the open editor world is not reported as resident"),
			UPythonTools::GetResidentMapWorlds().Contains(EditorWorld->GetPathName()));
	}

	// A world in a real (non-transient) package, inactive, exactly like a map opened as an asset.
	UPackage* Package = CreatePackage(TEXT("/Game/__VibeUETest/W_ResidentProbe"));
	if (!TestNotNull(TEXT("created the scratch package"), Package))
	{
		return false;
	}
	UWorld* Probe = NewObject<UWorld>(Package, TEXT("W_ResidentProbe"), RF_Public | RF_Standalone);
	if (!TestNotNull(TEXT("created the scratch world"), Probe))
	{
		return false;
	}
	Probe->WorldType = EWorldType::Inactive;
	const FString ProbePath = Probe->GetPathName();

	ON_SCOPE_EXIT
	{
		if (UWorld* Leftover = FindObject<UWorld>(Package, TEXT("W_ResidentProbe")))
		{
			Leftover->ClearFlags(RF_Public | RF_Standalone);
			Leftover->MarkAsGarbage();
		}
	};

	// It must be reported while it is alive, and the open level still must not be.
	const TArray<FString> During = UPythonTools::GetResidentMapWorlds();
	TestTrue(*FString::Printf(TEXT("the resident world %s is reported"), *ProbePath),
		During.Contains(ProbePath));
	if (EditorWorld)
	{
		TestFalse(TEXT("the open editor world is still not reported"),
			During.Contains(EditorWorld->GetPathName()));
	}

	// Once released it must stop being reported, so the warning clears instead of sticking forever.
	Probe->ClearFlags(RF_Public | RF_Standalone);
	Probe->MarkAsGarbage();
	Probe = nullptr;

	TestFalse(*FString::Printf(TEXT("%s is no longer reported once released"), *ProbePath),
		UPythonTools::GetResidentMapWorlds().Contains(ProbePath));

	return true;
}

#endif // WITH_AUTOMATION_TESTS
