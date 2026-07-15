// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UPerformanceTriageSkill.h"

UPerformanceTriageSkill::UPerformanceTriageSkill()
{
	// Shown in ListSkills — keep it one line so the assistant can decide whether to open the skill.
	Description = TEXT(
		"Frame-rate triage & validation: determine whether the game is CPU (game/render) or GPU bound "
		"BEFORE optimising, gate against an FPS budget, capture Unreal Insights traces, and validate the "
		"verdict with deliberately forced hitches.");

	// Read via GetSkills -> GetDetails(). This is the strategy the per-tool descriptions can't carry.
	Instructions = FString(
		TEXT("VibeUE's performance tools answer 'am I CPU- or GPU-bound?' and drive Insights traces. All tools return a JSON string.\n")
		TEXT("\n")
		TEXT("# Golden rule\n")
		TEXT("ALWAYS call frame_timing FIRST. Optimising the GPU is wasted effort if the frame is game- or render-thread bound, and vice-versa. Never start a trace or suggest a fix before you know the `bound` thread.\n")
		TEXT("\n")
		TEXT("# 1. Triage: frame_timing(target_fps=60)\n")
		TEXT("Read these fields:\n")
		TEXT("- `bound`: GameThread | RenderThread | RHIThread | GPU — the bottleneck thread.\n")
		TEXT("- `bound_confidence`: clear | moderate | marginal | none. Trust `clear`; be cautious below it.\n")
		TEXT("- `contested` / `contested_with`: when true the top two threads are within ~10% — frame-to-frame noise can flip the winner. Treat BOTH as bottlenecks; take several readings or trace both.\n")
		TEXT("- `gpu_ms == 0`: GPU timing was unavailable, so a CPU verdict is UNCONFIRMED. Confirm with the resolution test: `r.ScreenPercentage 50` should noticeably raise FPS if truly GPU-bound.\n")
		TEXT("- `budget`: per-thread headroom + PASS/FAIL against target_fps. Gate CI/perf checks on `budget.meets_target`; read each thread's `over_budget` to see which one blew the budget.\n")
		TEXT("- `pie_running`: if false you are reading the EDITOR viewport, NOT the game. Start PIE (start_pie, or use start_standalone) and park in a representative/worst spot before trusting numbers.\n")
		TEXT("\n")
		TEXT("# 2. Act on the verdict\n")
		TEXT("- bound == GPU: NOW profile the GPU. start_trace(channels 'frame,gpu,cpu') -> reproduce -> stop_trace -> analyse; or 'ProfileGPU'. Levers: shadows (Virtual Shadow Maps), Lumen, translucency, post-process, ScreenPercentage.\n")
		TEXT("- bound == RenderThread: too many draw calls / primitives or dynamic shadow-casting lights. Check 'stat scenerendering'. Levers: merge/instance meshes, Nanite, cut dynamic lights. Dropping r.ScreenPercentage will NOT help.\n")
		TEXT("- bound == RHIThread: GPU command submission is the bottleneck. Check 'stat rhi'. Levers: cut draw calls via instancing / HISM / Nanite, reduce material & state permutations. Dropping r.ScreenPercentage will NOT help.\n")
		TEXT("- bound == GameThread: Tick / Blueprint / AI / animation cost. Run 'stat dumpframe -ms=0.5 -root=gamethread' on the PIE world. Levers: throttle Tick, fewer ticking actors/AI, cut expensive BP tick. Dropping r.ScreenPercentage will NOT help.\n")
		TEXT("\n")
		TEXT("# 3. Capture a trace for detail\n")
		TEXT("start_trace -> reproduce the workload -> stop_trace -> analyse('both'). Use bookmark / region_start / region_end to mark points of interest. For a representative reading prefer start_standalone (a separate game process) over the bare editor. Render a shareable HTML printout with report().\n")
		TEXT("\n")
		TEXT("# 4. Validate the verdict (force_hitch)\n")
		TEXT("force_hitch deliberately stalls a KNOWN thread so you can confirm frame_timing reports it correctly. The CPU paths (game/render/both) self-measure and return `observed_peak_*_ms` + `verdict_matched_expect` in the SAME response — validate from that, do NOT follow up with frame_timing (that read races the stall and misses it). The gpu path is async, so for it read frame_timing on a following frame and compare against `expect`:\n")
		TEXT("- force_hitch('game', 250)   -> expect bound == GameThread\n")
		TEXT("- force_hitch('render', 250) -> expect bound == RenderThread\n")
		TEXT("- force_hitch('both', 250)   -> expect contested == true\n")
		TEXT("- force_hitch('gpu')         -> expect bound == GPU (scene-dependent; supersamples via r.ScreenPercentage, auto-restored)\n")
		TEXT("Use frames>1 to sustain the hitch and reproduce jitter rather than a single spike.\n"));
}
