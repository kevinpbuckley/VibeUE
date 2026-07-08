// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "PythonAPI/PerformanceVerdict.h"
#include "PythonAPI/UPerformanceService.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"

// Pure verdict/budget logic (PerformanceVerdict.h) is exercised headless — no editor, no live frame
// globals — and FrameTiming routes through the same helpers, so green here means the shipping verdict
// is green. The two live smoke tests additionally assert the JSON contract and input validation.

static const EAutomationTestFlags kPerfTestFlags =
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

// ---------------------------------------------------------------------------
// Verdict classification
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibePerfVerdictClearTest,
	"VibeUE.Performance.Verdict.ClearWinners", kPerfTestFlags)
bool FVibePerfVerdictClearTest::RunTest(const FString&)
{
	using namespace PerformanceVerdict;

	// Game clearly slowest -> GameThread, clear, runner-up is the next-slowest (GPU at 11).
	{
		const FVerdict V = Classify(25.0, 8.0, /*Rhi*/ 0.0, 11.0, /*bRhi*/ false, /*bGpuAvailable*/ true);
		TestEqual(TEXT("game bound"), V.Bound, FString(TEXT("GameThread")));
		TestEqual(TEXT("game confidence"), V.Confidence, FString(TEXT("clear")));
		TestFalse(TEXT("game not contested"), V.bContested);
		TestEqual(TEXT("game runner-up"), V.RunnerName, FString(TEXT("GPU")));
		TestEqual(TEXT("game margin"), V.MarginMs, 14.0, 1e-6);
	}
	// Render clearly slowest.
	{
		const FVerdict V = Classify(8.0, 25.0, 0.0, 11.0, false, true);
		TestEqual(TEXT("render bound"), V.Bound, FString(TEXT("RenderThread")));
		TestEqual(TEXT("render confidence"), V.Confidence, FString(TEXT("clear")));
	}
	// GPU clearly slowest.
	{
		const FVerdict V = Classify(8.0, 11.0, 0.0, 25.0, false, true);
		TestEqual(TEXT("gpu bound"), V.Bound, FString(TEXT("GPU")));
		TestEqual(TEXT("gpu confidence"), V.Confidence, FString(TEXT("clear")));
	}
	// RHI clearly slowest (RHI-threading on) -> a distinct RHI-bound verdict.
	{
		const FVerdict V = Classify(8.0, 11.0, /*Rhi*/ 25.0, 9.0, /*bRhi*/ true, true);
		TestEqual(TEXT("rhi bound"), V.Bound, FString(TEXT("RHIThread")));
		TestEqual(TEXT("rhi confidence"), V.Confidence, FString(TEXT("clear")));
		TestFalse(TEXT("rhi not contested"), V.bContested);
		TestEqual(TEXT("rhi runner-up is render"), V.RunnerName, FString(TEXT("RenderThread")));
	}
	// RHI available but not the winner -> existing game verdict is unchanged, RHI just joins the ranking.
	{
		const FVerdict V = Classify(25.0, 8.0, /*Rhi*/ 9.0, 11.0, /*bRhi*/ true, true);
		TestEqual(TEXT("still game bound"), V.Bound, FString(TEXT("GameThread")));
		TestEqual(TEXT("still clear"), V.Confidence, FString(TEXT("clear")));
		TestFalse(TEXT("rhi present but not contested"), V.bContested);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibePerfVerdictContestedTest,
	"VibeUE.Performance.Verdict.Contested", kPerfTestFlags)
bool FVibePerfVerdictContestedTest::RunTest(const FString&)
{
	using namespace PerformanceVerdict;

	// Top two within 10% -> a tie: contested, confidence downgraded to marginal, runner-up named.
	const FVerdict V = Classify(25.0, 24.0, 0.0, 5.0, false, true);
	TestEqual(TEXT("bound is the nominal top"), V.Bound, FString(TEXT("GameThread")));
	TestTrue(TEXT("contested"), V.bContested);
	TestEqual(TEXT("confidence marginal"), V.Confidence, FString(TEXT("marginal")));
	TestEqual(TEXT("contested_with"), V.RunnerName, FString(TEXT("RenderThread")));

	// marginal (tie) must win over moderate (GPU-unavailable) when both apply.
	const FVerdict M = Classify(10.0, 9.5, 0.0, 0.0, false, /*bGpuAvailable*/ false);
	TestTrue(TEXT("tie contested even w/o gpu"), M.bContested);
	TestEqual(TEXT("marginal beats moderate"), M.Confidence, FString(TEXT("marginal")));

	// RHI can contest the render thread -> contested tie names RHIThread as the runner-up.
	const FVerdict R = Classify(8.0, 25.0, /*Rhi*/ 24.0, 5.0, /*bRhi*/ true, true);
	TestEqual(TEXT("render nominally top"), R.Bound, FString(TEXT("RenderThread")));
	TestTrue(TEXT("render/rhi contested"), R.bContested);
	TestEqual(TEXT("contested_with rhi"), R.RunnerName, FString(TEXT("RHIThread")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibePerfVerdictBoundaryTest,
	"VibeUE.Performance.Verdict.ContestedBoundary", kPerfTestFlags)
bool FVibePerfVerdictBoundaryTest::RunTest(const FString&)
{
	using namespace PerformanceVerdict;

	// Exactly 10% margin is NOT contested (strict < CONTESTED_PCT). 10 vs 9 -> 1/10 == 0.10 -> clear.
	const FVerdict V = Classify(10.0, 9.0, 0.0, 3.0, false, true);
	TestFalse(TEXT("exactly 10% is not contested"), V.bContested);
	TestEqual(TEXT("boundary is clear"), V.Confidence, FString(TEXT("clear")));

	// Just inside 10% (9.5/10 -> margin 0.5, 5%) IS contested.
	const FVerdict C = Classify(10.0, 9.5, 0.0, 3.0, false, true);
	TestTrue(TEXT("just under 10% is contested"), C.bContested);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibePerfVerdictGpuUnavailableTest,
	"VibeUE.Performance.Verdict.GpuUnavailable", kPerfTestFlags)
bool FVibePerfVerdictGpuUnavailableTest::RunTest(const FString&)
{
	using namespace PerformanceVerdict;

	// GPU timing unavailable: GPU is dropped from the ranking, and a clear CPU winner is downgraded to
	// "moderate" because a hidden GPU cost can't be ruled out.
	const FVerdict V = Classify(25.0, 8.0, 0.0, 0.0, false, /*bGpuAvailable*/ false);
	TestEqual(TEXT("bound game"), V.Bound, FString(TEXT("GameThread")));
	TestEqual(TEXT("confidence moderate"), V.Confidence, FString(TEXT("moderate")));
	TestFalse(TEXT("not contested"), V.bContested);
	TestEqual(TEXT("runner is render (gpu excluded)"), V.RunnerName, FString(TEXT("RenderThread")));

	// RHI unavailable must NOT downgrade confidence (its work was counted on the render thread) — only a
	// missing GPU number does. GPU present + RHI absent + clear CPU winner stays "clear".
	const FVerdict G = Classify(25.0, 8.0, /*Rhi*/ 0.0, 11.0, /*bRhi*/ false, /*bGpuAvailable*/ true);
	TestEqual(TEXT("rhi-absent stays clear"), G.Confidence, FString(TEXT("clear")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibePerfVerdictNoneTest,
	"VibeUE.Performance.Verdict.NoTiming", kPerfTestFlags)
bool FVibePerfVerdictNoneTest::RunTest(const FString&)
{
	using namespace PerformanceVerdict;

	// No meaningful timing yet -> confidence "none", not contested.
	const FVerdict V = Classify(0.0, 0.0, 0.0, 0.0, false, true);
	TestEqual(TEXT("confidence none"), V.Confidence, FString(TEXT("none")));
	TestFalse(TEXT("zero not contested"), V.bContested);
	return true;
}

// ---------------------------------------------------------------------------
// Budget gate
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibePerfBudgetGateTest,
	"VibeUE.Performance.Budget.Gate", kPerfTestFlags)
bool FVibePerfBudgetGateTest::RunTest(const FString&)
{
	using namespace PerformanceVerdict;

	// 10 ms frame vs 60 FPS (16.67 ms) -> PASS with positive headroom.
	{
		const FBudget B = ComputeBudget(10.0, 60.0f);
		TestEqual(TEXT("budget ms"), B.BudgetMs, 1000.0 / 60.0, 1e-6);
		TestTrue(TEXT("10ms meets 60"), B.bMeetsTarget);
		TestEqual(TEXT("headroom"), B.FrameHeadroomMs, (1000.0 / 60.0) - 10.0, 1e-6);
	}
	// 25 ms frame vs 60 FPS -> FAIL, negative headroom.
	{
		const FBudget B = ComputeBudget(25.0, 60.0f);
		TestFalse(TEXT("25ms fails 60"), B.bMeetsTarget);
		TestTrue(TEXT("negative headroom"), B.FrameHeadroomMs < 0.0);
	}
	// A zero frame time is not a pass (no real timing).
	{
		const FBudget B = ComputeBudget(0.0, 60.0f);
		TestFalse(TEXT("zero frame not a pass"), B.bMeetsTarget);
	}
	// Other target: 8 ms vs 120 FPS (8.33 ms) -> PASS.
	{
		const FBudget B = ComputeBudget(8.0, 120.0f);
		TestEqual(TEXT("120fps budget"), B.BudgetMs, 1000.0 / 120.0, 1e-6);
		TestTrue(TEXT("8ms meets 120"), B.bMeetsTarget);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibePerfBudgetSafeFpsTest,
	"VibeUE.Performance.Budget.SafeTargetFps", kPerfTestFlags)
bool FVibePerfBudgetSafeFpsTest::RunTest(const FString&)
{
	using namespace PerformanceVerdict;

	// Invalid targets fall back to 60 FPS rather than dividing by zero / NaN.
	TestEqual(TEXT("zero -> 60"),     SafeTargetFps(0.0f),  60.0, 1e-9);
	TestEqual(TEXT("negative -> 60"), SafeTargetFps(-5.0f), 60.0, 1e-9);
	TestEqual(TEXT("nan -> 60"),      SafeTargetFps(FMath::Sqrt(-1.0f)), 60.0, 1e-9);
	TestEqual(TEXT("valid passes through"), SafeTargetFps(30.0f), 30.0, 1e-9);

	// ComputeBudget with a bad FPS still yields the 60 FPS budget.
	const FBudget B = ComputeBudget(10.0, 0.0f);
	TestEqual(TEXT("bad fps budget"), B.BudgetMs, 1000.0 / 60.0, 1e-6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibePerfBudgetPerThreadTest,
	"VibeUE.Performance.Budget.PerThreadOverBudget", kPerfTestFlags)
bool FVibePerfBudgetPerThreadTest::RunTest(const FString&)
{
	using namespace PerformanceVerdict;
	const double Budget = 1000.0 / 60.0; // 16.67 ms

	TestTrue(TEXT("20ms over 16.67"),  IsOverBudget(20.0, Budget));
	TestFalse(TEXT("10ms under"),      IsOverBudget(10.0, Budget));
	TestFalse(TEXT("exactly budget is not over"), IsOverBudget(Budget, Budget));
	return true;
}

// ---------------------------------------------------------------------------
// Live smoke tests — run in-editor, exercise the real AICallable surface
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibePerfFrameTimingShapeTest,
	"VibeUE.Performance.FrameTiming.JsonShape", kPerfTestFlags)
bool FVibePerfFrameTimingShapeTest::RunTest(const FString&)
{
	const FString Json = UPerformanceService::FrameTiming(60.0f);

	TSharedPtr<FJsonObject> Obj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
	{
		AddError(FString::Printf(TEXT("FrameTiming did not return valid JSON: %s"), *Json.Left(200)));
		return false;
	}

	TestTrue(TEXT("success"), Obj->GetBoolField(TEXT("success")));

	// Required top-level fields of the verdict contract.
	for (const TCHAR* Field : { TEXT("game_thread_ms"), TEXT("render_thread_ms"), TEXT("rhi_thread_ms"),
		TEXT("gpu_ms"), TEXT("frame_ms"), TEXT("fps"), TEXT("bound"), TEXT("bound_confidence"),
		TEXT("margin_ms"), TEXT("contested"), TEXT("hint"), TEXT("pie_running") })
	{
		TestTrue(FString::Printf(TEXT("has field %s"), Field), Obj->HasField(Field));
	}

	// Budget sub-object contract.
	const TSharedPtr<FJsonObject>* Budget = nullptr;
	if (TestTrue(TEXT("has budget object"), Obj->TryGetObjectField(TEXT("budget"), Budget)) && Budget)
	{
		for (const TCHAR* Field : { TEXT("target_fps"), TEXT("budget_ms"), TEXT("threads"),
			TEXT("frame_headroom_ms"), TEXT("meets_target"), TEXT("verdict") })
		{
			TestTrue(FString::Printf(TEXT("budget has %s"), Field), (*Budget)->HasField(Field));
		}
	}

	// The live bound must be one of the known verdicts (or none when there's no timing).
	const FString Bound = Obj->GetStringField(TEXT("bound"));
	const bool bKnown = Bound == TEXT("GameThread") || Bound == TEXT("RenderThread")
		|| Bound == TEXT("RHIThread") || Bound == TEXT("GPU");
	TestTrue(FString::Printf(TEXT("bound is a known thread: %s"), *Bound), bKnown);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibePerfForceHitchValidationTest,
	"VibeUE.Performance.ForceHitch.RejectsBadThread", kPerfTestFlags)
bool FVibePerfForceHitchValidationTest::RunTest(const FString&)
{
	// An unknown thread name must be rejected up front (before any stall is scheduled).
	const FString Json = UPerformanceService::ForceHitch(TEXT("banana"), 1.0f, 1);

	TSharedPtr<FJsonObject> Obj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
	{
		AddError(TEXT("ForceHitch did not return valid JSON"));
		return false;
	}
	TestFalse(TEXT("bad thread not success"), Obj->GetBoolField(TEXT("success")));
	TestEqual(TEXT("error code"), Obj->GetStringField(TEXT("error_code")), FString(TEXT("BAD_THREAD")));
	return true;
}

// Race-free self-validation logic: given the peak per-thread ms a hitch induced, decide whether it landed
// on the requested thread(s). Pure -> exercised headless, no live timing.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibePerfHitchExpectTest,
	"VibeUE.Performance.ForceHitch.MatchesExpect", kPerfTestFlags)
bool FVibePerfHitchExpectTest::RunTest(const FString&)
{
	using namespace PerformanceVerdict;
	const double Min = 125.0; // half of a 250 ms stall

	// game: game peak clears the floor AND dominates render -> match; render untouched must not match "game".
	TestTrue (TEXT("game hit"),          HitchMatchesExpect(TEXT("game"),   250.0,   0.0, Min));
	TestFalse(TEXT("game undershoot"),   HitchMatchesExpect(TEXT("game"),    50.0,   0.0, Min));
	TestFalse(TEXT("game but render bigger"), HitchMatchesExpect(TEXT("game"), 250.0, 300.0, Min));

	// render: symmetric.
	TestTrue (TEXT("render hit"),        HitchMatchesExpect(TEXT("render"),   0.0, 250.0, Min));
	TestFalse(TEXT("render undershoot"), HitchMatchesExpect(TEXT("render"),   0.0,  50.0, Min));

	// both: both threads must clear the floor.
	TestTrue (TEXT("both hit"),          HitchMatchesExpect(TEXT("both"),   250.0, 250.0, Min));
	TestFalse(TEXT("both one short"),    HitchMatchesExpect(TEXT("both"),   250.0,  50.0, Min));

	// rhi: the RHI peak (5th arg) must clear the floor AND dominate both CPU threads.
	TestTrue (TEXT("rhi hit"),           HitchMatchesExpect(TEXT("rhi"),      0.0,   0.0, Min, /*Rhi*/ 250.0));
	TestFalse(TEXT("rhi undershoot"),    HitchMatchesExpect(TEXT("rhi"),      0.0,   0.0, Min, /*Rhi*/  50.0));
	TestFalse(TEXT("rhi but render bigger"), HitchMatchesExpect(TEXT("rhi"),  0.0, 300.0, Min, /*Rhi*/ 250.0));
	// rhi with no RHI peak (threading off -> stall folded into render) must not falsely match.
	TestFalse(TEXT("rhi threading off"), HitchMatchesExpect(TEXT("rhi"),      0.0, 250.0, Min, /*Rhi*/   0.0));

	// gpu / unknown are never self-validated by CPU peaks.
	TestFalse(TEXT("gpu not matched here"), HitchMatchesExpect(TEXT("gpu"), 999.0, 999.0, Min));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibePerfForceHitchSelfMeasureTest,
	"VibeUE.Performance.ForceHitch.SelfMeasure", kPerfTestFlags)
bool FVibePerfForceHitchSelfMeasureTest::RunTest(const FString&)
{
	// A game-thread stall must be induced AND measured within the single call: the response carries the
	// observed peak and verdict_matched_expect=true, with no follow-up FrameTiming.
	const FString Json = UPerformanceService::ForceHitch(TEXT("game"), 20.0f, 1);

	TSharedPtr<FJsonObject> Obj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
	{
		AddError(FString::Printf(TEXT("ForceHitch did not return valid JSON: %s"), *Json.Left(200)));
		return false;
	}

	TestTrue(TEXT("success"), Obj->GetBoolField(TEXT("success")));
	TestTrue(TEXT("has observed_peak_game_ms"), Obj->HasField(TEXT("observed_peak_game_ms")));
	TestTrue(TEXT("has verdict_matched_expect"), Obj->HasField(TEXT("verdict_matched_expect")));

	// The 20 ms sleep should measure at least the 10 ms match floor (sleep never undershoots by half).
	const double Peak = Obj->GetNumberField(TEXT("observed_peak_game_ms"));
	TestTrue(FString::Printf(TEXT("induced >= 10ms (got %.1f)"), Peak), Peak >= 10.0);
	TestTrue(TEXT("verdict matched"), Obj->GetBoolField(TEXT("verdict_matched_expect")));
	return true;
}

// The rhi arm is a ground-truth trigger for the RHIThread verdict. Headless runs under -nullrhi with no
// separate RHI thread, so this asserts the graceful fallback: the mode is accepted (not BAD_THREAD),
// rhi_threading reports false, a warning is surfaced, and nothing was validated. The live RHIThread-wins
// path can only be confirmed in a session with RHI-threading ON.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibePerfForceHitchRhiTest,
	"VibeUE.Performance.ForceHitch.RhiArm", kPerfTestFlags)
bool FVibePerfForceHitchRhiTest::RunTest(const FString&)
{
	const FString Json = UPerformanceService::ForceHitch(TEXT("rhi"), 20.0f, 1);

	TSharedPtr<FJsonObject> Obj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
	{
		AddError(FString::Printf(TEXT("ForceHitch rhi did not return valid JSON: %s"), *Json.Left(200)));
		return false;
	}

	// 'rhi' is a valid mode — accepted, not rejected as BAD_THREAD.
	TestTrue(TEXT("success"), Obj->GetBoolField(TEXT("success")));
	TestEqual(TEXT("forcing rhi"), Obj->GetStringField(TEXT("forcing")), FString(TEXT("rhi")));
	TestTrue(TEXT("has rhi_threading"), Obj->HasField(TEXT("rhi_threading")));

	// Headless nullrhi has no separate RHI thread: expect the flagged fallback, no induced verdict.
	if (!Obj->GetBoolField(TEXT("rhi_threading")))
	{
		TestTrue(TEXT("warns rhi-threading off"), Obj->HasField(TEXT("rhi_warning")));
		TestFalse(TEXT("nothing validated w/o rhi thread"), Obj->GetBoolField(TEXT("verdict_matched_expect")));
	}
	return true;
}

// Report() renders a self-contained HTML "printout" from the live verdict + capture summary. Headless
// (nullrhi, no trace) exercises the no-capture path: the file must still be written and the JSON contract
// (report_file / fix_count / included_capture) honoured.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibePerfReportSmokeTest,
	"VibeUE.Performance.Report.WritesFile", kPerfTestFlags)
bool FVibePerfReportSmokeTest::RunTest(const FString&)
{
	const FString Json = UPerformanceService::Report(TEXT("Smoke Test"), TEXT("both"), TEXT(""));

	TSharedPtr<FJsonObject> Obj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
	{
		AddError(FString::Printf(TEXT("Report did not return valid JSON: %s"), *Json.Left(200)));
		return false;
	}

	TestTrue(TEXT("success"), Obj->GetBoolField(TEXT("success")));

	// Contract fields.
	TestTrue(TEXT("has report_file"), Obj->HasField(TEXT("report_file")));
	TestTrue(TEXT("has fix_count"), Obj->HasField(TEXT("fix_count")));
	TestTrue(TEXT("has included_capture"), Obj->HasField(TEXT("included_capture")));

	// No trace is available headless, so the capture column must be absent.
	TestFalse(TEXT("no capture headless"), Obj->GetBoolField(TEXT("included_capture")));

	// The reported path must point at a real, non-empty file on disk.
	const FString Path = Obj->GetStringField(TEXT("report_file"));
	TestTrue(TEXT("report_file non-empty"), !Path.IsEmpty());
	IFileManager& FM = IFileManager::Get();
	if (TestTrue(FString::Printf(TEXT("file exists: %s"), *Path), FM.FileExists(*Path)))
	{
		TestTrue(TEXT("file has content"), FM.FileSize(*Path) > 0);
		FM.Delete(*Path); // don't litter the project's Saved dir with test artifacts
	}
	return true;
}

#endif // WITH_AUTOMATION_TESTS
