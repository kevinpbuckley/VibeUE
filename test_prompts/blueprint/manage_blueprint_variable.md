# manage_blueprint_variable Test Prompts

## Prerequisites
- ✅ Unreal Engine 5.6+ running
- ✅ VibeUE plugin loaded
- ✅ MCP connection active
- ✅ Test Blueprint available

## Overview
Tests all 7 actions of `manage_blueprint_variable` with emphasis on type discovery workflow and complex object type handling.

## Test 1: Variable Type Discovery Workflow

**Purpose**: Demonstrate CRITICAL search_types → create workflow

### Steps

1. **Search for UserWidget Type**
   ```
   Search for variable types containing "UserWidget"
   ```

2. **Examine Type Path**
   ```
   Note the exact type_path returned (e.g., "/Script/UMG.UserWidget")
   ```

3. **Create Blueprint**
   ```
   Create BP_VariableTest with Actor parent
   ```

4. **Create Variable with Discovered Type**
   ```
   Create variable "AttributeWidget" using exact type_path from step 2:
   - variable_name: "AttributeWidget"
   - variable_config: {
       "type_path": "/Script/UMG.UserWidget",
       "category": "UI",
       "is_editable": true
     }
   ```

5. **Verify Creation**
   ```
   List variables to confirm AttributeWidget was created with correct type
   ```

### Expected Outcomes
- ✅ search_types returns UserWidget with type_path
- ✅ create accepts type_path in variable_config
- ✅ Variable created with correct object type
- ✅ list shows AttributeWidget with UserWidget type

### Critical Note
⚠️ **MUST use "type_path" key in variable_config**
- ✅ CORRECT: `{"type_path": "/Script/UMG.UserWidget"}`
- ❌ WRONG: `{"type": "UserWidget"}` or `{"type": "float"}`

---

## Test 2: Primitive Type Variables

**Purpose**: Create variables with basic primitive types

### Steps

1. **Create Float Variable**
   ```
   Create variable "Health" with type_path "/Script/CoreUObject.FloatProperty"
   ```

2. **Create Int Variable**
   ```
   Create variable "MaxHealth" with type_path "/Script/CoreUObject.IntProperty"
   ```

3. **Create Bool Variable**
   ```
   Create variable "IsAlive" with type_path "/Script/CoreUObject.BoolProperty"
   ```

4. **Create String Variable**
   ```
   Create variable "PlayerName" with type_path "/Script/CoreUObject.StrProperty"
   ```

5. **List All Variables**
   ```
   Verify all primitive variables created correctly
   ```

### Expected Outcomes
- ✅ Float variable accepts float type_path
- ✅ Int variable accepts int type_path
- ✅ Bool variable accepts bool type_path  
- ✅ String variable accepts string type_path
- ✅ All appear in variable list with correct types

---

## Test 3: Complex Type Variables

**Purpose**: Create variables with complex object/struct types

### Steps

1. **Search for Niagara Types**
   ```
   Search types with search_text="NiagaraSystem"
   ```

2. **Create Niagara Variable**
   ```
   Create variable "DeathEffect" with NiagaraSystem type_path from search
   ```

3. **Search for Audio Types**
   ```
   Search types with search_text="SoundCue"
   ```

4. **Create Audio Variable**
   ```
   Create variable "JumpSound" with SoundCue type_path
   ```

5. **Verify Complex Types**
   ```
   List variables and get_info on DeathEffect and JumpSound
   ```

### Expected Outcomes
- ✅ search_types finds Niagara and Audio types
- ✅ Complex object types create successfully
- ✅ get_info shows full type information
- ✅ Variables ready for asset assignment

---

## Test 4: Variable Metadata Management

**Purpose**: Set variable categories, tooltips, and flags

### Steps

1. **Create Variable with Metadata**
   ```
   Create variable "Health" with variable_config:
   {
     "type_path": "/Script/CoreUObject.FloatProperty",
     "category": "Combat",
     "tooltip": "Current health points",
     "is_editable": true,
     "is_blueprint_readonly": false
   }
   ```

2. **Get Variable Info**
   ```
   Get detailed info about Health variable
   ```

3. **Update Category**
   ```
   Use set_property to change category to "Stats|Combat"
   ```

4. **Set as Read-Only**
   ```
   Update is_blueprint_readonly to true
   ```

5. **Verify Changes**
   ```
   Get info again to verify metadata updates
   ```

### Expected Outcomes
- ✅ category organizes variables in Blueprint editor
- ✅ tooltip shows in editor hover
- ✅ is_editable controls instance editability
- ✅ is_blueprint_readonly prevents Blueprint modification
- ✅ Metadata persists after setting

