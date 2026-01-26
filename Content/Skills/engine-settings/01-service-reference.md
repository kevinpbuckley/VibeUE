# EngineSettingsService API Reference

**Python Module**: `unreal.EngineSettingsService`

All functions are static (class methods) - no instance required.

---

## Core Data Structures

### FEngineSettingInfo

Information about a single engine setting.

**Properties:**
- `key` (str) - Setting key name (e.g., "r.ReflectionMethod", "bEnableRayTracing")
- `display_name` (str) - Human-readable display name
- `description` (str) - Tooltip/description text
- `type` (str) - Setting type: "string", "int", "float", "bool", "array", "object"
- `value` (str) - Current value as string
- `default_value` (str) - Default value as string
- `config_section` (str) - INI section path (e.g., "/Script/Engine.RendererSettings")
- `config_file` (str) - INI file name (e.g., "DefaultEngine.ini")
- `requires_restart` (bool) - Whether this setting requires editor restart
- `read_only` (bool) - Whether this setting cannot be modified
- `is_console_variable` (bool) - Whether this is a console variable (cvar)

### FEngineSettingCategory

Information about an engine settings category.

**Properties:**
- `category_id` (str) - Category identifier (e.g., "rendering", "physics", "audio")
- `display_name` (str) - Human-readable category name
- `description` (str) - Category description
- `setting_count` (int) - Number of settings in this category (-1 for cvar = many)
- `settings_class_name` (str) - Associated UObject settings class path
- `config_file` (str) - Primary config file for this category

### FEngineSettingResult

Result of an engine settings operation.

**Properties:**
- `success` (bool) - Whether the operation succeeded
- `error_message` (str) - Error message if failed (empty if success)
- `modified_settings` (Array[str]) - Settings that were successfully modified
- `failed_settings` (Array[str]) - Settings that failed to modify with reasons
- `requires_restart` (bool) - Whether changes require editor restart

### FConsoleVariableInfo

Information about a console variable (cvar).

**Properties:**
- `name` (str) - Console variable name (e.g., "r.ReflectionMethod")
- `value` (str) - Current value as string
- `default_value` (str) - Default value as string
- `description` (str) - Help text/description
- `type` (str) - Value type: "int", "float", "string", "bool"
- `flags` (str) - CVar flags (e.g., "RenderThreadSafe, Scalability")
- `is_read_only` (bool) - Whether the cvar is read-only

---

## Available Categories

| Category ID | Display Name | Description |
|-------------|-------------|-------------|
| `rendering` | Rendering Settings | Graphics, shaders, ray tracing, reflections |
| `physics` | Physics Settings | Physics simulation, collision, dynamics |
| `audio` | Audio Settings | Sound, spatialization, mixing |
| `engine` | Core Engine Settings | Engine core configuration, tick rates |
| `gc` | Garbage Collection | Memory management, GC timing |
| `streaming` | Streaming Settings | Level streaming, texture streaming |
| `network` | Network Settings | Networking, replication |
| `collision` | Collision Profiles | Collision channels and profiles |
| `platform_windows` | Windows Platform | Windows-specific settings |
| `hardware` | Hardware Targeting | Target hardware and performance tier |
| `ai` | AI System | AI module settings, navigation |
| `input` | Input Settings | Input bindings and axis mappings |
| `cvar` | Console Variables | Direct cvar access (use cvar methods) |

---

## Category Operations

### list_categories()

List all available engine settings categories.

**Returns:** `Array[FEngineSettingCategory]`

**Example:**
```python
import unreal

categories = unreal.EngineSettingsService.list_categories()
for cat in categories:
    print(f"{cat.category_id}: {cat.display_name} ({cat.setting_count} settings)")
```

---

## Settings Discovery

### list_settings(category_id)

List all settings within a category with their current values.

**Parameters:**
- `category_id` (str) - Category identifier (e.g., "rendering", "physics")

**Returns:** `Array[FEngineSettingInfo]`

**Example:**
```python
import unreal

settings = unreal.EngineSettingsService.list_settings("rendering")
for s in settings:
    print(f"{s.key} = {s.value} ({s.type})")
```

### get_setting_info(category_id, key, out_info)

Get detailed information about a specific setting.

**Parameters:**
- `category_id` (str) - Category identifier
- `key` (str) - Setting key name
- `out_info` (FEngineSettingInfo) - Output structure with setting details

**Returns:** `bool` - True if setting was found

**Example:**
```python
import unreal

info = unreal.EngineSettingInfo()
found = unreal.EngineSettingsService.get_setting_info("rendering", "r.ReflectionMethod", info)
if found:
    print(f"Reflection method: {info.value}")
    print(f"Description: {info.description}")
```

