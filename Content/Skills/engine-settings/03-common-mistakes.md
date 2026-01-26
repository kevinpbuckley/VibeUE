# EngineSettingsService Common Mistakes

---

## Mistake 1: Using Folder Path Instead of Asset Path for Maps

**Problem:** Setting map paths without verifying the asset exists.

```python
# ❌ Wrong - using folder path, not the actual map file
import unreal
unreal.EngineSettingsService.set_engine_ini_value(
    "DefaultEngine.ini",
    "/Script/EngineSettings.GameMapsSettings",
    "GameDefaultMap",
    "/Game/Variant_Horror.Variant_Horror"  # This is the FOLDER, not the map!
)
```

**Solution:** Find and verify the actual map asset path.

```python
# ✅ Correct - verify asset exists first
import unreal

# The map file is INSIDE the folder
map_path = "/Game/Variant_Horror/Lvl_Horror"

if unreal.EditorAssetLibrary.does_asset_exist(map_path):
    full_path = f"{map_path}.Lvl_Horror"
    unreal.EngineSettingsService.set_engine_ini_value(
        "DefaultEngine.ini",
        "/Script/EngineSettings.GameMapsSettings",
        "GameDefaultMap",
        full_path
    )
```

**Common pattern:** `/Game/FolderName/Lvl_MapName.Lvl_MapName`

---

## Mistake 2: Using Wrong Category for CVars

**Problem:** Trying to access cvars through category methods instead of cvar methods.

```python
# ❌ Wrong - cvars aren't settings
import unreal
value = unreal.EngineSettingsService.get_setting("rendering", "r.ReflectionMethod")
# Returns empty - r.ReflectionMethod is a cvar, not a setting property
```

**Solution:** Use `get_console_variable` for cvars.

```python
# ✅ Correct - use cvar methods for console variables
import unreal
value = unreal.EngineSettingsService.get_console_variable("r.ReflectionMethod")
print(f"Reflection method: {value}")
```

---

## Mistake 2: Ignoring Read-Only CVars

**Problem:** Trying to set read-only console variables.

```python
# ❌ Wrong - some cvars are read-only
import unreal
result = unreal.EngineSettingsService.set_console_variable("r.MaxQualityMode", "1")
# May fail with "Console variable is read-only"
```

**Solution:** Check if cvar is writable before setting.

```python
# ✅ Correct - check read-only status first
import unreal

info = unreal.ConsoleVariableInfo()
if unreal.EngineSettingsService.get_console_variable_info("r.MaxQualityMode", info):
    if info.is_read_only:
        print(f"Cannot modify {info.name} - it's read-only")
    else:
        unreal.EngineSettingsService.set_console_variable(info.name, "1")
```

---

## Mistake 3: Expecting Immediate Effect

**Problem:** Some settings require editor restart or level reload to take effect.

```python
# ❌ Wrong - expecting immediate visual change
import unreal
unreal.EngineSettingsService.set_console_variable("r.Substrate", "1")
# Substrate rendering won't change until restart
```

**Solution:** Check `requires_restart` and inform user.

```python
# ✅ Correct - check and communicate restart requirements
import unreal

# Some settings that typically require restart
RESTART_REQUIRED = [
    "r.Substrate", "r.RayTracing", "r.VirtualTextures",
    "r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange"
]

cvar_name = "r.Substrate"
unreal.EngineSettingsService.set_console_variable(cvar_name, "1")
unreal.EngineSettingsService.save_all_engine_config()

if cvar_name in RESTART_REQUIRED:
    print(f"⚠️ {cvar_name} requires editor restart to take effect")
```

---

## Mistake 4: Wrong Config File for Setting

**Problem:** Looking for settings in the wrong config file.

```python
# ❌ Wrong - input settings aren't in DefaultEngine.ini
import unreal
value = unreal.EngineSettingsService.get_engine_ini_value(
    "/Script/Engine.InputSettings", "AxisMappings", "DefaultEngine.ini")
# Returns empty
```

**Solution:** Use the correct config file for each category.

```python
# ✅ Correct - input settings are in DefaultInput.ini
import unreal

# Check which config file a category uses
categories = unreal.EngineSettingsService.list_categories()
for cat in categories:
    if cat.category_id == "input":
        print(f"Input settings are in: {cat.config_file}")
        # Output: DefaultInput.ini

# Now use correct file
value = unreal.EngineSettingsService.get_engine_ini_value(
    "/Script/Engine.InputSettings", "DefaultPlayerInputClass", "DefaultInput.ini")
```

---

