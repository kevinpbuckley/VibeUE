---
name: engine-settings
display_name: Engine Settings System
description: Configure Unreal Engine core settings — rendering, physics, audio, garbage collection, console variables (cvars), and scalability levels (EngineSettingsService). Use when the user asks to change engine/rendering settings, set a console variable (r.*, etc.), adjust scalability/quality levels, or read engine config categories.
vibeue_classes:
  - EngineSettingsService
unreal_classes:
  - RendererSettings
  - PhysicsSettings
  - AudioSettings
---

# Engine Settings Skill

> 🔀 **Registered settings moved to the engine.** Category-based registered-setting access
> (`list_categories` / `list_settings` / `get_setting` / `set_setting`) is now Unreal 5.8's native
> **`ConfigSettingsToolset`** — reach it with `call_tool` (run `describe_toolset` on
> `ConfigSettingsToolset` for its actions/params). VibeUE's `EngineSettingsService` was trimmed to
> the delta the engine doesn't cover: **console variables (cvars), scalability levels, raw engine
> INI read/write, and config save**. Use those from `execute_python_code` as shown below.

## Critical Rules

### ⚠️ CVars (VibeUE) vs Registered Settings (engine toolset)

- **CVars** (console variables) → `unreal.EngineSettingsService.get_console_variable` /
  `set_console_variable` (kept in VibeUE).
- **Registered settings** (category + key, e.g. RendererSettings) → engine
  **`ConfigSettingsToolset`** via `call_tool`. `EngineSettingsService.get_setting` /
  `set_setting` / `list_categories` / `list_settings` were removed in the 5.8 consolidation.

```python
# CVars (names like r.ReflectionMethod) — VibeUE
value = unreal.EngineSettingsService.get_console_variable("r.ReflectionMethod")
unreal.EngineSettingsService.set_console_variable("r.ReflectionMethod", "1")

# Registered settings (category-based) — use the engine ConfigSettingsToolset via call_tool.
# Raw INI is also still available in VibeUE when you know the section/key:
value = unreal.EngineSettingsService.get_engine_ini_value(
    "/Script/Engine.RendererSettings", "bEnableRayTracing", "Engine.ini")
```

### ⚠️ Check Read-Only Before Setting CVars

```python
info = unreal.ConsoleVariableInfo()
if unreal.EngineSettingsService.get_console_variable_info("r.MaxQualityMode", info):
    if not info.is_read_only:
        unreal.EngineSettingsService.set_console_variable(info.name, "1")
```

### ⚠️ Some Settings Require Restart

`set_console_variable` / `set_engine_ini_value` return an `FEngineSettingResult` whose
`requires_restart` flag tells you whether the change takes effect immediately. (Registered-setting
results come from the engine `ConfigSettingsToolset` instead.)

---

## Browsing registered settings

Listing categories and the settings inside them is now an engine **`ConfigSettingsToolset`** job —
call it via `call_tool` and run `describe_toolset` for the available actions. These engine
categories cover rendering, physics, audio, GC, networking, collision, AI, input, and the rest.

For everything VibeUE still owns, see the cvar / INI / scalability workflows below.

---

## Workflows

### Enable Ray Tracing

```python
import unreal
import json

rt_settings = {
    "r.RayTracing": "1",
    "r.RayTracing.Shadows": "1",
    "r.RayTracing.Reflections": "1",
    "r.ReflectionMethod": "1"
}
result = unreal.EngineSettingsService.set_console_variables_from_json(json.dumps(rt_settings))
unreal.EngineSettingsService.save_all_engine_config()
```

### Optimize for Performance

```python
import unreal
import json

# Set overall scalability to Low (0)
unreal.EngineSettingsService.set_overall_scalability_level(0)

# Disable expensive features
perf_settings = {
    "r.RayTracing": "0",
    "r.VolumetricFog": "0",
    "r.MotionBlurQuality": "0",
    "r.Shadow.MaxResolution": "512"
}
unreal.EngineSettingsService.set_console_variables_from_json(json.dumps(perf_settings))
```

### Search CVars

```python
import unreal

shadows = unreal.EngineSettingsService.search_console_variables("shadow", 100)
for cvar in shadows:
    print(f"{cvar.name}: {cvar.value} ({cvar.type})")
```

### Set Engine INI Values

```python
import unreal

result = unreal.EngineSettingsService.set_engine_ini_value(
    "/Script/EngineSettings.GameMapsSettings",
    "GameDefaultMap",
    "/Game/Maps/Lvl_Main.Lvl_Main",
    "DefaultEngine.ini"
)
```

---

## Data Structures

> **Python Naming Convention**: C++ types like `FEngineSettingResult` are exposed as `EngineSettingResult` in Python (no `F` prefix).

### EngineSettingResult (returned by set_* and INI write methods)
- `success`, `error_message`
- `modified_settings`, `failed_settings`
- `requires_restart`

### ConsoleVariableInfo
- `name`, `value`, `default_value`, `description`
- `type`, `flags`, `is_read_only`

VibeUE `EngineSettingsService` covers:
- **Rendering optimization**: Adjust r.* cvars, ray tracing, shadows
- **Performance profiling**: GC cvars (gc.*), streaming, scalability
- **Quality presets**: Apply overall or per-group scalability levels
- **Raw engine INI**: read/write sections directly when you know the key

For browsing/editing *registered* settings by category (physics, audio, collision, AI, input
classes, etc.), use the engine **`ConfigSettingsToolset`** via `call_tool`.

## Sample scripts (run via `execute_python_code`)

- **`scripts/set_cvars.txt`** — read scalability and set a console variable.
