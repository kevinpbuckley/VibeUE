// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "Tools/PythonTools.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformTime.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Tests/AutomationCommon.h"

namespace VibePythonAwaitTest
{
	constexpr double TimeoutSeconds = 30.0;
	const TCHAR* const CancelKey = TEXT("VibePythonAwaitTest");

	struct FRun
	{
		bool bDone = false;
		FString Reply;
		TSharedPtr<FJsonObject> Json;
		double Started = 0.0;
	};

	TSharedRef<FRun> Start(const FString& Code, bool bFreshNamespace = false)
	{
		TSharedRef<FRun> Run = MakeShared<FRun>();
		Run->Started = FPlatformTime::Seconds();
		UPythonTools::StartAwaitRun(Code, /*bAutoSave=*/false, bFreshNamespace, CancelKey, [Run](const FString& Reply)
		{
			Run->Reply = Reply;
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Reply), Run->Json);
			Run->bDone = true;
		});
		return Run;
	}

	TSharedPtr<FJsonObject> RunNow(const FString& Code, bool bAutoSave = false, bool bFreshNamespace = false)
	{
		TSharedPtr<FJsonObject> Json;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(UPythonTools::ExecutePythonCode(Code, bAutoSave, bFreshNamespace)), Json);
		return Json;
	}

	FString Output(const TSharedPtr<FJsonObject>& Json)
	{
		return Json.IsValid() ? Json->GetStringField(TEXT("output")) : FString();
	}

	/** Waits for Run, then calls Check; a run that never ends is cancelled and fails the test. */
	void WhenDone(FAutomationTestBase* Test, const TSharedRef<FRun>& Run, TFunction<void()> Check)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Test, Run, Check = MoveTemp(Check)]()
		{
			if (!Run->bDone)
			{
				if (FPlatformTime::Seconds() - Run->Started < TimeoutSeconds)
				{
					return false;
				}
				Test->AddError(TEXT("the script did not end in time"));
				UPythonTools::CancelAwaitRuns(CancelKey);
				return true;
			}
			Check();
			return true;
		}));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibePythonAwaitDetectionTest, "VibeUE.Python.Await.Detection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibePythonAwaitDetectionTest::RunTest(const FString&)
{
	TestFalse(TEXT("no await"), UPythonTools::NeedsAwaitRun(TEXT("print(1)\n")));
	TestFalse(TEXT("await only inside a function"), UPythonTools::NeedsAwaitRun(TEXT("async def f():\n    await g()\n")));
	TestFalse(TEXT("the word in a string"), UPythonTools::NeedsAwaitRun(TEXT("print('await x')\n")));
	TestFalse(TEXT("the word in a comment"), UPythonTools::NeedsAwaitRun(TEXT("# await x\nprint(1)\n")));
	TestFalse(TEXT("a syntax error goes to the normal path, which reports it"), UPythonTools::NeedsAwaitRun(TEXT("await (\n")));
	TestTrue(TEXT("top-level await"), UPythonTools::NeedsAwaitRun(TEXT("import asyncio\nawait asyncio.sleep(0)\n")));
	TestTrue(TEXT("top-level async for"), UPythonTools::NeedsAwaitRun(TEXT("async for a in b:\n    pass\n")));
	TestTrue(TEXT("top-level async with"), UPythonTools::NeedsAwaitRun(TEXT("async with a as b:\n    pass\n")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibePythonAwaitTicksTest, "VibeUE.Python.Await.EditorTicksAndOutputIsCaptured",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibePythonAwaitTicksTest::RunTest(const FString&)
{
	using namespace VibePythonAwaitTest;
	const TSharedRef<FRun> Run = Start(TEXT(
		"import asyncio, unreal\n"
		"first = unreal.SystemLibrary.get_frame_count()\n"
		"print('AwaitBefore')\n"
		"await asyncio.sleep(0)\n"
		"await asyncio.sleep(0.05)\n"
		"print('AwaitAfter', unreal.SystemLibrary.get_frame_count() > first)\n"
		"_vibe_await_kept = 41 + 1\n"));
	TestEqual(TEXT("the script is paused, not finished"), UPythonTools::GetAwaitRunCount() > 0, true);

	WhenDone(this, Run, [this, Run]()
	{
		if (!TestTrue(FString::Printf(TEXT("the reply is JSON and a success: %s"), *Run->Reply),
			Run->Json.IsValid() && Run->Json->GetBoolField(TEXT("success"))))
		{
			return;
		}
		const FString Printed = Output(Run->Json);
		TestTrue(FString::Printf(TEXT("prints from before and after the awaits are in the reply: %s"), *Printed),
			Printed.Contains(TEXT("AwaitBefore")) && Printed.Contains(TEXT("AwaitAfter")));
		TestTrue(FString::Printf(TEXT("the editor ticked during the script: %s"), *Printed), Printed.Contains(TEXT("AwaitAfter True")));
		TestTrue(TEXT("the run has an id"), Run->Json->GetNumberField(TEXT("run_id")) > 0);
		TestTrue(TEXT("the reply carries the dirty report"), Run->Json->HasField(TEXT("dirtied_packages")) && Run->Json->HasField(TEXT("cleaned_packages")));
		TestEqual(TEXT("no script is left"), UPythonTools::GetAwaitRunCount(), 0);

		const FString Kept = Output(RunNow(TEXT("print('Kept', _vibe_await_kept)\ndel _vibe_await_kept\n")));
		TestTrue(FString::Printf(TEXT("by default a script's globals are the ones every call shares: %s"), *Kept), Kept.Contains(TEXT("Kept 42")));
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibePythonAwaitErrorTest, "VibeUE.Python.Await.ExceptionIsReported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibePythonAwaitErrorTest::RunTest(const FString&)
{
	using namespace VibePythonAwaitTest;

	// The interpreter prints the traceback through LogPython at Error verbosity. That output is the
	// point of this test, so expect it; matching on the category covers every line.
	AddExpectedError(TEXT("LogPython"), EAutomationExpectedErrorFlags::Contains, 0);

	const TSharedRef<FRun> Run = Start(TEXT("import asyncio\nawait asyncio.sleep(0)\nraise ValueError('await-boom')\n"));
	WhenDone(this, Run, [this, Run]()
	{
		if (!TestTrue(TEXT("the reply is JSON"), Run->Json.IsValid()))
		{
			return;
		}
		TestFalse(TEXT("the reply is a failure"), Run->Json->GetBoolField(TEXT("success")));
		TestEqual(TEXT("an exception is a runtime error, not a crash"), Run->Json->GetStringField(TEXT("error_code")), FString(TEXT("PYTHON_RUNTIME_ERROR")));
		const FString Message = Run->Json->GetStringField(TEXT("error_message"));
		TestTrue(FString::Printf(TEXT("the message has the exception and its line: %s"), *Message),
			Message.Contains(TEXT("await-boom")) && Message.Contains(TEXT("line 3")));
		TestEqual(TEXT("no script is left"), UPythonTools::GetAwaitRunCount(), 0);
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibePythonAwaitCancelTest, "VibeUE.Python.Await.Cancel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibePythonAwaitCancelTest::RunTest(const FString&)
{
	using namespace VibePythonAwaitTest;
	const TSharedRef<FRun> Run = Start(TEXT("import asyncio\nawait asyncio.sleep(3600)\n"));
	UPythonTools::CancelAwaitRuns(CancelKey);
	WhenDone(this, Run, [this, Run]()
	{
		if (TestTrue(TEXT("the reply is JSON"), Run->Json.IsValid()))
		{
			TestEqual(TEXT("the reply says cancelled"), Run->Json->GetStringField(TEXT("error_code")), FString(TEXT("OPERATION_CANCELLED")));
		}
		TestEqual(TEXT("no script is left"), UPythonTools::GetAwaitRunCount(), 0);
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibePythonAwaitAutoSaveTest, "VibeUE.Python.Await.NoAutoSaveWhileAScriptIsPaused",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibePythonAwaitAutoSaveTest::RunTest(const FString&)
{
	using namespace VibePythonAwaitTest;
	const TSharedRef<FRun> Run = Start(TEXT("import asyncio\nawait asyncio.sleep(3600)\n"));
	const TSharedPtr<FJsonObject> Other = RunNow(TEXT("print('OtherCall')\n"), /*bAutoSave=*/true);
	if (TestTrue(TEXT("another call still runs"), Other.IsValid() && Other->GetBoolField(TEXT("success"))))
	{
		TestFalse(TEXT("its sweep did not run"), Other->GetBoolField(TEXT("auto_save")));
		TestEqual(TEXT("and it says why"), Other->GetStringField(TEXT("auto_save_note")), FString(TEXT("await_run_in_progress")));
	}
	UPythonTools::CancelAwaitRuns(CancelKey);
	WhenDone(this, Run, [this]()
	{
		TestEqual(TEXT("no script is left"), UPythonTools::GetAwaitRunCount(), 0);
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibePythonAwaitBusyTest, "VibeUE.Python.Await.TooManyScriptsAreRefused",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibePythonAwaitBusyTest::RunTest(const FString&)
{
	using namespace VibePythonAwaitTest;
	TArray<TSharedRef<FRun>> Runs;
	for (int32 Index = 0; Index < 9; ++Index)
	{
		Runs.Add(Start(TEXT("import asyncio\nawait asyncio.sleep(3600)\n"), /*bFreshNamespace=*/true));
	}
	const TSharedRef<FRun> Refused = Runs.Last();
	if (TestTrue(TEXT("the ninth is answered at once"), Refused->bDone && Refused->Json.IsValid()))
	{
		TestEqual(TEXT("with BUSY"), Refused->Json->GetStringField(TEXT("error_code")), FString(TEXT("BUSY")));
	}
	UPythonTools::CancelAwaitRuns(CancelKey);
	WhenDone(this, Runs[0], [this, Runs]()
	{
		for (const TSharedRef<FRun>& Run : Runs)
		{
			TestTrue(TEXT("every script ended"), Run->bDone);
		}
		TestEqual(TEXT("no script is left"), UPythonTools::GetAwaitRunCount(), 0);
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibePythonFreshNamespaceTest, "VibeUE.Python.Await.FreshNamespace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibePythonFreshNamespaceTest::RunTest(const FString&)
{
	using namespace VibePythonAwaitTest;
	const FString Probe = TEXT("print('Seen', '_vibe_fresh_probe' in globals())\n");

	const TSharedPtr<FJsonObject> Fresh = RunNow(TEXT("_vibe_fresh_probe = 1\nprint(__name__)\n"), false, /*bFreshNamespace=*/true);
	TestTrue(TEXT("a fresh script runs as __main__"), Output(Fresh).Contains(TEXT("__main__")));
	TestTrue(TEXT("its global is gone afterwards"), Output(RunNow(Probe)).Contains(TEXT("Seen False")));

	RunNow(TEXT("_vibe_fresh_probe = 1\n"));
	TestTrue(TEXT("a default script's global stays"), Output(RunNow(Probe)).Contains(TEXT("Seen True")));
	TestTrue(TEXT("and a fresh script does not see it"), Output(RunNow(Probe, false, /*bFreshNamespace=*/true)).Contains(TEXT("Seen False")));
	RunNow(TEXT("del _vibe_fresh_probe\n"));

	const TSharedRef<FRun> Run = Start(TEXT("import asyncio\n_vibe_fresh_await = 1\nawait asyncio.sleep(0)\nprint('InScript', _vibe_fresh_await)\n"), /*bFreshNamespace=*/true);
	WhenDone(this, Run, [this, Run]()
	{
		TestTrue(TEXT("a fresh await script keeps its globals across an await"), Output(Run->Json).Contains(TEXT("InScript 1")));
		TestTrue(TEXT("and they are gone afterwards"), Output(RunNow(TEXT("print('Seen', '_vibe_fresh_await' in globals())\n"))).Contains(TEXT("Seen False")));
	});
	return true;
}

#endif // WITH_AUTOMATION_TESTS
