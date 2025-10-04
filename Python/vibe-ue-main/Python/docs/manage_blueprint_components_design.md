# Design: `manage_blueprint_components` - Unified Blueprint Component Management

## 📋 Executive Summary

Consolidate existing Blueprint component tools into a single multi-action tool following the successful patterns of `manage_blueprint_function`, `manage_blueprint_node`, and `manage_blueprint_variables`.

## 🎯 Goals

1. **Unify** all component operations under one tool with action-based dispatch
2. **Enhance** with full reflection-based property reading capabilities  
3. **Simplify** AI assistant usage with consistent parameter patterns
4. **Maintain** backward compatibility during transition
5. **Enable** component property inspection and configuration (critical for Blueprint Recreation Challenge)

## 📦 Current Tools to Consolidate

### Discovery & Inspection Tools (5)
1. ✅ `get_available_components()` → `action="search_types"`
2. ✅ `get_component_info()` → `action="get_info"` 
3. ✅ `get_property_metadata()` → `action="get_property_metadata"`
4. ✅ `get_component_hierarchy()` → `action="list"` or `action="get_hierarchy"`

### Component Lifecycle Tools (3)
5. ✅ `add_component()` → `action="create"`
6. ✅ `add_component_to_blueprint()` → **DEPRECATED** (duplicate of add_component)
7. ✅ `remove_component()` → `action="delete"`

### Property Management Tools (2)
8. ✅ `set_component_property()` (2 identical functions!) → `action="set_property"`
9. 🆕 **NEW** `get_component_property()` → `action="get_property"`

### Hierarchy Tools (1)
10. ✅ `reorder_components()` → `action="reorder"`

### Total: 10 tools → 1 unified `manage_blueprint_components`

## 🔧 New Tool Signature

```python
@mcp.tool()
def manage_blueprint_components(
    ctx: Context,
    blueprint_name: str,
    action: str,
    # Action-specific parameters
    component_type: str = "",
    component_name: str = "",
    property_name: str = "",
    property_value: Any = None,
    parent_name: str = "",
    properties: Dict[str, Any] = None,
    location: List[float] = None,
    rotation: List[float] = None,
    scale: List[float] = None,
    component_order: List[str] = None,
    remove_children: bool = True,
    include_property_values: bool = False,
    # Discovery parameters
    category: str = "",
    base_class: str = "",
    search_text: str = "",
    include_abstract: bool = False,
    include_deprecated: bool = False,
    # Additional options
    options: Dict[str, Any] = None
) -> Dict[str, Any]:
```

## 🎬 Actions Specification

### 1. Discovery & Inspection Actions

#### `action="search_types"`
**Purpose**: Discover available component types
**Required**: None (all optional filters)
**Optional**: `category`, `base_class`, `search_text`, `include_abstract`, `include_deprecated`
**Returns**: List of component types with metadata
**Replaces**: `get_available_components()`

```python
# Example
manage_blueprint_components(
    blueprint_name="",  # Not required for type discovery
    action="search_types",
    category="Rendering",
    search_text="Light"
)
```

#### `action="get_info"`
**Purpose**: Get comprehensive component type information with optional property values
**Required**: `component_type`
**Optional**: `blueprint_name`, `component_name`, `include_property_values`
**Returns**: Component metadata + optional property values
**Replaces**: `get_component_info()`

**🆕 CRITICAL ENHANCEMENT**: When `include_property_values=True`, returns actual property values from component instance!

```python
# Get type metadata only
manage_blueprint_components(
    blueprint_name="",
    action="get_info",
    component_type="SpotLightComponent"
)

# 🆕 Get metadata + actual values from BP_Player
manage_blueprint_components(
    blueprint_name="BP_Player",
    action="get_info",
    component_type="SpotLightComponent",
    component_name="SpotLight_Top",
    include_property_values=True
)
# Returns: {"property_values": {"Intensity": 5000.0, "InnerConeAngle": 0.0, ...}}
```

#### `action="get_property_metadata"`
**Purpose**: Get detailed metadata for a specific property
**Required**: `component_type`, `property_name`
**Returns**: Property metadata (type, constraints, flags)
**Replaces**: `get_property_metadata()`

