---
name: frame-rate
display_name: Frame Rate / FPS Caps
description: Diagnose and remove FPS caps so the editor and game run uncapped (or at a target FPS). Use when the user says the editor/game is "locked", "capped", or "stuck" at a frame rate (commonly 60 FPS), asks to "unlock"/"uncap"/"raise" FPS, or wants to set a max FPS. Covers t.MaxFPS, VSync, fixed/smoothed frame rate, and background CPU throttling. Cvars and project settings go through UE 5.9's own ConsoleToolset and ConfigSettingsToolset via call_tool; VibeUE supplies only the background-throttling override.
vibeue_classes:
  - PerformanceService
unreal_classes:
  - RendererSettings
keywords:
  - fps
  - uncap fps
  - unlock fps
  - 60 fps
  - frame rate
  - framerate
  - max fps
  - vsync
  - frame rate limit
---

# Frame Rate / FPS Caps Skill

A "locked at 60 FPS" editor or game is almost never caused by a single setting — there
are **five independent caps** and any one of them holds the frame rate down. Check and
clear them all, in this order, before declaring it fixed.

## ⚠️ Diagnose first — don't guess

`t.MaxFPS = 0` already means *uncapped*, so if the editor is still stuck at 60 the cap is
coming from somewhere else (usually VSync to a 60 Hz monitor, or the smoothed frame-rate
range). Read every source below before changing anything.

> 🩺 **"Capped" vs "slow" are different problems.** If the frame rate is *low* rather than *pinned*
> at a round number, it's not a cap — it's a workload. Read the per-thread split first with
> `call_tool GetFrameTimings {}` (Epic's `PerfToolset.StatsToolset`; VibeUE's `frame_timing` was
> removed) and work out the bottleneck yourself: the largest of `GameThreadMs` / `RenderThreadMs` /
> `RhiThreadMs` / `GpuMs` gates the frame, and it should be close to `FrameMs`. **If no single one of
> them comes near `FrameMs`, that is the signature of a cap** — carry on down this page. See the
> `profiling` skill for the full verdict/budget arithmetic.
> Cvar and settings work is Epic's `ConsoleToolset` / `ConfigSettingsToolset` via `call_tool` -- see the
> `engine-settings` skill. VibeUE's `EngineSettingsService` was removed on the 5-9 branch.

| # | Setting | Where | "Uncapped" value |
|---|---------|-------|------------------|
| 1 | `t.MaxFPS` | cvar | `0` (or a high target like `240`) |
| 2 | `r.VSync` / `r.VSyncEditor` | cvar | `0` — locks to monitor refresh (60 Hz) when `1` |
| 3 | `bUseFixedFrameRate` / `FixedFrameRate` | `[/Script/Engine.Engine]` DefaultEngine.ini | `False` |
| 4 | `bSmoothFrameRate` / `SmoothedFrameRateRange` | `[/Script/Engine.Engine]` DefaultEngine.ini | `False` — **the usual ~60 editor cap** |
| 5 | `bThrottleCPUWhenNotForeground` | `[/Script/UnrealEd.EditorPerformanceSettings]` EditorPerProjectUserSettings.ini | `False` — caps FPS when the editor isn't focused |

> Note: `r.FrameRateLimit` is **not** a console variable — `SetCVarValue`
> will report "Console variable not found". The INI key is `FrameRateLimit` under
> `[/Script/Engine.Engine]`, but `t.MaxFPS` is the reliable runtime cap.

## Workflow — read the current state

These are `call_tool` steps, **not** `execute_python`: Epic's `ConsoleToolset` and
`ConfigSettingsToolset` carry no `PythonCallable`, so `unreal.ConsoleToolset` does not exist.

```
# 1 + 2: the cvar caps
call_tool GetCVarValue {"Name": "t.MaxFPS"}
call_tool GetCVarValue {"Name": "r.VSync"}

# 3 + 4: the project caps — UEngine lives at Project / Engine / General
call_tool GetSectionPropertyValues {"Container": "Project", "Category": "Engine", "Section": "General",
                                    "Properties": ["bUseFixedFrameRate", "FixedFrameRate",
                                                   "bSmoothFrameRate", "SmoothedFrameRateRange"]}

# 5: the editor cap — Editor Preferences -> Performance
call_tool GetSectionPropertyValues {"Container": "Editor", "Category": "General", "Section": "Performance",
                                    "Properties": ["bThrottleCPUWhenNotForeground"]}
```

If a section coordinate above does not resolve, find it with
`call_tool SearchSettings {"Search": "Smooth Frame Rate"}` rather than guessing.

## Workflow — uncap FPS

Set a target with `t.MaxFPS` (use `0` for fully uncapped, or e.g. `240` to cap high), then
clear the other four caps. cvars take effect immediately; INI changes need a save and may
require an editor restart.

`scripts/uncap_fps.txt` is the copy-pasteable version of this.

```
# 1 + 2: runtime cvars (immediate — a cvar is not config, there is nothing to save)
call_tool SetCVarValue {"Name": "t.MaxFPS", "Value": "0"}        # or "240" to cap high
call_tool SetCVarValue {"Name": "r.VSync",  "Value": "0"}

# 3 + 4: disable the fixed and smoothed frame-rate caps (the setter persists it correctly)
call_tool SetSectionProperties {"Container": "Project", "Category": "Engine", "Section": "General",
                                "Values": "{\"bSmoothFrameRate\": false, \"bUseFixedFrameRate\": false}"}

# 5: don't throttle when the editor loses focus
call_tool SetSectionProperties {"Container": "Editor", "Category": "General", "Section": "Performance",
                                "Values": "{\"bThrottleCPUWhenNotForeground\": false}"}

# Verify
call_tool GetCVarValue {"Name": "t.MaxFPS"}
```

> ⚠️ `bThrottleCPUWhenNotForeground = false` is **not enough for a minimized editor**.
> `UEditorEngine::ShouldThrottleCPUUsage` falls through to `AreAllWindowsHidden()`, so a hidden
> editor throttles anyway. For a genuinely unthrottled background session use VibeUE's
> `unreal.PerformanceService.set_background_throttling(False)` from `execute_python` — it registers
> the `ShouldDisableCPUThrottlingDelegates` override that nothing in Epic's surface exposes. Snapshot
> `get_background_throttling()` first so you can put it back.

## Verify

After applying, confirm `t.MaxFPS` reads your target and the editor's FPS overlay (Show FPS,
or `stat fps` in PIE) climbs past 60. If it's still pinned at 60, the most likely remaining
cause is `r.VSync`/`r.VSyncEditor` re-enabled by a quality preset, or the monitor itself —
VSync can only go as high as the display refresh rate.

## Sample scripts (run via `execute_python`)

- **`scripts/uncap_fps.txt`** — read all five caps, then uncap to a target FPS.

## Reference pages

Open these with the paths below; UE rewrites them to absolute paths when it serves this skill.

- [scripts/uncap_fps.txt](scripts/uncap_fps.txt) - runnable sample
