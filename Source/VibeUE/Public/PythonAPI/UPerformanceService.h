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
	 * Read back a trace and/or the log and return a perf summary (frame stats, worst frames, notable
	 * log lines, hitches). "both" succeeds only when both sources succeed; a usable single source is
	 * returned as status="partial", success=false, partial=true with per-source results.
	 * @param Source "trace", "logs", or "both" (default).
	 * @param File Optional override path; empty uses the last trace started/stopped.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Performance")
	static FString Analyse(const FString& Source = TEXT("both"), const FString& File = TEXT(""));

	/**
	 * Launch the game as a separate standalone process with a direct-to-file trace attached. The
	 * initial response is pending (not successful) until GetStandaloneStatus verifies the child PID
	 * and exact non-empty trace destination.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Performance")
	static FString StartStandalone(const FString& Name = TEXT("standalone_capture"), const FString& Channels = TEXT(""));

	/** Stop the tracked standalone process and verify graceful exit plus exact trace finalization. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Performance")
	static FString StopStandalone();

	/** Report tracked session/PID/map/path provenance and capture/finalization verification state. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Performance")
	static FString GetStandaloneStatus();



	/**
	 * Enable/disable editor background CPU throttling for this session (issue #549).
	 *
	 * An unfocused editor throttles to a few FPS, which makes unattended PIE verification useless —
	 * and neither `t.IdleWhenNotInForeground` nor `Slate.bAllowThrottling` controls this knob (it is
	 * UEditorPerformanceSettings.bThrottleCPUWhenNotForeground, not exposed to Python). Pass
	 * bEnabled=false before automated PIE runs; pass true to restore the default. Also defeats the
	 * minimized-window render disable. Session-only — nothing is saved to EditorSettings.ini.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Performance")
	static FString SetBackgroundThrottling(bool bEnabled);

	/** Report the current background-throttling state (see SetBackgroundThrottling). */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Performance")
	static FString GetBackgroundThrottling();

	/**
	 * Remove the ShouldDisableCPUThrottlingDelegates entry SetBackgroundThrottling(false) registered and, if
	 * that override is still on, put bThrottleCPUWhenNotForeground back to its value from before it. Called
	 * from the module's OnPreExit (while GEditor still exists) and ShutdownModule. The entry is a lambda INTO
	 * THIS DLL, so it must not outlive an unload (plugin reload / Live Coding). Safe to call twice and with
	 * GEditor null (then only the handle is reset). Not a UFUNCTION. GAME-THREAD ONLY.
	 */
	static void ShutdownThrottlingHook();
};
