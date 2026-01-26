# Project Settings Workflows

Common patterns for managing project configuration.

---

## Workflow 1: Basic Project Info Setup

Set up project name, company, and description.

```python
import unreal

# Method 1: Individual settings
unreal.ProjectSettingsService.set_setting("general", "ProjectName", "My RPG Game")
unreal.ProjectSettingsService.set_setting("general", "CompanyName", "Indie Studios")
unreal.ProjectSettingsService.set_setting("general", "Description", "An epic fantasy adventure")

# Method 2: Batch update with JSON
import json

settings = {
    "ProjectName": "My RPG Game",
    "CompanyName": "Indie Studios",
    "Description": "An epic fantasy adventure",
    "CopyrightNotice": "Copyright 2026 Indie Studios"
}

result = unreal.ProjectSettingsService.set_category_settings_from_json(
    "general",
    json.dumps(settings)
)

if result.success:
    print(f"Updated {len(result.modified_settings)} settings")
    unreal.ProjectSettingsService.save_all_config()
```

---

## Workflow 2: Configure Default Maps

Set editor startup map and game default map.

```python
import unreal

# List current map settings
settings = unreal.ProjectSettingsService.list_settings("maps")
for s in settings:
    if "Map" in s.key:
        print(f"{s.key} = {s.value}")

# Set default maps
result = unreal.ProjectSettingsService.set_setting(
    "maps",
    "GameDefaultMap",
    "/Game/Maps/MainMenu.MainMenu"
)

if result.success:
    print("Default map set")
    unreal.ProjectSettingsService.save_config("DefaultEngine.ini")
```

---

## Workflow 3: Discover and Configure Custom Settings

Find UDeveloperSettings subclasses and configure them.

```python
import unreal

# Discover all settings classes
classes = unreal.ProjectSettingsService.discover_settings_classes()

# Filter for project-specific settings
project_classes = [c for c in classes if "MyGame" in c.class_name]

for cls in project_classes:
    print(f"\n{cls.class_name} ({cls.property_count} properties)")
    print(f"  File: {cls.config_file}")
    print(f"  Section: {cls.config_section}")
    
    # Get category ID (usually lowercase class name without U prefix)
    category_id = cls.class_name.lower().replace("u", "", 1)
    
    # List settings
    settings = unreal.ProjectSettingsService.list_settings(category_id)
    for s in settings:
        print(f"    {s.key} = {s.value}")
```

---

## Workflow 4: Advanced INI Manipulation

Direct INI access for scenarios not covered by predefined categories.

```python
import unreal

# List all sections in a config file
print("Sections in DefaultEngine.ini:")
sections = unreal.ProjectSettingsService.list_ini_sections("DefaultEngine.ini")
for section in sections:
    print(f"  [{section}]")

# Explore a specific section
section = "/Script/Engine.Engine"
print(f"\nKeys in {section}:")
keys = unreal.ProjectSettingsService.list_ini_keys(section, "DefaultEngine.ini")
for key in keys:
    value = unreal.ProjectSettingsService.get_ini_value(section, key, "DefaultEngine.ini")
    print(f"  {key} = {value}")

# Set a custom value
result = unreal.ProjectSettingsService.set_ini_value(
    "/Script/MyGame.CustomSettings",
    "EnableDebugMode",
    "True",
    "DefaultGame.ini"
)

if result.success:
    unreal.ProjectSettingsService.save_config("DefaultGame.ini")
```

---

## Workflow 5: Audit Project Configuration

Generate a report of all project settings.

```python
import unreal
import json

def audit_project_settings():
    """Generate comprehensive settings report"""
    
    report = {
        "categories": [],
        "config_files": {}
    }
    
    # Get all categories
    categories = unreal.ProjectSettingsService.list_categories()
    
    for cat in categories:
        category_data = {
            "id": cat.category_id,
            "name": cat.display_name,
            "description": cat.description,
            "config_file": cat.config_file,
            "settings": []
        }
        
        # Get all settings in category
        settings = unreal.ProjectSettingsService.list_settings(cat.category_id)
        for s in settings:
            category_data["settings"].append({
                "key": s.key,
                "value": s.value,
                "type": s.type,
                "default": s.default_value,
                "modified": s.value != s.default_value
            })
        
        report["categories"].append(category_data)
    
    # Save report
    report_json = json.dumps(report, indent=2)
    print(report_json)
    
    # Optionally save to file
    # with open("project_settings_audit.json", "w") as f:
    #     f.write(report_json)
    
    return report

audit_report = audit_project_settings()
```

---

## Workflow 6: Migrate Settings Between Projects

Copy settings from one project to another.

