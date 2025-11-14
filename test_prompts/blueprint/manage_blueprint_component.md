# manage_blueprint_component Test Prompts

## Prerequisites
- ✅ Unreal Engine 5.7+ running
- ✅ VibeUE plugin loaded
- ✅ MCP connection active
- ✅ Blueprint created (use BP_TestActor from manage_blueprint tests or create new)

## Setup: Create Test Assets

**Run these commands BEFORE starting tests:**

```
Create Blueprint "BP_ComponentTest" with parent "Actor"
```

## Overview
Tests all 12 actions of `manage_blueprint_component` including component discovery, creation, property management, and hierarchy operations.

## Test 1: Component Type Discovery

**Purpose**: Discover available component types before creation

### Steps

1. **Search for Light Components**
   ```
   Search for available component types containing "SpotLight"
   ```

2. **Get SpotLightComponent Info**
   ```
   Get detailed information about SpotLightComponent type including properties
   ```

3. **Get Property Metadata**
   ```
   Get metadata for the Intensity property of SpotLightComponent
   ```

### Expected Outcomes
- ✅ search_types returns SpotLightComponent
- ✅ get_info shows component capabilities and categories
- ✅ get_property_metadata shows Intensity as float with min/max values

---

## Test 2: Component Creation and Listing

**Purpose**: Add components to Blueprint and list them

### Steps

1. **Create Blueprint** (if not exists)
   ```
   Create BP_LightTest Blueprint with Actor parent
   ```

2. **List Initial Components**
   ```
   List all components in BP_LightTest
   ```

3. **Create SpotLight Component**
   ```
   Create SpotLightComponent named "MainLight" in BP_LightTest
   ```

4. **List Components Again**
   ```
   List components to verify MainLight was added
   ```

5. **Create Second Component**
   ```
   Create another SpotLightComponent named "FillLight"
   ```

### Expected Outcomes
- ✅ New Blueprint has minimal default components
- ✅ create adds MainLight component
- ✅ list shows MainLight in hierarchy
- ✅ Can create multiple components of same type

---

## Test 3: Property Management

**Purpose**: Get and set component properties

### Steps

1. **Get Single Property**
   ```
   Get the Intensity property value from MainLight component
   ```

2. **Set Intensity**
   ```
   Set MainLight Intensity to 5000.0
   ```

3. **Set Light Color**
   ```
   Set MainLight LightColor to warm color (e.g., RGB values)
   ```

4. **Get All Properties**
   ```
   Get all properties from MainLight to verify changes
   ```

5. **Set Cone Angles**
   ```
   Set InnerConeAngle to 20.0 and OuterConeAngle to 45.0
   ```

### Expected Outcomes
- ✅ get_property returns current Intensity value
- ✅ set_property updates Intensity
- ✅ LightColor accepts color values
- ✅ get_all_properties shows all current values
- ✅ Cone angle properties set correctly

---

## Test 4: Component Hierarchy

**Purpose**: Test parent/child relationships and reordering

### Steps

1. **Create Scene Component**
   ```
   Create SceneComponent named "LightRig" as container
   ```

2. **Reparent MainLight**
   ```
   Reparent MainLight to make LightRig its parent
   ```

3. **Reparent FillLight**
   ```
   Reparent FillLight to also attach to LightRig
   ```

4. **Reorder Components**
   ```
   Reorder components: [LightRig, MainLight, FillLight]
   ```

5. **List to Verify Hierarchy**
   ```
   List components to see parent-child relationships
   ```

### Expected Outcomes
- ✅ SceneComponent created successfully
- ✅ reparent attaches MainLight to LightRig
- ✅ Multiple children can attach to same parent
- ✅ reorder changes component order in list
- ✅ list shows hierarchical structure

---

## Test 5: Property Comparison

**Purpose**: Compare properties between Blueprints

### Steps

1. **Create Second Blueprint**
   ```
   Create BP_LightTest2 with same components as BP_LightTest
   ```

2. **Set Different Property Values**
   ```
   Set MainLight Intensity to different values in each Blueprint
   ```

3. **Compare Properties**
   ```
   Compare MainLight properties between BP_LightTest and BP_LightTest2
   ```

