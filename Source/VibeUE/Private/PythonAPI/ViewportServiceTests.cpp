// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "PythonAPI/UViewportService.h"
#include "Editor.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

// ViewportService needs a level viewport client and the window helpers need a main editor window. A headless
// (-nullrhi, unattended) host may have neither, so each test checks first and skips with a warning, the suite's
// pattern. Test path prefix VibeUE.ViewportService.*

namespace
{
	const EAutomationTestFlags kViewportTestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	// Checked without calling the service, whose getters log a warning when there is no client.
	bool HasLevelViewportClient()
	{
		return GEditor && GEditor->GetLevelViewportClients().Num() > 0;
	}

	TSharedPtr<FJsonObject> ParseJson(const FString& Text)
	{
		TSharedPtr<FJsonObject> Obj;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Obj);
		return Obj;
	}

	FString StringFieldOf(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
	{
		FString Value;
		if (Obj.IsValid())
		{
			Obj->TryGetStringField(Field, Value);
		}
		return Value;
	}

	bool BoolFieldOf(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, bool bDefault)
	{
		bool bValue = bDefault;
		if (Obj.IsValid())
		{
			Obj->TryGetBoolField(Field, bValue);
		}
		return bValue;
	}
}

// ---------------------------------------------------------------------------
// get_viewport_info -> change -> restore_viewport_info reads back the snapshot
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeViewportRestoreRoundTripTest,
	"VibeUE.ViewportService.RestoreViewportInfoRoundTrip", kViewportTestFlags)
