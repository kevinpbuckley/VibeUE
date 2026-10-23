// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Utils/VibeUEReadinessSignal.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Json.h"
#include "Module.h"
#include "CoreGlobals.h"
#include "Misc/CoreDelegates.h"
#include "Utils/VibeUEHealthSignal.h"
#include "Async/Async.h"

#if WITH_AUTOMATION_TESTS

// Covers the readiness signal added in PR #527: path/PID binding, payload shape, and the
// publish/remove round trip against the real filesystem. Test path prefix VibeUE.Readiness.*.
// The end-to-end contract (launch script -> Editor-PID -> file appears) is a manual/agent check.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeReadinessSignalPathTest, "VibeUE.Readiness.SignalPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeReadinessSignalPathTest::RunTest(const FString&)
{
	const uint32 CurrentPid = FPlatformProcess::GetCurrentProcessId();
	const FString CurrentPath = FVibeUEReadinessSignal::GetSignalPath();

	TestEqual(TEXT("GetSignalPath == GetSignalPathForPid(current pid)"),
		CurrentPath, FVibeUEReadinessSignal::GetSignalPathForPid(CurrentPid));

	TestEqual(TEXT("file name is editor-<pid>-true.json"),
		FPaths::GetCleanFilename(CurrentPath), FString::Printf(TEXT("editor-%u-true.json"), CurrentPid));

	TestEqual(TEXT("lives in Saved/VibeUE/Signals"),
		FPaths::GetCleanFilename(FPaths::GetPath(CurrentPath)), TEXT("Signals"));

	TestTrue(TEXT("path is under the project Saved dir"),
		FPaths::IsUnderDirectory(CurrentPath, FPaths::ProjectSavedDir()));

	// A different PID must never resolve to this process's signal — that binding is the whole point.
	TestNotEqual(TEXT("a different pid yields a different path"),
		CurrentPath, FVibeUEReadinessSignal::GetSignalPathForPid(CurrentPid + 1));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeReadinessSignalPayloadTest, "VibeUE.Readiness.SignalPayload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeReadinessSignalPayloadTest::RunTest(const FString&)
{
	const FDateTime SessionStart(2026, 8, 3, 10, 0, 0);
	const FDateTime Created(2026, 8, 3, 10, 0, 42);
	const FString Payload = FVibeUEReadinessSignal::BuildSignalJson(4242, SessionStart, Created, TEXT("/Game/Maps/TestMap"), 8000, true);

	// The file is named .json, so it has to parse as JSON — the original implementation wrote an
	// empty file, which every consumer would have choked on.
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Payload);
	if (!TestTrue(TEXT("payload parses as JSON"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid()))
	{
		return false;
	}

	TestEqual(TEXT("signal name"), Root->GetStringField(TEXT("signal")), TEXT("toolsets-registered"));
	TestEqual(TEXT("pid round trips as a number"), static_cast<uint32>(Root->GetNumberField(TEXT("pid"))), 4242u);
	TestEqual(TEXT("createdUtc is ISO 8601"), Root->GetStringField(TEXT("createdUtc")), Created.ToIso8601());
	// sessionStartUtc is what lets an agent reject a stale signal from a reused PID.
	TestEqual(TEXT("sessionStartUtc is ISO 8601"), Root->GetStringField(TEXT("sessionStartUtc")), SessionStart.ToIso8601());
	TestFalse(TEXT("pluginVersion is populated"), Root->GetStringField(TEXT("pluginVersion")).IsEmpty());
	// currentMap lets an agent gate world edits on the loaded level without an editor round-trip.
	TestEqual(TEXT("currentMap round trips"), Root->GetStringField(TEXT("currentMap")), TEXT("/Game/Maps/TestMap"));
	// MCP endpoint status (issue B6): the port and listening flag must round-trip for agents that
	// gate on a live link.
	TestEqual(TEXT("mcpPort round trips as a number"), static_cast<uint32>(Root->GetNumberField(TEXT("mcpPort"))), 8000u);
	TestTrue(TEXT("mcpListening round trips"), Root->GetBoolField(TEXT("mcpListening")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeReadinessSignalPublishRemoveTest, "VibeUE.Readiness.PublishAndRemove",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeReadinessSignalPublishRemoveTest::RunTest(const FString&)
{
	IFileManager& FileManager = IFileManager::Get();
	const FString SignalPath = FVibeUEReadinessSignal::GetSignalPath();

	// RegisterToolsets already published this signal for the running editor, so restore it either way.
	const bool bWasPublished = FileManager.FileExists(*SignalPath);

	FVibeUEReadinessSignal::Remove();
	TestFalse(TEXT("Remove() deletes the signal"), FileManager.FileExists(*SignalPath));
	// Remove() on a missing file must be a no-op, not an error.
	FVibeUEReadinessSignal::Remove();

	TestTrue(TEXT("Publish() succeeds"), FVibeUEReadinessSignal::Publish());
	TestTrue(TEXT("signal exists after Publish()"), FileManager.FileExists(*SignalPath));
	TestFalse(TEXT("no .tmp staging file is left behind"), FileManager.FileExists(*(SignalPath + TEXT(".tmp"))));

	FString OnDisk;
	if (TestTrue(TEXT("published signal is readable"), FFileHelper::LoadFileToString(OnDisk, *SignalPath)))
	{
		TestFalse(TEXT("published signal is not empty"), OnDisk.IsEmpty());

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(OnDisk);
		if (TestTrue(TEXT("published signal parses as JSON"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid()))
		{
			TestEqual(TEXT("published pid is this process"),
				static_cast<uint32>(Root->GetNumberField(TEXT("pid"))), FPlatformProcess::GetCurrentProcessId());
		}
	}

	// Publishing twice must overwrite cleanly rather than fail on the existing file.
	TestTrue(TEXT("Publish() is idempotent"), FVibeUEReadinessSignal::Publish());
	TestTrue(TEXT("signal still exists after republish"), FileManager.FileExists(*SignalPath));

	if (!bWasPublished)
	{
		FVibeUEReadinessSignal::Remove();
	}

	return true;
}

// The bug: a forced exit (-ExecCmds="Automation RunTests ...; Quit" -> RequestExitWithStatus(true) ->
// TerminateProcess) runs neither OnPreExit nor ShutdownModule, so headless runs left editor-<pid>-true.json behind.
// The only callback that path still makes is FCoreDelegates::GetApplicationWillTerminateDelegate(); the module
// must be bound to it after startup, unbind on shutdown, and both must be idempotent. The delegate is never
// broadcast here: other engine listeners would act on it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeReadinessForcedExitHookTest, "VibeUE.Readiness.ForcedExitHook",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeReadinessForcedExitHookTest::RunTest(const FString&)
{
	if (!FModule::IsAvailable())
	{
		AddError(TEXT("The VibeUE module is not loaded."));
		return false;
	}
	FModule& Module = FModule::Get();
	const TTSMulticastDelegate<void()>& WillTerminate = FCoreDelegates::GetApplicationWillTerminateDelegate();

	// StartupModule skips every service under a commandlet, this hook included.
	if (IsRunningCommandlet())
	{
		TestFalse(TEXT("no forced-exit hook under a commandlet"), WillTerminate.IsBoundToObject(&Module));
		return true;
	}

	TestTrue(TEXT("the module is bound to ApplicationWillTerminate after startup"), WillTerminate.IsBoundToObject(&Module));
	TestTrue(TEXT("and holds its handle"), Module.IsWillTerminateHookRegistered());

	// Shutdown's unbind, twice.
	Module.UnregisterWillTerminateHook();
	TestFalse(TEXT("UnregisterWillTerminateHook unbinds it"), WillTerminate.IsBoundToObject(&Module));
	TestFalse(TEXT("and resets its handle"), Module.IsWillTerminateHookRegistered());
	Module.UnregisterWillTerminateHook();
	TestFalse(TEXT("a second UnregisterWillTerminateHook is a no-op"), WillTerminate.IsBoundToObject(&Module));

	// A second register must not bind twice: one unregister then leaves nothing bound.
	Module.RegisterWillTerminateHook();
	Module.RegisterWillTerminateHook();
	TestTrue(TEXT("RegisterWillTerminateHook binds it"), WillTerminate.IsBoundToObject(&Module));
	Module.UnregisterWillTerminateHook();
	TestFalse(TEXT("a repeated register bound only once"), WillTerminate.IsBoundToObject(&Module));

	// Leave the running editor hooked, as startup did.
	Module.RegisterWillTerminateHook();
	TestTrue(TEXT("the hook is restored for the rest of the session"), WillTerminate.IsBoundToObject(&Module) && Module.IsWillTerminateHookRegistered());
	return true;
}

// The bug: WillTerminate is not always broadcast on the game thread (ConsoleCtrlHandler's console-control thread on
// Ctrl-C / console close / logoff; a crashing worker, possibly the health writer, inside RequestExit(true)), and the
// first version of the handler called FVibeUEHealthSignal::Stop(), which Join()s the writer: a self-join on the
// writer, or a race with the game thread's own Stop() from OnPreExit. Run the real handler on a worker thread: it
// must finish promptly, delete both signal files, keep a heartbeat in flight from re-creating the health file, and
// leave the writer for the game thread to join, after which a second Stop() is a no-op. The editor's own heartbeat
// and readiness signal are restarted / republished at the end.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeReadinessForcedExitOffGameThreadTest, "VibeUE.Readiness.ForcedExitOffGameThread",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeReadinessForcedExitOffGameThreadTest::RunTest(const FString&)
{
	if (!FModule::IsAvailable())
	{
		AddError(TEXT("The VibeUE module is not loaded."));
		return false;
	}
	FModule& Module = FModule::Get();
	IFileManager& FileManager = IFileManager::Get();
	const FString HealthPath = FVibeUEHealthSignal::GetHealthPathForPid(FPlatformProcess::GetCurrentProcessId());
	const FString SignalPath = FVibeUEReadinessSignal::GetSignalPath();
	const bool bSignalWasPublished = FileManager.FileExists(*SignalPath);

	auto WaitForFile = [&FileManager](const FString& Path, const double Seconds)
	{
		const double Deadline = FPlatformTime::Seconds() + Seconds;
		while (!FileManager.FileExists(*Path) && FPlatformTime::Seconds() < Deadline)
		{
			FPlatformProcess::Sleep(0.05f);
		}
		return FileManager.FileExists(*Path);
	};

	// A running writer with a heartbeat on disk, and a readiness signal to delete.
	FVibeUEHealthSignal::Start();
	if (!TestTrue(TEXT("the health writer runs"), FVibeUEHealthSignal::IsWriterThreadOwned() && WaitForFile(HealthPath, 3.0)))
	{
		return false;
	}
	TestTrue(TEXT("the readiness signal is published"), FVibeUEReadinessSignal::Publish());

	// The handler, on a thread that is not the game thread.
	TFuture<void> Done = Async(EAsyncExecution::Thread, [&Module]() { Module.OnApplicationWillTerminate(); });
	if (!TestTrue(TEXT("the off-game-thread handler finishes within 5 s (no join, no hang)"), Done.WaitFor(FTimespan::FromSeconds(5.0))))
	{
		return false;
	}
	TestTrue(TEXT("the game thread still owns the writer after an off-game-thread forced-exit cleanup"), FVibeUEHealthSignal::IsWriterThreadOwned());
	TestFalse(TEXT("the health file is deleted"), FileManager.FileExists(*HealthPath));
	TestFalse(TEXT("the readiness signal is deleted"), FileManager.FileExists(*SignalPath));

	// The woken writer leaves without writing again: no heartbeat re-creates the file.
	FPlatformProcess::Sleep(0.5f);
	TestFalse(TEXT("no heartbeat re-creates the health file after the forced-exit cleanup"), FileManager.FileExists(*HealthPath));

	// The game thread's later Stop() (OnPreExit after a Ctrl-C) joins and frees it; a second one is a no-op.
	FVibeUEHealthSignal::Stop();
	TestFalse(TEXT("the game thread's Stop() joins the writer"), FVibeUEHealthSignal::IsWriterThreadOwned());
	FVibeUEHealthSignal::Stop();
	TestFalse(TEXT("a second Stop() is a no-op"), FVibeUEHealthSignal::IsWriterThreadOwned());

	// Put the running editor's signals back.
	FVibeUEHealthSignal::Start();
	TestTrue(TEXT("the heartbeat restarts afterwards"), FVibeUEHealthSignal::IsWriterThreadOwned() && WaitForFile(HealthPath, 3.0));
	if (bSignalWasPublished)
	{
		TestTrue(TEXT("the readiness signal is republished"), FVibeUEReadinessSignal::Publish());
	}
	else
	{
		FVibeUEReadinessSignal::Remove();
	}
	return true;
}

#endif // WITH_AUTOMATION_TESTS
