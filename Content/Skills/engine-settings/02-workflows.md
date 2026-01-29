# EngineSettingsService Workflows

Common workflows for configuring Unreal Engine settings.

---

## Workflow 1: Enable Ray Tracing

Enable ray tracing with optimal settings for quality.

```python
import unreal
import json

# Step 1: Check current ray tracing status
rt_enabled = unreal.EngineSettingsService.get_console_variable("r.RayTracing")
print(f"Ray tracing currently: {'enabled' if rt_enabled == '1' else 'disabled'}")

# Step 2: Enable ray tracing and configure settings
rt_settings = {
    "r.RayTracing": "1",
    "r.RayTracing.Shadows": "1",
    "r.RayTracing.Reflections": "1",
    "r.RayTracing.GlobalIllumination": "1",
    "r.RayTracing.AmbientOcclusion": "1"
}

result = unreal.EngineSettingsService.set_console_variables_from_json(json.dumps(rt_settings))
print(f"Enabled {len(result.modified_settings)} ray tracing features")

# Step 3: Set reflection method to Lumen (works with RT)
unreal.EngineSettingsService.set_console_variable("r.ReflectionMethod", "1")

# Step 4: Save configuration
unreal.EngineSettingsService.save_all_engine_config()
print("Configuration saved - restart editor for full effect")
```

---

## Workflow 2: Optimize for Performance

Configure engine for maximum performance during development.

```python
import unreal
import json

# Step 1: Set overall scalability to Low
result = unreal.EngineSettingsService.set_overall_scalability_level(0)
print(f"Applied Low quality: {result.modified_settings}")

# Step 2: Disable expensive features
perf_settings = {
    "r.RayTracing": "0",
    "r.VolumetricFog": "0",
    "r.DepthOfFieldQuality": "0",
    "r.MotionBlurQuality": "0",
    "r.LensFlareQuality": "0",
    "r.SSR.Quality": "0",
    "r.Shadow.MaxResolution": "512"
}

result = unreal.EngineSettingsService.set_console_variables_from_json(json.dumps(perf_settings))
print(f"Disabled {len(result.modified_settings)} expensive features")

# Step 3: Reduce GC pressure
gc_settings = {
    "gc.TimeBetweenPurgingPendingKillObjects": "30.0"
}
unreal.EngineSettingsService.set_console_variables_from_json(json.dumps(gc_settings))

print("Performance mode enabled")
```

---

## Workflow 3: Configure Collision Profiles

Set up custom collision channels and profiles.

```python
import unreal

# Step 1: List current collision settings
settings = unreal.EngineSettingsService.list_settings("collision")
print(f"Found {len(settings)} collision settings")

# Step 2: View existing collision channels
for s in settings:
    if "Channel" in s.key or "Profile" in s.key:
        print(f"{s.key}: {s.value[:80]}...")

# Step 3: Add a custom collision channel via INI
# Note: Complex collision setups are better done in Project Settings UI
result = unreal.EngineSettingsService.set_engine_ini_value(
    "/Script/Engine.CollisionProfile",
    "+DefaultChannelResponses",
    "(Channel=ECC_GameTraceChannel2,Name=\"Interactable\",DefaultResponse=ECR_Ignore,bTraceType=False,bStaticObject=False)",
    "DefaultEngine.ini"
)
print(f"Added collision channel: {result.success}")
```

---

## Workflow 4: Discover and Document CVars

Find all cvars related to a feature for documentation.

```python
import unreal

# Step 1: Search for shadow-related cvars
shadows = unreal.EngineSettingsService.search_console_variables("shadow", 100)
print(f"Found {len(shadows)} shadow-related cvars\n")

# Step 2: Document each cvar
for cvar in shadows[:10]:  # First 10
    print(f"### {cvar.name}")
    print(f"- **Type**: {cvar.type}")
    print(f"- **Current**: {cvar.value}")
    print(f"- **Flags**: {cvar.flags}")
    if cvar.description:
        print(f"- **Description**: {cvar.description[:100]}...")
    print()

# Step 3: Export to JSON for documentation
import json
cvar_docs = []
for cvar in shadows:
    cvar_docs.append({
        "name": cvar.name,
        "type": cvar.type,
        "value": cvar.value,
        "description": cvar.description
    })
print(json.dumps(cvar_docs, indent=2))
```

---

## Workflow 5: Quality Preset Management

Create and apply custom quality presets.

```python
import unreal
import json

# Define quality presets
PRESETS = {
    "Ultra": {
        "ViewDistance": 4,
        "AntiAliasing": 4,
        "Shadow": 4,
        "GlobalIllumination": 4,
        "Reflection": 4,
        "PostProcess": 4,
        "Texture": 4,
        "Effects": 4,
        "Foliage": 4,
        "Shading": 4
    },
    "Balanced": {
        "ViewDistance": 3,
        "AntiAliasing": 3,
        "Shadow": 2,
        "GlobalIllumination": 2,
        "Reflection": 2,
        "PostProcess": 3,
        "Texture": 3,
        "Effects": 2,
        "Foliage": 2,
        "Shading": 3
    },
    "Performance": {
        "ViewDistance": 1,
        "AntiAliasing": 1,
        "Shadow": 1,
        "GlobalIllumination": 1,
        "Reflection": 1,
        "PostProcess": 1,
        "Texture": 2,
        "Effects": 1,
        "Foliage": 1,
        "Shading": 1
    }
}

def apply_preset(preset_name):
    """Apply a quality preset."""
    if preset_name not in PRESETS:
        print(f"Unknown preset: {preset_name}")
        return
    
    preset = PRESETS[preset_name]
    for group, level in preset.items():
        result = unreal.EngineSettingsService.set_scalability_level(group, level)
        if result.success:
            print(f"  {group}: {level}")
        else:
            print(f"  {group}: FAILED - {result.error_message}")

# Apply a preset
print("Applying 'Balanced' preset:")
apply_preset("Balanced")

# Verify current settings
current = unreal.EngineSettingsService.get_scalability_settings()
print(f"\nCurrent settings: {current}")
```

