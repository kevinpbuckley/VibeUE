# Test Prompts for manage_blueprint_variable

This file contains test prompts for all 7 actions of the `manage_blueprint_variable` tool.

## Overview

The `manage_blueprint_variable` tool provides unified variable management for Blueprint assets with 7 actions:

1. **search_types** - Discover available variable types
2. **get_info** - Get variable information
3. **create** - Create new variable
4. **get_property** - Get variable metadata
5. **set_property** - Set variable value/metadata
6. **list** - List all variables
7. **delete** - Delete variable

## Test Workflow

This workflow demonstrates best practices for using the tool, including the CRITICAL type path discovery pattern.

### Setup: Create Test Blueprint

First, create a Blueprint to work with:

```python
# Create a test Blueprint
manage_blueprint(
    action="create",
    blueprint_name="BP_VariableTest",
    parent_class="Actor"
)
```

---

## Action 1: search_types - Discover Available Variable Types

**Purpose**: Find the correct `type_path` for creating variables. This is CRITICAL before creating complex type variables.

### Test 1a: Search for Widget Types

```python
# Search for UserWidget types
manage_blueprint_variable(
    blueprint_name="BP_VariableTest",
    action="search_types",
    search_criteria={
        "search_text": "UserWidget",
        "category": "Widget",
        "include_blueprints": True,
        "include_engine_types": True
    }
)
```

**Expected Result**:
- Returns list of widget types
- Includes `/Script/UMG.UserWidget` with full type_path
- Shows both engine types and Blueprint widget classes

### Test 1b: Search for Audio Types

```python
# Search for audio/sound types
manage_blueprint_variable(
    blueprint_name="BP_VariableTest",
    action="search_types",
    search_criteria={
        "search_text": "Sound",
        "category": "Audio",
        "include_engine_types": True
    }
)
```

**Expected Result**:
- Returns audio types like `/Script/Engine.SoundBase`, `/Script/Engine.SoundCue`
- Filtered to Audio category only

### Test 1c: Search All Available Types

```python
# Browse all primitive types
manage_blueprint_variable(
    blueprint_name="BP_VariableTest",
    action="search_types",
    search_criteria={
        "category": "Basic",
        "include_engine_types": True
    }
)
```

**Expected Result**:
- Returns basic types: FloatProperty, IntProperty, BoolProperty, etc.
- Each with canonical type_path

---

## Action 2: create - Create New Variables

**CRITICAL**: MUST use `type_path` (NOT `type`) in variable_config.

### Test 2a: Create Float Variable

```python
# Create a simple float variable for Health
manage_blueprint_variable(
    blueprint_name="BP_VariableTest",
    action="create",
    variable_name="Health",
    variable_config={
        "type_path": "/Script/CoreUObject.FloatProperty",
        "category": "Combat",
        "tooltip": "Current health of the actor",
        "is_editable": True,
        "default_value": 100.0
    }
)
```

**Expected Result**:
- Variable "Health" created successfully
- Shows in "Combat" category
- Default value set to 100.0

### Test 2b: Create UserWidget Variable (With Type Discovery)

**CRITICAL WORKFLOW**: First discover the type_path, then create the variable.

```python
# Step 1: Search for the correct type_path
type_search = manage_blueprint_variable(
    blueprint_name="BP_VariableTest",
    action="search_types",
    search_criteria={
        "search_text": "UserWidget"
    }
)
# Result contains: type_path="/Script/UMG.UserWidget"

# Step 2: Create variable using exact type_path from search
manage_blueprint_variable(
    blueprint_name="BP_VariableTest",
    action="create",
    variable_name="AttributeWidget",
    variable_config={
        "type_path": "/Script/UMG.UserWidget",  # ✅ CORRECT: Use "type_path"
        "category": "UI",
        "tooltip": "Player's attribute display widget",
        "is_editable": True,
        "is_expose_on_spawn": True,
        "default_value": None
    }
)
```

**Expected Result**:
- Variable "AttributeWidget" created successfully
- Type shows as UserWidget
- Exposed on spawn
- Shows in "UI" category

### Test 2c: Create Boolean Variable

```python
# Create a boolean flag
manage_blueprint_variable(
    blueprint_name="BP_VariableTest",
    action="create",
    variable_name="IsAlive",
    variable_config={
        "type_path": "/Script/CoreUObject.BoolProperty",
        "category": "State",
        "tooltip": "Whether the actor is alive",
        "is_editable": True,
        "default_value": True
    }
)
```

**Expected Result**:
- Boolean variable created
- Default value is true

### Test 2d: Create Integer Variable

```python
# Create an integer variable for ammo count
manage_blueprint_variable(
    blueprint_name="BP_VariableTest",
    action="create",
    variable_name="AmmoCount",
    variable_config={
        "type_path": "/Script/CoreUObject.IntProperty",
        "category": "Combat",
        "tooltip": "Current ammunition count",
        "is_editable": True,
        "is_blueprint_readonly": False,
        "default_value": 30
    }
)
```

**Expected Result**:
- Integer variable created
- Default value is 30
- Editable in Blueprint

### Test 2e: Create Sound Variable (With Type Discovery)

