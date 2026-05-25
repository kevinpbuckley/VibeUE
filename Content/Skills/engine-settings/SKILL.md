---
name: engine-settings
display_name: Engine Settings System
description: Configure Unreal Engine core settings including rendering, physics, audio, garbage collection, console variables (cvars), and scalability levels
vibeue_classes:
  - EngineSettingsService
unreal_classes:
  - RendererSettings
  - PhysicsSettings
  - AudioSettings
---

# Engine Settings Skill

## Critical Rules

### ⚠️ CVars vs Settings

- **CVars** (console variables) use `get_console_variable` / `set_console_variable`
- **Settings** use `get_setting` / `set_setting` with category

```python
# CVars (names like r.ReflectionMethod)
value = unreal.EngineSettingsService.get_console_variable("r.ReflectionMethod")
unreal.EngineSettingsService.set_console_variable("r.ReflectionMethod", "1")

# Settings (category-based)
value = unreal.EngineSettingsService.get_setting("rendering", "bEnableRayTracing")
```

### ⚠️ Check Read-Only Before Setting CVars

```python
info = unreal.ConsoleVariableInfo()
if unreal.EngineSettingsService.get_console_variable_info("r.MaxQualityMode", info):
    if not info.is_read_only:
        unreal.EngineSettingsService.set_console_variable(info.name, "1")
```

### ⚠️ Some Settings Require Restart

Check `requires_restart` in `FEngineSettingInfo` before expecting immediate changes.

---

## Categories

| Category | Description |
|----------|-------------|
| `rendering` | Graphics, shaders, ray tracing |
| `physics` | Physics simulation, collision |
| `audio` | Sound, spatialization |
| `gc` | Garbage collection, memory |
| `network` | Networking, replication |
| `collision` | Collision channels and profiles |
| `ai` | AI module, navigation |
| `input` | Input bindings |

---

## Workflows

### List Categories and Settings

```python
import unreal

categories = unreal.EngineSettingsService.list_categories()
for cat in categories:
    print(f"{cat.category_id}: {cat.display_name}")

settings = unreal.EngineSettingsService.list_settings("rendering")
for s in settings:
    print(f"{s.key} = {s.value}")
```

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

> **Python Naming Convention**: C++ types like `FEngineSettingInfo` are exposed as `EngineSettingInfo` in Python (no `F` prefix).

### EngineSettingInfo
- `key`, `display_name`, `description`
- `type`, `value`, `default_value`
- `requires_restart`, `read_only`, `is_console_variable`

### ConsoleVariableInfo
- `name`, `value`, `default_value`, `description`
- `type`, `flags`, `is_read_only`

- **Rendering optimization**: Adjust r.* cvars, ray tracing, shadows
- **Physics tuning**: Configure collision profiles, physics simulation
- **Audio configuration**: Sample rates, channels, spatialization
- **Performance profiling**: GC settings, streaming, scalability
- **Platform settings**: Windows graphics API, shader formats
- **Quality presets**: Apply overall or per-group scalability levels