---

## Workflow 6: Physics Configuration

Configure physics simulation settings.

```python
import unreal

# Step 1: List physics settings
physics_settings = unreal.EngineSettingsService.list_settings("physics")
print(f"Found {len(physics_settings)} physics settings\n")

# Step 2: Display key settings
key_physics = ["DefaultGravityZ", "RagdollAggregateThreshold", "bEnableAsyncScene"]
for s in physics_settings:
    if any(key in s.key for key in key_physics) or len(physics_settings) < 20:
        print(f"{s.display_name}: {s.value} ({s.type})")

# Step 3: Find physics-related cvars
physics_cvars = unreal.EngineSettingsService.list_console_variables_with_prefix("p.", 20)
print(f"\nPhysics CVars (p.*): {len(physics_cvars)}")
for cvar in physics_cvars[:5]:
    print(f"  {cvar.name} = {cvar.value}")
```

---

## Workflow 7: Audio Configuration

Configure audio settings for optimal quality.

```python
import unreal
import json

# Step 1: List audio settings
audio_settings = unreal.EngineSettingsService.list_settings("audio")
print(f"Found {len(audio_settings)} audio settings")

# Step 2: Get audio-related cvars
audio_cvars = unreal.EngineSettingsService.search_console_variables("audio", 30)
print(f"Found {len(audio_cvars)} audio-related cvars")

# Step 3: Configure audio quality
audio_config = {
    "au.MaxChannels": "64",
    "au.3dVisualize.Enabled": "0"
}

for name, value in audio_config.items():
    cvar_info = unreal.ConsoleVariableInfo()
    if unreal.EngineSettingsService.get_console_variable_info(name, cvar_info):
        print(f"Setting {name} from {cvar_info.value} to {value}")
        unreal.EngineSettingsService.set_console_variable(name, value)
```

---

## Workflow 8: Export/Import Engine Configuration

Export and reimport engine settings for backup or sharing.

```python
import unreal
import json
import os

def export_engine_config(output_path):
    """Export all engine settings to JSON."""
    config = {}
    
    # Export each category
    categories = unreal.EngineSettingsService.list_categories()
    for cat in categories:
        if cat.category_id == "cvar":
            continue  # Skip cvar category (too large)
        
        settings = unreal.EngineSettingsService.list_settings(cat.category_id)
        if settings:
            config[cat.category_id] = {}
            for s in settings:
                config[cat.category_id][s.key] = s.value
    
    # Export scalability
    config["scalability"] = json.loads(unreal.EngineSettingsService.get_scalability_settings())
    
    # Export key cvars
    key_prefixes = ["r.Shadow", "r.RayTracing", "r.Reflection"]
    config["cvars"] = {}
    for prefix in key_prefixes:
        cvars = unreal.EngineSettingsService.list_console_variables_with_prefix(prefix, 50)
        for cvar in cvars:
            config["cvars"][cvar.name] = cvar.value
    
    with open(output_path, 'w') as f:
        json.dump(config, f, indent=2)
    
    print(f"Exported engine config to {output_path}")
    return config

def import_engine_config(input_path):
    """Import engine settings from JSON."""
    with open(input_path, 'r') as f:
        config = json.load(f)
    
    results = {"success": [], "failed": []}
    
    # Import cvars
    if "cvars" in config:
        for name, value in config["cvars"].items():
            result = unreal.EngineSettingsService.set_console_variable(name, str(value))
            if result.success:
                results["success"].append(name)
            else:
                results["failed"].append(f"{name}: {result.error_message}")
    
    print(f"Imported {len(results['success'])} settings, {len(results['failed'])} failed")
    return results

# Usage
# export_engine_config("E:/Backups/engine_config.json")
# import_engine_config("E:/Backups/engine_config.json")
```

---

## Workflow 9: Development vs Production Profiles

Switch between development and production settings.

```python
import unreal
import json

def apply_development_settings():
    """Optimize for development/iteration speed."""
    print("Applying DEVELOPMENT settings...")
    
    # Lower quality for faster iteration
    unreal.EngineSettingsService.set_overall_scalability_level(1)  # Medium
    
    # Disable expensive features
    dev_cvars = {
        "r.RayTracing": "0",
        "r.VolumetricCloud": "0",
        "r.VolumetricFog": "0",
        "r.ScreenPercentage": "75"
    }
    unreal.EngineSettingsService.set_console_variables_from_json(json.dumps(dev_cvars))
    
    print("Development settings applied")

def apply_production_settings():
    """Optimize for final quality."""
    print("Applying PRODUCTION settings...")
    
    # Maximum quality
    unreal.EngineSettingsService.set_overall_scalability_level(4)  # Cinematic
    
    # Enable all features
    prod_cvars = {
        "r.RayTracing": "1",
        "r.VolumetricCloud": "1",
        "r.VolumetricFog": "1",
        "r.ScreenPercentage": "100"
    }
    unreal.EngineSettingsService.set_console_variables_from_json(json.dumps(prod_cvars))
    
    # Save to config
    unreal.EngineSettingsService.save_all_engine_config()
    
    print("Production settings applied and saved")

# Usage
# apply_development_settings()
# apply_production_settings()
```