```python
manage_blueprint_components(
    blueprint_name="",
    action="get_property_metadata",
    component_type="SpotLightComponent",
    property_name="Intensity"
)
```

#### `action="list"`
**Purpose**: List all components in a Blueprint
**Required**: `blueprint_name`
**Returns**: Component hierarchy with metadata
**Replaces**: `get_component_hierarchy()`

```python
manage_blueprint_components(
    blueprint_name="BP_Player",
    action="list"
)
```

### 2. Component Lifecycle Actions

#### `action="create"`
**Purpose**: Add new component to Blueprint
**Required**: `blueprint_name`, `component_type`, `component_name`
**Optional**: `parent_name`, `properties`, `location`, `rotation`, `scale`
**Returns**: Created component info
**Replaces**: `add_component()`, `add_component_to_blueprint()`

```python
manage_blueprint_components(
    blueprint_name="BP_Player2",
    action="create",
    component_type="SpotLightComponent",
    component_name="SpotLight_Top",
    parent_name="BoxCollision",
    location=[0, 0, 100],
    rotation=[0, -90, 0],
    properties={"Intensity": 5000, "InnerConeAngle": 0}
)
```

#### `action="delete"`
**Purpose**: Remove component from Blueprint
**Required**: `blueprint_name`, `component_name`
**Optional**: `remove_children`
**Returns**: Deletion status
**Replaces**: `remove_component()`

```python
manage_blueprint_components(
    blueprint_name="BP_Player2",
    action="delete",
    component_name="SpotLight_Top",
    remove_children=True
)
```

### 3. Property Management Actions

#### `action="get_property"` 🆕 NEW!
**Purpose**: Get current value of a component property
**Required**: `blueprint_name`, `component_name`, `property_name`
**Returns**: Property value with metadata
**NEW CAPABILITY**: Read property values from existing components

```python
# Read Intensity from BP_Player's SpotLight_Top
result = manage_blueprint_components(
    blueprint_name="BP_Player",
    action="get_property",
    component_name="SpotLight_Top",
    property_name="Intensity"
)
# Returns: {"value": 5000.0, "type": "float", "property_name": "Intensity"}
```

#### `action="set_property"`
**Purpose**: Set component property value
**Required**: `blueprint_name`, `component_name`, `property_name`, `property_value`
**Returns**: Success status
**Replaces**: `set_component_property()` (both versions!)

```python
manage_blueprint_components(
    blueprint_name="BP_Player2",
    action="set_property",
    component_name="SpotLight_Top",
    property_name="Intensity",
    property_value=5000.0
)
```

#### `action="get_all_properties"` 🆕 NEW!
**Purpose**: Get all property values from a component instance
**Required**: `blueprint_name`, `component_name`
**Optional**: `include_inherited` (default: True)
**Returns**: Dict of all properties with values
**NEW CAPABILITY**: Bulk property reading for comparison/copying

```python
# Get ALL properties from BP_Player's SpotLight_Top
result = manage_blueprint_components(
    blueprint_name="BP_Player",
    action="get_all_properties",
    component_name="SpotLight_Top"
)
# Returns: {
#   "Intensity": 5000.0,
#   "InnerConeAngle": 0.0,
#   "OuterConeAngle": 44.0,
#   "LightColor": {"R": 1.0, "G": 1.0, "B": 1.0, "A": 1.0},
#   "RelativeLocation": {"X": 0, "Y": 0, "Z": 100},
#   ...
# }
```



### 4. Hierarchy Actions

#### `action="reorder"`
**Purpose**: Change component order in hierarchy
**Required**: `blueprint_name`, `component_order`
**Returns**: New order confirmation
**Replaces**: `reorder_components()`

```python
manage_blueprint_components(
    blueprint_name="BP_Player2",
    action="reorder",
    component_order=["SpotLight_Top", "SpotLight_Right", "SpotLight_Left", "TrailVFX", "PointLight"]
)
```

#### `action="reparent"` 🆕 NEW!
**Purpose**: Change component's parent attachment
**Required**: `blueprint_name`, `component_name`, `parent_name`
**Returns**: Reparenting status
**NEW CAPABILITY**: Dynamic hierarchy reorganization

