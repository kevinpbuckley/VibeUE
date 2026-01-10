# BlueprintService API Reference

All methods are called via `unreal.BlueprintService.<method_name>(...)`.

**ALWAYS use `discover_python_class("unreal.BlueprintService")` for parameter details before calling.**

---

## ⚠️ Method Name Gotchas - CRITICAL

The following method names are commonly confused. **Always use the correct version:**

| ❌ WRONG | ✅ CORRECT |
|----------|-----------|
| `add_node` | Use specific: `add_branch_node`, `add_math_node`, etc. |
| `add_variable_get_node` | `add_get_variable_node` |
| `add_variable_set_node` | `add_set_variable_node` |
| `add_local_variable` | `add_function_local_variable` |
| `remove_node` | `delete_node` |
| `list_functions` | Use `get_blueprint_info().functions` |
| `connect_pins` | `connect_nodes` |
| `set_pin_default_value` | `set_node_pin_value` |

---

## ⚠️ Return Types - CRITICAL

**ALWAYS discover return type properties BEFORE accessing them.** The AI frequently guesses wrong property names.

### BlueprintDetailedInfo (from `get_blueprint_info`)
```
.blueprint_name
.blueprint_path
.parent_class
.variables            # Array[BlueprintVariableInfo] - use len() for count
.functions            # Array[BlueprintFunctionInfo] - use len() for count
.components           # Array[BlueprintComponentInfo]
.is_widget_blueprint
```

### BlueprintFunctionDetailedInfo (from `get_function_info`)
```
.function_name        # NOT .name
.input_parameters     # NOT .inputs (Array[BlueprintFunctionParameterInfo]) - use len() for count
.output_parameters    # NOT .outputs - use len() for count
.local_variables      # Array[BlueprintLocalVariableInfo] - use len() for count
.graph_guid
.is_pure
.is_override
.node_count
```

### BlueprintFunctionParameterInfo (items in input_parameters/output_parameters)
```
.parameter_name       # NOT .pin_name, NOT .name
.parameter_type       # NOT .pin_type
.default_value
.is_output
.is_reference
```

### BlueprintLocalVariableInfo (items in local_variables)
```
.variable_name        # NOT .name
.variable_type
.default_value
.category
.guid
.is_array
.is_const
.is_reference
```

### BlueprintNodeInfo (from `get_nodes_in_graph`)
```
.node_id              # GUID string
.node_title           # NOT .node_name - Display title like "Branch", "Get Health"
.node_type            # Class like "K2Node_IfThenElse", "K2Node_VariableGet"
.pins                 # Array[BlueprintPinInfo]
.pin_names            # Quick list of pin names
.pos_x, .pos_y
```

### BlueprintPinInfo (from node.pins or `get_node_pins`)
```
.pin_name
.pin_type             # "exec", "bool", "float", etc.
.is_input             # True for inputs, False for outputs
.is_connected         # NOT .is_linked
.default_value
```

### BlueprintConnectionInfo (from `get_connections`)
```
.source_node_id
.source_node_title
.source_pin_name
.target_node_id
.target_node_title
.target_pin_name
```

### BlueprintVariableInfo (from `list_variables`)
```
.variable_name        # NOT .name
.variable_type
.category
.is_replicated
.is_instance_editable
```

---

## Lifecycle & Properties

### create_blueprint(name, parent_class, path)
Create a new Blueprint asset.

**Returns:** Full asset path (str)

**Example:**
```python
import unreal
path = unreal.BlueprintService.create_blueprint("Enemy", "Character", "/Game/Blueprints/")
print(f"Created: {path}")  # "/Game/Blueprints/BP_Enemy"
```

### compile_blueprint(path)
Compile the blueprint. **REQUIRED after adding variables, functions, or components.**

**Example:**
```python
unreal.BlueprintService.add_variable("/Game/BP_Player", "Health", "float", "100.0")
unreal.BlueprintService.compile_blueprint("/Game/BP_Player")  # Must compile!
```

### reparent_blueprint(path, new_parent_class)
Change blueprint parent class.

### get_property(path, property_name)
Get CDO property value.

**Returns:** str or None

### set_property(path, property_name, value)
Set CDO property value.

### diff_blueprints(path_a, path_b)
Compare two blueprints.

