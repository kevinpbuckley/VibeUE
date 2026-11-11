---
name: profiling
display_name: Profiling & Performance
description: Diagnose frame-rate bottlenecks (CPU vs GPU bound FIRST), read live per-thread frame timings, drive Unreal Insights traces, and capture representative standalone runs. Use when FPS is low/bad, the game is slow, or you need to find what is limiting the frame rate.
vibeue_classes:
  - PerformanceService
unreal_classes:
  - PerfToolset
  - ConsoleToolset
  - EditorAppToolset
  - RewindDebuggerToolset
keywords:
  - profiling
  - performance
  - trace
  - unreal insights
  - frame time
  - fps
  - low fps
  - bad fps
  - cpu
  - gpu
  - cpu bound
  - gpu bound
  - game thread
  - render thread
  - draw calls
  - stat unit
  - memory
  - frame rate
  - bottleneck
  - budget
  - slow
  - hitch
  - spike
  - benchmark
  - capture
  - utrace
---

# Profiling Skill

## 🚨 Two call styles — pick the wrong one and the call fails

Unreal 5.9 ships its own performance surface, so most of this skill is now **Epic's toolsets**,
not VibeUE's:

- **Epic's toolsets** — `PerfToolset` (with `StatsToolset`, `CsvProfilerToolset`,
  `GpuProfilerToolset`), `ConsoleToolset` (with `LogsToolset`), `EditorAppToolset`,
  `FileSystemToolset`, `RewindDebuggerToolset` — are **`call_tool` actions**. They carry no
  `PythonCallable`, so `unreal.PerfToolset` **does not exist**. Run `describe_toolset` on one for
  its exact action names and params.
- **VibeUE's 7 surviving `PerformanceService` methods** are `unreal.PerformanceService.<method>()`
  run through **`execute_python`**, and each returns a JSON string. Read the live signatures with
  `get_symbol(class_name="unreal.PerformanceService")`.

### What moved to Epic in 5.9 (10 VibeUE tools removed)

| Was (removed) | Use instead | Style |
|---|---|---|
| `frame_timing` | `PerfToolset.StatsToolset.GetFrameTimings` → `FFrameTimings` (`FrameMs`, `GameThreadMs`, `RenderThreadMs`, `RhiThreadMs`, `GpuMs`) — the same numbers `stat unit` shows. **The CPU-vs-GPU verdict and the budget check are now *your* arithmetic over those fields** (STEP 0 below). | `call_tool`; `FFrameTimings` is `BlueprintType`, so it is also readable from Python |
| `start_trace`, `stop_trace`, `get_trace_status`, `bookmark`, `region_start`, `region_end` | `ConsoleToolset.RunConsoleCommand` with `Trace.File <name> <channels>`, `Trace.Start`, `Trace.Stop`, `Trace.Status`, `Trace.Bookmark <name>`, `Trace.RegionBegin <name>`, `Trace.RegionEnd <name>`; read the replies with `ConsoleToolset.LogsToolset.GetLogEntries` | `call_tool` |
| `report` | Gone, with no replacement: compose the summary yourself and write it with `FileSystemToolset.WriteTextFile` | `call_tool` |
| `start_pie`, `stop_pie` | `EditorAppToolset.StartPIE` / `StopPIE` / `IsPIERunning` — broader than VibeUE's pair (PIE vs Simulate, play mode, spawn transform, warmup) | `call_tool` |

Also on Epic's side and worth reaching for:

- **`PerfToolset.CsvProfilerToolset`** — `GetCsvInfo`, `ReduceStat` (avg/min/max, P50–P99),
  `ReadStatValues`, `TopStats`, `ListEvents`. Drive the capture itself with
  `RunConsoleCommand("CsvProfile start")` / `("CsvProfile stop")`. Cheapest route to statistical
  frame-time aggregates.
- **`PerfToolset.GpuProfilerToolset`** — GPU pass breakdown, once you know you are GPU-bound.
- **`RewindDebuggerToolset`** (10 tools) — loads a trace for **object / property / event**
  timelines. It does *not* give frame timings.

### The 7 VibeUE methods that survive — and when to reach for them

