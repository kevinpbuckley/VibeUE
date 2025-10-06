# Node Tools Reference

## Node Discovery

### get_available_blueprint_nodes

**Purpose**: Find exact node types with complete metadata including spawner_keys

```python
nodes = get_available_blueprint_nodes(
    blueprint_name="/Game/Blueprints/BP_Player",
    search_term="Random Integer",
    category="Math"
)
```

**Returns complete descriptors including:**
- `spawner_key`: Unique identifier for exact creation (e.g., "KismetMathLibrary::RandomIntegerInRange")
- `expected_pin_count`: Number of pins the node will have
- `pins`: Complete pin metadata with types and directions
- `function_metadata`: Class path, static status, purity

## Node Creation

### manage_blueprint_node (action="create")

**✅ RECOMMENDED: Use spawner_key for exact node creation**

```python
# 1. Discover with metadata
nodes = get_available_blueprint_nodes(
    blueprint_name="/Game/Blueprints/BP_Player",
    search_term="GetPlayerController"
)

# 2. Use exact spawner_key (NO AMBIGUITY!)
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="create",
    graph_scope="function",
    function_name="MyFunction",
    node_params={"spawner_key": "GameplayStatics::GetPlayerController"},
    position=[200, 100]
)
```

### CRITICAL: node_params Requirements

**Variable Set Nodes** (REQUIRED):
```python
manage_blueprint_node(
    action="create",
    node_type="SET Health",
    node_params={"variable_name": "Health"}  # ⚠️ CRITICAL!
)
# Creates 5-pin node with value input
# Without node_params: Only 2 pins (broken node)
```

**Variable Get Nodes** (REQUIRED):
```python
manage_blueprint_node(
    action="create",
    node_type="GET Health",
    node_params={"variable_name": "Health"}
)
```

**Cast Nodes** (REQUIRED for Blueprint types):
```python
manage_blueprint_node(
    action="create",
    node_type="Cast To BP_MicrosubHUD",
    node_params={
        "cast_target": "/Game/Blueprints/HUD/BP_MicrosubHUD.BP_MicrosubHUD_C"
    }
)
# Format: /Full/Path/BP_Class.BP_Class_C
```

## Pin Connections

### manage_blueprint_node (action="connect_pins")

**✅ CORRECT Format:**

```python
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="connect_pins",
    graph_scope="function",
    function_name="MyFunction",
    extra={
        "connections": [
            {
                "source_node_id": "{F937A594-1C52-3D1A-B353-2C8C4125C0B7}",
                "source_pin_name": "ReturnValue",
                "target_node_id": "{64DE8C1B-47F7-EEDA-2B71-3B8604257954}",
                "target_pin_name": "self"
            }
        ]
    }
)
```

### Getting Node IDs and Pin Names

```python
# Use describe to get node GUIDs and pins
result = manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="describe",
    graph_scope="function",
    function_name="MyFunction"
)

# Result contains nodes with:
# - node_id: "{GUID}" format
# - pins: array with pin_id and name for each pin
```

### Connection Options

Each connection can include:
- `allow_conversion_node`: Auto conversion nodes (default: true)
- `allow_promotion`: Type promotion/array wrapping (default: true)
- `break_existing_links`: Break existing connections (default: true)

## Common Pin Names by Node Type

| Node Type | Pins |
|-----------|------|
| Function Entry | "then" (exec output) |
| Function Call | "self" (target), "ReturnValue", "execute" |
| Cast Node | "execute", "then", "CastFailed", "Object", "As[ClassName]" |
| Variable Get | Variable name (output) |
| Variable Set | "execute", "then", Variable name (input), "Output_Get" |
| Branch | "execute", "Condition", "True", "False" |

## Node Inspection

### manage_blueprint_node (action="describe")

```python
# Describe all nodes in a graph
result = manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="describe",
    graph_scope="function",
    function_name="RandomInteger"
)

# Describe specific node
result = manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="describe",
    extra={"node_id": "{NODE_GUID}"}
)
```

## Troubleshooting

| Problem | Cause | Solution |
|---------|-------|----------|
| Variable Set has only 2 pins | Missing node_params.variable_name | Add variable_name to node_params |
| Cast shows generic Object type | Missing node_params.cast_target | Add full Blueprint path with _C suffix |
| Pins don't match expected | Wrong node_type or missing node_params | Use get_available_blueprint_nodes() first |
| Can't find node type | Using fuzzy search | Use spawner_key from discovery |

## Best Practices

1. ✅ Always use `get_available_blueprint_nodes()` before creating
2. ✅ Use `spawner_key` for exact node creation
3. ✅ Include `node_params` for variable/cast nodes
4. ✅ Use `describe` to verify structure before connecting
5. ✅ Connect nodes immediately after creation
6. ✅ Compile Blueprint after node changes