**Returns:** str or None with diff text

---

## Blueprint Info & Variables

### get_blueprint_info(path)
Get comprehensive blueprint information (parent class, variable count, function count, etc.).

### get_parent_class(path)
Get parent class name.

**Returns:** str

### is_widget_blueprint(path)
Check if blueprint is a Widget Blueprint.

**Returns:** bool

### list_variables(path)
List all variables in the blueprint.

**Returns:** Array[BlueprintVariableInfo] with properties: `.variable_name`, `.variable_type`, `.category`, `.is_replicated`, `.is_instance_editable`

**Example:**
```python
import unreal
vars = unreal.BlueprintService.list_variables("/Game/BP_Player")
for v in vars:
    print(f"{v.variable_name}: {v.variable_type}")  # Use .variable_name NOT .name
```

### list_functions(path)
List all functions in the blueprint.

**Returns:** Array of function info structs

### list_components(path)
List all components in the blueprint.

**Returns:** Array of component info structs

### get_component_hierarchy(path)
Get component tree with parent-child relationships.

### add_variable(blueprint_path, variable_name, variable_type, default_value="", is_array=False, container_type="")
Add a new variable to the blueprint.

**Parameters:**
- `blueprint_path`: Full path to the blueprint
- `variable_name`: Name of the variable
- `variable_type`: Type string (e.g., "float", "FVector", "AActor", "TSubclassOf<AActor>")
- `default_value`: Default value as string (optional)
- `is_array`: Whether this is an array type
- `container_type`: Container type: "Array", "Set", "Map", or empty

**Returns:** bool

**NOTE:** Category, tooltip, and instance_editable are NOT parameters of add_variable. Use `modify_variable()` after adding the variable to set these metadata properties.

**Example:**
```python
import unreal

# Simple variable
unreal.BlueprintService.add_variable("/Game/BP_Player", "Health", "float", "100.0")

# Array variable
unreal.BlueprintService.add_variable("/Game/BP_Player", "Inventory", "AActor", "", True, "Array")

# To set category/tooltip, use modify_variable AFTER adding:
unreal.BlueprintService.add_variable("/Game/BP_Player", "MaxHealth", "float", "100.0")
unreal.BlueprintService.modify_variable("/Game/BP_Player", "MaxHealth", new_category="Stats", new_tooltip="Maximum health value")

# MUST compile before using in nodes
unreal.BlueprintService.compile_blueprint("/Game/BP_Player")
```

### remove_variable(path, name)
Remove a variable from the blueprint.

### set_variable_default_value(path, name, value)
Set variable default value.

### get_variable_info(path, var_name)
Get detailed variable info (type, category, replication, etc.).

**Returns:** BlueprintVariableDetailedInfo or None

### modify_variable(path, var_name, new_name=None, new_category=None, new_tooltip=None, set_instance_editable=None, new_replication_condition=None)
Modify variable properties (rename, category, tooltip, replication, etc.).

**Example:**
```python
import unreal

# Rename variable
unreal.BlueprintService.modify_variable("/Game/BP_Player", "Health", new_name="CurrentHealth")

# Make editable in Details panel
unreal.BlueprintService.modify_variable("/Game/BP_Player", "MaxHealth", set_instance_editable=1)

# Enable replication
unreal.BlueprintService.modify_variable("/Game/BP_Player", "Health", new_replication_condition="Replicated")
```

### search_variable_types(search_term, category, max_results)
Search available variable types.

**Example:**
```python
import unreal

# Find vector types
types = unreal.BlueprintService.search_variable_types("Vector", "", 10)
for t in types:
    print(f"{t.type_name}: {t.type_path}")

# List all Structure types
structs = unreal.BlueprintService.search_variable_types("", "Structure", 20)
```

---

## Component Management

### get_available_components(search_filter, max_results)
Search available component types.

**Example:**
```python
import unreal

# Find mesh components
comps = unreal.BlueprintService.get_available_components("Mesh", 10)
for c in comps:
    print(c)
```

### get_component_info(component_type)
Get detailed info about a component type.

**Returns:** ComponentDetailedInfo or None

### add_component(path, component_type, name, parent_name="")
Add component to blueprint.