```python
# Step 1: Find sound types
sound_types = manage_blueprint_variable(
    blueprint_name="BP_VariableTest",
    action="search_types",
    search_criteria={
        "search_text": "SoundBase",
        "category": "Audio"
    }
)

# Step 2: Create sound variable
manage_blueprint_variable(
    blueprint_name="BP_VariableTest",
    action="create",
    variable_name="HitSound",
    variable_config={
        "type_path": "/Script/Engine.SoundBase",
        "category": "Audio",
        "tooltip": "Sound played when actor is hit",
        "is_editable": True,
        "default_value": None
    }
)
```

**Expected Result**:
- Sound variable created
- Accepts SoundBase or derived types (SoundCue, SoundWave)

---

## Action 3: list - List All Variables

**Purpose**: View all variables in a Blueprint with optional filtering.

### Test 3a: List All Variables

```python
# List all variables in the Blueprint
manage_blueprint_variable(
    blueprint_name="BP_VariableTest",
    action="list",
    list_criteria={
        "include_private": True,
        "include_metadata": True
    }
)
```

**Expected Result**:
- Returns all created variables: Health, AttributeWidget, IsAlive, AmmoCount, HitSound
- Includes type information, categories, and metadata

### Test 3b: Filter Variables by Category

```python
# List only Combat variables
manage_blueprint_variable(
    blueprint_name="BP_VariableTest",
    action="list",
    list_criteria={
        "category": "Combat",
        "include_metadata": False
    }
)
```

**Expected Result**:
- Returns only: Health, AmmoCount
- No metadata included

### Test 3c: Filter Variables by Name

```python
# List variables with "Health" in name
manage_blueprint_variable(
    blueprint_name="BP_VariableTest",
    action="list",
    list_criteria={
        "name_contains": "Health",
        "include_private": False
    }
)
```

**Expected Result**:
- Returns only: Health
- Excludes private variables

---

## Action 4: get_info - Get Variable Information

**Purpose**: Get detailed information about a specific variable.

### Test 4a: Get Health Variable Info

```python
# Get detailed info for Health variable
manage_blueprint_variable(
    blueprint_name="BP_VariableTest",
    action="get_info",
    variable_name="Health",
    info_options={
        "include_metadata": True,
        "include_default_value": True
    }
)
```

**Expected Result**:
- Returns variable type: FloatProperty
- Returns category: Combat
- Returns default value: 100.0
- Returns tooltip and other metadata

### Test 4b: Get AttributeWidget Variable Info

```python
# Get detailed info for widget variable
manage_blueprint_variable(
    blueprint_name="BP_VariableTest",
    action="get_info",
    variable_name="AttributeWidget",
    info_options={
        "include_metadata": True
    }
)
```

**Expected Result**:
- Returns type_path: /Script/UMG.UserWidget
- Returns is_expose_on_spawn: True
- Returns category: UI

---

## Action 5: get_property - Get Variable Property Values

**Purpose**: Get specific property values from variables.

### Test 5a: Get Health Default Value

```python
# Get the default value of Health
manage_blueprint_variable(
    blueprint_name="BP_VariableTest",
    action="get_property",
    variable_name="Health",
    property_path="default_value"
)
```

**Expected Result**:
- Returns: 100.0

### Test 5b: Get Category Property

```python
# Get the category of AmmoCount
manage_blueprint_variable(
    blueprint_name="BP_VariableTest",
    action="get_property",
    variable_name="AmmoCount",
    property_path="category"
)
```

**Expected Result**:
- Returns: "Combat"

### Test 5c: Get IsEditable Property

```python
# Check if AttributeWidget is editable
manage_blueprint_variable(
    blueprint_name="BP_VariableTest",
    action="get_property",
    variable_name="AttributeWidget",
    property_path="is_editable"
)
```

**Expected Result**:
- Returns: True

---

## Action 6: set_property - Set Variable Property Values

**Purpose**: Modify variable properties and default values.

### Test 6a: Set Health Default Value

```python
# Update Health default value
manage_blueprint_variable(
    blueprint_name="BP_VariableTest",
    action="set_property",
    variable_name="Health",
    property_path="default_value",
    value=150.0
)
```

**Expected Result**:
- Default value updated to 150.0
- Variable still works correctly

### Test 6b: Update Variable Category

```python
# Change AmmoCount category
manage_blueprint_variable(
    blueprint_name="BP_VariableTest",
    action="set_property",
    variable_name="AmmoCount",
    property_path="category",
    value="Weapons"
)
```

**Expected Result**:
- AmmoCount now appears in "Weapons" category

### Test 6c: Update Tooltip

```python
# Update tooltip for IsAlive
manage_blueprint_variable(
    blueprint_name="BP_VariableTest",
    action="set_property",
    variable_name="IsAlive",
    property_path="tooltip",
    value="Indicates whether the actor is currently alive or dead"
)
```

**Expected Result**:
- Tooltip updated successfully
- Visible in editor

### Test 6d: Set Expose on Spawn

```python
# Make Health editable on spawn
manage_blueprint_variable(
    blueprint_name="BP_VariableTest",
    action="set_property",
    variable_name="Health",
    property_path="is_expose_on_spawn",
    value=True
)
```