bool FVibeViewportRestoreRoundTripTest::RunTest(const FString&)
{
	if (!HasLevelViewportClient())
	{
		AddWarning(TEXT("SKIPPED: no level viewport client in this host (headless / -nullrhi), so the viewport round trip cannot be exercised."));
		return true;
	}
	if (GEditor->PlayWorld)
	{
		AddWarning(TEXT("SKIPPED: a PIE session is running; the level viewport may be showing it."));
		return true;
	}

	const FViewportInfo Before = UViewportService::GetViewportInfo();

	// Change everything RestoreViewportInfo owns, through the public setters a script would use.
	const float OtherFOV = Before.FOV > 100.0f ? Before.FOV - 23.0f : Before.FOV + 23.0f;
	TestTrue(TEXT("set_fov applied"), UViewportService::SetFOV(OtherFOV));
	TestTrue(TEXT("set_exposure applied"), UViewportService::SetExposure(!Before.bExposureFixed, Before.ExposureEV100 + 3.5f));

	const FViewportInfo Changed = UViewportService::GetViewportInfo();
	TestNotEqual(TEXT("precondition: FOV changed"), Changed.FOV, Before.FOV);
	TestNotEqual(TEXT("precondition: exposure mode changed"), Changed.bExposureFixed, Before.bExposureFixed);

	// Also move the camera, via a modified copy of the snapshot (the service has no camera setter of its own).
	FViewportInfo Moved = Changed;
	Moved.Location = Before.Location + FVector(123.0, -45.0, 67.0);
	Moved.Rotation = FRotator(Before.Rotation.Pitch - 10.0, Before.Rotation.Yaw + 30.0, Before.Rotation.Roll);
	TestTrue(TEXT("restore_viewport_info applies a modified snapshot"), UViewportService::RestoreViewportInfo(Moved));
	const FViewportInfo MovedReadback = UViewportService::GetViewportInfo();
	TestTrue(TEXT("precondition: camera moved"), MovedReadback.Location.Equals(Moved.Location, 0.01));

	// The restore under test.
	TestTrue(TEXT("restore_viewport_info returns true"), UViewportService::RestoreViewportInfo(Before));
	const FViewportInfo After = UViewportService::GetViewportInfo();

	TestTrue(TEXT("location restored"), After.Location.Equals(Before.Location, 0.01));
	TestTrue(TEXT("rotation restored"), After.Rotation.Equals(Before.Rotation, 0.01));
	TestEqual(TEXT("FOV restored"), After.FOV, Before.FOV);
	TestEqual(TEXT("exposure mode restored"), After.bExposureFixed, Before.bExposureFixed);
	TestEqual(TEXT("EV100 restored"), After.ExposureEV100, Before.ExposureEV100);
	TestEqual(TEXT("base realtime unchanged"), After.bRealtimeBase, Before.bRealtimeBase);
	TestEqual(TEXT("game view unchanged"), After.bIsGameView, Before.bIsGameView);

	// Restoring a snapshot that already matches is a no-op that still succeeds.
	TestTrue(TEXT("restoring a matching snapshot succeeds"), UViewportService::RestoreViewportInfo(Before));
	const FViewportInfo Again = UViewportService::GetViewportInfo();
	TestEqual(TEXT("FOV still restored"), Again.FOV, Before.FOV);
	TestEqual(TEXT("exposure still restored"), Again.bExposureFixed, Before.bExposureFixed);

	// Getter/writer parity: restore reads realtime_base, never is_realtime (which includes overrides such as a
	// backgrounded editor's "Background Process" realtime-off). A snapshot whose is_realtime disagrees with its
	// base, as one taken in the background does, must leave the base flag alone.
	FViewportInfo OverrideSkewed = Before;
	OverrideSkewed.bIsRealtime = !Before.bRealtimeBase;
	TestTrue(TEXT("restore with an override-skewed is_realtime succeeds"), UViewportService::RestoreViewportInfo(OverrideSkewed));
	TestEqual(TEXT("an override-skewed is_realtime does not change the base realtime flag"),
		UViewportService::GetViewportInfo().bRealtimeBase, Before.bRealtimeBase);

	// A snapshot from another view type: the pose would land in the wrong (perspective vs ortho) transform, so it
	// is skipped with a warning, and the rest is still restored.
	FViewportInfo OtherType = Before;
	OtherType.ViewportType = Before.ViewportType.Equals(TEXT("top"), ESearchCase::IgnoreCase) ? TEXT("perspective") : TEXT("top");
	OtherType.Location = Before.Location + FVector(0.0, 0.0, 999.0);
	AddExpectedMessagePlain(TEXT("camera pose NOT restored"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	TestTrue(TEXT("restore of an other-view-type snapshot still returns true"), UViewportService::RestoreViewportInfo(OtherType));
	TestTrue(TEXT("an other-view-type pose is not written"), UViewportService::GetViewportInfo().Location.Equals(Before.Location, 0.01));
	return true;
}

// ---------------------------------------------------------------------------
// The apply-and-restore core, driven without an RHI (no Draw): the view is applied inside the scope and
// restored by its destructor. This is what makes capture_with_view's restore claim testable headless.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeViewportTemporaryViewRestoresTest,
	"VibeUE.ViewportService.TemporaryViewRestores", kViewportTestFlags)
bool FVibeViewportTemporaryViewRestoresTest::RunTest(const FString&)
{
	if (!HasLevelViewportClient())
	{
		AddWarning(TEXT("SKIPPED: no level viewport client in this host, so the temporary view cannot be applied."));
		return true;
	}
	if (GEditor->PlayWorld)
	{
		AddWarning(TEXT("SKIPPED: a PIE session is running; the level viewport may be showing it."));
		return true;
	}

	const FViewportInfo Before = UViewportService::GetViewportInfo();
	const FVector ShotLocation = Before.Location + FVector(500.0, -250.0, 300.0);
	const FRotator ShotRotation(Before.Rotation.Pitch - 15.0, Before.Rotation.Yaw + 45.0, Before.Rotation.Roll);
	const float ShotFOV = Before.FOV > 100.0f ? 60.0f : 110.0f;
	const float ShotEV = Before.ExposureEV100 + 4.0f;

	bool bBodyRan = false;
	FViewportInfo During;
	const bool bRan = UViewportService::RunWithTemporaryView(ShotLocation, ShotRotation, ShotFOV, /*bFixedExposure=*/true, ShotEV,
		[&bBodyRan, &During]()
		{
			bBodyRan = true;
			During = UViewportService::GetViewportInfo();
		});
	if (!TestTrue(TEXT("the temporary view ran"), bRan && bBodyRan))
	{
		return false;
	}

	// Inside the scope the view really was applied (so the restore below is not comparing the viewport with itself).
	TestTrue(TEXT("during: location applied"), During.Location.Equals(ShotLocation, 0.01));
	TestTrue(TEXT("during: rotation applied"), During.Rotation.Equals(ShotRotation, 0.01));
	TestEqual(TEXT("during: FOV applied"), During.FOV, ShotFOV);
	TestTrue(TEXT("during: exposure fixed"), During.bExposureFixed);
	TestEqual(TEXT("during: EV100 applied"), During.ExposureEV100, ShotEV);

	// After the scope: everything back.
	const FViewportInfo After = UViewportService::GetViewportInfo();
	TestTrue(TEXT("after: location restored"), After.Location.Equals(Before.Location, 0.01));
	TestTrue(TEXT("after: rotation restored"), After.Rotation.Equals(Before.Rotation, 0.01));
	TestEqual(TEXT("after: FOV restored"), After.FOV, Before.FOV);
	TestEqual(TEXT("after: exposure mode restored"), After.bExposureFixed, Before.bExposureFixed);
	TestEqual(TEXT("after: EV100 restored"), After.ExposureEV100, Before.ExposureEV100);
	TestEqual(TEXT("after: base realtime unchanged"), After.bRealtimeBase, Before.bRealtimeBase);
	TestEqual(TEXT("after: game view unchanged"), After.bIsGameView, Before.bIsGameView);
	return true;
}

// ---------------------------------------------------------------------------
// capture_with_view leaves the viewport exactly as it found it
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeViewportCaptureWithViewRestoresTest,
	"VibeUE.ViewportService.CaptureWithViewRestores", kViewportTestFlags)
