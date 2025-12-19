# Multi-Action Tools Reference

## Overview

Complete reference for VibeUE's unified multi-action management tools that consolidate multiple individual tools into single interfaces with action parameters.

## Why Multi-Action Tools?

**Benefits:**
- Single tool learns all related operations
- Consistent parameter patterns across actions
- Better AI comprehension through related operations
- Reduced tool count improves discoverability
- Unified error handling and validation

## Multi-Action Tool Pattern

```python
tool_name(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="action_name",
    action_specific_param="value",
    ...
)
```

## manage_blueprint_function

**Purpose**: Complete Blueprint function lifecycle management

### Available Actions

#### Discovery Actions

**list** - List all functions in Blueprint
```python
manage_blueprint_function(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="list"
)
# Returns: {"functions": [{"name": "FuncName", "node_count": 5}, ...]}
```

**get** - Get detailed function information
```python
manage_blueprint_function(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="get",
    function_name="CalculateHealth"
)
# Returns: {"name": "CalculateHealth", "node_count": 10, "graph_guid": "..."}
```

**list_params** - List function parameters
```python
manage_blueprint_function(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="list_params",
    function_name="CalculateHealth"
)
# Returns: {"parameters": [{"name": "BaseHealth", "direction": "input", "type": "float"}, ...]}
```

#### Function Lifecycle Actions

**create** - Create new function
```python
manage_blueprint_function(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="create",
    function_name="CalculateHealth"
)
```

**delete** - Remove function
```python
manage_blueprint_function(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="delete",
    function_name="OldFunction"
)
```

#### Parameter Management Actions

**add_param** - Add input/output parameter

⚠️ **CRITICAL**: Use `"input"` or `"out"` for direction, NOT `"output"`!

```python
# Add input parameter
manage_blueprint_function(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="add_param",
    function_name="CalculateHealth",
    param_name="BaseHealth",
    direction="input",  # ✅ CORRECT
    type="float"
)

# Add output parameter
manage_blueprint_function(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="add_param",
    function_name="CalculateHealth",
    param_name="ResultHealth",
    direction="out",  # ✅ CORRECT for outputs (NOT "output"!)
    type="float"
)

# Add object reference
manage_blueprint_function(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="add_param",
    function_name="ProcessActor",
    param_name="TargetActor",
    direction="input",
    type="object:AActor"  # Format: "object:ClassName"
)
```

**remove_param** - Remove parameter
```python
manage_blueprint_function(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="remove_param",
    function_name="CalculateHealth",
    param_name="OldParam",
    direction="input"
)
```

**update_param** - Update parameter type/name
```python
manage_blueprint_function(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="update_param",
    function_name="CalculateHealth",
    param_name="OldParamName",
    direction="input",
    new_type="int",
    new_name="NewParamName"
)
```

#### Local Variable Actions

**list_locals** - List local variables
```python
manage_blueprint_function(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="list_locals",
    function_name="CalculateHealth"
)
```

**add_local** - Add local variable
```python
manage_blueprint_function(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="add_local",
    function_name="CalculateHealth",
    param_name="TempResult",  # Uses param_name field
    type="float"
)
```

**remove_local** - Remove local variable
```python
manage_blueprint_function(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="remove_local",
    function_name="CalculateHealth",
    param_name="TempResult"
)
```

#### Function Properties Action

**update_properties** - Update function metadata
```python
manage_blueprint_function(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="update_properties",
    function_name="CalculateHealth",
    properties={
        "CallInEditor": True,
        "BlueprintPure": True,
        "Category": "Combat|Health"
    }
)
```

### Parameter Type Reference

**Primitives:**
- `"int"`, `"float"`, `"bool"`, `"string"`, `"byte"`, `"name"`
- ⚠️ When list_params returns `"real"`, use `"float"` for add_param

**Object References:**
- Format: `"object:ClassName"`
- Examples: `"object:AActor"`, `"object:UUserWidget"`, `"object:ABP_Enemy_C"`

**Struct Types:**
- Format: `"struct:StructName"`
- Examples: `"struct:FVector"`, `"struct:FRotator"`

**Container Types:**
- Arrays: `"array<float>"`, `"array<object:AActor>"`

---