---

## Test 5: Variable Default Values

**Purpose**: Set and get default values for variables

### Steps

1. **Set Float Default**
   ```
   Set default value for Health variable to 100.0
   ```

2. **Set Bool Default**
   ```
   Set default value for IsAlive to true
   ```

3. **Get Property Values**
   ```
   Get default value of Health and IsAlive
   ```

4. **Set String Default**
   ```
   Set PlayerName default to "TestPlayer"
   ```

### Expected Outcomes
- ✅ set_property updates default values
- ✅ get_property retrieves current defaults
- ✅ Different types handle values correctly
- ✅ Defaults persist in Blueprint

---

## Test 6: Variable Listing and Filtering

**Purpose**: Test list action with various criteria

### Steps

1. **List All Variables**
   ```
   List all variables in BP_VariableTest
   ```

2. **List by Category**
   ```
   Use list_criteria to filter by category="Combat"
   ```

3. **List by Name Pattern**
   ```
   Use list_criteria with name_contains="Health"
   ```

4. **List with Metadata**
   ```
   List with include_metadata=true
   ```

### Expected Outcomes
- ✅ list returns all variables
- ✅ Category filter shows only Combat variables
- ✅ Name filter finds variables containing "Health"
- ✅ include_metadata shows full details

---

## Test 7: Variable Deletion

**Purpose**: Remove variables and verify cleanup

### Steps

1. **Delete Single Variable**
   ```
   Delete the JumpSound variable
   ```

2. **Verify Removal**
   ```
   List variables - JumpSound should be gone
   ```

3. **Delete Multiple Variables**
   ```
   Delete Health, MaxHealth, IsAlive
   ```

4. **Final Verification**
   ```
   List to confirm only desired variables remain
   ```

### Expected Outcomes
- ✅ delete removes variable
- ✅ Deleted variable not in subsequent lists
- ✅ Blueprint remains valid
- ✅ Can delete multiple variables

---

## Test 8: Blueprint Class Variables

**Purpose**: Create variables referencing custom Blueprint classes

### Steps

1. **Search for Blueprint Asset**
   ```
   Use manage_asset search to find BP_Enemy Blueprint
   ```

2. **Get Package Path**
   ```
   Note exact package path with _C suffix:
   /Game/Blueprints/BP_Enemy.BP_Enemy_C
   ```

3. **Create Blueprint Variable**
   ```
   Create variable "Target" with type_path from step 2
   ```

4. **Verify Blueprint Type**
   ```
   Get info on Target - should show Blueprint class type
   ```

### Expected Outcomes
- ✅ Blueprint class variables use package_path + "_C" suffix
- ✅ type_path format: "/Game/Path/BP_Name.BP_Name_C"
- ✅ Variable accepts Blueprint class reference
- ✅ Shows as Blueprint type in editor

---

## Common Type Paths Reference

### Primitives
```
float:  "/Script/CoreUObject.FloatProperty"
int:    "/Script/CoreUObject.IntProperty"
bool:   "/Script/CoreUObject.BoolProperty"
string: "/Script/CoreUObject.StrProperty"
name:   "/Script/CoreUObject.NameProperty"
byte:   "/Script/CoreUObject.ByteProperty"
```

### Common Objects
```
Widget:        "/Script/UMG.UserWidget"
NiagaraSystem: "/Script/Niagara.NiagaraSystem"
SoundCue:      "/Script/Engine.SoundCue"
Material:      "/Script/Engine.Material"
Texture2D:     "/Script/Engine.Texture2D"
Actor:         "/Script/Engine.Actor"
```

### Blueprint Classes
```
Format: "/Game/Path/To/BP_Name.BP_Name_C"
Example: "/Game/Blueprints/Characters/BP_Player.BP_Player_C"
```

---

## Reference: All Actions Summary

| Action | Purpose | Key Parameters |
|--------|---------|----------------|
| **search_types** | Discover variable types | search_criteria with search_text, category |
| **get_info** | Get variable details | blueprint_name, variable_name |
| **create** | Create variable | blueprint_name, variable_name, variable_config with **type_path** |
| **get_property** | Get variable value/metadata | blueprint_name, variable_name, property_path |
| **set_property** | Set variable value/metadata | blueprint_name, variable_name, property_path, value |
| **list** | List variables | blueprint_name, list_criteria |
| **delete** | Remove variable | blueprint_name, variable_name |

---

**Test Coverage**: 7/7 actions tested ✅  
**Last Updated**: November 3, 2025  
**Related Issues**: #69, #73