bool FVibeViewportCaptureWithViewRestoresTest::RunTest(const FString&)
{
	if (!HasLevelViewportClient())
	{
		AddWarning(TEXT("SKIPPED: no level viewport client in this host (headless / -nullrhi), so capture_with_view cannot be exercised."));
		return true;
	}
	if (GEditor->PlayWorld)
	{
		AddWarning(TEXT("SKIPPED: a PIE session is running; capture_with_view refuses during PIE."));
		return true;
	}

	const FViewportInfo Before = UViewportService::GetViewportInfo();
	const FString OutPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("VibeUE"), TEXT("Captures"), TEXT("ViewportServiceTest_CaptureWithView.png"));

	// A view unlike the current one in every field the capture applies.
	const FSceneCaptureResult Res = UViewportService::CaptureWithView(
		Before.Location + FVector(500.0, 0.0, 300.0), FRotator(-25.0, Before.Rotation.Yaw + 45.0, 0.0),
		64, 36, OutPath, Before.FOV > 100.0f ? 60.0f : 110.0f, /*bFixedExposure=*/true, Before.ExposureEV100 + 4.0f);
	// The capture itself needs a real RHI and a drawable, unminimized window. A refusal raised before the view was
	// applied (a headless viewport has zero size) never reaches the restore, so asserting "restored" would compare
	// the viewport with itself: skip instead. TemporaryViewRestores covers the restore path headless.
	AddInfo(FString::Printf(TEXT("capture_with_view: success=%s view_applied=%s %s"), Res.bSuccess ? TEXT("true") : TEXT("false"),
		Res.bViewApplied ? TEXT("true") : TEXT("false"), Res.bSuccess ? *Res.OutputPath : *Res.ErrorMessage));
	if (!Res.bViewApplied)
	{
		TestFalse(TEXT("a refusal is not a success"), Res.bSuccess);
		AddWarning(FString::Printf(TEXT("SKIPPED: capture_with_view refused before applying the view (%s), so its restore was not exercised here."), *Res.ErrorMessage));
		return true;
	}
	if (Res.bSuccess)
	{
		TestEqual(TEXT("output width"), Res.Width, 64);
		TestEqual(TEXT("output height"), Res.Height, 36);
		TestTrue(TEXT("the PNG is on disk"), IFileManager::Get().FileSize(*Res.OutputPath) > 0);
		IFileManager::Get().Delete(*Res.OutputPath, /*RequireExists=*/false, /*EvenReadOnly=*/true, /*Quiet=*/true);
	}

	const FViewportInfo After = UViewportService::GetViewportInfo();
	TestTrue(TEXT("location restored"), After.Location.Equals(Before.Location, 0.01));
	TestTrue(TEXT("rotation restored"), After.Rotation.Equals(Before.Rotation, 0.01));
	TestEqual(TEXT("FOV restored"), After.FOV, Before.FOV);
	TestEqual(TEXT("exposure mode restored"), After.bExposureFixed, Before.bExposureFixed);
	TestEqual(TEXT("EV100 restored"), After.ExposureEV100, Before.ExposureEV100);
	TestEqual(TEXT("base realtime unchanged"), After.bRealtimeBase, Before.bRealtimeBase);
	TestEqual(TEXT("game view unchanged"), After.bIsGameView, Before.bIsGameView);
	return true;
}