**Expected Result**:
- Health now exposed on spawn
- Shows in spawn actor nodes

---

## Action 7: delete - Delete Variable

**Purpose**: Remove variables from Blueprint.

### Test 7a: Delete Single Variable

```python
# Delete the IsAlive variable
manage_blueprint_variable(
    blueprint_name="BP_VariableTest",
    action="delete",
    variable_name="IsAlive",
    delete_options={
        "check_references": True
    }
)
```

**Expected Result**:
- IsAlive variable deleted
- Warning if variable was referenced anywhere

### Test 7b: Delete with Reference Check

```python
# Try to delete Health (may have references)
manage_blueprint_variable(
    blueprint_name="BP_VariableTest",
    action="delete",
    variable_name="Health",
    delete_options={
        "check_references": True,
        "force": False
    }
)
```

**Expected Result**:
- If Health is referenced: Returns error with reference locations
- If no references: Deletes successfully

---

## Complete Test Workflow Summary

This demonstrates a complete workflow using all 7 actions in a realistic scenario:

```python
# 1. SEARCH_TYPES: Find UserWidget type
type_result = manage_blueprint_variable(
    blueprint_name="BP_VariableTest",
    action="search_types",
    search_criteria={"search_text": "UserWidget"}
)

# 2. CREATE: Create float variable
manage_blueprint_variable(
    blueprint_name="BP_VariableTest",
    action="create",
    variable_name="Health",
    variable_config={
        "type_path": "/Script/CoreUObject.FloatProperty",
        "category": "Combat",
        "default_value": 100.0
    }
)

# 3. CREATE: Create UserWidget variable using discovered type_path
manage_blueprint_variable(
    blueprint_name="BP_VariableTest",
    action="create",
    variable_name="AttributeWidget",
    variable_config={
        "type_path": "/Script/UMG.UserWidget",
        "category": "UI",
        "is_editable": True
    }
)

# 4. LIST: Verify variables created
list_result = manage_blueprint_variable(
    blueprint_name="BP_VariableTest",
    action="list",
    list_criteria={"include_metadata": True}
)

# 5. GET_INFO: Get detailed variable information
info_result = manage_blueprint_variable(
    blueprint_name="BP_VariableTest",
    action="get_info",
    variable_name="Health"
)

# 6. GET_PROPERTY: Check current default value
current_value = manage_blueprint_variable(
    blueprint_name="BP_VariableTest",
    action="get_property",
    variable_name="Health",
    property_path="default_value"
)

# 7. SET_PROPERTY: Update default value
manage_blueprint_variable(
    blueprint_name="BP_VariableTest",
    action="set_property",
    variable_name="Health",
    property_path="default_value",
    value=150.0
)

# 8. DELETE: Remove a variable
manage_blueprint_variable(
    blueprint_name="BP_VariableTest",
    action="delete",
    variable_name="AttributeWidget",
    delete_options={"check_references": True}
)
```

---

## Common Type Paths Reference

For quick reference when creating variables:

### Primitive Types
```python
"/Script/CoreUObject.FloatProperty"    # float
"/Script/CoreUObject.IntProperty"      # int
"/Script/CoreUObject.BoolProperty"     # bool
"/Script/CoreUObject.StrProperty"      # string
"/Script/CoreUObject.NameProperty"     # name
"/Script/CoreUObject.ByteProperty"     # byte
"/Script/CoreUObject.DoubleProperty"   # double
```

### Common Object Types
```python
"/Script/UMG.UserWidget"                      # UI Widgets
"/Script/Engine.Actor"                        # Actors
"/Script/Engine.Character"                    # Characters
"/Script/Engine.SoundBase"                    # Sounds
"/Script/Engine.SoundCue"                     # Sound Cues
"/Script/Engine.Material"                     # Materials
"/Script/Engine.MaterialInstance"             # Material Instances
"/Script/Engine.Texture2D"                    # Textures
"/Script/Niagara.NiagaraSystem"              # Niagara VFX
"/Script/Niagara.NiagaraComponent"           # Niagara Components
```

### Blueprint Classes
```python
# Format: /Game/Path/BP_Name.BP_Name_C
"/Game/Blueprints/UI/BP_MicrosubHUD.BP_MicrosubHUD_C"
```

---

## Critical Best Practices

1. **ALWAYS use search_types first** for complex object types to get the correct type_path
2. **NEVER use "type" key** - always use "type_path" in variable_config
3. **Use full canonical paths** - e.g., "/Script/UMG.UserWidget" not just "UserWidget"
4. **Check references before deleting** - use delete_options with check_references: True
5. **List variables before bulk operations** - verify current state first
6. **Use categories** - organize variables for better Blueprint editor experience

---

## Validation Checklist

- ✅ All 7 actions tested
- ✅ Type path discovery workflow demonstrated
- ✅ Primitive types tested (float, int, bool)
- ✅ Object types tested (UserWidget, SoundBase)
- ✅ Variable metadata manipulation verified
- ✅ List with filtering demonstrated
- ✅ Property get/set operations shown
- ✅ Delete with reference checking included
