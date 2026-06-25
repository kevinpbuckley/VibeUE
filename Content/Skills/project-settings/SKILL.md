---
name: project-settings
display_name: Project & Editor Settings
description: Configure Unreal Engine project settings and editor preferences — UI appearance, toolbar icons, scale, colors, and any UDeveloperSettings subclass (ProjectSettingsService). Use when the user asks to change a project setting or editor preference, configure a UDeveloperSettings category, or read/write project config values.
vibeue_classes:
  - ProjectSettingsService
unreal_classes:
  - EditorAssetLibrary
---

# Project Settings Skill

> 🔀 **Registered settings moved to the engine.** Category-based registered-setting access
> (`list_categories` / `list_settings` / `get_setting` / `set_setting` /
> `set_category_settings_from_json`) is now Unreal 5.8's native **`ConfigSettingsToolset`** — reach
> it with `call_tool` (run `describe_toolset` on `ConfigSettingsToolset` for its actions/params).
> VibeUE's `ProjectSettingsService` was trimmed to the delta the engine doesn't cover:
> **`discover_settings_classes` + direct INI read/write (`get_ini_value` / `set_ini_value` /
> `*_ini_array` / `list_ini_sections` / `list_ini_keys`) + config save**. Use those from
> `execute_python_code` as shown below.

## Critical Rules

### ⚠️ `EditorStyleSettings` is NOT a Python Class

`EditorStyleSettings` is **not** discoverable via `discover_python_class('unreal.EditorStyleSettings')` — it returns `PYTHON_CLASS_NOT_FOUND`. Reach it through the engine **`ConfigSettingsToolset`** as a settings category (via `call_tool`), or write its values directly via raw INI:

```python
# ❌ WRONG — will fail
discover_python_class('unreal.EditorStyleSettings')  # NOT FOUND

# ✅ CORRECT — engine ConfigSettingsToolset (registered settings) via call_tool, OR raw INI:
result = unreal.ProjectSettingsService.set_ini_value(
    "/Script/EditorStyle.EditorStyleSettings",
    "AssetEditorOpenLocation", "MainWindow", "EditorPerProjectUserSettings.ini")
```

### ⚠️ Category vs Raw INI

Named registered-setting categories (`general`, `maps`, `EditorStyleSettings`, etc.) are handled by
the engine **`ConfigSettingsToolset`**. From VibeUE you instead address settings by their INI
**section / key / file**:

| Need | Use |
|------|-----|
| Project info, maps, any registered category by name | engine `ConfigSettingsToolset` via `call_tool` |
| Read/write a known INI section+key | `ProjectSettingsService.get_ini_value` / `set_ini_value` |
| Discover which classes/sections exist | `ProjectSettingsService.discover_settings_classes()` |

### ⚠️ Map Paths Must Be Full Asset Paths

```python
# WRONG - folder path
"/Game/Variant_Horror.Variant_Horror"

# CORRECT - full map asset path  
"/Game/Variant_Horror/Lvl_Horror.Lvl_Horror"
```

Always verify with `does_asset_exist()` before setting.

### ⚠️ Always Check Operation Results

The INI write methods return an `FProjectSettingResult`:

```python
result = unreal.ProjectSettingsService.set_ini_value(
    "/Script/EngineSettings.GeneralProjectSettings", "ProjectName", "MyGame", "DefaultGame.ini")
if not result.success:
    print(f"Failed: {result.error_message}")
```

(Registered-setting writes via the engine `ConfigSettingsToolset` return their own result — check it the same way.)

---

## Workflows

### Basic Project Info Setup

Project info (name/company/description) is a registered category — set it via the engine
**`ConfigSettingsToolset`** (`call_tool`). Or write the INI keys directly from VibeUE:

