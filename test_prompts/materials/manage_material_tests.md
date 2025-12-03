# manage_material Test Prompts

## Prerequisites
- ✅ Unreal Engine 5.7+ running
- ✅ VibeUE plugin loaded
- ✅ MCP connection active

## 🚨 IMPORTANT: Test Asset Management

**DO NOT delete test assets until after reviewing ALL test results!**

### Setup: Verify Test Material Exists

**Run these commands at the START of testing:**

1. **Search for Existing Test Material**
   ```
   Use manage_asset with action="search":
   - search_term: "M_MaterialTest"
   - asset_type: "Material"
   ```

2. **If Not Found, Create Test Material**
   ```
   Use manage_material with action="create":
   - destination_path: "/Game/Materials/test"
   - material_name: "M_MaterialTest"
   ```

3. **Open Material in Editor**
   ```
   Use manage_asset with action="open_in_editor":
   - asset_path: "/Game/Materials/test/M_MaterialTest"
   - This opens the Material Editor so you can see changes in real-time
   ```

**💡 TIP**: Keep the Material Editor open throughout testing to watch changes appear in real-time!

---

## Test 1: Create Material

**Purpose**: Create a new material asset

### Steps

```
Use manage_material with action="create":
- destination_path: "/Game/Materials/test"
- material_name: "M_TestCreate"

Then open it:
Use manage_asset with action="open_in_editor":
- asset_path: "/Game/Materials/test/M_TestCreate"
```

### Expected Outcomes
- ✅ Material created at /Game/Materials/test/M_TestCreate
- ✅ Material opens in editor
- ✅ Default material appearance (grey surface)

---

## Test 2: Get Material Info

**Purpose**: Retrieve comprehensive material information

### Steps

```
Use manage_material with action="get_info":
- material_path: "/Game/Materials/test/M_MaterialTest"
```

### Expected Outcomes
- ✅ Returns material domain (Surface)
- ✅ Returns blend mode (Opaque)
- ✅ Returns shading model (DefaultLit)
- ✅ Lists any existing expressions and parameters

---

## Test 3: List Material Properties

**Purpose**: Discover all editable material properties

### Steps

```
Use manage_material with action="list_properties":
- material_path: "/Game/Materials/test/M_MaterialTest"
- include_advanced: true
```

### Expected Outcomes
- ✅ Lists TwoSided, BlendMode, ShadingModel, etc.
- ✅ Shows property types (bool, enum, float)
- ✅ Shows current values
- ✅ Shows allowed values for enums

---

## Test 4: Get Single Property

**Purpose**: Get a specific property value

### Steps

```
Use manage_material with action="get_property":
- material_path: "/Game/Materials/test/M_MaterialTest"
- property_name: "TwoSided"
```

### Expected Outcomes
- ✅ Returns property value (false by default)
- ✅ Shows property type

---

## Test 5: Set Single Property

**Purpose**: Modify a material property

### Steps

```
Use manage_material with action="set_property":
- material_path: "/Game/Materials/test/M_MaterialTest"
- property_name: "TwoSided"
- value: "true"

👀 Watch the Material Editor - the "Two Sided" checkbox should become checked!
```

### Expected Outcomes
- ✅ Property updated successfully
- ✅ Material Editor shows Two Sided = checked
- ✅ Material needs recompile indicator may appear

---

## Test 6: Set Multiple Properties

**Purpose**: Batch property updates

### Steps

```
Use manage_material with action="set_properties":
- material_path: "/Game/Materials/test/M_MaterialTest"
- properties: {
    "TwoSided": "true",
    "BlendMode": "Masked",
    "OpacityMaskClipValue": "0.33"
  }

👀 Watch the Material Editor - Blend Mode should change and clip value should update!
```

### Expected Outcomes
- ✅ All properties updated
- ✅ Material now uses Masked blend mode
- ✅ Opacity clip value set to 0.33

---

## Test 7: Get Property Info (Metadata)

**Purpose**: Get detailed property metadata including allowed values

### Steps

```
Use manage_material with action="get_property_info":
- material_path: "/Game/Materials/test/M_MaterialTest"
- property_name: "BlendMode"
```

