// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UPerformanceService.generated.h"

/**
 * Performance & tracing service — frame-timing triage, Unreal Insights trace capture, trace+log
 * analysis, and trace-attached standalone play.
 *
 * Unreal 5.8's native toolsets have NO performance or tracing tools, so this is VibeUE's net-new
 * capability: it answers "are we CPU- or GPU-bound?" and drives Insights captures the engine's
 * EditorAppToolset (which can start PIE/Simulate) cannot measure.
 *
 * Recommended flow: FrameTiming() FIRST (CPU vs GPU bound verdict) → StartTrace → reproduce the
 * workload → StopTrace → Analyse. For a representative reading, profile under PIE or a standalone
 * session, not the bare editor viewport. Methods return a JSON string.
 */
UCLASS(BlueprintType)
class VIBEUE_API UPerformanceService : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Report Game/Render/GPU/RHI thread ms + a CPU-vs-GPU bound verdict and hint for the most recently
	 * rendered frame (the same data as the on-screen "stat unit"). RUN THIS FIRST in any frame-rate
	 * investigation — optimising the GPU does nothing if the frame is game- or render-thread bound.
	 *
	 * The verdict is robust: when the top two threads are within a small margin it reports the frame as
	 * "contested" with a confidence level rather than declaring a false winner, and it flags when GPU
	 * timing is unavailable (gpu_ms == 0) so you don't trust a CPU verdict that GPU could overturn.
	 *
	 * Also reports a per-thread budget breakdown against a target frame rate: each thread's headroom
	 * against the per-frame budget and a pass/fail gate, so the same call that triages can also guard.
	 * @param TargetFPS Frame-rate budget to gate against (e.g. 60, 120). Defaults to 60.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Performance")
	static FString FrameTiming(float TargetFPS = 60.0f);

	/**
	 * TEST/VALIDATION HELPER — deliberately induce a frame hitch on a chosen thread to verify that
	 * FrameTiming's verdict, budget gate and contested detection fire correctly against a KNOWN ground truth.
	 *
	 *   Thread = "game"   -> stall the game thread   `Milliseconds` ms/frame  (expect bound=GameThread)
	 *   Thread = "render" -> stall the render thread `Milliseconds` ms/frame  (expect bound=RenderThread)
	 *   Thread = "both"   -> stall game AND render by the same amount         (expect contested=true)
	 *   Thread = "gpu"    -> supersample via r.ScreenPercentage to force GPU cost, auto-restored after
	 *                        `Frames` frames (scene-dependent best-effort; `Milliseconds` is ignored).
	 *
	 * The CPU paths (game/render/both) are SELF-MEASURING and race-free: the stall is applied
	 * synchronously inside the call and timed directly, so the response carries `observed_peak_game_ms` /
	 * `observed_peak_render_ms` and a `verdict_matched_expect` bool. Validate from THIS single response —
	 * do NOT follow up with FrameTiming, whose read runs on the game thread the stall blocks and reliably
	 * lands on a clean frame. The gpu path is async (cost persists across frames), so for it alone read
	 * FrameTiming on a following frame and confirm `expect`. Total CPU blocking is capped (~10 s) for safety.
	 *
	 * @param Thread       "game" (default), "render", "both", or "gpu".
	 * @param Milliseconds Stall size per frame for the CPU threads. Clamped to [1, 5000]. Ignored for gpu.
	 * @param Frames       Consecutive frames to sustain the hitch. Clamped to [1, 600]; CPU frames further
	 *                     capped so total synchronous stall stays under ~10 s. Use >1 to simulate jitter.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Performance")
	static FString ForceHitch(const FString& Thread = TEXT("game"), float Milliseconds = 250.0f, int32 Frames = 1);

	/**
	 * Render a self-contained HTML performance report from the current verdict and the most recent capture,
	 * and write it to <Project>/Saved/VibeUE/Performance/report_<timestamp>.html — a shareable "printout" you
	 * can open in any browser, screenshot, or hand to the team. Pulls the live FrameTiming verdict + budget and
	 * (when a trace/log is available) the Analyse frame stats, worst frames and PSO hitches, then builds a
	 * data-driven "fix in this order" list — each fix carries the concrete stat/console commands to run and
	 * a link to the matching Unreal Engine docs. No external assets (CSS is inlined), so the file is portable.
	 *
	 * @param Title  Heading for the report. Defaults to "VibeUE Performance Report".
	 * @param Source "trace", "logs", or "both" (default) — which capture to summarise, same as Analyse.
	 * @param File   Optional trace/log override; empty uses the last trace started/stopped.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Performance")
	static FString Report(const FString& Title = TEXT("VibeUE Performance Report"),
	                      const FString& Source = TEXT("both"), const FString& File = TEXT(""));

	/**
	 * Start an Unreal Insights trace to file.
	 * @param Name Trace file name (without extension).
	 * @param Channels Comma-separated trace channels; empty uses the default set (frame,cpu,gpu,log,...).
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Performance")
	static FString StartTrace(const FString& Name = TEXT("mcp_capture"), const FString& Channels = TEXT(""));

	/** Stop the active trace. Returns the trace file path and size. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Performance")
	static FString StopTrace();

	/** Report whether a trace is active and which channels are enabled. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Performance")
	static FString GetTraceStatus();

	/** Drop a named bookmark in the active trace. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Performance")
	static FString Bookmark(const FString& Name);

	/** Begin a named region in the active trace. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Performance")
	static FString RegionStart(const FString& Name);

	/** End a named region in the active trace. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Performance")
	static FString RegionEnd(const FString& Name);

	/**
	 * Read back a trace and/or the log and return a perf summary (frame stats, worst frames, notable
	 * log lines, hitches).
	 * @param Source "trace", "logs", or "both" (default).
	 * @param File Optional override path; empty uses the last trace started/stopped.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Performance")
	static FString Analyse(const FString& Source = TEXT("both"), const FString& File = TEXT(""));

	/**
	 * Launch the game as a separate standalone process with a trace attached (representative readings
	 * that the editor viewport can't give). Connects back to the editor's Unreal Trace Server.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Performance")
	static FString StartStandalone(const FString& Name = TEXT("standalone_capture"), const FString& Channels = TEXT(""));

	/** Stop the standalone process and finalise its trace/log. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Performance")
	static FString StopStandalone();

	/** Report whether a standalone session is running and which trace/log it is writing. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Performance")
	static FString GetStandaloneStatus();

	/**
	 * Start Play-In-Editor IN-PROCESS (inside the editor's own process) so FrameTiming and ForceHitch
	 * read the live PIE game world. This is what makes the in-process CPU-hitch validation land:
	 * ForceHitch's game/render stalls only take effect when a game world is actually ticking.
	 *
	 * PREFER StartStandalone for real stall identification. PIE is NOT representative — it shares the
	 * editor's already-warm shader/PSO caches and on-demand cooked data, so it hides costs a standalone
	 * or shipping build actually pays. Use PIE for quick in-process checks and for validating the
	 * verdict/hitch logic against a live world; use Standalone when the numbers have to be trusted.
	 *
	 * PIE starts on the next editor tick, so read FrameTiming on a FOLLOWING frame (pie_running flips true).
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Performance")
	static FString StartPIE();

	/** Stop the in-process Play-In-Editor session started with StartPIE. Tears down on the next tick. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Performance")
	static FString StopPIE();
};