| Method | Purpose | Why it is still here |
|---|---|---|
| `analyse(source="both", file="")` | Read a finished trace and/or log → frame stats, worst frames, hitches, notable log lines. Reports `complete` / `partial` / `failed`; partial is never top-level success. | Nothing in Epic's surface parses a `.utrace` for timings — `RewindDebuggerToolset` loads traces, but only for objects, properties and events. |
| `force_hitch(thread="game", ms=250.0, frames=1)` | Deliberately stall a KNOWN thread (`"game"`/`"render"`/`"both"`/`"gpu"`) to prove your verdict logic fires. CPU paths are self-measuring — validate from the SAME response (`observed_peak_*_ms`, `verdict_matched_expect`); only the async `gpu` path is read back on a following frame. Clamped (≤5000 ms, ~10 s total). | No Epic tool and no console command stalls a named thread, and Epic's `execute_python` blocks `time.sleep` by design. |
| `start_standalone(name=..., channels="")` | Launch a **separate** game process with one unique direct-to-file trace. First reply is `start_pending`; poll status for verified capture. | `EditorAppToolset.StartPIE` documents that out-of-process play modes (NewProcess, MobilePreview, VR, QuickLaunch) are **downgraded to in-viewport PIE**, so nothing in Epic launches, tracks or terminates a separate game process with a trace attached. |
| `stop_standalone()` | Gracefully stop the exact tracked process and verify the exact trace finalized; forced/partial outcomes are explicit failures. | as above |
| `get_standalone_status()` | Session ID, PID, map, exact trace/log destinations, process state, capture/finalization verification. | as above |
| `set_background_throttling(bool)` | Stop the editor throttling while it is not in the foreground. | **The one that matters for unattended runs.** `UEditorEngine::ShouldThrottleCPUUsage` consults `ShouldDisableCPUThrottlingDelegates` FIRST and returns early; a plain `bThrottleCPUWhenNotForeground=false` then still falls through to `AreAllWindowsHidden()`, so a minimized/background editor throttles anyway. Only VibeUE registers that delegate. |
| `get_background_throttling()` | Read the current throttling state — snapshot it so you can restore it. | as above |

For a representative reading, profile under PIE (`EditorAppToolset.StartPIE`) or a standalone
session (`start_standalone`), not the bare editor viewport.

## 🚦 STEP 0 — Is it CPU-bound or GPU-bound? (DO THIS FIRST, ALWAYS)

**Never optimise before you know which processor is the bottleneck.** The frame time is roughly
`max(GameThread, RenderThread, GPU)` — these run in parallel, so only the *longest* one sets your
FPS. Cutting GPU cost (shadows, Lumen, post-process) does **nothing** if the frame is actually
game-thread or render-thread bound, and vice-versa. Getting this wrong wastes the whole session.

### Fastest check — one `call_tool`

```
call_tool GetFrameTimings {}
```

Start PIE first (`call_tool StartPIE {}`, or the `pie-testing` skill) and park in a
representative/worst spot, then read it. The reply is an `FFrameTimings`:

| Field | Meaning |
|---|---|
| `FrameMs` | Wall-clock frame time — this is what sets FPS (`1000 / FrameMs`) |
| `GameThreadMs` | CPU, game thread: Tick, Blueprints, AI, animation |
| `RenderThreadMs` | CPU, render thread: draw setup, visibility, shadow setup |
| `RhiThreadMs` | CPU, RHI thread: command-list submission to the driver |
| `GpuMs` | GPU work for the frame |

These are the same numbers the `stat unit` overlay shows, read straight from engine globals — no
screenshots, no trace parsing.

### The verdict is now YOUR arithmetic — here is how

VibeUE used to hand back a `bound` verdict, a `hint` and a `budget` block. Compute them yourself
from the five fields:

1. **Pick the bottleneck.** Take the largest of `GameThreadMs`, `RenderThreadMs`, `RhiThreadMs`,
   `GpuMs`. That subsystem gates the frame.
2. **Confirm it actually accounts for the frame.** The winner should be close to `FrameMs`
   (say ≥ 85 % of it). If *nothing* reaches that, no single subsystem is saturated — suspect a
   frame-rate cap (see the `frame-rate` skill), VSync, or a sync/stall rather than a workload.