### Expected Outcomes
- ✅ Shows property type (enum)
- ✅ Lists allowed values: Opaque, Masked, Translucent, etc.
- ✅ Shows current value
- ✅ Shows tooltip/description

---

## Test 8: List Material Parameters

**Purpose**: List all parameter expressions in the material

### Steps

```
First, ensure material has parameters (see manage_material_node tests)

Use manage_material with action="list_parameters":
- material_path: "/Game/Materials/test/M_MaterialTest"
```

### Expected Outcomes
- ✅ Lists any scalar parameters with values
- ✅ Lists any vector parameters with values
- ✅ Lists any texture parameters
- ✅ Shows parameter groups

---

## Test 9: Get Parameter Info

**Purpose**: Get specific parameter details

### Steps

```
Use manage_material with action="get_parameter":
- material_path: "/Game/Materials/test/M_MaterialTest"
- parameter_name: "Roughness" (or any existing parameter)
```

### Expected Outcomes
- ✅ Shows parameter type
- ✅ Shows current/default value
- ✅ Shows group name

---

## Test 10: Set Parameter Default

**Purpose**: Modify a parameter's default value

### Steps

```
Use manage_material with action="set_parameter_default":
- material_path: "/Game/Materials/test/M_MaterialTest"
- parameter_name: "Roughness"
- value: "0.75"

👀 Watch the parameter node in the Material Editor - value should update!
```

### Expected Outcomes
- ✅ Parameter default updated
- ✅ Material Editor shows new value

---

## Test 11: Compile Material

**Purpose**: Force material shader recompilation

### Steps

```
Use manage_material with action="compile":
- material_path: "/Game/Materials/test/M_MaterialTest"
```

### Expected Outcomes
- ✅ Material compiles successfully
- ✅ Any compile errors would be reported

---

## Test 12: Save Material

**Purpose**: Save material to disk

### Steps

```
Use manage_material with action="save":
- material_path: "/Game/Materials/test/M_MaterialTest"
```

### Expected Outcomes
- ✅ Material saved
- ✅ Dirty flag cleared (no asterisk in tab)

---

## Test 13: Create Material Instance

**Purpose**: Create a Material Instance Constant from a parent material

### Steps

```
Use manage_material with action="create_instance":
- parent_material_path: "/Game/Materials/test/M_MaterialTest"
- destination_path: "/Game/Materials/test"
- instance_name: "MI_MaterialTest_Red"
- scalar_parameters: {"Roughness": 0.8, "Metallic": 0.2}
- vector_parameters: {"BaseColor": [1.0, 0.0, 0.0, 1.0]}

Then open it:
Use manage_asset with action="open_in_editor":
- asset_path: "/Game/Materials/test/MI_MaterialTest_Red"

👀 The instance should show red base color with custom roughness/metallic!
```

### Expected Outcomes
- ✅ Material instance created
- ✅ Inherits from parent material
- ✅ Parameter overrides applied
- ✅ Red color visible in preview

---

## Test 14: Get Material Instance Info

**Purpose**: Retrieve instance information

### Steps

```
Use manage_material with action="get_instance_info":
- instance_path: "/Game/Materials/test/MI_MaterialTest_Red"
```

### Expected Outcomes
- ✅ Shows parent material path
- ✅ Lists parameter overrides
- ✅ Shows blend mode (inherited or overridden)

---

## Test 15: List Instance Properties

**Purpose**: List editable instance properties

### Steps

```
Use manage_material with action="list_instance_properties":
- instance_path: "/Game/Materials/test/MI_MaterialTest_Red"
- include_advanced: true
```

### Expected Outcomes
- ✅ Lists available properties
- ✅ Shows which are overridable

---

## Test 16: List Instance Parameters

**Purpose**: List all parameters with current override values

### Steps

```
Use manage_material with action="list_instance_parameters":
- instance_path: "/Game/Materials/test/MI_MaterialTest_Red"
```

### Expected Outcomes
- ✅ Lists all inherited parameters
- ✅ Shows which are overridden
- ✅ Shows current values

---

## Test 17: Set Instance Scalar Parameter

**Purpose**: Override a scalar parameter on instance

### Steps

