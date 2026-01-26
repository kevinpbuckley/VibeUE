# Project Settings Common Mistakes

---

## ❌ MISTAKE 1: Forgetting to Save Changes

**Wrong:**
```python
import unreal

# Set project name
unreal.ProjectSettingsService.set_setting("general", "ProjectName", "My Game")
# Changes not saved! Will be lost on editor restart
```

**Correct:**
```python
import unreal

# Set project name
result = unreal.ProjectSettingsService.set_setting("general", "ProjectName", "My Game")

if result.success:
    # MUST save to persist changes
    unreal.ProjectSettingsService.save_all_config()
    print("Changes saved successfully")
```

**Why:** Settings are cached in memory. You must explicitly save to write to disk.

---

## ❌ MISTAKE 2: Using Wrong Category Name

**Wrong:**
```python
import unreal

# Category "project" doesn't exist
value = unreal.ProjectSettingsService.get_setting("project", "ProjectName")
# Returns empty string
```

**Correct:**
```python
import unreal

# Use correct category: "general"
value = unreal.ProjectSettingsService.get_setting("general", "ProjectName")

# Or discover available categories first
categories = unreal.ProjectSettingsService.list_categories()
for cat in categories:
    print(f"Available: {cat.category_id}")
```

**Predefined categories:**
- `general` - Project info (name, company, description)
- `maps` - Default maps and game modes
- `custom` - Use for direct INI access

---

## ❌ MISTAKE 3: Not Checking Operation Results

**Wrong:**
```python
import unreal

# Blindly setting without checking
unreal.ProjectSettingsService.set_setting("general", "InvalidKey", "Value")
# Fails silently!
```

**Correct:**
```python
import unreal

result = unreal.ProjectSettingsService.set_setting("general", "InvalidKey", "Value")

if result.success:
    print("Setting updated")
else:
    print(f"Failed: {result.error_message}")
    print(f"Failed settings: {result.failed_settings}")
```

**Why:** Operations can fail for many reasons (invalid key, read-only setting, type mismatch). Always check the result.

---

## ❌ MISTAKE 4: Ignoring Restart Requirements

**Wrong:**
```python
import unreal

# Some settings require restart
result = unreal.ProjectSettingsService.set_setting("maps", "GameDefaultMap", "/Game/Maps/Main.Main")
unreal.ProjectSettingsService.save_all_config()
# Expecting immediate effect
```

**Correct:**
```python
import unreal

result = unreal.ProjectSettingsService.set_setting("maps", "GameDefaultMap", "/Game/Maps/Main.Main")

if result.success:
    unreal.ProjectSettingsService.save_all_config()
    
    if result.requires_restart:
        print("⚠️ Editor restart required for changes to take effect")
    else:
        print("Changes applied immediately")
```

**Why:** Some settings only take effect after editor restart. Check `requires_restart` flag.

---

## ❌ MISTAKE 5: Using Incorrect INI Section Format

**Wrong:**
```python
import unreal

# Missing /Script/ prefix
value = unreal.ProjectSettingsService.get_ini_value(
    "Engine.Engine",  # ❌ Wrong format
    "GameEngine",
    "DefaultEngine.ini"
)
```

**Correct:**
```python
import unreal

# Use full section path with /Script/ prefix
value = unreal.ProjectSettingsService.get_ini_value(
    "/Script/Engine.Engine",  # ✓ Correct format
    "GameEngine",
    "DefaultEngine.ini"
)
```

**Why:** INI sections use full paths. Use `list_ini_sections()` to see exact format.

---

## ❌ MISTAKE 6: Wrong Config File Name

**Wrong:**
```python
import unreal

# Project settings are not in DefaultEngine.ini
value = unreal.ProjectSettingsService.get_ini_value(
    "/Script/EngineSettings.GeneralProjectSettings",
    "ProjectName",
    "DefaultEngine.ini"  # ❌ Wrong file!
)
# Returns empty
```

**Correct:**
```python
import unreal

# Project settings are in DefaultGame.ini
value = unreal.ProjectSettingsService.get_ini_value(
    "/Script/EngineSettings.GeneralProjectSettings",
    "ProjectName",
    "DefaultGame.ini"  # ✓ Correct file
)

# Or use predefined category (recommended)
value = unreal.ProjectSettingsService.get_setting("general", "ProjectName")
```

**Common file mapping:**
- `DefaultGame.ini` - Project info, game settings
- `DefaultEngine.ini` - Engine settings, maps, rendering, physics
- `DefaultEditor.ini` - Editor preferences

---

## ❌ MISTAKE 7: Not Handling Array Values Correctly

