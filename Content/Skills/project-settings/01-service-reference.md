# ProjectSettingsService API Reference

**Python Module**: `unreal.ProjectSettingsService`

All functions are static (class methods) - no instance required.

---

## Core Data Structures

### FProjectSettingInfo

Information about a single project setting.

**Properties:**
- `key` (str) - Setting key name (e.g., "ProjectName", "bGlobalGravitySet")
- `display_name` (str) - Human-readable display name
- `description` (str) - Tooltip/description text
- `type` (str) - Setting type: "string", "int", "float", "bool", "array", "object"
- `value` (str) - Current value as string (JSON-encoded for complex types)
- `default_value` (str) - Default value as string
- `config_section` (str) - INI section path (e.g., "/Script/EngineSettings.GeneralProjectSettings")
- `config_file` (str) - INI file name (e.g., "DefaultGame.ini")
- `requires_restart` (bool) - Whether this setting requires editor restart
- `read_only` (bool) - Whether this setting cannot be modified

### FProjectSettingCategory

Information about a settings category.

**Properties:**
- `category_id` (str) - Category identifier (e.g., "general", "maps", "rendering")
- `display_name` (str) - Human-readable category name
- `description` (str) - Category description
- `setting_count` (int) - Number of settings in this category
- `settings_class_name` (str) - Associated UObject settings class path
- `config_file` (str) - Primary config file for this category

### FProjectSettingResult

Result of a project settings operation.

**Properties:**
- `success` (bool) - Whether the operation succeeded
- `error_message` (str) - Error message if failed (empty if success)
- `modified_settings` (Array[str]) - Settings that were successfully modified
- `failed_settings` (Array[str]) - Settings that failed to modify with reasons
- `requires_restart` (bool) - Whether changes require editor restart

### FSettingsClassInfo

Information about a discovered settings class.

**Properties:**
- `class_name` (str) - UClass name (e.g., "UGeneralProjectSettings")
- `class_path` (str) - Full class path
- `config_section` (str) - INI section this class maps to
- `config_file` (str) - INI file this class saves to
- `property_count` (int) - Number of configurable properties
- `is_developer_settings` (bool) - Whether this is a UDeveloperSettings subclass

---

## Category Operations

### list_categories()

List all available settings categories (predefined + discovered UDeveloperSettings).

**Returns:** `Array[FProjectSettingCategory]`

**Example:**
```python
import unreal

categories = unreal.ProjectSettingsService.list_categories()
for cat in categories:
    print(f"{cat.category_id}: {cat.display_name} ({cat.setting_count} settings)")
```

---

## Settings Discovery

### discover_settings_classes()

Discover all UDeveloperSettings subclasses in the engine and project.

**Returns:** `Array[FSettingsClassInfo]`

**Example:**
```python
import unreal

classes = unreal.ProjectSettingsService.discover_settings_classes()
for c in classes:
    print(f"{c.class_name}: {c.property_count} properties in {c.config_file}")
```

### list_settings(category_id)

List all settings within a category with their current values.

**Parameters:**
- `category_id` (str) - Category identifier (e.g., "general", "maps")

**Returns:** `Array[FProjectSettingInfo]`

**Example:**
```python
import unreal

settings = unreal.ProjectSettingsService.list_settings("general")
for s in settings:
    print(f"{s.key} = {s.value} ({s.type})")
```

### get_setting_info(category_id, key, out_info)

Get detailed information about a specific setting.

**Parameters:**
- `category_id` (str) - Category identifier
- `key` (str) - Setting key name
- `out_info` (FProjectSettingInfo) - Output structure with setting details

**Returns:** `bool` - True if setting was found

**Example:**
```python
import unreal

info = unreal.ProjectSettingInfo()
found = unreal.ProjectSettingsService.get_setting_info("general", "ProjectName", info)
if found:
    print(f"Project name: {info.value}")
    print(f"Type: {info.type}")
    print(f"Config: {info.config_file}:{info.config_section}")
```

---

## Get/Set Individual Settings

### get_setting(category_id, key)

Get a single setting value.

**Parameters:**
- `category_id` (str) - Category identifier
- `key` (str) - Setting key name

**Returns:** `str` - Setting value as string (empty if not found)

**Example:**
```python
import unreal

project_name = unreal.ProjectSettingsService.get_setting("general", "ProjectName")
print(f"Project: {project_name}")
```

### set_setting(category_id, key, value)

Set a single setting value.

**Parameters:**
- `category_id` (str) - Category identifier
- `key` (str) - Setting key name
- `value` (str) - New value as string

**Returns:** `FProjectSettingResult`

**Example:**
```python
import unreal

result = unreal.ProjectSettingsService.set_setting("general", "ProjectName", "My Game")
if result.success:
    print("Setting updated!")
    if result.requires_restart:
        print("Editor restart required")
else:
    print(f"Failed: {result.error_message}")
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

settings_json = unreal.ProjectSettingsService.get_category_settings_as_json("general")
settings = json.loads(settings_json)
print(json.dumps(settings, indent=2))
```

### set_category_settings_from_json(category_id, settings_json)

Set multiple settings in a category from a JSON object.

**Parameters:**
- `category_id` (str) - Category identifier
- `settings_json` (str) - JSON object string with key-value pairs

**Returns:** `FProjectSettingResult` - Operation result with success/failure lists