**Example:**
```python
import unreal

# Add root component
unreal.BlueprintService.add_component("/Game/BP_Enemy", "StaticMeshComponent", "BodyMesh")

# Add child component
unreal.BlueprintService.add_component("/Game/BP_Enemy", "PointLightComponent", "Glow", "BodyMesh")

unreal.BlueprintService.compile_blueprint("/Game/BP_Enemy")
```

### remove_component(path, name, remove_children=False)
Remove component from blueprint.

### get_component_property(path, comp_name, prop_name)
Get component property value.

**Returns:** str or None

### set_component_property(path, comp_name, prop_name, value)
Set component property.

**Example:**
```python
import unreal

# Set visibility
unreal.BlueprintService.set_component_property("/Game/BP_Player", "Mesh", "bVisible", "true")

# Set location (use Unreal format for structs)
unreal.BlueprintService.set_component_property("/Game/BP_Player", "Camera", "RelativeLocation", "(X=0,Y=0,Z=50)")
```

### get_all_component_properties(path, comp_name, include_inherited)
Get all component properties.

### reparent_component(path, comp_name, new_parent)
Change component's parent in hierarchy.

---

## Functions

### create_function(path, name, is_pure=False)
Create a new function.

**Example:**
```python
import unreal

unreal.BlueprintService.create_function("/Game/BP_Player", "TakeDamage", is_pure=False)
```

### delete_function(path, func_name)
Delete a function from the blueprint.

### get_function_info(path, func_name)
Get detailed function info (inputs, outputs, local variables, node count).

**Returns:** BlueprintFunctionDetailedInfo or None

### get_function_parameters(path, func_name)
Get function parameters (inputs and outputs).

### add_function_parameter(path, func_name, param_name, param_type, is_output, default_value="")
Add parameter to function (input or output).

### add_function_input(path, func_name, param_name, type, default_value="")
Add input parameter (convenience method).

**Example:**
```python
import unreal

unreal.BlueprintService.create_function("/Game/BP_Player", "TakeDamage")
unreal.BlueprintService.add_function_input("/Game/BP_Player", "TakeDamage", "Amount", "float", "0.0")
unreal.BlueprintService.add_function_input("/Game/BP_Player", "TakeDamage", "DamageType", "string", "Physical")
```

### add_function_output(path, func_name, param_name, type)
Add output parameter (convenience method).

### remove_function_parameter(path, func_name, param_name, is_output=False)
Remove a parameter.

### add_function_local_variable(path, func_name, var_name, var_type, default_value="")
Add local variable to function.

### remove_function_local_variable(path, func_name, var_name)
Remove local variable.

### update_function_local_variable(path, func_name, var_name, new_name, new_type, new_default)
Update local variable.

### list_function_local_variables(path, func_name)
List all local variables in function.

---

## Nodes - Adding

**⚠️ CRITICAL: There is NO generic `add_node()` method. Use specific methods below.**

### add_branch_node(path, graph, x, y)
Add Branch node.

**Returns:** node_id (str - GUID, NOT int)

### add_get_variable_node(path, graph, var_name, x, y)
Add Get Variable node. **Blueprint must be compiled after adding the variable.**

**Returns:** node_id (str - GUID)

### add_set_variable_node(path, graph, var_name, x, y)
Add Set Variable node.

**Returns:** node_id (str - GUID)

### add_print_string_node(path, graph, x, y)
Add Print String node.

**Returns:** node_id (str - GUID)

### add_math_node(path, graph, operation, type, x, y)
Add math node.

**Parameters:**
- `operation`: "Add", "Subtract", "Multiply", "Divide", "Clamp", "Min", "Max", "Abs", "Negate"
- `type`: "Float", "Int", "Double", "Vector"

**Returns:** node_id (str - GUID)

**Example:**
```python
import unreal

# Add two floats
add_node = unreal.BlueprintService.add_math_node("/Game/BP_Player", "TakeDamage", "Add", "Float", -200, 0)
```

### add_comparison_node(path, graph, comparison, type, x, y)
Add comparison node.

**Parameters:**
- `comparison`: "Greater", "Less", "GreaterEqual", "LessEqual", "Equal", "NotEqual"
- `type`: "Float", "Int", "Double", "Vector"

**Returns:** node_id (str - GUID)

### add_function_call_node(path, graph, class, func, x, y)
Add any function call node.

