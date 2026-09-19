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

#endif // WITH_AUTOMATION_TESTS
