# Blueprint Workflows

Common patterns for working with Blueprints.

---

## Create Blueprint with Variables

```python
import unreal

# 1. Check if blueprint exists
existing = unreal.AssetDiscoveryService.find_asset_by_path("/Game/BP_Player")
if existing:
    print("Blueprint already exists")
else:
    # 2. Create blueprint
    path = unreal.BlueprintService.create_blueprint("Player", "Character", "/Game/")

    # 3. Add variables
    unreal.BlueprintService.add_variable(path, "Health", "float", "100.0")
    unreal.BlueprintService.add_variable(path, "MaxHealth", "float", "100.0")
    unreal.BlueprintService.add_variable(path, "IsAlive", "bool", "true")
    
    # 3b. Set metadata (category/tooltip) via modify_variable
    unreal.BlueprintService.modify_variable(path, "MaxHealth", new_category="Stats")

    # 4. REQUIRED: Compile before using variables in nodes
    unreal.BlueprintService.compile_blueprint(path)

    # 5. Save
    unreal.EditorAssetLibrary.save_asset(path)

    print(f"Created blueprint: {path}")
```

---

## Create Function with Logic

```python
import unreal

bp_path = "/Game/BP_Player"
func_name = "TakeDamage"

# 1. Create function
unreal.BlueprintService.create_function(bp_path, func_name, is_pure=False)

# 2. Add input parameters
unreal.BlueprintService.add_function_input(bp_path, func_name, "Amount", "float", "0.0")

# 3. Add output parameters
unreal.BlueprintService.add_function_output(bp_path, func_name, "NewHealth", "float")

# 4. Add local variables if needed
unreal.BlueprintService.add_function_local_variable(bp_path, func_name, "TempDamage", "float", "0.0")

# 5. REQUIRED: Compile before adding nodes
unreal.BlueprintService.compile_blueprint(bp_path)

# 6. Add nodes (save returned IDs!)
entry_node = 0  # Entry node is always ID 0
get_health = unreal.BlueprintService.add_get_variable_node(bp_path, func_name, "Health", -400, 0)
get_amount = unreal.BlueprintService.add_get_variable_node(bp_path, func_name, "Amount", -400, 100)
subtract = unreal.BlueprintService.add_math_node(bp_path, func_name, "Subtract", "Float", -200, 0)
set_health = unreal.BlueprintService.add_set_variable_node(bp_path, func_name, "Health", 0, 0)
result_node = 1  # Result node is always ID 1

# 7. Get node details to discover pin names
nodes = unreal.BlueprintService.get_nodes_in_graph(bp_path, func_name)
for node in nodes:
    print(f"Node {node.node_id}: {node.node_type}")
    for pin in node.pins:
        print(f"  {pin.name} ({pin.direction})")

# 8. Connect nodes
# Entry → Set Health (execution)
unreal.BlueprintService.connect_nodes(bp_path, func_name, entry_node, "then", set_health, "execute")

# Get Health → Subtract (A input)
unreal.BlueprintService.connect_nodes(bp_path, func_name, get_health, "Health", subtract, "A")

# Get Amount → Subtract (B input)
unreal.BlueprintService.connect_nodes(bp_path, func_name, get_amount, "Amount", subtract, "B")

# Subtract → Set Health (value input)
unreal.BlueprintService.connect_nodes(bp_path, func_name, subtract, "ReturnValue", set_health, "Health")

# Set Health → Result (execution)
unreal.BlueprintService.connect_nodes(bp_path, func_name, set_health, "then", result_node, "execute")

# Set Health output → Result output
unreal.BlueprintService.connect_nodes(bp_path, func_name, set_health, "Output_Get", result_node, "NewHealth")

# 9. Compile
unreal.BlueprintService.compile_blueprint(bp_path)

# 10. Save
unreal.EditorAssetLibrary.save_asset(bp_path)
```

---

## Inspect Blueprint