**Parameters:**
- `class`: "KismetMathLibrary", "KismetSystemLibrary", "GameplayStatics", etc.
- `func`: Function name

**Returns:** node_id (str - GUID)

**Example:**
```python
import unreal

# Add PrintString node (alternative to add_print_string_node)
node = unreal.BlueprintService.add_function_call_node(
    "/Game/BP_Player",
    "TestFunc",
    "KismetSystemLibrary",
    "PrintString",
    -300, 0
)
```

---

## Nodes - Graph Operations

### get_nodes_in_graph(path, graph)
Get all nodes with IDs and pins. **Use this to discover pin names.**

**Returns:** Array of node info structs

**Example:**
```python
import unreal

nodes = unreal.BlueprintService.get_nodes_in_graph("/Game/BP_Player", "TakeDamage")
for node in nodes:
    print(f"Node {node.node_id}: {node.node_type}")
    for pin in node.pins:
        print(f"  Pin: {pin.name} ({pin.direction})")
```

### get_node_pins(path, graph, node_id)
Get detailed pin info for a node.

### get_node_details(path, graph, node_id)
Get comprehensive node info (pins with connections, properties).

**Returns:** BlueprintNodeDetailedInfo or None

### connect_nodes(path, graph, src_id, src_pin, tgt_id, tgt_pin)
Connect two nodes.

**Example:**
```python
import unreal

# Entry node (ID 0) "then" → Branch node (ID 1) "execute"
unreal.BlueprintService.connect_nodes("/Game/BP_Player", "TakeDamage", 0, "then", 1, "execute")

# Get variable (ID 2) "Health" → Branch (ID 1) "Condition"
unreal.BlueprintService.connect_nodes("/Game/BP_Player", "TakeDamage", 2, "Health", 1, "Condition")
```

### get_connections(path, graph)
Get all connections in a graph.

### disconnect_pin(path, graph, node_id, pin_name)
Disconnect a pin.

### delete_node(path, graph, node_id)
Delete a node.

### set_node_pin_value(path, graph, node_id, pin_name, value)
Set a pin's default value.

**Example:**
```python
import unreal

# Set default print string
unreal.BlueprintService.set_node_pin_value("/Game/BP_Player", "TestFunc", 5, "InString", "Hello World")
```

### configure_node(path, graph, node_id, property_name, value)
Set internal node properties.

**Example:**
```python
import unreal

# Configure SpawnActor node's ActorClass property
unreal.BlueprintService.configure_node("/Game/BP_GameMode", "SpawnEnemy", 10, "ActorClass", "/Game/BP_Enemy")
```

---

## Nodes - Discovery & Advanced

### discover_nodes(path, search_term, category, max_results)
Search for available node types.

**Returns:** Array of node info with spawner keys

**Example:**
```python
import unreal

# Find clamp nodes
nodes = unreal.BlueprintService.discover_nodes("/Game/BP_Player", "Clamp", "", 10)
for n in nodes:
    print(f"{n.name}: {n.spawner_key}")
```

### create_node_by_key(path, graph, spawner_key, x, y)
Create node using discovered spawner key.

**Returns:** node_id (int)

### split_pin(path, graph, node_id, pin_name)
Split struct pin into member pins (FVector → X, Y, Z).

**Example:**
```python
import unreal

# Split a Vector pin
unreal.BlueprintService.split_pin("/Game/BP_Player", "Movement", 5, "ReturnValue")
# Now can connect to "ReturnValue_X", "ReturnValue_Y", "ReturnValue_Z"
```

### recombine_pin(path, graph, node_id, pin_name)
Recombine split pins back to struct.

### refresh_node(path, graph, node_id, compile=True)
Refresh node after function signature changes.

---

## Common Pin Names

| Node Type | Input Pins | Output Pins |
|-----------|------------|-------------|
| Function Entry | — | `then`, parameter names |
| Function Result | `execute`, return params | — |
| Branch | `execute`, `Condition` | `then`, `else` |
| Variable Get | `self` | variable name |
| Variable Set | `execute`, variable name | `then`, `Output_Get` |
| Math/Comparison | `A`, `B`, `self` | `ReturnValue` |
| Clamp | `Value`, `Min`, `Max` | `ReturnValue` |
| Print String | `execute`, `InString`, ... | `then` |