3. **Gate against the target.** `budget_ms = 1000 / target_fps`. Every thread *and* the GPU must
   individually finish inside `budget_ms`; report each one's headroom (`budget_ms - thread_ms`) and
   PASS only when all of them fit.

```python
# FFrameTimings is BlueprintType, so the same fields are readable from execute_python.
# (Prefer `call_tool GetFrameTimings {}` when you are not already inside Python.)
TARGET_FPS = 60.0
budget_ms = 1000.0 / TARGET_FPS

t = {"GameThread": 24.8, "RenderThread": 9.1, "RhiThread": 1.4, "GPU": 11.2}   # FFrameTimings
frame_ms = 25.1                                                                # FrameMs

bound, bound_ms = max(t.items(), key=lambda kv: kv[1])
saturated = bound_ms >= 0.85 * frame_ms

print(f"frame {frame_ms:.2f} ms = {1000.0 / frame_ms:.1f} FPS")
for name, ms in sorted(t.items(), key=lambda kv: -kv[1]):
    print(f"  {name:<12} {ms:6.2f} ms   headroom {budget_ms - ms:+6.2f} ms"
          f"   {'OVER' if ms > budget_ms else 'ok'}")
print(f"bound: {bound} ({bound_ms:.2f} ms)" if saturated else
      "bound: NO single subsystem saturates the frame — look for a cap / VSync / stall")
print(f"meets {TARGET_FPS:.0f} FPS:",
      "PASS" if all(ms <= budget_ms for ms in t.values()) else "FAIL")
```

Always state the bottleneck's ms next to the target budget so the gap is explicit, e.g. "game
thread 24.8 ms vs 16.66 ms for 60 FPS → must cut 8.2 ms on the game thread". To prove your verdict
logic works against a known ground truth, stall a thread on purpose with `force_hitch`.

## ⏱️ Frame-time budgets — what a target FPS actually costs

| Target FPS | Per-frame budget | Meaning |
|---|---|---|
| **30 FPS**  | **33.33 ms** | Maximum allowable time per frame |
| **60 FPS**  | **16.66 ms** | Maximum allowable time per frame |
| **120 FPS** | **8.33 ms**  | Maximum allowable time per frame |
| **240 FPS** | **4.16 ms**  | Maximum allowable time per frame |
| **360 FPS** | **2.77 ms**  | Maximum allowable time per frame |

Because the threads run in parallel, **every** one of them must *individually* finish inside the
budget. A 12 ms GPU is wasted if the game thread takes 25 ms: you still get ~40 FPS, no matter what
you do to the GPU.

## 🛠️ CVars tune the renderer — they do NOT fix the game thread

`r.*` console variables almost exclusively move **GPU** and **RenderThread** cost (Lumen,
shadows, reflections, resolution, draw setup). There is **no CVar that makes your Tick,
AI, or animation logic cheaper.** When the game thread is the bottleneck, the fix lives in **code
and Blueprints**, driven by what the profiler shows:

| Profiler symptom (`stat dumpframe -root=gamethread`) | Fix lives in | Typical change |
|---|---|---|
| High `FTickFunctionTask` / many ticking actors | C++ / Blueprint | Throttle `PrimaryActorTick.TickInterval`, disable tick when idle/far, event-drive instead of polling every frame |
| High `AnimGameThreadTime` / many skeletal meshes | C++ / mesh setup | Enable URO (`bEnableUpdateRateOptimizations`), `VisibilityBasedAnimTickOption = OnlyTickPoseWhenRendered` |
| Expensive Blueprint `ReceiveTick` | Blueprint graph | Move per-frame logic to timers/events, cache results, early-out |
| `CharacterMovement` / physics / AI heavy | C++ / config | Significance-based LOD, fewer simulated bodies, coarser AI update rate |
| Per-frame `SetTimer`, allocations, logging in hot paths | C++ / Blueprint | Remove redundant work; gate `UE_LOG` behind a debug flag |

So the workflow for a game-thread bottleneck is: **profile → read the offending scope →
edit the code/Blueprint that owns it → rebuild → re-profile to confirm.** Reaching for CVars
here is a category error; they will not move the number.

