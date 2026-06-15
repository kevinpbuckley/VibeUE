---
name: profiling
display_name: Profiling & Performance
description: Diagnose frame-rate bottlenecks (CPU vs GPU bound FIRST), control Unreal Insights traces, sample live frame times, and annotate performance captures. Use when FPS is low/bad, the game is slow, or you need to find what is limiting the frame rate.
unreal_classes:
  - TraceUtilLibrary
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

Full Unreal Insights integration and live frame time sampling are available from Python
with no C++ required. `TraceUtilLibrary` gives complete trace control;
`register_ticker_callback` delivers real frame deltas every engine tick.

## 🚦 STEP 0 — Is it CPU-bound or GPU-bound? (DO THIS FIRST, ALWAYS)

**Never optimise before you know which processor is the bottleneck.** The frame time is
roughly `max(GameThread, RenderThread, GPU)` — these run in parallel, so only the *longest*
one sets your FPS. Cutting GPU cost (shadows, Lumen, post-process) does **nothing** if the
frame is actually game-thread or render-thread bound, and vice-versa. Getting this wrong
wastes the whole session.

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

Read `frame_timing` against this table: if `game_thread_ms = 25`, you are hard-capped at
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

### Fastest check — one tool call
```
editor_control(action="frame_timing")   # alias: action="bound"
```
Start PIE first and park in a representative/worst spot, then call it. It returns
`game_thread_ms`, `render_thread_ms`, `gpu_ms`, `rhi_thread_ms`, the `frame_ms`, a `bound`
verdict (`GameThread` / `RenderThread` / `GPU`), and a `hint` with what to do next. This is
the same data the `stat unit` overlay shows, read straight from engine globals — no
screenshots, no trace parsing.

> If `frame_timing` is unavailable (older plugin build), use the fallbacks below.

### Decision tree
| `bound` | Meaning | Where to look next |
|---|---|---|
| **GameThread** | CPU, game thread | Tick / Blueprint / AI / animation. Run `stat dumpframe -ms=0.5 -root=gamethread` → read_logs. **Fix is code/Blueprint, not CVars** (see "CVars do NOT fix the game thread" above): throttle ticks, enable anim URO, event-drive logic. Compare the ms to the budget table. |
| **RenderThread** | CPU, render thread | Draw calls & primitives, **dynamic shadow-casting lights**. Check `stat scenerendering`. Instance/merge meshes, enable Nanite, cut dynamic lights. |
| **GPU** | GPU | *Now* run ProfileGPU (below). Shadows (VSM), Lumen, translucency, resolution. |

### Confirm / fallback methods (no new tool needed)
1. **Numeric thread split via log** — reliable, works headless:
   ```python
   import unreal
   w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
   unreal.SystemLibrary.execute_console_command(w, "stat dumpframe -ms=0.5 -root=gamethread")
   unreal.SystemLibrary.execute_console_command(w, "stat dumpframe -ms=0.5 -root=renderthread")
   ```
   Then read the dump with `read_logs(action="tail", file="main", lines=150)` — it prints the
   full thread hierarchy in ms (e.g. `World Tick Time`, `FTickFunctionTask`, individual
   Blueprint `ReceiveTick` costs). This is the single most useful CPU drill-down.
2. **Resolution-scaling test** (decisive GPU-bound proof) — sample frame time at full res,
   then `r.ScreenPercentage 50`, and compare with the live ticker sampler (see Live Frame
   Time Sampling below). FPS jumps a lot → **GPU-bound**. FPS barely moves → **CPU-bound**.
3. **`stat unit`** is the canonical overlay, but its numbers do **not** go to the log, and
   PIE often runs in a separate window so screenshots are unreliable. Prefer methods 1–2 or
   `frame_timing` over trying to OCR `stat unit`.

## ⚠️ Gotchas — read before writing any code

0. **`ProfileGPU` tells you WHERE GPU time goes, not WHETHER you are GPU-bound.** It always
   produces a GPU breakdown even on a CPU-bound frame — so reading it first will happily send
   you optimising shadows on a frame whose real cost is the game thread. Run STEP 0 first;
   only reach for `ProfileGPU` once `bound == GPU`.

0b. **Never run `ProfileGPU` (or `stat dumpframe`) *during* a frame-time trace you intend to
   average.** Each `ProfileGPU` stalls the GPU for a full readback (hundreds of ms to
   seconds), and those stall frames poison `avg_frame_ms`/`p95`. Capture clean frame-time
   traces separately from GPU/CPU drill-downs.

0c. **`editor_control analyse` reports frame time only** (avg/p95/worst frames), **not** the
   CPU/GPU split. Use `frame_timing` for the split.

