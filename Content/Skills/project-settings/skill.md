---
name: project-settings
display_name: Project & Editor Settings
description: Configure Unreal Engine project settings AND editor preferences including UI appearance, toolbar icons, scale, colors, and all UDeveloperSettings subclasses
vibeue_classes:
  - ProjectSettingsService
unreal_classes:
  - EditorAssetLibrary
---

# Project Settings Skill

## Critical Rules

### ⚠️ `EditorStyleSettings` is NOT a Python Class

`EditorStyleSettings` is **not** discoverable via `discover_python_class('unreal.EditorStyleSettings')` — it will return `PYTHON_CLASS_NOT_FOUND`. It is only accessible as a **category string** through `ProjectSettingsService`:

```python
# ❌ WRONG — will fail
discover_python_class('unreal.EditorStyleSettings')  # NOT FOUND

# ✅ CORRECT — use as a category string
settings = unreal.ProjectSettingsService.list_settings("EditorStyleSettings")
result = unreal.ProjectSettingsService.set_setting("EditorStyleSettings", "AssetEditorOpenLocation", "MainWindow")
```

### ⚠️ Use Correct Category Names

| Category | Description |
|----------|-------------|
| `general` | Project info (name, company, description) |
| `maps` | Default maps and game modes |
| `custom` | Direct INI access |
| `EditorStyleSettings` | UI scale, toolbar icons, colors, asset editor open location |

### ⚠️ Map Paths Must Be Full Asset Paths

```python
# WRONG - folder path
"/Game/Variant_Horror.Variant_Horror"

# CORRECT - full map asset path  
"/Game/Variant_Horror/Lvl_Horror.Lvl_Horror"
```

Always verify with `does_asset_exist()` before setting.

### ⚠️ Always Check Operation Results

```python
result = unreal.ProjectSettingsService.set_setting("general", "ProjectName", "MyGame")
if not result.success:
    print(f"Failed: {result.error_message}")
```

---

## Workflows

### Basic Project Info Setup

```python
import unreal
import json

settings = {
    "ProjectName": "My RPG Game",
    "CompanyName": "Indie Studios",
    "Description": "An epic fantasy adventure"
}

result = unreal.ProjectSettingsService.set_category_settings_from_json("general", json.dumps(settings))
if result.success:
    unreal.ProjectSettingsService.save_all_config()
```

### Configure Default Maps

```python
import unreal

# set_setting auto-persists to the correct config file - no save_config needed
result = unreal.ProjectSettingsService.set_setting("maps", "GameDefaultMap", "/Game/Maps/MainMenu.MainMenu")
if result.success:
    print("Default map updated and saved")
```

### Set Editor Startup Map

```python
import unreal

# EditorStartupMap controls which level opens when the editor launches
# Use full asset path format: /Game/Path/To/Level.Level
result = unreal.ProjectSettingsService.set_setting("maps", "EditorStartupMap", "/Game/Levels/MyLevel.MyLevel")
if result.success:
    print("Editor startup map updated and saved")
```

### Discover Settings Classes

```python
import unreal

classes = unreal.ProjectSettingsService.discover_settings_classes()
for c in classes:
    print(f"{c.class_name}: {c.property_count} properties in {c.config_file}")
```

### List Settings in Category

```python
import unreal

settings = unreal.ProjectSettingsService.list_settings("general")
for s in settings:
    print(f"{s.key} = {s.value} ({s.type})")
```

### Find a Setting by Keyword (When You Don't Know the Category)

When you need to find a setting but don't know which category it belongs to, search across all discovered categories:

```python
import unreal

keyword = "AssetEditor"  # partial match on setting key or display_name
classes = unreal.ProjectSettingsService.discover_settings_classes()
for c in classes:
    settings = unreal.ProjectSettingsService.list_settings(c.class_name)
    for s in settings:
        if keyword.lower() in s.key.lower() or keyword.lower() in s.display_name.lower():
            print(f"{c.class_name} -> {s.key} = {s.value}")
```

**Use this workflow instead of guessing Python class names.** Many settings classes (e.g., `EditorStyleSettings`) are NOT exposed as Python classes — they are only accessible as category strings through `ProjectSettingsService`.

### Direct INI Access

```python
import unreal

# List sections
sections = unreal.ProjectSettingsService.list_ini_sections("DefaultEngine.ini")

# Get value
value = unreal.ProjectSettingsService.get_ini_value("/Script/Engine.Engine", "GameEngine", "DefaultEngine.ini")

# Set value
result = unreal.ProjectSettingsService.set_ini_value(
    "/Script/MyGame.CustomSettings",
    "EnableDebugMode",
    "True",
    "DefaultGame.ini"
)
```

---

## Data Structures

> **Python Naming Convention**: C++ types like `FProjectSettingInfo` are exposed as `ProjectSettingInfo` in Python (no `F` prefix).

### ProjectSettingInfo
- `key`, `display_name`, `description`
- `type`, `value`, `default_value`
- `config_section`, `config_file`
- `requires_restart`, `read_only`

### ProjectSettingResult
- `success`, `error_message`
- `modified_settings`, `failed_settings`
- `requires_restart`