### Decision tree

| Bottleneck field | Meaning | Where to look next |
|---|---|---|
| **`GameThreadMs`** | CPU, game thread | Tick / Blueprint / AI / animation. Run `stat dumpframe -ms=0.5 -root=gamethread` then read the log (`ConsoleToolset.LogsToolset.GetLogEntries`). **Fix is code/Blueprint, not CVars** (see above): throttle ticks, enable anim URO, event-drive logic. Compare the ms to the budget table. |
| **`RenderThreadMs`** | CPU, render thread | Draw calls & primitives, **dynamic shadow-casting lights**. Check `stat scenerendering`. Instance/merge meshes, enable Nanite, cut dynamic lights. |
| **`RhiThreadMs`** | CPU, RHI submission | Command-list volume / driver overhead — usually a symptom of too many draws, so attack the render thread's draw count. |
| **`GpuMs`** | GPU | *Now* run `ProfileGPU` or `PerfToolset.GpuProfilerToolset`. Shadows (VSM), Lumen, translucency, resolution. |

### Confirm / fallback methods

1. **Numeric thread split via log** — reliable, works headless:
   ```
   call_tool RunConsoleCommand {"Command": "stat dumpframe -ms=0.5 -root=gamethread"}
   call_tool RunConsoleCommand {"Command": "stat dumpframe -ms=0.5 -root=renderthread"}
   call_tool GetLogEntries {}
   ```
   `LogsToolset.GetLogEntries` returns the full thread hierarchy in ms (e.g. `World Tick Time`,
   `FTickFunctionTask`, individual Blueprint `ReceiveTick` costs). This is the single most useful
   CPU drill-down.
2. **Resolution-scaling test** (decisive GPU-bound proof) — read `GetFrameTimings` at full res,
   then `RunConsoleCommand {"Command": "r.ScreenPercentage 50"}`, and read it again. Frame time
   drops a lot → **GPU-bound**. Barely moves → **CPU-bound**.
3. **`stat unit`** is the canonical overlay, but its numbers do **not** go to the log, and PIE
   often runs in a separate window so screenshots are unreliable. Prefer `GetFrameTimings` or
   methods 1–2 over trying to OCR `stat unit`.

## ⚠️ Gotchas — read before writing any code

0. **`ProfileGPU` tells you WHERE GPU time goes, not WHETHER you are GPU-bound.** It always
   produces a GPU breakdown even on a CPU-bound frame — so reading it first will happily send
   you optimising shadows on a frame whose real cost is the game thread. Run STEP 0
   (`GetFrameTimings`) first; only reach for `ProfileGPU` / `GpuProfilerToolset` once the GPU is
   the bottleneck.

0b. **Never run `ProfileGPU` (or `stat dumpframe`) *during* a trace you intend to average.**
   Each `ProfileGPU` stalls the GPU for a full readback (hundreds of ms to seconds), and those
   stall frames poison the frame-time stats. Capture clean frame-time traces separately from
   GPU/CPU drill-downs.

0c. **`analyse()` reports frame-time aggregates** (avg/p95/worst frames, hitches, notable log
   lines), **not** the live CPU/GPU split. Use `GetFrameTimings` for the per-thread split.

1. **Profile under PIE or standalone, not the bare editor viewport.** The empty editor viewport
   is not representative. Start PIE (`EditorAppToolset.StartPIE`) or use `start_standalone()`.

2. **Trace files are large** — a ~10 second trace with default channels is 30–50 MB.
   Traces land under `Saved/Profiling/` (already excluded from source control).

3. **A trace must be running before you bookmark or mark regions.** `Trace.Bookmark`,
   `Trace.RegionBegin` and `Trace.RegionEnd` only do something while `Trace.Status` reports a live
   trace. Issue `Trace.File` + `Trace.Start` first.

4. **Channels are chosen when you name the file.** `Trace.File <name> <channels>` sets the
   destination *and* the channel set; `Trace.Start` then begins recording. Confirm with
   `Trace.Status` → `GetLogEntries`, because channel naming varies slightly by build.

---