**Wrong:**
```python
import unreal

# Trying to set array as single value
result = unreal.ProjectSettingsService.set_ini_value(
    "/Script/Engine.Engine",
    "+ActiveGameNameRedirects",
    "(OldGameName=\"/Script/OldName\",NewGameName=\"/Script/NewName\")",  # Only sets one
    "DefaultEngine.ini"
)
```

**Correct:**
```python
import unreal

# Use set_ini_array for array values
values = [
    "(OldGameName=\"/Script/OldName1\",NewGameName=\"/Script/NewName1\")",
    "(OldGameName=\"/Script/OldName2\",NewGameName=\"/Script/NewName2\")"
]

result = unreal.ProjectSettingsService.set_ini_array(
    "/Script/Engine.Engine",
    "+ActiveGameNameRedirects",
    values,
    "DefaultEngine.ini"
)
```

**Why:** INI keys starting with `+` or `-` are arrays. Use `get_ini_array()` / `set_ini_array()`.

---

## ❌ MISTAKE 8: Assuming Setting Exists

**Wrong:**
```python
import unreal

# Assuming key exists
value = unreal.ProjectSettingsService.get_setting("general", "CustomGameMode")
print(f"Game mode: {value}")  # Prints empty string!
```

**Correct:**
```python
import unreal

# List available settings first
settings = unreal.ProjectSettingsService.list_settings("general")
available_keys = [s.key for s in settings]

if "CustomGameMode" in available_keys:
    value = unreal.ProjectSettingsService.get_setting("general", "CustomGameMode")
    print(f"Game mode: {value}")
else:
    print("CustomGameMode not found in general category")
    print(f"Available: {available_keys}")
```

**Why:** Not all settings exist in all projects. List settings first or check result.

---

## ❌ MISTAKE 9: Invalid JSON for Batch Updates

**Wrong:**
```python
import unreal

# Invalid JSON
settings_json = "{ ProjectName: 'My Game' }"  # Single quotes, no quotes on key
result = unreal.ProjectSettingsService.set_category_settings_from_json(
    "general",
    settings_json
)
# Fails to parse
```

**Correct:**
```python
import unreal
import json

# Use json.dumps for valid JSON
settings = {
    "ProjectName": "My Game",
    "CompanyName": "Studio"
}

result = unreal.ProjectSettingsService.set_category_settings_from_json(
    "general",
    json.dumps(settings)  # Ensures valid JSON
)
```

**Why:** `set_category_settings_from_json()` expects valid JSON string. Use `json.dumps()`.

---

## ❌ MISTAKE 10: Modifying Read-Only Settings

**Wrong:**
```python
import unreal

# Some settings are read-only
result = unreal.ProjectSettingsService.set_setting("general", "EngineVersion", "5.7")
# Fails - EngineVersion is read-only
```

**Correct:**
```python
import unreal

# Check if setting is read-only first
info = unreal.ProjectSettingInfo()
found = unreal.ProjectSettingsService.get_setting_info("general", "EngineVersion", info)

if found:
    if info.read_only:
        print(f"Cannot modify {info.key} - it's read-only")
    else:
        result = unreal.ProjectSettingsService.set_setting("general", "EngineVersion", "5.7")
```

**Why:** Some settings are computed or engine-controlled. Check `read_only` flag.

---

## ❌ MISTAKE 11: Using discover_python_class on Wrong Type

**Wrong:**
```python
import unreal

# Trying to discover the service class
discover_python_class("unreal.ProjectSettingsService")
# Returns methods, not useful for understanding structs
```

**Correct:**
```python
import unreal

# Discover struct types for understanding return values
discover_python_class("unreal.ProjectSettingInfo")
discover_python_class("unreal.ProjectSettingCategory")
discover_python_class("unreal.ProjectSettingResult")

# Service methods are static - call them directly
categories = unreal.ProjectSettingsService.list_categories()
```

**Why:** Discovery is for understanding struct properties. Service methods are documented in skill.

---

## ❌ MISTAKE 12: Not Understanding Category Discovery

**Wrong:**
```python
import unreal

# Expecting all possible categories to exist
unreal.ProjectSettingsService.get_setting("rendering", "SomeSetting")
# May fail if no rendering settings configured
```

**Correct:**
```python
import unreal

# Discover available categories dynamically
categories = unreal.ProjectSettingsService.list_categories()
print("Available categories:")
for cat in categories:
    print(f"  {cat.category_id}: {cat.display_name}")

# Discover UDeveloperSettings subclasses
classes = unreal.ProjectSettingsService.discover_settings_classes()
for cls in classes:
    print(f"Found settings class: {cls.class_name}")
```

**Why:** Categories are discovered from UDeveloperSettings. Different projects have different settings classes.