## manage_blueprint_variable

**Purpose**: Unified Blueprint variable management with proper type handling

⚠️ **CRITICAL**: variable_config MUST use `"type_path"` NOT `"type"`!

### Available Actions

#### Discovery Actions

**search_types** - Discover available variable types (200+ types)
```python
manage_blueprint_variable(
    blueprint_name="BP_Player2",
    action="search_types",
    search_criteria={
        "category": "Widget",  # Widget|Audio|Particle|Blueprint|Basic|Struct
        "search_text": "UserWidget",
        "include_blueprints": True,
        "include_engine_types": True
    }
)
# Returns type_path like "/Script/UMG.UserWidget"
```

**get_info** - Get variable information
```python
manage_blueprint_variable(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="get_info",
    variable_name="Health"
)
```

**list** - List all variables (FUTURE)

#### Variable Lifecycle Actions

**create** - Create new variable

⚠️ **CRITICAL**: MUST use search_types first to get correct type_path!

```python
# 1. Search for type
type_search = manage_blueprint_variable(
    blueprint_name="BP_Player2",
    action="search_types",
    search_criteria={"search_text": "UserWidget"}
)

# 2. Create with exact type_path
manage_blueprint_variable(
    blueprint_name="BP_Player2",
    action="create",
    variable_name="AttributeWidget",
    variable_config={
        "type_path": "/Script/UMG.UserWidget",  # ✅ REQUIRED: Use "type_path"
        "category": "UI",
        "tooltip": "Player's attribute display widget",
        "is_editable": True,
        "default_value": None
    }
)
```

**delete** - Remove variable
```python
manage_blueprint_variable(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="delete",
    variable_name="OldVariable"
)
```

#### Property Actions

**get_property** - Get variable property value
```python
manage_blueprint_variable(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="get_property",
    variable_name="Health",
    property_path="CurrentValue"
)
```

**set_property** - Set variable property value
```python
manage_blueprint_variable(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="set_property",
    variable_name="Health",
    property_path="CurrentValue",
    value=100.0
)
```

### Common Type Paths

**Primitives:**
- float: `"/Script/CoreUObject.FloatProperty"`
- int: `"/Script/CoreUObject.IntProperty"`
- bool: `"/Script/CoreUObject.BoolProperty"`
- string: `"/Script/CoreUObject.StrProperty"`

**Common Objects:**
- UserWidget: `"/Script/UMG.UserWidget"`
- NiagaraSystem: `"/Script/Niagara.NiagaraSystem"`
- SoundBase: `"/Script/Engine.SoundBase"`
- Actor: `"/Script/Engine.Actor"`

**Blueprint Classes:**
- Format: `"/Game/Blueprints/UI/BP_MicrosubHUD.BP_MicrosubHUD_C"`
- Always append `.ClassName_C` suffix

---

## manage_blueprint_component

**Purpose**: Unified Blueprint component management for actors

⚠️ **CRITICAL**: blueprint_name MUST be full package path!

### Available Actions

#### Discovery Actions

**search_types** - Discover component types
```python
manage_blueprint_component(
    blueprint_name="",  # Not required for type search
    action="search_types",
    category="Rendering",
    search_text="Light"
)
```

**get_info** - Get component type information
```python
# Type metadata only
manage_blueprint_component(
    blueprint_name="",
    action="get_info",
    component_type="SpotLightComponent"
)

# Type + instance property values
manage_blueprint_component(
    blueprint_name="/Game/Blueprints/Characters/BP_Player",
    action="get_info",
    component_type="SpotLightComponent",
    component_name="SpotLight_Top",
    include_property_values=True
)
```

**get_property_metadata** - Get property details
```python
manage_blueprint_component(
    blueprint_name="",
    action="get_property_metadata",
    component_type="SpotLightComponent",
    property_name="Intensity"
)
```

**list** - List all components in Blueprint
```python
manage_blueprint_component(
    blueprint_name="/Game/Blueprints/Characters/BP_Player",
    action="list"
)
```

#### Component Lifecycle Actions

