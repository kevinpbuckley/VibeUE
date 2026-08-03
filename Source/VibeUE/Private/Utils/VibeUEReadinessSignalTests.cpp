// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Utils/VibeUEReadinessSignal.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Json.h"

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
	const FString Payload = FVibeUEReadinessSignal::BuildSignalJson(4242, SessionStart, Created);

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

#endif // WITH_AUTOMATION_TESTS
