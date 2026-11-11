# Performance Tests

Tests for the CPU-vs-GPU triage path, hitch validation (`force_hitch`), trace capture and analysis.

**Read this before running the suite:** performance work is now split. Frame timings are Epic's
(`PerfToolset.StatsToolset.GetFrameTimings` via `call_tool`) and trace start/stop is a console
command (`ConsoleToolset.RunConsoleCommand "Trace.Start" / "Trace.Stop"`). `PerformanceService` keeps
exactly seven tools: `force_hitch`, `analyse`, `start_standalone`, `stop_standalone`,
`get_standalone_status`, `set_background_throttling`, `get_background_throttling` — each returns a
JSON string, so parse it and assert on fields. `frame_timing`, the budget gate, the trace-control
tools, the HTML report and PIE start/stop on the performance service are **gone**; a prompt below
that asks the agent to find the replacement is testing exactly that.

Most of these need a live game world, so the pack starts PIE first. Run sequentially.

---

## Setup — live world

Start a PIE session, then confirm on a following frame that it is actually running. (Expected: the
agent reaches for `EditorAppToolset.StartPIE` — the performance service has no `start_pie` any more —
and confirms with `IsPIERunning` in a LATER call, because PIE start is asynchronous. An agent that
asserts on the same frame has failed the prompt, not the engine.)

---

## Frame Timing & Verdict

Read the current frame timing and report the game/render/GPU thread ms, the frame ms, and your
CPU-bound vs GPU-bound verdict with the reasoning behind it. (Expected: `GetFrameTimings` from
`PerfToolset.StatsToolset`, and a verdict the agent derives from the numbers itself — nothing returns
a `bound` field or a `hint` for it now.)

---

If GPU timing came back 0 or absent, say so explicitly and refuse to call the frame CPU-bound on that
evidence. If it is non-zero, state that the comparison is trustworthy and why.

---

## Budget Gate

Gate the current frame against a 60 FPS target: report each thread's headroom against the 16.66 ms
budget and whether the frame passes. (Expected: the agent computes the headroom itself from
`GetFrameTimings` — there is no budget-gate tool. The arithmetic and the per-thread verdicts must be
shown.)

---

Now gate the same reading against an absurd 1000 FPS target and confirm it fails, naming which
threads blew the 1 ms budget.

---

## Hitch Validation — CPU paths (self-measured, single response)

Force a 250 ms hitch on the game thread and validate the verdict from that same response. Do NOT
follow up with a frame-timing read — the CPU stall blocks the game thread, so a follow-up read
reliably lands on a clean frame and misses it. (Expected: `observed_peak_game_ms` ≈ 250 or more and
`verdict_matched_expect=true`, in the `force_hitch` response itself.)

---

Force a 250 ms hitch on the render thread and validate the same way. (Expected:
`observed_peak_render_ms` ≈ 250 or more, `verdict_matched_expect=true`.)

---

Force a 250 ms hitch on game AND render simultaneously and confirm the frame is reported contested
rather than declaring a false single winner. (Expected: `contested=true`.)

---

Try to force a 60000 ms hitch for 600 frames. Confirm the service clamps rather than hanging the
editor. (Expected: milliseconds clamped to 5000; total synchronous stall capped around 10 s; the
editor stays responsive afterwards.)

---

## Hitch Validation — GPU path (async)

Force a GPU hitch, then read frame timing on a following frame and compare against the expected
GPU-bound result. This path is scene-dependent best-effort — if the verdict doesn't flip, report the
observed GPU ms honestly rather than retrying more than once. (Expected: whatever the verdict,
`r.ScreenPercentage` is auto-restored after the configured frames — verify it returned to its prior
value.)

---

## Trace Capture & Analysis

Start an Unreal Insights trace, drop a bookmark, wrap a forced 100 ms game-thread hitch between a
region start and end, then stop the trace. (Expected: the agent drives this with
`ConsoleToolset.RunConsoleCommand` — `Trace.Start`, `Trace.Bookmark`, `Trace.Stop` and friends — since
`start_trace` / `stop_trace` / `get_trace_status` / the region and bookmark tools no longer exist. It
should report the trace file path from the command output or the log.)

---

Analyse the capture with source "both". Report the frame stats (avg/p95/worst), the worst frames, and
any hitches found. (Expected: `unreal.PerformanceService.analyse("both", "")` — still a VibeUE Python
call — and the forced 100 ms hitch shows up among the worst frames/hitches.)

---

## Standalone Capture

Start a standalone capture, confirm it is running, then stop it. (Expected: `start_standalone` /
`get_standalone_status` / `stop_standalone`; status reports not-running once stopped.)

---

## Unfocused Full-Rate Verification

Disable editor background throttling, confirm the setting read back, then restore it. (Expected:
`set_background_throttling(False)` → `get_background_throttling()` reports it off →
`set_background_throttling(True)`. Both return JSON strings; assert on the field, not on truthiness.)

---

## Cleanup

Stop PIE and verify nothing is left running: no active trace and no standalone session. (Expected:
`EditorAppToolset.StopPIE`; `get_standalone_status` not running; `Trace.Stop` already issued or
reported as inactive.)
