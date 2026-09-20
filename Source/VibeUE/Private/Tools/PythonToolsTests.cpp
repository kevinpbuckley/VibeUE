// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "Tools/PythonTools.h"
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

#endif // WITH_AUTOMATION_TESTS
