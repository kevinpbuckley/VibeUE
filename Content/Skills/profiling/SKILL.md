---
name: profiling
display_name: Profiling & Performance
description: Control Unreal Insights traces, sample live frame times, and annotate performance captures — all from Python with no C++ required
unreal_classes:
  - TraceUtilLibrary
keywords:
  - profiling
  - performance
  - trace
  - unreal insights
  - frame time
  - fps
  - cpu
  - gpu
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

## ⚠️ Gotchas — read before writing any code

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