## Trace Control — Epic's `ConsoleToolset` (`call_tool`, not Python)

### Start / stop a trace

```
call_tool RunConsoleCommand {"Command": "Trace.File combat_encounter frame,cpu,gpu,stats,log"}
call_tool RunConsoleCommand {"Command": "Trace.Start"}
call_tool GetLogEntries {}                   # confirm the destination it chose

# ... reproduce the workload ...

call_tool RunConsoleCommand {"Command": "Trace.Stop"}
call_tool GetLogEntries {}                   # trace path + size
```

### Check status

```
call_tool RunConsoleCommand {"Command": "Trace.Status"}
call_tool GetLogEntries {}                   # active? which channels?
```

### Annotations — bookmarks and regions

Use these **while a trace is running** to label interesting moments in the Insights timeline.

```
call_tool RunConsoleCommand {"Command": "Trace.Bookmark spawn_wave_3"}

call_tool RunConsoleCommand {"Command": "Trace.RegionBegin loading_level"}
# ... trigger the thing you want to measure ...
call_tool RunConsoleCommand {"Command": "Trace.RegionEnd loading_level"}
```

### CSV profiler — aggregates without opening Insights

```
call_tool RunConsoleCommand {"Command": "CsvProfile start"}
# ... workload ...
call_tool RunConsoleCommand {"Command": "CsvProfile stop"}
call_tool GetCsvInfo {}                      # then ReduceStat / ReadStatValues / TopStats / ListEvents
```

`CsvProfilerToolset.ReduceStat` gives avg/min/max and P50–P99 for a named stat — use it instead of
hand-rolling percentiles.

---

## Reading a capture back — `analyse()` (VibeUE, `execute_python`)

`analyse()` reads a finished trace and/or the log and returns a perf summary (frame stats, worst
frames, hitches, notable log lines) without opening Unreal Insights:

```python
import unreal, json

summary = json.loads(unreal.PerformanceService.analyse("both"))   # "trace", "logs", or "both"
print(summary)  # avg/p95/worst frame ms, hitches, notable log lines

# Analyse a specific file instead of the last trace recorded
unreal.PerformanceService.analyse("trace", "Saved/Profiling/combat_encounter.utrace")
```

For `source="both"`, inspect `status`: `complete` means both sources succeeded, `partial` means
exactly one did, and `failed` means neither did. A partial result deliberately has `success=false`
and names `available_sources` / `failed_sources`; this prevents a missing trace from masquerading as
a successful capture. Log output separates `pso_hitch_event_lines`, cumulative
`pso_hitches_reported`, and `pso_hitch_summary_lines`; repeated cumulative summary lines are not
summed. The backward-compatible `pso_hitches` field is the best supported count.

Remember: `analyse()` is frame-time aggregates only — use `GetFrameTimings` for the CPU/GPU split.

---

## Standalone capture — representative readings

The editor viewport (and even PIE) is not always representative of a packaged run, and
`EditorAppToolset.StartPIE` **downgrades** out-of-process play modes back to in-viewport PIE.
`start_standalone()` launches the game as a real **separate process** with one unique,
direct-to-file trace. It does not use the global Unreal Trace Server store and never substitutes an
unrelated "latest" trace:

```python
import unreal, json

start = json.loads(unreal.PerformanceService.start_standalone("standalone_capture"))
# start is request_accepted=true, success=false, pending=true until the child and trace exist
status = json.loads(unreal.PerformanceService.get_standalone_status())
# poll on later editor ticks until status["capture_verified"] is true

# ... let it run / drive the workload ...

stopped = json.loads(unreal.PerformanceService.stop_standalone())
# require stopped["success"] and status == "finalized" before trusting the trace
```

Record the returned `session_id`, `pid`, `map`, `trace_file`, and `log_file` with benchmark results.
If shutdown is forced or the exact trace stays missing/empty, stop returns failure and may mark the
available log/partial trace as `partial`; do not silently use another store trace. `analyse("both")`
can still return a clearly marked log-only partial summary.