**create** - Add component to Blueprint
```python
manage_blueprint_component(
    blueprint_name="/Game/Blueprints/BP_Player2",
    action="create",
    component_type="SpotLightComponent",
    component_name="SpotLight_Top",
    parent_name="BoxCollision",
    location=[0, 0, 100],
    rotation=[0, -90, 0],
    properties={"Intensity": 5000}
)
```

**delete** - Remove component
```python
manage_blueprint_component(
    blueprint_name="/Game/Blueprints/BP_Player2",
    action="delete",
    component_name="SpotLight_Top",
    remove_children=True
)
```

#### Property Actions

**get_property** - Get single property value
```python
result = manage_blueprint_component(
    blueprint_name="/Game/Blueprints/Characters/BP_Player",
    action="get_property",
    component_name="SpotLight_Top",
    property_name="Intensity"
)
# Returns: {"value": 5000.0, "type": "float"}
```

**set_property** - Set property value
```python
manage_blueprint_component(
    blueprint_name="/Game/Blueprints/BP_Player2",
    action="set_property",
    component_name="SpotLight_Top",
    property_name="Intensity",
    property_value=5000.0
)
```

**get_all_properties** - Get all property values
```python
manage_blueprint_component(
    blueprint_name="/Game/Blueprints/Characters/BP_Player",
    action="get_all_properties",
    component_name="SpotLight_Top",
    include_inherited=True
)
```

**compare_properties** - Compare between Blueprints
```python
manage_blueprint_component(
    blueprint_name="/Game/Blueprints/BP_Player2",
    action="compare_properties",
    component_name="SpotLight_Top",
    options={
        "compare_to_blueprint": "/Game/Blueprints/Characters/BP_Player",
        "compare_to_component": "SpotLight_Top"
    }
)
```

#### Hierarchy Actions

**reorder** - Change component order
```python
manage_blueprint_component(
    blueprint_name="/Game/Blueprints/BP_Player2",
    action="reorder",
    component_order=["SpotLight_Top", "SpotLight_Right", "TrailVFX"]
)
```

**reparent** - Change parent attachment
```python
manage_blueprint_component(
    blueprint_name="/Game/Blueprints/BP_Player2",
    action="reparent",
    component_name="SpotLight_Top",
    parent_name="CameraBoom"
)
```

### Critical Property Names

**SkeletalMesh Component:**
- Mesh asset: `"SkeletalMeshAsset"` or `"SkinnedAsset"` (NOT `"SkeletalMesh"`)
- Materials: `"OverrideMaterials"`

**Common Properties:**
- Lights: `"Intensity"`, `"LightColor"`, `"AttenuationRadius"`, `"CastShadows"`
- SpotLights: `"InnerConeAngle"`, `"OuterConeAngle"`
- Transforms: `"RelativeLocation"`, `"RelativeRotation"`, `"RelativeScale"`
- Niagara: `"Asset"` for NiagaraSystem reference

---

## manage_blueprint_node

**Purpose**: Universal Blueprint node operations

### Available Actions

#### Discovery Actions

**list** - List all nodes in graph
```python
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="list",
    graph_scope="function",
    function_name="MyFunction"
)
```

**describe** - Get rich node metadata
```python
# Describe all nodes
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="describe",
    graph_scope="function",
    function_name="MyFunction"
)

# Describe specific node
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="describe",
    extra={"node_id": "{GUID}"}
)
```

**get_details** - Get detailed node information
```python
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="get_details",
    node_id="{GUID}",
    graph_scope="function",
    function_name="MyFunction"
)
```

#### Node Lifecycle Actions

**create** - Create new node

✅ **RECOMMENDED**: Use spawner_key from get_available_blueprint_nodes()

```python
# 1. Discover exact node
nodes = get_available_blueprint_nodes(
    blueprint_name="/Game/Blueprints/BP_Player",
    search_term="GetPlayerController"
)

# 2. Use spawner_key for exact creation
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="create",
    node_params={"spawner_key": "GameplayStatics::GetPlayerController"},
    position=[200, 100]
)
```

**delete** - Remove node
```python
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="delete",
    node_id="{GUID}"
)
```

**move** - Reposition node
```python
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="move",
    node_id="{GUID}",
    position=[400, 200]
)
```

#### Connection Actions

**connect** - Simple direct connection (RECOMMENDED for single connections)