1. **`AutomationPerformaceHelper` crashes** — it requires a FunctionalTest outer world.
   Do NOT instantiate it. Use the patterns in this skill instead.

2. **Channel names use the `*Channel` suffix for `start_trace_to_file`** — pass
   `'CpuChannel'` not `'Cpu'`. `get_enabled_channels()` returns short names (`'Cpu'`);
   `get_all_channels()` returns full names (`'CpuChannel'`). They are the same channel,
   just different representations.

3. **`start_trace_to_file` adds default channels automatically** — even if you only
   request `['FrameChannel']`, UE will also enable Gpu, Screenshot, Region, Bookmark,
   Log, etc. You get more than you asked for; that's fine.

4. **Trace files are large** — a ~10 second trace with default channels is 30–50 MB.
   Store in `Saved/Profiling/` (already excluded from source control).

5. **Ticker callback handle must stay in scope** — if the variable holding the handle
   is garbage collected, the callback silently stops firing. Store in a module-level
   variable or a list when doing multi-step sampling.

6. **`is_tracing()` may return `True` briefly after `stop_tracing()`** — there is a
   small flush delay. The `.utrace` file is valid and complete once stop returns `True`.

---

## Trace Control

### Start a trace
```python
import unreal

saved = unreal.Paths.project_saved_dir()  # relative, resolves to Saved/
trace_path = saved + 'Profiling/my_capture'  # no extension — .utrace added automatically

channels = ['FrameChannel', 'CpuChannel', 'GpuChannel', 'StatsChannel']
ok = unreal.TraceUtilLibrary.start_trace_to_file(trace_path, channels)
print('started:', ok)  # True on success
print('is_tracing:', unreal.TraceUtilLibrary.is_tracing())
```

### Stop a trace
```python
import unreal
ok = unreal.TraceUtilLibrary.stop_tracing()
print('stopped:', ok)
# File is at: <project>/Saved/Profiling/my_capture.utrace
```

### Pause / resume (keep file open, stop writing temporarily)
```python
unreal.TraceUtilLibrary.pause_tracing()
# ... do something you don't want in the trace ...
unreal.TraceUtilLibrary.resume_tracing()
```

### Check status and active channels
```python
import unreal
print('tracing:', unreal.TraceUtilLibrary.is_tracing())
enabled = [str(c) for c in unreal.TraceUtilLibrary.get_enabled_channels()]
print('enabled:', enabled)
```

### Toggle a specific channel mid-trace
```python
unreal.TraceUtilLibrary.toggle_channel('MemAllocChannel', True)   # enable
unreal.TraceUtilLibrary.toggle_channel('NiagaraChannel', False)  # disable
```

---

## Annotations — Mark Regions and Bookmarks

Use these **while tracing** to label interesting moments in the Unreal Insights timeline.

```python
import unreal

# Single point-in-time bookmark (shows as a vertical line in Insights)
unreal.TraceUtilLibrary.trace_bookmark('spawn_wave_3')

# Named region (shows as a coloured span)
unreal.TraceUtilLibrary.trace_mark_region_start('loading_level')
# ... trigger the thing you want to measure ...
unreal.TraceUtilLibrary.trace_mark_region_end('loading_level')

# Screenshot embedded in the trace
unreal.TraceUtilLibrary.trace_screenshot('before_explosion', show_ui=False)
```

---

## Live Frame Time Sampling

Use `register_ticker_callback` to collect real frame deltas over a duration.
This is a **two-step pattern** because the callback fires asynchronously on engine ticks
between Python requests.

### Step 1 — Start sampling (call this first)
```python
import unreal

_ft_samples = []
_ft_handle = None

def _frame_sampler(dt):
    _ft_samples.append(dt)
    return True  # keep ticking — stop by calling unregister or returning False

_ft_handle = unreal.register_ticker_callback(_frame_sampler)
print('sampling started, handle:', _ft_handle)
```

### Step 2 — Collect results (call after waiting N seconds)
```python
import unreal, statistics

samples = _ft_samples[:]  # snapshot
if _ft_handle:
    unreal.unregister_ticker_callback(_ft_handle)

if not samples:
    print('no samples collected — did you wait long enough?')
else:
    ms = [s * 1000 for s in samples]
    print(f'frames:  {len(ms)}')
    print(f'avg:     {statistics.mean(ms):.2f} ms  ({1000/statistics.mean(ms):.1f} fps)')
    print(f'median:  {statistics.median(ms):.2f} ms')
    print(f'p95:     {sorted(ms)[int(len(ms)*0.95)]:.2f} ms')
    print(f'max:     {max(ms):.2f} ms  (worst hitch)')
    print(f'min:     {min(ms):.2f} ms')
```

