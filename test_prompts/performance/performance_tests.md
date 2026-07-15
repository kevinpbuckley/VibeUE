# Performance Service Tests

Tests for frame-timing triage, the budget gate, hitch validation (force_hitch), Insights tracing,
and the shareable HTML report. Run sequentially. Every PerformanceService method returns a JSON
string — parse it and assert on fields; print the evidence behind each pass/fail.

Most of these need a live game world, so the pack starts in-process PIE first.

---

## Setup — live world

Start an in-process PIE session using the performance service (not the editor toolset), then confirm on a following frame that frame timing reports PIE running. (Expected: start_pie succeeds; PIE starts on the next editor tick, so a later frame_timing response has pie_running=true — do not assert on the same frame.)

---

## Frame Timing & Verdict

Read the current frame timing. Report the game/render/GPU/RHI thread ms, the frame ms, the bound verdict, and the hint. (Expected: JSON with game_thread_ms, render_thread_ms, gpu_ms, rhi_thread_ms, frame_ms, bound, hint.)

---

If gpu_ms came back 0, confirm the response explicitly flags that GPU timing is unavailable instead of silently trusting a CPU verdict that GPU could overturn. If gpu_ms is non-zero, state that the flag is absent and why that's correct.

---

## Budget Gate

Gate the current frame against a 60 FPS target. Report each thread's headroom against the 16.66 ms budget and whether the overall gate passed. (Expected: a budget block with per-thread headroom/over_budget and a meets_target bool.)

---

Now gate against an absurd 1000 FPS target and confirm the gate fails, naming which threads blew the 1 ms budget. (Expected: budget.meets_target=false; at least one thread over budget.)

---

## Hitch Validation — CPU paths (self-measured, single response)

Force a 250 ms hitch on the game thread and validate the verdict from that same response. Do NOT follow up with a frame_timing call — the CPU stall blocks the game thread, so a follow-up read reliably lands on a clean frame and misses it. (Expected: observed_peak_game_ms ≈ 250 or more and verdict_matched_expect=true, in the force_hitch response itself.)

---

Force a 250 ms hitch on the render thread and validate the same way. (Expected: observed_peak_render_ms ≈ 250 or more, verdict_matched_expect=true.)

---

Force a 250 ms hitch on game AND render simultaneously and confirm the frame is reported contested rather than declaring a false single winner. (Expected: contested=true.)

---

Try to force a 60000 ms hitch for 600 frames. Confirm the service clamps rather than hanging the editor. (Expected: milliseconds clamped to 5000; total synchronous stall capped around 10 s; the editor stays responsive afterwards.)

---

## Hitch Validation — GPU path (async)

Force a GPU hitch, then read frame timing on a following frame and compare the verdict against the expected GPU-bound result. This path is scene-dependent best-effort — if the verdict doesn't flip, report the observed gpu_ms honestly rather than retrying more than once. (Expected: whatever the verdict, r.ScreenPercentage is auto-restored after the configured frames — verify it returned to its prior value.)

---

## Trace Capture & Analysis

Start an Insights trace named "perf_pack_capture". Verify it's live. (Expected: get_trace_status reports an active trace and its channels.)

---

Drop a bookmark named "PerfPackMark", then wrap a region named "PerfPackRegion" around a forced 100 ms game-thread hitch (region_start → force_hitch → region_end).

---

Stop the trace. Report the trace file path and size. (Expected: a .utrace path; size > 0.)

---

Analyse the capture with source "both". Report the frame stats (avg/p95/worst), the worst frames, and any hitches found. (Expected: the forced 100 ms hitch shows up among the worst frames/hitches.)

---

## HTML Report

Generate an HTML performance report titled "Perf Pack Report". (Expected: report() returns an output path under Saved/VibeUE/Performance/.)

---

Verify the report from disk — do not just trust the returned path. Read the file and confirm it exists, is non-empty, contains the title "Perf Pack Report", and carries a "fix in this order" section with concrete stat/console commands. (Expected: self-contained HTML — CSS inlined, no external asset references.)

---

## Cleanup

Stop PIE via the performance service, then verify nothing is left running: no active trace and no standalone session. (Expected: stop_pie succeeds; get_trace_status inactive; get_standalone_status not running.)