### Expected Outcomes
- ✅ compare_properties shows differences
- ✅ Highlights which properties differ
- ✅ Shows values from both Blueprints

---

## Test 6: Component Deletion

**Purpose**: Test component removal and cleanup

### Steps

1. **Delete FillLight**
   ```
   Delete the FillLight component from BP_LightTest
   ```

2. **Verify Removal**
   ```
   List components - FillLight should be gone
   ```

3. **Delete with Children**
   ```
   Delete LightRig (which has MainLight as child) with remove_children=true
   ```

4. **Verify Cascade Delete**
   ```
   List components - both LightRig and MainLight should be removed
   ```

### Expected Outcomes
- ✅ delete removes FillLight
- ✅ Deleted component no longer in list
- ✅ Deleting parent with remove_children=true removes children
- ✅ Blueprint remains valid after deletions

---

## Test 7: Component Type Variations

**Purpose**: Test creating different component types

### Steps

1. **Create Audio Component**
   ```
   Create AudioComponent named "SoundEffect"
   ```

2. **Create Particle Component**
   ```
   Create ParticleSystemComponent named "VFX"
   ```

3. **Create Niagara Component**
   ```
   Create NiagaraComponent named "ModernVFX"
   ```

4. **List All Components**
   ```
   List to see variety of component types
   ```

### Expected Outcomes
- ✅ Audio components create successfully
- ✅ Particle components create successfully
- ✅ Niagara components create successfully
- ✅ Different component types coexist in Blueprint

---

## Critical Discoveries & Best Practices

### Property Naming
**CRITICAL**: Component property names may differ from UI labels
- ✅ **SkeletalMesh**: Use `SkeletalMeshAsset` or `SkinnedAsset` (NOT `SkeletalMesh`)
- ✅ **Materials**: Use `OverrideMaterials` for material array
- ✅ **Transforms**: Use `RelativeLocation`, `RelativeRotation`, `RelativeScale`

### Package Path Requirement
⚠️ **CRITICAL**: `blueprint_name` MUST be full package path
- ✅ CORRECT: `/Game/Blueprints/BP_LightTest`
- ❌ WRONG: `BP_LightTest` (will fail with "Blueprint not found")

**How to get correct path:**
1. Use `search_items(search_term="BP_LightTest", asset_type="Blueprint")`
2. Use `package_path` field from results
3. Pass exact path to manage_blueprint_component

### Common Component Properties

**Lights:**
- `Intensity`, `LightColor`, `AttenuationRadius`, `CastShadows`
- **SpotLights**: `InnerConeAngle`, `OuterConeAngle`

**Audio:**
- `Sound`, `VolumeMultiplier`, `PitchMultiplier`, `bAutoActivate`

**Niagara:**
- `Asset` (for NiagaraSystem reference)

---

## Reference: All Actions Summary

| Action | Purpose | Key Parameters |
|--------|---------|----------------|
| **search_types** | Find component types | category, search_text, include_blueprints |
| **get_info** | Get component type info | component_type, include_property_values |
| **get_property_metadata** | Get property metadata | component_type, property_name |
| **list** | List all components | blueprint_name |
| **create** | Add component | blueprint_name, component_type, component_name, parent_name, properties |
| **get_property** | Get property value | blueprint_name, component_name, property_name |
| **set_property** | Set property value | blueprint_name, component_name, property_name, property_value |
| **get_all_properties** | Get all properties | blueprint_name, component_name, include_inherited |
| **compare_properties** | Compare between BPs | blueprint_name (source), component_name, options with target BP |
| **reorder** | Change order | blueprint_name, component_order (array) |
| **reparent** | Change parent | blueprint_name, component_name, parent_name |
| **delete** | Remove component | blueprint_name, component_name, remove_children |

---

## Cleanup: Delete Test Assets

**Run these commands AFTER completing all tests:**

```
Delete test Blueprint:
- Delete /Game/Blueprints/BP_ComponentTest with force_delete=True and show_confirmation=False
```

---

**Test Coverage**: 12/12 actions tested ✅  
**Last Updated**: November 4, 2025  
**Related Issues**: #69, #71