```python
manage_blueprint_components(
    blueprint_name="BP_Player2",
    action="reparent",
    component_name="SpotLight_Top",
    parent_name="CameraBoom"  # Move to different parent
)
```

## 🚀 Critical New Features for Blueprint Challenge

### Feature 1: Bulk Property Reading
**Solves**: "How do I get ALL property values from BP_Player components?"

```python
# Get all properties from original component
original_props = manage_blueprint_components(
    blueprint_name="BP_Player",
    action="get_all_properties",
    component_name="SpotLight_Top"
)

# Apply to recreation
for prop_name, prop_value in original_props["properties"].items():
    manage_blueprint_components(
        blueprint_name="BP_Player2",
        action="set_property",
        component_name="SpotLight_Top",
        property_name=prop_name,
        property_value=prop_value
    )
```

### Feature 2: Property Comparison/Validation
**Solves**: "How do I verify 100% property parity?"

```python
# New action for validation
result = manage_blueprint_components(
    blueprint_name="BP_Player2",
    action="compare_properties",
    component_name="SpotLight_Top",
    options={
        "compare_to_blueprint": "BP_Player",
        "compare_to_component": "SpotLight_Top"
    }
)
# Returns: {
#   "matches": True/False,
#   "differences": [{"property": "Intensity", "source": 5000, "target": 3000}, ...],
#   "matching_count": 45,
#   "difference_count": 2
# }
```

## 📐 Implementation Plan

### Phase 1: Core Infrastructure (Week 1)
1. Create `manage_blueprint_components()` function skeleton
2. Implement action dispatcher
3. Add parameter validation
4. Wire up existing tool calls (backward compatibility)

### Phase 2: New Property Reading (Week 1)
1. Implement `get_property` action (read single property value)
2. Implement `get_all_properties` action (read all properties)
3. Add C++ support in VibeUE plugin for property value extraction
4. Test property reading on various component types

### Phase 3: New Property Operations (Week 2)
1. Implement `copy_properties` action
2. Implement `compare_properties` action
3. Implement `reparent` action
4. Add bulk operations support

### Phase 4: Documentation & Migration (Week 2)
1. Update `help.md` with new tool documentation
2. Create migration guide from old tools to new
3. Add examples for Blueprint Challenge use cases
4. Update AI guidance patterns

### Phase 5: Deprecation (Week 3)
1. Mark old tools as deprecated in docstrings
2. Add deprecation warnings in responses
3. Update all example scripts to use new tool
4. Plan removal timeline for old tools

## 🔄 Migration Strategy

### Backward Compatibility
Keep old tools operational during transition:
- Old tools call new `manage_blueprint_components` internally
- Deprecation warnings logged but not shown to users initially
- 3-month deprecation period before removal

### Example Migrations

```python
# OLD
get_component_info("SpotLightComponent")
# NEW
manage_blueprint_components(blueprint_name="", action="get_info", component_type="SpotLightComponent")

# OLD  
add_component("BP_Player", "SpotLightComponent", "SpotLight_Top", parent_name="BoxCollision")
# NEW
manage_blueprint_components(blueprint_name="BP_Player", action="create", component_type="SpotLightComponent", component_name="SpotLight_Top", parent_name="BoxCollision")

# OLD
set_component_property("BP_Player", "SpotLight_Top", "Intensity", 5000)
# NEW
manage_blueprint_components(blueprint_name="BP_Player", action="set_property", component_name="SpotLight_Top", property_name="Intensity", property_value=5000)
```

## 🎯 Success Criteria

1. ✅ All 11 existing tools replaced by single multi-action tool
2. ✅ New property reading capabilities enable Blueprint Challenge completion
3. ✅ `copy_properties` action achieves 100% property parity in single call
4. ✅ AI assistant can complete component configuration without manual inspection
5. ✅ Backward compatibility maintained during migration
6. ✅ Documentation updated with comprehensive examples
7. ✅ Performance: Property copying completes in <2 seconds per component

## 📊 Action Quick Reference Matrix

