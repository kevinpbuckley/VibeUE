---
name: project-settings
display_name: Project & Editor Settings
description: Configure Unreal Engine project settings AND editor preferences including UI appearance, toolbar icons, scale, colors, and all UDeveloperSettings subclasses
vibeue_classes:
  - ProjectSettingsService
unreal_classes:
  - EditorAssetLibrary
  - EditorStyleSettings
---

# Project Settings Skill

## Critical Rules

### ⚠️ Use Correct Category Names

| Category | Description |
|----------|-------------|
| `general` | Project info (name, company, description) |
| `maps` | Default maps and game modes |
| `custom` | Direct INI access |
| `editorstylesettings` | UI scale, toolbar icons, colors |

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

result = unreal.ProjectSettingsService.set_setting("maps", "GameDefaultMap", "/Game/Maps/MainMenu.MainMenu")
if result.success:
    unreal.ProjectSettingsService.save_config("DefaultEngine.ini")
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

### FProjectSettingInfo
- `key`, `display_name`, `description`
- `type`, `value`, `default_value`
- `config_section`, `config_file`
- `requires_restart`, `read_only`

### FProjectSettingResult
- `success`, `error_message`
- `modified_settings`, `failed_settings`
- `requires_restart`
