---
name: frame-rate
display_name: Frame Rate / FPS Caps
description: Diagnose and remove FPS caps so the editor and game run uncapped (or at a target FPS). Use when the user says the editor/game is "locked", "capped", or "stuck" at a frame rate (commonly 60 FPS), asks to "unlock"/"uncap"/"raise" FPS, or wants to set a max FPS. Covers t.MaxFPS, VSync, fixed/smoothed frame rate, and background CPU throttling (EngineSettingsService).
vibeue_classes:
  - EngineSettingsService
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

| # | Setting | Where | "Uncapped" value |
|---|---------|-------|------------------|
| 1 | `t.MaxFPS` | cvar | `0` (or a high target like `240`) |
| 2 | `r.VSync` / `r.VSyncEditor` | cvar | `0` — locks to monitor refresh (60 Hz) when `1` |
| 3 | `bUseFixedFrameRate` / `FixedFrameRate` | `[/Script/Engine.Engine]` DefaultEngine.ini | `False` |
| 4 | `bSmoothFrameRate` / `SmoothedFrameRateRange` | `[/Script/Engine.Engine]` DefaultEngine.ini | `False` — **the usual ~60 editor cap** |
| 5 | `bThrottleCPUWhenNotForeground` | `[/Script/UnrealEd.EditorPerformanceSettings]` EditorPerProjectUserSettings.ini | `False` — caps FPS when the editor isn't focused |

> Note: `r.FrameRateLimit` is **not** a console variable in UE 5.7 — `set_console_variable`
> will report "Console variable not found". The INI key is `FrameRateLimit` under
> `[/Script/Engine.Engine]`, but `t.MaxFPS` is the reliable runtime cap.

## Workflow — read the current state

```python
import unreal
es = unreal.EngineSettingsService

for cvar in ["t.MaxFPS", "r.VSync", "r.VSyncEditor"]:
    print(f"{cvar} = {es.get_console_variable(cvar)}")

eng = "/Script/Engine.Engine"
for key in ["bUseFixedFrameRate", "FixedFrameRate", "bSmoothFrameRate", "SmoothedFrameRateRange"]:
    print(f"{key} = {es.get_engine_ini_value(eng, key, 'DefaultEngine.ini')!r}")

print("bThrottleCPUWhenNotForeground =",
      es.get_engine_ini_value("/Script/UnrealEd.EditorPerformanceSettings",
                              "bThrottleCPUWhenNotForeground", "EditorPerProjectUserSettings.ini"))
```

## Workflow — uncap FPS

Set a target with `t.MaxFPS` (use `0` for fully uncapped, or e.g. `240` to cap high), then
clear the other four caps. cvars take effect immediately; INI changes need a save and may
require an editor restart.

```python
import unreal, json
es = unreal.EngineSettingsService

TARGET_MAX_FPS = "240"   # "0" = fully uncapped

# 1 + 2: runtime cvars (immediate)
es.set_console_variables_from_json(json.dumps({
    "t.MaxFPS":     TARGET_MAX_FPS,
    "r.VSync":      "0",
    "r.VSyncEditor":"0",
}))

# 3 + 4: disable fixed and smoothed frame-rate caps (persisted to ini)
eng = "/Script/Engine.Engine"
es.set_engine_ini_value(eng, "bUseFixedFrameRate", "False", "DefaultEngine.ini")
es.set_engine_ini_value(eng, "bSmoothFrameRate",   "False", "DefaultEngine.ini")

# 5: don't throttle FPS when the editor loses focus
es.set_engine_ini_value("/Script/UnrealEd.EditorPerformanceSettings",
                        "bThrottleCPUWhenNotForeground", "False",
                        "EditorPerProjectUserSettings.ini")

es.save_all_engine_config()
print("FPS caps cleared. t.MaxFPS now:", es.get_console_variable("t.MaxFPS"))
```

## Verify

After applying, confirm `t.MaxFPS` reads your target and the editor's FPS overlay (Show FPS,
or `stat fps` in PIE) climbs past 60. If it's still pinned at 60, the most likely remaining
cause is `r.VSync`/`r.VSyncEditor` re-enabled by a quality preset, or the monitor itself —
VSync can only go as high as the display refresh rate.

## Sample scripts (run via `execute_python_code`)

- **`scripts/uncap_fps.pyx`** — read all five caps, then uncap to a target FPS.