```python
import unreal

bp_path = "/Game/BP_Player"

# 1. Get overview
info = unreal.BlueprintService.get_blueprint_info(bp_path)
print(f"Parent: {info.parent_class}")
print(f"Variables: {len(info.variables)}")
print(f"Functions: {len(info.functions)}")

# 2. List variables
vars = unreal.BlueprintService.list_variables(bp_path)
for v in vars:
    print(f"Variable: {v.variable_name} ({v.variable_type})")  # Use .variable_name NOT .name

# 3. List functions
funcs = info.functions  # Use the array from get_blueprint_info
for f in funcs:
    print(f"Function: {f.function_name}")  # Use .function_name NOT .name

# 4. Get function details
func_info = unreal.BlueprintService.get_function_info(bp_path, "TakeDamage")
if func_info:
    print(f"Function: {func_info.function_name}")  # Use .function_name NOT .name
    print(f"  Inputs: {len(func_info.input_parameters)}")  # Use len() on array
    print(f"  Outputs: {len(func_info.output_parameters)}")  # Use len() on array
    print(f"  Locals: {len(func_info.local_variables)}")  # Use len() on array
    print(f"  Nodes: {func_info.node_count}")

# 5. Get nodes in function
nodes = unreal.BlueprintService.get_nodes_in_graph(bp_path, "TakeDamage")
for node in nodes:
    print(f"Node {node.node_id}: {node.node_type} at ({node.x}, {node.y})")

# 6. Get connections
connections = unreal.BlueprintService.get_connections(bp_path, "TakeDamage")
for conn in connections:
    print(f"{conn.source_node}:{conn.source_pin} → {conn.target_node}:{conn.target_pin}")
```

---

## Modify Variable Properties

```python
import unreal

bp_path = "/Game/BP_Player"

# 1. Get current info
var_info = unreal.BlueprintService.get_variable_info(bp_path, "Health")
if var_info:
    print(f"Current category: {var_info.category}")
    print(f"Current tooltip: {var_info.tooltip}")

# 2. Rename variable
unreal.BlueprintService.modify_variable(bp_path, "Health", new_name="CurrentHealth")

# 3. Change category
unreal.BlueprintService.modify_variable(bp_path, "CurrentHealth", new_category="Stats")

# 4. Add tooltip
unreal.BlueprintService.modify_variable(bp_path, "CurrentHealth", new_tooltip="Current health value")

# 5. Make editable in Details panel
unreal.BlueprintService.modify_variable(bp_path, "CurrentHealth", set_instance_editable=1)

# 6. Enable replication
unreal.BlueprintService.modify_variable(bp_path, "CurrentHealth", new_replication_condition="Replicated")

# 7. Compile and save
unreal.BlueprintService.compile_blueprint(bp_path)
unreal.EditorAssetLibrary.save_asset(bp_path)
```

---

## Add Component with Properties

```python
import unreal

bp_path = "/Game/BP_Enemy"

# 1. Search for component type
comps = unreal.BlueprintService.get_available_components("StaticMesh", 10)
print(f"Found {len(comps)} mesh components")

# 2. Get component details
comp_info = unreal.BlueprintService.get_component_info("StaticMeshComponent")
if comp_info:
    print(f"Component: {comp_info.name}")

# 3. Check if component already exists
existing_comps = unreal.BlueprintService.list_components(bp_path)
if not any(c.name == "BodyMesh" for c in existing_comps):
    # 4. Add component
    unreal.BlueprintService.add_component(bp_path, "StaticMeshComponent", "BodyMesh")

    # 5. Add child component
    unreal.BlueprintService.add_component(bp_path, "PointLightComponent", "Glow", "BodyMesh")

    # 6. Compile
    unreal.BlueprintService.compile_blueprint(bp_path)

# 7. Set component properties
unreal.BlueprintService.set_component_property(bp_path, "BodyMesh", "bVisible", "true")
unreal.BlueprintService.set_component_property(bp_path, "BodyMesh", "RelativeLocation", "(X=0,Y=0,Z=50)")

# 8. Set child component properties
unreal.BlueprintService.set_component_property(bp_path, "Glow", "Intensity", "5000.0")
unreal.BlueprintService.set_component_property(bp_path, "Glow", "LightColor", "(R=1.0,G=0.0,B=0.0,A=1.0)")

# 9. Save
unreal.EditorAssetLibrary.save_asset(bp_path)
```