---

## Get/Set Individual Settings

### get_setting(category_id, key)

Get a single engine setting value.

**Parameters:**
- `category_id` (str) - Category identifier
- `key` (str) - Setting key name

**Returns:** `str` - Setting value as string (empty if not found)

**Example:**
```python
import unreal

value = unreal.EngineSettingsService.get_setting("rendering", "r.RayTracing")
print(f"Ray tracing enabled: {value}")
```

### set_setting(category_id, key, value)

Set a single engine setting value.

**Parameters:**
- `category_id` (str) - Category identifier
- `key` (str) - Setting key name
- `value` (str) - New value as string

**Returns:** `FEngineSettingResult`

**Example:**
```python
import unreal

result = unreal.EngineSettingsService.set_setting("rendering", "r.RayTracing", "True")
if result.success:
    print("Ray tracing enabled!")
    if result.requires_restart:
        print("Restart required for changes to take effect")
else:
    print(f"Failed: {result.error_message}")
```

---

## Console Variables (CVars)

### get_console_variable(name)

Get a console variable value.

**Parameters:**
- `name` (str) - Console variable name (e.g., "r.ReflectionMethod")

**Returns:** `str` - Current value as string (empty if not found)

**Example:**
```python
import unreal

method = unreal.EngineSettingsService.get_console_variable("r.ReflectionMethod")
print(f"Reflection method: {method}")  # 0=None, 1=Lumen, 2=SSR
```

### set_console_variable(name, value)

Set a console variable value.

**Parameters:**
- `name` (str) - Console variable name
- `value` (str) - New value as string

**Returns:** `FEngineSettingResult`

**Example:**
```python
import unreal

# Enable Lumen reflections
result = unreal.EngineSettingsService.set_console_variable("r.ReflectionMethod", "1")
if result.success:
    print("Lumen reflections enabled")
```

### get_console_variable_info(name, out_info)

Get detailed information about a console variable.

**Parameters:**
- `name` (str) - Console variable name
- `out_info` (FConsoleVariableInfo) - Output structure

**Returns:** `bool` - True if cvar was found

**Example:**
```python
import unreal

info = unreal.ConsoleVariableInfo()
found = unreal.EngineSettingsService.get_console_variable_info("r.Shadow.MaxResolution", info)
if found:
    print(f"Name: {info.name}")
    print(f"Value: {info.value}")
    print(f"Description: {info.description}")
    print(f"Type: {info.type}")
    print(f"Flags: {info.flags}")
```

### search_console_variables(search_term, max_results=50)

Search for console variables by name or description.

**Parameters:**
- `search_term` (str) - Search string (matches name and description)
- `max_results` (int) - Maximum results (0 = unlimited)

**Returns:** `Array[FConsoleVariableInfo]`

**Example:**
```python
import unreal

# Find all shadow-related cvars
shadows = unreal.EngineSettingsService.search_console_variables("shadow", 20)
for cvar in shadows:
    print(f"{cvar.name}: {cvar.description[:60]}...")
```

### list_console_variables_with_prefix(prefix, max_results=100)

List all console variables with a specific prefix.

**Parameters:**
- `prefix` (str) - CVar prefix (e.g., "r.", "gc.", "p.")
- `max_results` (int) - Maximum results (0 = unlimited)

**Returns:** `Array[FConsoleVariableInfo]`

**Example:**
```python
import unreal

# List all rendering cvars
r_cvars = unreal.EngineSettingsService.list_console_variables_with_prefix("r.", 50)
print(f"Found {len(r_cvars)} rendering cvars")

# List all GC cvars
gc_cvars = unreal.EngineSettingsService.list_console_variables_with_prefix("gc.")
for cvar in gc_cvars:
    print(f"{cvar.name} = {cvar.value}")
```

---

## Batch Operations

### get_category_settings_as_json(category_id)

Get all settings in a category as a JSON object.

**Parameters:**
- `category_id` (str) - Category identifier

**Returns:** `str` - JSON object string with all key-value pairs

**Example:**
```python
import unreal
import json

json_str = unreal.EngineSettingsService.get_category_settings_as_json("rendering")
settings = json.loads(json_str)
for key, value in settings.items():
    print(f"{key}: {value}")
```

### set_category_settings_from_json(category_id, settings_json)

Set multiple settings in a category from a JSON object.

**Parameters:**
- `category_id` (str) - Category identifier
- `settings_json` (str) - JSON object string with key-value pairs

**Returns:** `FEngineSettingResult`