// ---------------------------------------------------------------------------
// get/set_editor_window_state JSON contract
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeViewportEditorWindowStateTest,
	"VibeUE.ViewportService.EditorWindowState", kViewportTestFlags)
bool FVibeViewportEditorWindowStateTest::RunTest(const FString&)
{
	// A bad state is refused before any window is looked up, so this holds headless too.
	const TSharedPtr<FJsonObject> Bad = ParseJson(UViewportService::SetEditorWindowState(TEXT("sideways")));
	if (TestTrue(TEXT("bad-state reply is JSON"), Bad.IsValid()))
	{
		TestFalse(TEXT("bad state fails"), BoolFieldOf(Bad, TEXT("success"), true));
		TestEqual(TEXT("bad state error code"), StringFieldOf(Bad, TEXT("error_code")), FString(TEXT("BAD_STATE")));
	}

	const TSharedPtr<FJsonObject> State = ParseJson(UViewportService::GetEditorWindowState());
	if (!TestTrue(TEXT("get_editor_window_state reply is JSON"), State.IsValid()))
	{
		return false;
	}
	const bool bHasWindow = BoolFieldOf(State, TEXT("success"), false);
	if (!bHasWindow)
	{
		// The headless assertion: no window is reported cleanly, not as a crash or a malformed reply.
		TestEqual(TEXT("no window error code"), StringFieldOf(State, TEXT("error_code")), FString(TEXT("NO_WINDOW")));
		TestFalse(TEXT("no window has an error message"), StringFieldOf(State, TEXT("error_message")).IsEmpty());
		const TSharedPtr<FJsonObject> Restore = ParseJson(UViewportService::SetEditorWindowState(TEXT("restore")));
		TestEqual(TEXT("restore without a window reports NO_WINDOW"), StringFieldOf(Restore, TEXT("error_code")), FString(TEXT("NO_WINDOW")));
		AddWarning(TEXT("SKIPPED window arm: this host has no main editor window, so restore/no-op cannot be exercised."));
		return true;
	}

	for (const TCHAR* Field : { TEXT("is_minimized"), TEXT("is_maximized"), TEXT("is_foreground"), TEXT("is_visible") })
	{
		TestTrue(FString::Printf(TEXT("state has bool field %s"), Field), State->HasTypedField<EJson::Boolean>(Field));
	}
	TestTrue(TEXT("state has a title"), State->HasTypedField<EJson::String>(TEXT("title")));

	if (BoolFieldOf(State, TEXT("is_minimized"), false))
	{
		AddWarning(TEXT("SKIPPED no-op arm: the editor window is minimized right now, and this test does not change the user's window."));
		return true;
	}

	// "restore" on a window that is not minimized is a no-op that succeeds.
	const bool bWasMaximized = BoolFieldOf(State, TEXT("is_maximized"), false);
	const TSharedPtr<FJsonObject> Restored = ParseJson(UViewportService::SetEditorWindowState(TEXT("restore")));
	if (TestTrue(TEXT("restore reply is JSON"), Restored.IsValid()))
	{
		TestTrue(TEXT("restore on a non-minimized window succeeds"), BoolFieldOf(Restored, TEXT("success"), false));
		TestEqual(TEXT("reply names the state"), StringFieldOf(Restored, TEXT("state")), FString(TEXT("restore")));
		TestFalse(TEXT("was_minimized is false"), BoolFieldOf(Restored, TEXT("was_minimized"), true));
		TestFalse(TEXT("is_minimized is false"), BoolFieldOf(Restored, TEXT("is_minimized"), true));
		TestEqual(TEXT("a maximized window stays maximized"), BoolFieldOf(Restored, TEXT("is_maximized"), !bWasMaximized), bWasMaximized);
		TestTrue(TEXT("reply has is_foreground"), Restored->HasTypedField<EJson::Boolean>(TEXT("is_foreground")));
		TestTrue(TEXT("reply has a title"), Restored->HasTypedField<EJson::String>(TEXT("title")));
	}
	return true;
}

#endif // WITH_AUTOMATION_TESTS