| Action | Blueprint Name | Component Type | Component Name | Property Name | Property Value | Other |
|--------|---------------|----------------|----------------|---------------|----------------|-------|
| `search_types` | ❌ | ❌ | ❌ | ❌ | ❌ | filters |
| `get_info` | ⚪ | ✅ | ⚪ | ❌ | ❌ | `include_property_values` |
| `get_property_metadata` | ❌ | ✅ | ❌ | ✅ | ❌ | - |
| `list` | ✅ | ❌ | ❌ | ❌ | ❌ | - |
| `create` | ✅ | ✅ | ✅ | ❌ | ❌ | parent, transform, properties |
| `delete` | ✅ | ❌ | ✅ | ❌ | ❌ | `remove_children` |
| `get_property` 🆕 | ✅ | ❌ | ✅ | ✅ | ❌ | - |
| `set_property` | ✅ | ❌ | ✅ | ✅ | ✅ | - |
| `get_all_properties` 🆕 | ✅ | ❌ | ✅ | ❌ | ❌ | `include_inherited` |
| `compare_properties` 🆕 | ✅ | ❌ | ✅ | ❌ | ❌ | `options` (compare to blueprint/component) |
| `reorder` | ✅ | ❌ | ❌ | ❌ | ❌ | `component_order` list |
| `reparent` 🆕 | ✅ | ❌ | ✅ | ❌ | ❌ | `parent_name` |

Legend: ✅ Required | ⚪ Optional | ❌ Not used

## 💡 Usage Examples for Blueprint Challenge

### Complete Component Property Parity Workflow

```python
# Step 1: List components in original Blueprint
original_components = manage_blueprint_components(
    blueprint_name="BP_Player",
    action="list"
)

# Step 2: For each custom component, read ALL properties and apply to BP_Player2
for comp in original_components["components"]:
    if not comp["is_inherited"]:  # Only custom components
        print(f"Processing {comp['name']}...")
        
        # Get all properties from original
        original_props = manage_blueprint_components(
            blueprint_name="BP_Player",
            action="get_all_properties",
            component_name=comp["name"]
        )
        
        # Set each property on recreation
        for prop_name, prop_value in original_props["properties"].items():
            manage_blueprint_components(
                blueprint_name="BP_Player2",
                action="set_property",
                component_name=comp["name"],
                property_name=prop_name,
                property_value=prop_value
            )
        
        print(f"✅ Configured {len(original_props['properties'])} properties")

# Step 3: Validate 100% parity
validation_results = []
for comp in ["SpotLight_Top", "SpotLight_Right", "SpotLight_Left", "TrailVFX", "PointLight", "QuestPlayer"]:
    result = manage_blueprint_components(
        blueprint_name="BP_Player2",
        action="compare_properties",
        component_name=comp,
        options={
            "compare_to_blueprint": "BP_Player",
            "compare_to_component": comp
        }
    )
    validation_results.append({
        "component": comp,
        "matches": result["matches"],
        "differences": result["differences"]
    })

# Step 4: Report
all_match = all(v["matches"] for v in validation_results)
if all_match:
    print("🎉 100% PROPERTY PARITY ACHIEVED!")
else:
    print("❌ Discrepancies found:")
    for v in validation_results:
        if not v["matches"]:
            print(f"  - {v['component']}: {len(v['differences'])} differences")
```

## 🔮 Future Enhancements

1. **Batch Operations**: `action="batch"` to perform multiple operations atomically
2. **Property Presets**: `action="apply_preset"` for common lighting/component configurations
3. **Component Templates**: Save/load complete component configurations
4. **Diff Visualization**: Generate visual reports of component property differences
5. **Smart Copying**: Intelligent property filtering based on component categories

## ✅ Next Steps

1. Review and approve this design
2. Create implementation ticket
3. Begin Phase 1 development
4. **Immediate Priority**: Implement `get_all_properties` and `copy_properties` for Blueprint Challenge completion

---

**Document Status**: 🟢 Ready for Review  
**Priority**: 🔴 HIGH - Blocks Blueprint Recreation Challenge Phase 2.2  
**Estimated Effort**: 2-3 weeks full implementation, 3-5 days for critical features only