```
Use manage_material with action="set_instance_scalar_parameter":
- instance_path: "/Game/Materials/test/MI_MaterialTest_Red"
- parameter_name: "Roughness"
- value: 0.2

👀 Watch the Material Instance Editor - roughness preview should change!
```

### Expected Outcomes
- ✅ Scalar parameter overridden
- ✅ Preview updates to show smoother surface

---

## Test 18: Set Instance Vector Parameter

**Purpose**: Override a vector/color parameter

### Steps

```
Use manage_material with action="set_instance_vector_parameter":
- instance_path: "/Game/Materials/test/MI_MaterialTest_Red"
- parameter_name: "BaseColor"
- r: 0.0
- g: 1.0
- b: 0.0
- a: 1.0

👀 Watch the preview change from red to green!
```

### Expected Outcomes
- ✅ Vector parameter overridden
- ✅ Preview shows green color

---

## Test 19: Set Instance Texture Parameter

**Purpose**: Override a texture parameter

### Steps

```
(Requires parent material to have a texture parameter)

Use manage_material with action="set_instance_texture_parameter":
- instance_path: "/Game/Materials/test/MI_MaterialTest_Red"
- parameter_name: "DiffuseTexture"
- texture_path: "/Engine/EngineMaterials/DefaultDiffuse"
```

### Expected Outcomes
- ✅ Texture parameter overridden
- ✅ Preview shows new texture

---

## Test 20: Clear Instance Parameter Override

**Purpose**: Remove parameter override, reverting to parent value

### Steps

```
Use manage_material with action="clear_instance_parameter_override":
- instance_path: "/Game/Materials/test/MI_MaterialTest_Red"
- parameter_name: "Roughness"

👀 The roughness should revert to the parent material's default!
```

### Expected Outcomes
- ✅ Override removed
- ✅ Value reverts to parent default

---

## Test 21: Save Material Instance

**Purpose**: Save instance changes to disk

### Steps

```
Use manage_material with action="save_instance":
- instance_path: "/Game/Materials/test/MI_MaterialTest_Red"
```

### Expected Outcomes
- ✅ Instance saved
- ✅ Dirty flag cleared

---

## Test 22: Refresh Editor

**Purpose**: Refresh the material editor view

### Steps

```
Use manage_material with action="refresh_editor":
- material_path: "/Game/Materials/test/M_MaterialTest"
```

### Expected Outcomes
- ✅ Editor refreshed
- ✅ Graph layout updated

---

## Cleanup (Run AFTER manual review)

⚠️ **Only run after reviewing all results in Unreal Editor!**

```
Use manage_asset with action="delete":
- asset_path: "/Game/Materials/test/MI_MaterialTest_Red"
- force_delete: true
- show_confirmation: false

Use manage_asset with action="delete":
- asset_path: "/Game/Materials/test/M_TestCreate"
- force_delete: true
- show_confirmation: false

Use manage_asset with action="delete":
- asset_path: "/Game/Materials/test/M_MaterialTest"
- force_delete: true
- show_confirmation: false
```

---

## Summary Checklist

| Test | Action | Status |
|------|--------|--------|
| 1 | create | ⬜ |
| 2 | get_info | ⬜ |
| 3 | list_properties | ⬜ |
| 4 | get_property | ⬜ |
| 5 | set_property | ⬜ |
| 6 | set_properties | ⬜ |
| 7 | get_property_info | ⬜ |
| 8 | list_parameters | ⬜ |
| 9 | get_parameter | ⬜ |
| 10 | set_parameter_default | ⬜ |
| 11 | compile | ⬜ |
| 12 | save | ⬜ |
| 13 | create_instance | ⬜ |
| 14 | get_instance_info | ⬜ |
| 15 | list_instance_properties | ⬜ |
| 16 | list_instance_parameters | ⬜ |
| 17 | set_instance_scalar_parameter | ⬜ |
| 18 | set_instance_vector_parameter | ⬜ |
| 19 | set_instance_texture_parameter | ⬜ |
| 20 | clear_instance_parameter_override | ⬜ |
| 21 | save_instance | ⬜ |
| 22 | refresh_editor | ⬜ |