```python
import unreal
import json

def export_category_settings(category_id):
    """Export category settings as JSON"""
    settings_json = unreal.ProjectSettingsService.get_category_settings_as_json(category_id)
    return json.loads(settings_json)

def import_category_settings(category_id, settings_dict):
    """Import category settings from dict"""
    result = unreal.ProjectSettingsService.set_category_settings_from_json(
        category_id,
        json.dumps(settings_dict)
    )
    return result

# Export settings (run in source project)
general_settings = export_category_settings("general")
print(f"Exported: {json.dumps(general_settings, indent=2)}")

# Import settings (run in target project)
# result = import_category_settings("general", general_settings)
# if result.success:
#     unreal.ProjectSettingsService.save_all_config()
#     print("Settings imported successfully")
```

---

## Workflow 7: Validate Required Settings

Check that critical project settings are configured.

```python
import unreal

def validate_project_settings():
    """Ensure required settings are configured"""
    
    issues = []
    
    # Check project name
    project_name = unreal.ProjectSettingsService.get_setting("general", "ProjectName")
    if not project_name or project_name == "DefaultProject":
        issues.append("Project name not set")
    
    # Check company name
    company_name = unreal.ProjectSettingsService.get_setting("general", "CompanyName")
    if not company_name:
        issues.append("Company name not set")
    
    # Check default game map
    default_map = unreal.ProjectSettingsService.get_setting("maps", "GameDefaultMap")
    if not default_map or "None" in default_map:
        issues.append("Default game map not set")
    
    if issues:
        print("⚠️ Configuration issues found:")
        for issue in issues:
            print(f"  - {issue}")
        return False
    else:
        print("✓ Project settings validated")
        return True

validate_project_settings()
```

---

## Workflow 8: Configure Physics Settings

Modify physics-related settings via INI access.

```python
import unreal

# Get current physics settings
gravity = unreal.ProjectSettingsService.get_ini_value(
    "/Script/Engine.PhysicsSettings",
    "DefaultGravityZ",
    "DefaultEngine.ini"
)

print(f"Current gravity: {gravity}")

# Set custom gravity (Mars gravity is about -3.7 m/s²)
result = unreal.ProjectSettingsService.set_ini_value(
    "/Script/Engine.PhysicsSettings",
    "DefaultGravityZ",
    "-370.0",  # UE units: cm/s²
    "DefaultEngine.ini"
)

if result.success:
    print("Gravity updated to Mars levels")
    unreal.ProjectSettingsService.save_config("DefaultEngine.ini")
    
    if result.requires_restart:
        print("⚠️ Editor restart required for changes to take effect")
```

---

## Workflow 9: Batch Configure Multiple Categories

Set up a new project with all required settings in one go.

```python
import unreal
import json

def setup_new_project(config):
    """
    Configure multiple categories at once
    
    config = {
        "general": { "ProjectName": "...", ... },
        "maps": { "GameDefaultMap": "...", ... },
        "custom_ini": [
            {"section": "...", "key": "...", "value": "...", "file": "..."}
        ]
    }
    """
    
    results = {
        "success": True,
        "modified": [],
        "failed": []
    }
    
    # Update predefined categories
    for category, settings in config.items():
        if category == "custom_ini":
            continue
            
        result = unreal.ProjectSettingsService.set_category_settings_from_json(
            category,
            json.dumps(settings)
        )
        
        if result.success:
            results["modified"].extend(result.modified_settings)
        else:
            results["success"] = False
            results["failed"].extend(result.failed_settings)
    
    # Handle custom INI values
    if "custom_ini" in config:
        for ini_setting in config["custom_ini"]:
            result = unreal.ProjectSettingsService.set_ini_value(
                ini_setting["section"],
                ini_setting["key"],
                ini_setting["value"],
                ini_setting["file"]
            )
            
            if not result.success:
                results["success"] = False
                results["failed"].append(
                    f"{ini_setting['section']}.{ini_setting['key']}"
                )
    
    # Save all changes
    if results["success"]:
        unreal.ProjectSettingsService.save_all_config()
    
    return results

# Example usage
project_config = {
    "general": {
        "ProjectName": "Space Shooter",
        "CompanyName": "Indie Games Co",
        "Description": "A fast-paced space combat game"
    },
    "maps": {
        "GameDefaultMap": "/Game/Maps/MainMenu.MainMenu",
        "EditorStartupMap": "/Game/Maps/TestLevel.TestLevel"
    },
    "custom_ini": [
        {
            "section": "/Script/Engine.PhysicsSettings",
            "key": "DefaultGravityZ",
            "value": "0.0",  # Zero gravity!
            "file": "DefaultEngine.ini"
        }
    ]
}

result = setup_new_project(project_config)
print(f"Setup complete. Modified: {len(result['modified'])}, Failed: {len(result['failed'])}")
```
