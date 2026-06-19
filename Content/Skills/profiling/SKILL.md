---
name: profiling
display_name: Profiling & Performance
description: Diagnose frame-rate bottlenecks (CPU vs GPU bound FIRST), control Unreal Insights traces, sample live frame times, and annotate performance captures. Use when FPS is low/bad, the game is slow, or you need to find what is limiting the frame rate.
vibeue_classes:
  - PerformanceService
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

Unreal 5.8's native toolsets have **no** performance or tracing tools, so this is VibeUE's net-new
capability: `unreal.PerformanceService` answers "are we CPU- or GPU-bound?" and drives Unreal
Insights captures from Python with no C++ required. Run everything here through `execute_python_code`.

The whole API is small — read the live signatures once with
`discover_python_class(class_name="unreal.PerformanceService")`:

| Method | Purpose |
|--------|---------|
| `frame_timing()` | Game/Render/GPU/RHI thread ms + a CPU-vs-GPU `bound` verdict + a `hint`. **Run FIRST.** |
| `start_trace(name="mcp_capture", channels="")` | Start an Insights trace to file (default channel set if `channels` empty). |
| `stop_trace()` | Stop the active trace; returns the trace file path + size. |
| `get_trace_status()` | Whether a trace is active and which channels are enabled. |
| `bookmark(name)` | Drop a point-in-time bookmark in the active trace. |
| `region_start(name)` / `region_end(name)` | Begin / end a named region span in the active trace. |
| `analyse(source="both", file="")` | Read back trace and/or log → frame stats, worst frames, hitches, notable log lines. |
| `start_standalone(name="standalone_capture", channels="")` | Launch the game as a separate standalone process with a trace attached (representative readings the editor viewport can't give). |
| `stop_standalone()` | Stop the standalone process and finalise its trace/log. |
| `get_standalone_status()` | Whether a standalone session is running and which trace/log it writes. |

All methods return a JSON string. For a representative reading, profile under PIE or a standalone
session (`start_standalone`), not the bare editor viewport.

## 🚦 STEP 0 — Is it CPU-bound or GPU-bound? (DO THIS FIRST, ALWAYS)

**Never optimise before you know which processor is the bottleneck.** The frame time is
roughly `max(GameThread, RenderThread, GPU)` — these run in parallel, so only the *longest*
one sets your FPS. Cutting GPU cost (shadows, Lumen, post-process) does **nothing** if the
frame is actually game-thread or render-thread bound, and vice-versa. Getting this wrong
wastes the whole session.

### Fastest check — one call

```python
import unreal, json
result = json.loads(unreal.PerformanceService.frame_timing())
print(result)  # game_thread_ms, render_thread_ms, gpu_ms, rhi_thread_ms, frame_ms, bound, hint
```

Start PIE first (see the `pie-testing` skill) and park in a representative/worst spot, then call it.
It returns `game_thread_ms`, `render_thread_ms`, `gpu_ms`, `rhi_thread_ms`, the `frame_ms`, a `bound`
verdict (`GameThread` / `RenderThread` / `GPU`), and a `hint` with what to do next. This is the same
data the `stat unit` overlay shows, read straight from engine globals — no screenshots, no trace
parsing.

## ⏱️ Frame-time budgets — what a target FPS actually costs

FPS is just `1000 / frame_ms`. Because the threads run in parallel, **every** thread
(GameThread, RenderThread, GPU) must *individually* finish inside the budget below — the
slowest one alone sets your FPS. A 12 ms GPU is wasted if the game thread takes 25 ms: you
still get ~40 FPS.

| Target FPS | Per-frame budget | Meaning |
|---|---|---|
| **30 FPS**  | **33.33 ms** | Maximum allowable time per frame |
| **60 FPS**  | **16.66 ms** | Maximum allowable time per frame |
| **120 FPS** | **8.33 ms**  | Maximum allowable time per frame |
| **240 FPS** | **4.16 ms**  | Maximum allowable time per frame |
| **360 FPS** | **2.77 ms**  | Maximum allowable time per frame |

Read `frame_timing()` against this table: if `game_thread_ms = 25`, you are hard-capped at
~40 FPS **no matter what you do to the GPU**. To hit 60 FPS the game thread must drop under
16.66 ms; to hit 120 FPS, under 8.33 ms. Always state the bottleneck thread's ms next to the
target budget so the gap is explicit (e.g. "game thread 25 ms vs 16.66 ms for 60 FPS → must
cut 8.3 ms on the game thread").

## 🛠️ CVars tune the renderer — they do NOT fix the game thread

`r.*` console variables almost exclusively move **GPU** and **RenderThread** cost (Lumen,
shadows, reflections, resolution, draw setup). There is **no CVar that makes your Tick,
AI, or animation logic cheaper.** When `bound == GameThread`, the fix lives in **code and
Blueprints**, driven by what the profiler shows:

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
| `bound` | Meaning | Where to look next |
|---|---|---|
| **GameThread** | CPU, game thread | Tick / Blueprint / AI / animation. Run `stat dumpframe -ms=0.5 -root=gamethread` then read the log (`LogsToolset`). **Fix is code/Blueprint, not CVars** (see "CVars do NOT fix the game thread" above): throttle ticks, enable anim URO, event-drive logic. Compare the ms to the budget table. |
| **RenderThread** | CPU, render thread | Draw calls & primitives, **dynamic shadow-casting lights**. Check `stat scenerendering`. Instance/merge meshes, enable Nanite, cut dynamic lights. |
| **GPU** | GPU | *Now* run ProfileGPU (below). Shadows (VSM), Lumen, translucency, resolution. |

### Confirm / fallback methods
1. **Numeric thread split via log** — reliable, works headless:
   ```python
   import unreal
   w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
   unreal.SystemLibrary.execute_console_command(w, "stat dumpframe -ms=0.5 -root=gamethread")
   unreal.SystemLibrary.execute_console_command(w, "stat dumpframe -ms=0.5 -root=renderthread")
   ```
   Then read the dump with the engine **`LogsToolset`** (tail the main log, ~150 lines) — it prints
   the full thread hierarchy in ms (e.g. `World Tick Time`, `FTickFunctionTask`, individual
   Blueprint `ReceiveTick` costs). This is the single most useful CPU drill-down.
2. **Resolution-scaling test** (decisive GPU-bound proof) — sample frame time at full res via
   `frame_timing()`, then `r.ScreenPercentage 50`, and compare. FPS jumps a lot → **GPU-bound**.
   FPS barely moves → **CPU-bound**.
3. **`stat unit`** is the canonical overlay, but its numbers do **not** go to the log, and
   PIE often runs in a separate window so screenshots are unreliable. Prefer `frame_timing()`
   or methods 1–2 over trying to OCR `stat unit`.

## ⚠️ Gotchas — read before writing any code

0. **`ProfileGPU` tells you WHERE GPU time goes, not WHETHER you are GPU-bound.** It always
   produces a GPU breakdown even on a CPU-bound frame — so reading it first will happily send
   you optimising shadows on a frame whose real cost is the game thread. Run STEP 0
   (`frame_timing()`) first; only reach for `ProfileGPU` once `bound == GPU`.

0b. **Never run `ProfileGPU` (or `stat dumpframe`) *during* a trace you intend to average.**
   Each `ProfileGPU` stalls the GPU for a full readback (hundreds of ms to seconds), and those
   stall frames poison the frame-time stats. Capture clean frame-time traces separately from
   GPU/CPU drill-downs.

0c. **`analyse()` reports frame-time aggregates** (avg/p95/worst frames, hitches, notable log
   lines), **not** the live CPU/GPU split. Use `frame_timing()` for the per-thread split.

1. **Profile under PIE or standalone, not the bare editor viewport.** The empty editor viewport
   is not representative. Start PIE (`pie-testing` skill) or use `start_standalone()`.

2. **Trace files are large** — a ~10 second trace with default channels is 30–50 MB.
   Traces land under `Saved/Profiling/` (already excluded from source control).

3. **A trace must be active before you bookmark / mark regions.** `bookmark()` and
   `region_start()/region_end()` only do something while `get_trace_status()` reports a live
   trace. Call `start_trace()` first.

---

## Trace Control

### Start / stop a trace
```python
import unreal, json

# Default channel set (frame, cpu, gpu, log, ...) — pass channels="" or omit
res = json.loads(unreal.PerformanceService.start_trace("combat_encounter"))
print(res)  # includes the trace file path

# ... reproduce the workload ...

stopped = json.loads(unreal.PerformanceService.stop_trace())
print(stopped)  # trace file path + size
```

### Custom channels
```python
# Comma-separated channel list (short names accepted by the service)
unreal.PerformanceService.start_trace("mem_capture", "frame,cpu,memalloc,memtag,object,loadtime")
```

### Check status
```python
import unreal, json
print(json.loads(unreal.PerformanceService.get_trace_status()))  # active? which channels?
```

---

## Annotations — Mark Regions and Bookmarks

Use these **while a trace is active** to label interesting moments in the Unreal Insights timeline.

```python
import unreal

# Single point-in-time bookmark (vertical line in Insights)
unreal.PerformanceService.bookmark("spawn_wave_3")

# Named region (coloured span)
unreal.PerformanceService.region_start("loading_level")
# ... trigger the thing you want to measure ...
unreal.PerformanceService.region_end("loading_level")
```

---

## Reading a capture back — `analyse()`

`analyse()` reads a finished trace and/or the log and returns a perf summary (frame stats, worst
frames, hitches, notable log lines) without opening Unreal Insights:

```python
import unreal, json

summary = json.loads(unreal.PerformanceService.analyse("both"))   # "trace", "logs", or "both"
print(summary)  # avg/p95/worst frame ms, hitches, notable log lines

# Analyse a specific file instead of the last trace started/stopped
unreal.PerformanceService.analyse("trace", "Saved/Profiling/combat_encounter.utrace")
```

Remember: `analyse()` is frame-time aggregates only — use `frame_timing()` for the CPU/GPU split.

---

## Standalone capture — representative readings

The editor viewport (and even PIE) is not always representative of a packaged run. `start_standalone()`
launches the game as a **separate standalone process** with a trace attached, connecting back to the
editor's Unreal Trace Server:

```python
import unreal, json

unreal.PerformanceService.start_standalone("standalone_capture")
print(json.loads(unreal.PerformanceService.get_standalone_status()))  # running? trace/log path

# ... let it run / drive the workload ...

unreal.PerformanceService.stop_standalone()  # finalises the trace + log
```

Once stopped, point `analyse()` at the standalone trace/log to summarise it.

---

## Recommended flow

```python
import unreal, json

# 1. CPU vs GPU verdict FIRST (PIE running, parked in a representative spot)
print(json.loads(unreal.PerformanceService.frame_timing()))

# 2. Capture a clean trace around the workload
unreal.PerformanceService.start_trace("combat_encounter")
unreal.PerformanceService.region_start("wave_spawn")
# (trigger the gameplay here)
unreal.PerformanceService.region_end("wave_spawn")
res = json.loads(unreal.PerformanceService.stop_trace())
print("trace:", res)

# 3. Summarise without leaving Python
print(json.loads(unreal.PerformanceService.analyse("both")))
```

---

## Channel Reference

`start_trace` / `start_standalone` accept a comma-separated `channels` string; empty uses the
default set (frame, cpu, gpu, log, …). Common channels:

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

> Channel naming may vary slightly by build; call `get_trace_status()` after `start_trace()` to see
> the channels actually enabled.

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

`analyse()` covers most needs from Python, but you can open a `.utrace` for the full timeline UI:

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