For comparisons, keep map/route, resolution, scalability, difficulty, power mode, and warm-up fixed.
For unattended runs, snapshot `get_background_throttling()`, call `set_background_throttling(False)`,
and restore the prior value afterward — a background editor throttles even with
`bThrottleCPUWhenNotForeground=false`, and this is the only tool that defeats that. Treat short or
sparse samples as observations, not stable averages. Compare median/p95/p99 and frame counts above
33/50/100 ms, and separate cold-start/shader/PSO runs from warm-cache runs. Run `ProfileGPU` or
`stat dumpframe` separately because their instrumentation stalls contaminate traces.

---

## Recommended flow

```
# 1. Live per-thread split FIRST (PIE running, parked in a representative spot)
call_tool StartPIE {}
call_tool GetFrameTimings {}
#    → then do the STEP 0 arithmetic: largest field, vs FrameMs, vs 1000/target_fps

# 2. Capture a clean trace around the workload
call_tool RunConsoleCommand {"Command": "Trace.File combat_encounter frame,cpu,gpu,stats,log"}
call_tool RunConsoleCommand {"Command": "Trace.Start"}
call_tool RunConsoleCommand {"Command": "Trace.RegionBegin wave_spawn"}
# (trigger the gameplay here)
call_tool RunConsoleCommand {"Command": "Trace.RegionEnd wave_spawn"}
call_tool RunConsoleCommand {"Command": "Trace.Stop"}
call_tool GetLogEntries {}
```

```python
# 3. Summarise the trace without leaving Python (VibeUE)
import unreal, json
print(json.loads(unreal.PerformanceService.analyse("both")))
```

```
# 4. Write up the findings yourself — there is no report tool any more
call_tool WriteTextFile {"Path": "Saved/VibeUE/Performance/combat_encounter.md", "Contents": "..."}
```

Put in that write-up what the old `report` generated: the per-thread numbers, the bottleneck and
the budget gap you computed, the `analyse()` aggregates, and a "fix in this order" list derived from
the decision tree above.

---

## Channel Reference

`Trace.File <name> <channels>` and `start_standalone(..., channels=...)` both take a
comma-separated channel list; empty uses the default set (frame, cpu, gpu, log, …).

| Channel | What it captures | Use for |
|---|---|---|
| `frame` | Frame boundaries, wall time | Always include — baseline for everything |
| `cpu` | CPU named scopes | CPU hotspots, Blueprint tick |
| `gpu` | GPU pass timings | GPU bound? Where are draw calls going? |
| `stats` | UE stat counters | Actor/component counts, frame budget |
| `log` | Log output embedded in trace | Correlate log spam with frame spikes |
| `memalloc` | Per-allocation callstacks | Memory churn, GC pressure |
| `memtag` | High-level memory category totals | Which system is eating RAM? |
| `object` | UObject create/destroy | Asset streaming, actor spawning |
| `niagara` | Niagara system tick | Particle perf |
| `animation` | Animation graph evaluation | Anim Blueprint cost |
| `net` | Replication, RPC timing | Multiplayer performance |
| `slate` | UI widget tick and paint | UI overhead |
| `loadtime` | Asset streaming / load events | Load hitches |

> Channel naming may vary slightly by build; run `Trace.Status` and read `GetLogEntries` after
> starting to see the channels actually enabled.

### Common channel presets

| Investigation | `channels` string |
|---|---|
| General (balanced) | `frame,cpu,gpu,stats,log` |
| Memory | `frame,memalloc,memtag,object,loadtime` |
| Animation / character | `frame,cpu,animation,stats` |
| UI / Slate | `frame,cpu,slate,stats` |
| Niagara / VFX | `frame,cpu,gpu,niagara` |
| Multiplayer | `frame,cpu,net,stats` |
| Load / streaming | `frame,loadtime,object,log` |

---

## Opening Traces in Unreal Insights

`analyse()` covers most needs from Python, and `RewindDebuggerToolset` can load a trace for
object/property/event timelines, but you can open a `.utrace` for the full timeline UI:

```
Editor menu: Tools → Run Unreal Insights
```

Or from the command line:
```
"<UE install>/Engine/Binaries/Win64/UnrealInsights.exe" "<path>/combat_encounter.utrace"
```

Trace files are written under:
```
<project root>/Saved/Profiling/<name>.utrace
```