**Example:**
```python
import unreal
import json

settings = {
    "r.RayTracing": "True",
    "r.ReflectionMethod": "1"
}
result = unreal.EngineSettingsService.set_category_settings_from_json(
    "rendering", json.dumps(settings))
print(f"Modified: {result.modified_settings}")
print(f"Failed: {result.failed_settings}")
```

### set_console_variables_from_json(settings_json)

Set multiple console variables from a JSON object.

**Parameters:**
- `settings_json` (str) - JSON object with cvar name->value pairs

**Returns:** `FEngineSettingResult`

**Example:**
```python
import unreal
import json

cvars = {
    "r.Shadow.MaxResolution": "2048",
    "r.Shadow.MaxCSMResolution": "2048",
    "r.Shadow.DistanceScale": "1.5"
}
result = unreal.EngineSettingsService.set_console_variables_from_json(json.dumps(cvars))
print(f"Updated {len(result.modified_settings)} cvars")
```

---

## Direct Engine INI Access

### list_engine_sections(config_file, include_base=False)

List all sections in an engine config file.

**Parameters:**
- `config_file` (str) - Config file name (e.g., "DefaultEngine.ini")
- `include_base` (bool) - Include base engine configs

**Returns:** `Array[str]` - Section names

**Example:**
```python
import unreal

sections = unreal.EngineSettingsService.list_engine_sections("DefaultEngine.ini")
for section in sections:
    print(section)
```

### get_engine_ini_value(section, key, config_file)

Get a value from an engine config file.

**Parameters:**
- `section` (str) - INI section
- `key` (str) - Key name
- `config_file` (str) - Config file name

**Returns:** `str` - Value (empty if not found)

**Example:**
```python
import unreal

near_clip = unreal.EngineSettingsService.get_engine_ini_value(
    "/Script/Engine.Engine", "NearClipPlane", "DefaultEngine.ini")
print(f"Near clip plane: {near_clip}")
```

### set_engine_ini_value(section, key, value, config_file)

Set a value in an engine config file.

**Parameters:**
- `section` (str) - INI section
- `key` (str) - Key name
- `value` (str) - Value to set
- `config_file` (str) - Config file name

**Returns:** `FEngineSettingResult`

### get_engine_ini_array(section, key, config_file)

Get an array of values from an engine config file.

**Returns:** `Array[str]`

### set_engine_ini_array(section, key, values, config_file)

Set an array of values in an engine config file.

**Returns:** `FEngineSettingResult`

---

## Scalability Settings

### get_scalability_settings()

Get current scalability settings as JSON.

**Returns:** `str` - JSON object with quality levels

**Example:**
```python
import unreal
import json

json_str = unreal.EngineSettingsService.get_scalability_settings()
settings = json.loads(json_str)
print(f"Shadow Quality: {settings['ShadowQuality']}")
print(f"Texture Quality: {settings['TextureQuality']}")
```

### set_scalability_level(group_name, quality_level)

Set scalability quality level for a group.

**Parameters:**
- `group_name` (str) - Scalability group: "ViewDistance", "AntiAliasing", "Shadow", "GlobalIllumination", "Reflection", "PostProcess", "Texture", "Effects", "Foliage", "Shading"
- `quality_level` (int) - 0=Low, 1=Medium, 2=High, 3=Epic, 4=Cinematic

**Returns:** `FEngineSettingResult`

**Example:**
```python
import unreal

# Set shadows to Epic quality
result = unreal.EngineSettingsService.set_scalability_level("Shadow", 3)
if result.success:
    print("Shadow quality set to Epic")
```

### set_overall_scalability_level(quality_level)

Apply quality level to all scalability groups.

**Parameters:**
- `quality_level` (int) - 0=Low, 1=Medium, 2=High, 3=Epic, 4=Cinematic

**Returns:** `FEngineSettingResult`

**Example:**
```python
import unreal

# Set everything to Epic quality
result = unreal.EngineSettingsService.set_overall_scalability_level(3)
print(f"Applied Epic quality to all groups")
```

---

## Persistence

### save_all_engine_config()

Force save all pending engine config changes to disk.

**Returns:** `bool` - True if successful

### save_engine_config(config_file)

Save a specific engine config file.

**Parameters:**
- `config_file` (str) - Config file name (e.g., "DefaultEngine.ini")

**Returns:** `bool` - True if successful

**Example:**
```python
import unreal

# Make changes and save
unreal.EngineSettingsService.set_setting("rendering", "r.RayTracing", "True")
unreal.EngineSettingsService.save_engine_config("DefaultEngine.ini")
```
