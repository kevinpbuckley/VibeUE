// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Pure, side-effect-free hitch validation for UPerformanceService::ForceHitch.
//
// This file used to hold the whole frame-timing verdict and budget model (Classify, ComputeBudget,
// IsOverBudget, SafeTargetFps, FVerdict, FBudget). All of it served FrameTiming, which was cut on
// the 5-9 branch because the engine's PerfToolset.StatsToolset.GetFrameTimings reports the same
// per-thread split -- and the arithmetic on top of it (largest thread gates the frame, compare it
// against 1000/target_fps) is the agent's to do, which is what the profiling and frame-rate skills
// now teach. The second reason the split existed, "so it can be exercised headless by automation
// tests", also went: the C++ automation tests were removed in favour of test_prompts.
namespace PerformanceVerdict
{
	// Race-free self-validation for ForceHitch. Given the peak per-thread ms the hitch actually induced --
	// measured synchronously by ForceHitch itself, NOT read back via a following frame that races the
	// stall on the game thread -- did the stall land on the thread(s) the caller asked for? MinExpectedMs is
	// the floor a thread's induced time must clear to count as hit (set below the requested stall to tolerate
	// scheduler slop). "gpu" is not validated here: its cost is scene-dependent and can't be measured
	// synchronously, so it keeps the read-a-following-frame path. "rhi" self-validates only when RHI-threading
	// is on (a distinct RHI thread exists to stall); the peak must clear the floor AND dominate both CPU
	// threads, or the stall folded into the render thread and there's no RHIThread verdict to confirm.
	inline bool HitchMatchesExpect(const FString& Mode, double PeakGameMs, double PeakRenderMs,
	                               double MinExpectedMs, double PeakRhiMs = 0.0)
	{
		const bool bGameHit   = PeakGameMs   >= MinExpectedMs;
		const bool bRenderHit = PeakRenderMs >= MinExpectedMs;
		const bool bRhiHit    = PeakRhiMs    >= MinExpectedMs;
		if (Mode == TEXT("game"))   return bGameHit   && PeakGameMs   > PeakRenderMs;
		if (Mode == TEXT("render")) return bRenderHit && PeakRenderMs > PeakGameMs;
		if (Mode == TEXT("both"))   return bGameHit   && bRenderHit;
		if (Mode == TEXT("rhi"))    return bRhiHit    && PeakRhiMs > PeakRenderMs && PeakRhiMs > PeakGameMs;
		return false; // gpu / unknown -- not self-validated by induced CPU peaks
	}
}
