// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Pure, side-effect-free verdict + budget logic, factored out of UPerformanceService::FrameTiming so it
// can be exercised headless by automation tests (no editor, no live frame globals). FrameTiming routes
// through these so the tests and the shipping method share one implementation and can never disagree.
namespace PerformanceVerdict
{
	// Result of classifying which thread bounds the frame. Mirrors the fields FrameTiming emits.
	struct FVerdict
	{
		FString Bound;          // "GameThread" | "RenderThread" | "RHIThread" | "GPU"
		FString Confidence;     // "clear" | "moderate" | "marginal" | "none"
		double  TopMs = 0.0;    // the bottleneck thread's ms
		double  RunnerMs = 0.0; // the runner-up's ms
		FString RunnerName;     // runner-up thread name ("" when only one thread ranked)
		double  MarginMs = 0.0; // TopMs - RunnerMs
		bool    bContested = false; // top two within CONTESTED_PCT -> a genuine tie
	};

	// Top must lead the runner-up by more than this fraction of its own cost to be a clear win.
	static constexpr double CONTESTED_PCT = 0.10;

	// Rank game/render/RHI/GPU, then judge how decisive the winner is. RHI and GPU each count only when
	// their timing is available: GRHIThreadTime is 0 when RHI-threading is off (its cost then folds into the
	// render thread, so ranking it would double-count), and GPU timing isn't always present. RHI IS a real
	// distinct bottleneck -- draw-call submission can saturate it independently of render-thread scene setup.
	// Note only GPU-unavailable downgrades confidence to "moderate": a missing GPU number hides a possible
	// bottleneck, whereas missing RHI just means its work was already counted on the render thread.
	inline FVerdict Classify(double GameMs, double RenderMs, double RhiMs, double GpuMs,
	                         bool bRhiAvailable, bool bGpuAvailable)
	{
		struct FThread { const TCHAR* Name; double Ms; };
		TArray<FThread, TInlineAllocator<4>> Threads;
		Threads.Add({ TEXT("GameThread"),   GameMs });
		Threads.Add({ TEXT("RenderThread"), RenderMs });
		if (bRhiAvailable) Threads.Add({ TEXT("RHIThread"), RhiMs });
		if (bGpuAvailable) Threads.Add({ TEXT("GPU"), GpuMs });
		Threads.Sort([](const FThread& A, const FThread& B) { return A.Ms > B.Ms; });

		FVerdict V;
		V.Bound      = Threads[0].Name;
		V.TopMs      = Threads[0].Ms;
		V.RunnerMs   = Threads.Num() > 1 ? Threads[1].Ms : 0.0;
		V.RunnerName = Threads.Num() > 1 ? FString(Threads[1].Name) : FString();
		V.MarginMs   = V.TopMs - V.RunnerMs;

		V.bContested = V.TopMs > 0.0 && (V.MarginMs / V.TopMs) < CONTESTED_PCT;

		if (V.TopMs <= 0.0)      V.Confidence = TEXT("none");     // no meaningful timing yet
		else if (V.bContested)   V.Confidence = TEXT("marginal"); // a tie
		else if (!bGpuAvailable) V.Confidence = TEXT("moderate"); // CPU winner, GPU unknown
		else                     V.Confidence = TEXT("clear");
		return V;
	}

	// Coerce a caller-supplied target FPS to something sane (NaN/inf/<=0 -> 60).
	inline double SafeTargetFps(float TargetFPS)
	{
		return (FMath::IsFinite(TargetFPS) && TargetFPS > 0.0f) ? (double)TargetFPS : 60.0;
	}

	// A single thread finishes inside the per-frame budget iff its ms does not exceed it.
	inline bool IsOverBudget(double Ms, double BudgetMs)
	{
		return Ms > BudgetMs;
	}

	struct FBudget
	{
		double TargetFps = 60.0;
		double BudgetMs = 0.0;
		double FrameMs = 0.0;
		double FrameHeadroomMs = 0.0;
		bool   bMeetsTarget = false; // PASS iff the frame has real timing AND fits the budget
	};

	inline FBudget ComputeBudget(double FrameMs, float TargetFPS)
	{
		FBudget B;
		B.TargetFps        = SafeTargetFps(TargetFPS);
		B.BudgetMs         = 1000.0 / B.TargetFps;
		B.FrameMs          = FrameMs;
		B.FrameHeadroomMs  = B.BudgetMs - FrameMs;
		B.bMeetsTarget     = FrameMs > 0.0 && FrameMs <= B.BudgetMs;
		return B;
	}

	// Race-free self-validation for ForceHitch. Given the peak per-thread ms the hitch actually induced --
	// measured synchronously by ForceHitch itself, NOT read back via a follow-up FrameTiming that races the
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