---

## Discover and Create Custom Nodes

```python
import unreal

bp_path = "/Game/BP_Player"
func_name = "CalculateDistance"

# 1. Discover nodes by search term
nodes = unreal.BlueprintService.discover_nodes(bp_path, "Clamp", "", 10)
for n in nodes:
    print(f"{n.name}: {n.spawner_key}")

# 2. Create node using spawner key
# (Use the spawner_key from discover_nodes result)
node_id = unreal.BlueprintService.create_node_by_key(
    bp_path,
    func_name,
    "K2_CallFunction:Clamp",  # Example key
    -200, 0
)

# 3. Get node details
details = unreal.BlueprintService.get_node_details(bp_path, func_name, node_id)
if details:
    print(f"Node: {details.node_type}")
    for pin in details.pins:
        print(f"  {pin.name}: {pin.type} ({pin.direction})")

# 4. Set pin default values
unreal.BlueprintService.set_node_pin_value(bp_path, func_name, node_id, "Min", "0.0")
unreal.BlueprintService.set_node_pin_value(bp_path, func_name, node_id, "Max", "100.0")

# 5. Compile
unreal.BlueprintService.compile_blueprint(bp_path)
```

---

## Split and Connect Struct Pins

```python
import unreal

bp_path = "/Game/BP_Player"
func_name = "Movement"

# 1. Add a function that returns a Vector
unreal.BlueprintService.create_function(bp_path, func_name)
unreal.BlueprintService.add_function_output(bp_path, func_name, "Location", "Vector")
unreal.BlueprintService.compile_blueprint(bp_path)

# 2. Add a GetActorLocation node (returns Vector)
get_loc = unreal.BlueprintService.add_function_call_node(
    bp_path, func_name, "Actor", "GetActorLocation", -200, 0
)

# 3. Split the ReturnValue pin (Vector → X, Y, Z)
unreal.BlueprintService.split_pin(bp_path, func_name, get_loc, "ReturnValue")

# 4. Now you can connect to individual components
# Connect X component to something
# unreal.BlueprintService.connect_nodes(bp_path, func_name, get_loc, "ReturnValue_X", target_node, "X")

# 5. To recombine the pin later
# unreal.BlueprintService.recombine_pin(bp_path, func_name, get_loc, "ReturnValue")
```

---

## Common Function Classes

When using `add_function_call_node`, use these classes:

- **KismetMathLibrary** - Math operations (Add, Multiply, Sin, Cos, Sqrt, etc.)
- **KismetSystemLibrary** - System functions (PrintString, Delay, DrawDebugLine)
- **KismetStringLibrary** - String operations (Concat, Split, Contains)
- **KismetArrayLibrary** - Array operations (Add, Remove, Find, Length)
- **GameplayStatics** - Game functions (GetPlayerController, SpawnActor, PlaySound)
- **Actor** - Actor functions (GetActorLocation, SetActorLocation, Destroy)

---

## Best Practices

1. **Always check before creating**
   ```python
   existing = unreal.AssetDiscoveryService.find_asset_by_path(path)
   if not existing:
       # Create
   ```

2. **Compile after structure changes**
   ```python
   # After adding variables, functions, components
   unreal.BlueprintService.compile_blueprint(path)
   ```

3. **Get node details to discover pins**
   ```python
   nodes = unreal.BlueprintService.get_nodes_in_graph(path, graph)
   # Inspect pins before connecting
   ```

4. **Save after making changes**
   ```python
   unreal.EditorAssetLibrary.save_asset(path)
   ```

5. **Use discover for complex node types**
   ```python
   nodes = unreal.BlueprintService.discover_nodes(path, "Spawn", "", 10)
   # Use spawner_key to create the exact node you need
   ```