```python
import unreal

sec  = "/Script/EngineSettings.GeneralProjectSettings"
file = "DefaultGame.ini"
unreal.ProjectSettingsService.set_ini_value(sec, "ProjectName",  "My RPG Game", file)
unreal.ProjectSettingsService.set_ini_value(sec, "CompanyName",  "Indie Studios", file)
unreal.ProjectSettingsService.set_ini_value(sec, "Description",  "An epic fantasy adventure", file)
unreal.ProjectSettingsService.save_all_config()
```

### Configure Default Maps

```python
import unreal

# Registered "maps" category → engine ConfigSettingsToolset via call_tool, OR raw INI:
result = unreal.ProjectSettingsService.set_ini_value(
    "/Script/EngineSettings.GameMapsSettings", "GameDefaultMap",
    "/Game/Maps/MainMenu.MainMenu", "DefaultEngine.ini")
if result.success:
    unreal.ProjectSettingsService.save_all_config()
    print("Default map updated and saved")
```

### Set Editor Startup Map

```python
import unreal

# EditorStartupMap controls which level opens when the editor launches.
# Registered setting → engine ConfigSettingsToolset (via call_tool), OR raw INI:
# Use full asset path format: /Game/Path/To/Level.Level
result = unreal.ProjectSettingsService.set_ini_value(
    "/Script/UnrealEd.EditorLoadingSavingSettings", "EditorStartupMap",
    "/Game/Levels/MyLevel.MyLevel", "EditorPerProjectUserSettings.ini")
if result.success:
    unreal.ProjectSettingsService.save_all_config()
    print("Editor startup map updated and saved")
```

> 💾 **If the level must be saved first** (untitled level, or "save as <name> and set as startup"): the editor world comes from `unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()` (NOT `LevelEditorSubsystem`, NOT deprecated `EditorLevelLibrary`), then `unreal.EditorLoadingAndSavingUtils.save_map(world, "/Game/Maps/MyLevel")`. Full workflow: load the `level-actors` skill → "Save the Current Level".

### Discover Settings Classes

```python
import unreal

classes = unreal.ProjectSettingsService.discover_settings_classes()
for c in classes:
    print(f"{c.class_name}: {c.property_count} properties in {c.config_file}")
```

### List Settings in a Category

Listing the settings inside a named category is an engine **`ConfigSettingsToolset`** action (via
`call_tool`). From VibeUE you can enumerate the raw INI keys of a known section instead:

```python
import unreal

sec  = "/Script/EngineSettings.GeneralProjectSettings"
file = "DefaultGame.ini"
for key in unreal.ProjectSettingsService.list_ini_keys(sec, file):
    print(f"{key} = {unreal.ProjectSettingsService.get_ini_value(sec, key, file)}")
```

### Find a Setting (When You Don't Know the Section)

`discover_settings_classes()` maps each settings class to its INI **section + file**, so you can
locate a setting without guessing Python class names, then read its keys via raw INI:

```python
import unreal

keyword = "AssetEditor"  # partial match on class/section name
for c in unreal.ProjectSettingsService.discover_settings_classes():
    if keyword.lower() in c.class_name.lower() or keyword.lower() in c.config_section.lower():
        print(f"{c.class_name} -> section {c.config_section} in {c.config_file}")
        for key in unreal.ProjectSettingsService.list_ini_keys(c.config_section, c.config_file):
            print(f"   {key}")
```

**Use `discover_settings_classes()` instead of guessing Python class names.** Many settings classes
(e.g., `EditorStyleSettings`) are NOT exposed as Python classes — edit them through the engine
`ConfigSettingsToolset` or via their INI section.

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

> **Python Naming Convention**: C++ types like `FProjectSettingResult` are exposed as `ProjectSettingResult` in Python (no `F` prefix).

### SettingsClassInfo (from `discover_settings_classes()`)
- `class_name`, `class_path`
- `config_section`, `config_file`
- `property_count`, `is_developer_settings`

### ProjectSettingResult (from INI write methods)
- `success`, `error_message`
- `modified_settings`, `failed_settings`
- `requires_restart`

## Sample scripts (run via `execute_python_code`)

- **`scripts/configure_setting.txt`** — discover settings classes and read/write a project setting.