## Mistake 5: Scalability Level Out of Range

**Problem:** Using invalid scalability level values.

```python
# ❌ Wrong - level 5 doesn't exist
import unreal
result = unreal.EngineSettingsService.set_scalability_level("Shadow", 5)
# May apply unexpected settings
```

**Solution:** Use valid scalability levels (0-4).

```python
# ✅ Correct - use valid levels
import unreal

# Quality levels:
# 0 = Low
# 1 = Medium  
# 2 = High
# 3 = Epic
# 4 = Cinematic

result = unreal.EngineSettingsService.set_scalability_level("Shadow", 3)  # Epic
```

---

## Mistake 6: Modifying Engine Base Configs

**Problem:** Trying to modify engine base configuration (read-only).

```python
# ❌ Wrong - can't modify base engine configs
import unreal
# Engine/Config/BaseEngine.ini is read-only
result = unreal.EngineSettingsService.set_engine_ini_value(
    "/Script/Engine.Engine", "NearClipPlane", "1.0", "BaseEngine.ini")
```

**Solution:** Modify project-level overrides (DefaultEngine.ini).

```python
# ✅ Correct - modify project config overrides
import unreal
result = unreal.EngineSettingsService.set_engine_ini_value(
    "/Script/Engine.Engine", "NearClipPlane", "1.0", "DefaultEngine.ini")
# This creates a project-level override
```

---

## Mistake 7: Not Handling Result Errors

**Problem:** Not checking operation results for errors.

```python
# ❌ Wrong - assuming success
import unreal
unreal.EngineSettingsService.set_console_variable("r.NonExistentCVar", "1")
# Silently fails
```

**Solution:** Always check operation results.

```python
# ✅ Correct - check results
import unreal

result = unreal.EngineSettingsService.set_console_variable("r.NonExistentCVar", "1")
if result.success:
    print(f"Modified: {result.modified_settings}")
else:
    print(f"Failed: {result.error_message}")
    # Output: Failed: Console variable not found: r.NonExistentCVar
```

---

## Mistake 8: Batch JSON Format Errors

**Problem:** Using wrong JSON format for batch operations.

```python
# ❌ Wrong - passing Python dict directly
import unreal
settings = {"r.Shadow": "1", "r.RayTracing": "1"}
result = unreal.EngineSettingsService.set_console_variables_from_json(settings)
# Error: expects string, not dict
```

**Solution:** Convert to JSON string.

```python
# ✅ Correct - convert dict to JSON string
import unreal
import json

settings = {"r.Shadow.MaxResolution": "2048", "r.RayTracing": "1"}
result = unreal.EngineSettingsService.set_console_variables_from_json(json.dumps(settings))
```

---

## Mistake 9: Platform Settings Confusion

**Problem:** Modifying wrong platform settings for current platform.

```python
# ❌ Wrong - modifying Linux settings on Windows
import unreal
unreal.EngineSettingsService.set_setting("platform_linux", "TargetedRHIs", "SF_VULKAN_SM6")
# Has no effect on Windows build
```

**Solution:** Use correct platform category for target platform.

```python
# ✅ Correct - check and modify appropriate platform
import unreal
import platform

# Determine current platform
current_os = platform.system()
category = f"platform_{current_os.lower()}"

# List available platform categories
categories = unreal.EngineSettingsService.list_categories()
platform_cats = [c for c in categories if c.category_id.startswith("platform_")]
print(f"Available platforms: {[c.category_id for c in platform_cats]}")
```

---

## Quick Reference: Category to Config File Mapping

| Category | Config File |
|----------|-------------|
| rendering | DefaultEngine.ini |
| physics | DefaultEngine.ini |
| audio | DefaultEngine.ini |
| engine | DefaultEngine.ini |
| gc | DefaultEngine.ini |
| streaming | DefaultEngine.ini |
| network | DefaultEngine.ini |
| collision | DefaultEngine.ini |
| platform_windows | DefaultEngine.ini |
| hardware | DefaultEngine.ini |
| ai | DefaultEngine.ini |
| input | DefaultInput.ini |
| cvar | N/A (runtime only unless saved) |

---

## Quick Reference: Common CVar Prefixes

| Prefix | Domain |
|--------|--------|
| `r.` | Rendering |
| `p.` | Physics |
| `gc.` | Garbage Collection |
| `au.` | Audio |
| `t.` | Threading |
| `fx.` | Effects (Niagara/Cascade) |
| `foliage.` | Foliage |
| `a.` | Animation |
| `ai.` | AI |
| `net.` | Networking |