```python
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="connect",
    source_node_id="{SOURCE_GUID}",
    source_pin="ReturnValue",
    target_node_id="{TARGET_GUID}",
    target_pin="self",
    function_name="MyFunction"  # Optional: for function graphs
)
```

**connect_pins** - Batch connections using extra parameter

```python
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="connect_pins",
    graph_scope="function",
    function_name="MyFunction",
    extra={
        "connections": [{
            "source_node_id": "{SOURCE_GUID}",
            "source_pin_name": "ReturnValue",
            "target_node_id": "{TARGET_GUID}",
            "target_pin_name": "self"
        }]
    }
)
```

**disconnect** - Disconnect a specific pin
```python
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="disconnect",
    node_id="{GUID}",
    source_pin="ReturnValue",
    function_name="MyFunction"  # Optional: for function graphs
)
```

**disconnect_pins** - Break specific connections
```python
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="disconnect_pins",
    extra={
        "connections": [{
            "source_node_id": "{SOURCE_GUID}",
            "source_pin_name": "ReturnValue",
            "target_node_id": "{TARGET_GUID}",
            "target_pin_name": "Input"
        }]
    }
)
```

#### Configuration Actions

**configure** - Set node properties
```python
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="configure",
    node_id="{GUID}",
    property_name="PropertyName",
    property_value="value"
)
```

### Critical node_params Patterns

**Variable Set Nodes** (REQUIRED):
```python
manage_blueprint_node(
    action="create",
    node_type="SET Health",
    node_params={"variable_name": "Health"}  # ← CRITICAL!
)
```

**Cast Nodes** (REQUIRED):
```python
manage_blueprint_node(
    action="create",
    node_type="Cast To BP_MicrosubHUD",
    node_params={
        "cast_target": "/Game/Blueprints/HUD/BP_MicrosubHUD.BP_MicrosubHUD_C"
    }
)
```

**Spawner Key** (RECOMMENDED):
```python
manage_blueprint_node(
    action="create",
    node_params={"spawner_key": "GameplayStatics::GetPlayerController"}
)
```

## Multi-Action Tool Best Practices

1. **Discovery First**: Use list/search actions before modifying
2. **Validate Actions**: Check action name spelling (case-insensitive)
3. **Full Paths**: Always use full package paths for blueprint_name
4. **Type Paths**: For variables, ALWAYS use search_types to get type_path
5. **Direction Values**: For function params, use "input"/"out" (not "output")
6. **Node Creation**: Prefer spawner_key over fuzzy node_type search
7. **Error Handling**: Check success field in all responses

## Common Multi-Action Workflows

### Complete Function Recreation

```python
# 1. Discover original
params = manage_blueprint_function(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="list_params",
    function_name="CalculateHealth"
)

# 2. Create new function
manage_blueprint_function(
    blueprint_name="/Game/Blueprints/BP_Player2",
    action="create",
    function_name="CalculateHealth"
)

# 3. Recreate parameters
for param in params["parameters"]:
    if param["name"] != "execute":
        manage_blueprint_function(
            blueprint_name="/Game/Blueprints/BP_Player2",
            action="add_param",
            function_name="CalculateHealth",
            param_name=param["name"],
            direction=param["direction"],
            type=param["type"]
        )
```

### Component Property Comparison

```python
# 1. Get source properties
source_props = manage_blueprint_component(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="get_all_properties",
    component_name="SpotLight_Top"
)

# 2. Compare with target
comparison = manage_blueprint_component(
    blueprint_name="/Game/Blueprints/BP_Player2",
    action="compare_properties",
    component_name="SpotLight_Top",
    options={
        "compare_to_blueprint": "/Game/Blueprints/BP_Player",
        "compare_to_component": "SpotLight_Top"
    }
)

# 3. Apply differences
for diff in comparison["differences"]:
    manage_blueprint_component(
        blueprint_name="/Game/Blueprints/BP_Player2",
        action="set_property",
        component_name="SpotLight_Top",
        property_name=diff["property"],
        property_value=diff["source_value"]
    )
```

## See Also

- **get_help(topic="node-tools")** - Node creation patterns
- **get_help(topic="blueprint-workflow")** - Dependency order
- **get_help(topic="troubleshooting")** - Error recovery