### Single-call version (auto-stops after N frames)
```python
import unreal, statistics

_samples = []
_handle_ref = [None]

def _sampler(dt):
    _samples.append(dt * 1000)
    if len(_samples) >= 300:  # ~5 sec at 60fps
        return False  # unregisters automatically
    return True

_handle_ref[0] = unreal.register_ticker_callback(_sampler)
print(f'collecting 300 frames... check _samples list when done')
# In a follow-up call: analyse _samples
```

---

## Channel Reference

| Channel | What it captures | Use for |
|---|---|---|
| `FrameChannel` | Frame boundaries, wall time | Always include — baseline for everything |
| `CpuChannel` | CPU named scopes (TRACE_CPUPROFILER_EVENT_SCOPE) | CPU hotspots, Blueprint tick |
| `GpuChannel` | GPU pass timings | GPU bound? Where are draw calls going? |
| `StatsChannel` | UE stat counters (stat unit values etc.) | Actor/component counts, frame budget |
| `LogChannel` | Log output embedded in trace | Correlate log spam with frame spikes |
| `MemAllocChannel` | Per-allocation callstacks | Memory churn, GC pressure |
| `MemTagChannel` | High-level memory category totals | Which system is eating RAM? |
| `ObjectChannel` | UObject create/destroy | Asset streaming, actor spawning |
| `NiagaraChannel` | Niagara system tick | Particle perf |
| `AnimationChannel` | Animation graph evaluation | Anim Blueprint cost |
| `NetChannel` | Replication, RPC timing | Multiplayer performance |
| `SlateChannel` | UI widget tick and paint | UI overhead |
| `TaskChannel` | Task Graph threads | Async task scheduling |
| `LoadTimeChannel` | Asset streaming / load events | Load hitches |
| `BookmarkChannel` | `trace_bookmark()` calls | Always included when annotating |
| `RegionChannel` | `trace_mark_region_*()` calls | Always included when annotating |
| `ScreenshotChannel` | `trace_screenshot()` captures | Visual reference in Insights |

---

## Common Presets

### General performance capture (balanced)
```python
channels = ['FrameChannel', 'CpuChannel', 'GpuChannel', 'StatsChannel', 'LogChannel']
```

### Memory investigation
```python
channels = ['FrameChannel', 'MemAllocChannel', 'MemTagChannel', 'ObjectChannel', 'LoadTimeChannel']
```

### Animation / character perf
```python
channels = ['FrameChannel', 'CpuChannel', 'AnimationChannel', 'StatsChannel']
```

### UI / Slate overhead
```python
channels = ['FrameChannel', 'CpuChannel', 'SlateChannel', 'StatsChannel']
```

### Niagara / VFX
```python
channels = ['FrameChannel', 'CpuChannel', 'GpuChannel', 'NiagaraChannel']
```

### Multiplayer / networking
```python
channels = ['FrameChannel', 'CpuChannel', 'NetChannel', 'StatsChannel']
```

### Load time / streaming hitches
```python
channels = ['FrameChannel', 'LoadTimeChannel', 'ObjectChannel', 'LogChannel']
```

---

## Full Workflow Example

```python
import unreal, time

saved = unreal.Paths.project_saved_dir()
trace_path = saved + 'Profiling/combat_encounter'

# Start
channels = ['FrameChannel', 'CpuChannel', 'GpuChannel', 'StatsChannel', 'LogChannel']
unreal.TraceUtilLibrary.start_trace_to_file(trace_path, channels)
unreal.TraceUtilLibrary.trace_bookmark('capture_start')

# Mark a region around the thing you care about
unreal.TraceUtilLibrary.trace_mark_region_start('wave_spawn')
# (trigger the gameplay here)
unreal.TraceUtilLibrary.trace_mark_region_end('wave_spawn')

unreal.TraceUtilLibrary.trace_bookmark('capture_end')
unreal.TraceUtilLibrary.stop_tracing()

import os
abs_path = os.path.abspath(trace_path + '.utrace')
size_mb = os.path.getsize(abs_path) / 1024 / 1024
print(f'Trace saved: {abs_path}  ({size_mb:.1f} MB)')
print('Open with: UnrealInsights.exe or from Editor > Tools > Run Unreal Insights')
```

---

## Opening Traces

Traces can be opened from within the editor:
```
Editor menu: Tools → Run Unreal Insights
```

Or from the command line:
```
"<UE install>/Engine/Binaries/Win64/UnrealInsights.exe" "<path>/my_capture.utrace"
```

The trace file path is always:
```
<project root>/Saved/Profiling/<name>.utrace
```