**Example:**
```python
import unreal
import json

new_settings = {
    "ProjectName": "My RPG",
    "CompanyName": "Indie Studio",
    "Description": "An epic adventure"
}

result = unreal.ProjectSettingsService.set_category_settings_from_json(
    "general",
    json.dumps(new_settings)
)

print(f"Modified: {result.modified_settings}")
print(f"Failed: {result.failed_settings}")
```

---

## Direct INI Access

For advanced scenarios where predefined categories don't cover your needs.

### list_ini_sections(config_file)

List all sections in a config file.

**Parameters:**
- `config_file` (str) - Config file name (e.g., "DefaultEngine.ini", "DefaultGame.ini")

**Returns:** `Array[str]` - Array of section names

**Example:**
```python
import unreal

sections = unreal.ProjectSettingsService.list_ini_sections("DefaultEngine.ini")
for section in sections:
    print(section)
```

### list_ini_keys(section, config_file)

List all keys in a config section.

**Parameters:**
- `section` (str) - INI section (e.g., "/Script/Engine.Engine")
- `config_file` (str) - Config file name

**Returns:** `Array[str]` - Array of key names

**Example:**
```python
import unreal

keys = unreal.ProjectSettingsService.list_ini_keys(
    "/Script/Engine.Engine",
    "DefaultEngine.ini"
)
for key in keys:
    print(key)
```

### get_ini_value(section, key, config_file)

Get a value directly from an INI config file.

**Parameters:**
- `section` (str) - INI section (e.g., "/Script/Engine.Engine")
- `key` (str) - Key name within the section
- `config_file` (str) - Config file name (e.g., "DefaultEngine.ini")

**Returns:** `str` - Value as string (empty if not found)

**Example:**
```python
import unreal

engine_class = unreal.ProjectSettingsService.get_ini_value(
    "/Script/Engine.Engine",
    "GameEngine",
    "DefaultEngine.ini"
)
print(f"Game engine class: {engine_class}")
```

### set_ini_value(section, key, value, config_file)

Set a value directly in an INI config file.

**Parameters:**
- `section` (str) - INI section
- `key` (str) - Key name within the section
- `value` (str) - Value to set
- `config_file` (str) - Config file name

**Returns:** `FProjectSettingResult`

**Example:**
```python
import unreal

result = unreal.ProjectSettingsService.set_ini_value(
    "/Script/MyGame.MySettings",
    "CustomFeature",
    "True",
    "DefaultGame.ini"
)
```

### get_ini_array(section, key, config_file)

Get an array of values from an INI config file. Some INI keys have multiple values (e.g., +ActiveGameNameRedirects).

**Parameters:**
- `section` (str) - INI section
- `key` (str) - Key name within the section
- `config_file` (str) - Config file name

**Returns:** `Array[str]` - Array of values

**Example:**
```python
import unreal

redirects = unreal.ProjectSettingsService.get_ini_array(
    "/Script/Engine.Engine",
    "+ActiveGameNameRedirects",
    "DefaultEngine.ini"
)
for redirect in redirects:
    print(redirect)
```

### set_ini_array(section, key, values, config_file)

Set an array of values in an INI config file.

**Parameters:**
- `section` (str) - INI section
- `key` (str) - Key name within the section
- `values` (Array[str]) - Array of values to set
- `config_file` (str) - Config file name

**Returns:** `FProjectSettingResult`

**Example:**
```python
import unreal

values = [
    "(OldGameName=\"/Script/OldName\",NewGameName=\"/Script/NewName\")"
]

result = unreal.ProjectSettingsService.set_ini_array(
    "/Script/Engine.Engine",
    "+ActiveGameNameRedirects",
    values,
    "DefaultEngine.ini"
)
```

---

## Persistence

### save_all_config()

Force save all pending config changes to disk.

**Returns:** `bool` - True if successful

**Example:**
```python
import unreal

# Make multiple changes
unreal.ProjectSettingsService.set_setting("general", "ProjectName", "New Name")
unreal.ProjectSettingsService.set_setting("general", "CompanyName", "New Company")

# Save all changes
if unreal.ProjectSettingsService.save_all_config():
    print("All config files saved successfully")
```

### save_config(config_file)

Save a specific config file.

**Parameters:**
- `config_file` (str) - Config file name (e.g., "DefaultEngine.ini")

**Returns:** `bool` - True if successful

**Example:**
```python
import unreal

# Make changes to engine settings
unreal.ProjectSettingsService.set_ini_value(
    "/Script/Engine.Engine",
    "CustomKey",
    "CustomValue",
    "DefaultEngine.ini"
)

# Save just that file
if unreal.ProjectSettingsService.save_config("DefaultEngine.ini"):
    print("Engine config saved")
```

---

## Predefined Categories

### "general"
Project name, company, description, legal info
- **Config File:** DefaultGame.ini
- **Settings Class:** UGeneralProjectSettings

### "maps"
Default maps, game modes
- **Config File:** DefaultEngine.ini
- **Settings Class:** UGameMapsSettings

### "custom"
Use for direct INI access with `get_ini_value` / `set_ini_value`

---

## Common Config Files

- `DefaultEngine.ini` - Engine settings (rendering, physics, audio)
- `DefaultGame.ini` - Project/game settings (name, company, legal)
- `DefaultInput.ini` - Input bindings (legacy, use Enhanced Input instead)
- `DefaultEditor.ini` - Editor preferences
- `DefaultEditorPerProjectUserSettings.ini` - Per-user editor settings
