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
| Function Entry | "then" (exec output), INPUT parameter names (as outputs from entry) |
| Function Return | "execute" (exec input), OUTPUT parameter names (as inputs to return) |
| Function Call | "self" (target), "ReturnValue", "execute" |
| Cast Node | "execute", "then", "CastFailed", "Object", "As[ClassName]" |
| Variable Get | Variable name (output) |
| Variable Set | "execute", "then", Variable name (input), "Output_Get" |
| Branch | "execute", "Condition", "True", "False" |

### Understanding Function Entry vs Return Pins

**Function Entry Node (FIRST node in function):**
- Contains **INPUT parameters as OUTPUT pins**
- Why? Because the entry node OUTPUTS the values that were passed INTO the function
- Example: If function has `Low` and `High` input parameters:
  - Entry node has `Low` and `High` as **output pins** (data flows OUT of entry)
  - These connect TO the logic nodes that use them

**Function Return Node (LAST node in function):**
- Contains **OUTPUT parameters as INPUT pins**
- Why? Because the return node receives (inputs) the values to send OUT of the function
- Example: If function returns `NewTemp` output parameter:
  - Return node has `NewTemp` as an **input pin** (data flows INTO return)
  - Logic nodes connect their results TO this input pin

**Connection Flow:**
```
Entry.Low (output) ──→ RandomInteger.Min (input)
Entry.High (output) ──→ RandomInteger.Max (input)
RandomInteger.ReturnValue (output) ──→ Return.NewTemp (input)
```

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
| **Nodes flow right-to-left** | **Position X decreasing** | **Use increasing X values (left-to-right flow)** |
| **Messy zigzag connections** | **Inconsistent Y coordinates** | **Keep main flow nodes at same Y value** |
| **Unreadable node layout** | **Poor position planning** | **Calculate positions before creating nodes** |
| **Pin default values not set** | **Configure action syntax unclear** | **Set values manually in editor Details panel** |

## Node Positioning & Layout

### Understanding Node Coordinates

**Coordinate System:**
- Origin (0, 0) is typically at the top-left of the graph canvas
- X increases to the RIGHT (positive = rightward)
- Y increases DOWNWARD (positive = downward)

**Typical Spacing:**
- Horizontal spacing between nodes: 200-400 units
- Vertical spacing between rows: 100-200 units
- Start positions: [0-200, 0-200] for first node

### Event Graph Layout Patterns

**Recommended Left-to-Right Flow:**

```python
# ✅ CORRECT: Readable left-to-right execution flow
manage_blueprint_node(action="create", position=[0, 100])      # BeginPlay event
manage_blueprint_node(action="create", position=[300, 100])    # Function call
manage_blueprint_node(action="create", position=[600, 100])    # Variable set
manage_blueprint_node(action="create", position=[900, 100])    # Next action
```

**Visual Result:**
```
[BeginPlay] --> [Function Call] --> [Set Variable] --> [Next Action]
   (0,100)         (300,100)           (600,100)          (900,100)
```

### Function Graph Layout Patterns

**⚠️ CRITICAL FUNCTION NODE ORDERING:**

Blueprint functions MUST follow this strict node order:

1. **Function Entry Node (FIRST)** - Contains INPUT parameters
   - Position: LEFT side of graph [0, 100] or [100, 100]
   - This node outputs the input parameters that were passed TO the function
   - Example pins: "Low", "High", "then" (exec output)

2. **Logic Nodes (MIDDLE)** - Process the data
   - Position: Between entry and return
   - Example: Random Integer in Range, math operations, conditionals

3. **Return Node (LAST)** - Contains OUTPUT parameters
   - Position: RIGHT side of graph [600, 100] or further right
   - This node receives the output values that will be returned FROM the function
   - Example pins: "NewTemp" (receives result), "execute" (exec input)

**Why This Order Matters:**
- Function Entry node has the INPUT parameters (Low, High) that flow INTO the function
- Return node has the OUTPUT parameters (NewTemp) that flow OUT of the function
- Logic nodes in the middle connect entry outputs → processing → return inputs
- This creates proper left-to-right data flow: Inputs → Processing → Outputs

**Function Entry Node:**
- Positioned at LEFT side of graph
- Common position: [0, 0] or [100, 100]
- Contains function INPUT parameters as output pins
- Execution flows OUT from this node (via "then" pin)
- All other nodes flow to the RIGHT from here

**Return Node:**
- Positioned at RIGHT side of graph (after all logic)
- Common position: [600, 100] or further right
- Contains function OUTPUT parameters as input pins
- Execution flows INTO this node (via "execute" pin)
- This is the terminal node of the function

**Pure Function Nodes (no exec pins):**
- Position vertically below the main execution flow
- Connect data outputs upward to main flow

```python
# ✅ CORRECT: Function graph layout with proper node ordering
# 1. FIRST: Function Entry with INPUT parameters (Low, High)
manage_blueprint_node(
    action="create",
    node_params={"spawner_key": "FunctionEntry"},
    position=[0, 100]        # Entry at LEFT - has INPUTS
)

# 2. MIDDLE: Logic nodes that process the inputs
manage_blueprint_node(
    action="create", 
    node_params={"spawner_key": "KismetMathLibrary::RandomIntegerInRange"},
    position=[300, 100]      # Logic nodes in CENTER
)

# 3. LAST: Return Node with OUTPUT parameters (NewTemp)
manage_blueprint_node(
    action="create",
    node_params={"spawner_key": "FunctionReturn"},
    position=[600, 100]      # Return at RIGHT - has OUTPUTS
)
```

**Visual Result:**
```
[Function Entry] --> [Random Integer] --> [Return Node]
     (0,100)            (300,100)           (600,100)
  INPUTS: Low, High   Process inputs     OUTPUTS: NewTemp
  (outputs from entry) (logic)          (inputs to return)
         |                  |                   ^
         v                  v                   |
    Low → ─────────────→ Min                   |
    High → ────────────→ Max                   |
                      ReturnValue → ──────────┘
```

### Common Layout Mistakes

❌ **WRONG: Nodes flowing RIGHT TO LEFT**
```python
manage_blueprint_node(action="create", position=[800, 100])  # First node at right
manage_blueprint_node(action="create", position=[500, 100])  # Second node moving left
manage_blueprint_node(action="create", position=[200, 100])  # Third node further left
```
This creates unreadable graphs that flow backward!

❌ **WRONG: Inconsistent vertical alignment**
```python
manage_blueprint_node(action="create", position=[0, 50])
manage_blueprint_node(action="create", position=[300, 200])
manage_blueprint_node(action="create", position=[600, 75])
```
Creates messy zigzag connections.

✅ **CORRECT: Consistent left-to-right, aligned vertically**
```python
manage_blueprint_node(action="create", position=[0, 100])
manage_blueprint_node(action="create", position=[300, 100])
manage_blueprint_node(action="create", position=[600, 100])
```

### Position Parameter Usage

```python
# Basic node creation with position
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="create",
    graph_scope="event",  # or "function"
    function_name="MyFunction",  # required if graph_scope="function"
    node_params={"spawner_key": "..."},
    position=[X, Y]  # ← [horizontal, vertical] coordinates
)
```

### Dynamic Position Calculation

```python
# Calculate positions for sequential nodes
base_x = 0
base_y = 100
spacing = 300

positions = [
    [base_x, base_y],                    # First node: [0, 100]
    [base_x + spacing, base_y],          # Second: [300, 100]
    [base_x + spacing * 2, base_y],      # Third: [600, 100]
    [base_x + spacing * 3, base_y]       # Fourth: [900, 100]
]

# Use in node creation loop
for i, pos in enumerate(positions):
    manage_blueprint_node(
        blueprint_name="/Game/Blueprints/BP_Player",
        action="create",
        node_params={"spawner_key": node_types[i]},
        position=pos
    )
```

### Repositioning Existing Nodes

```python
# Move node to new position
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="move",
    node_id="{GUID}",
    position=[400, 150]  # New coordinates
)
```

### Layout Best Practices Summary

1. **Left-to-Right Flow**: Always flow execution from left to right
2. **Consistent Y-Coordinates**: Keep main flow nodes aligned horizontally
3. **Adequate Spacing**: Use 250-400 units horizontal spacing
4. **Group Related Nodes**: Keep related operations vertically close
5. **Pure Nodes Below**: Place data-only nodes below execution flow
6. **Use describe First**: Check existing node positions before adding
7. **Plan Layout**: Calculate positions before creating multiple nodes

### Example: Complete Function Layout

```python
# RandomTemp function with proper layout and node ordering
# Function signature: RandomTemp(Low: int, High: int) -> NewTemp: int

base_x = 0
base_y = 100
h_spacing = 300

# 1. FIRST: Function Entry (LEFT) - Has INPUT parameters as OUTPUT pins
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="create",
    graph_scope="function",
    function_name="RandomTemp",
    node_params={"spawner_key": "FunctionEntry"},
    position=[base_x, base_y]
)
# This node OUTPUTS: Low, High, then (exec)
# Why? Because these are the inputs TO the function, flowing OUT of the entry

# 2. MIDDLE: Random Integer in Range (CENTER) - Logic processing
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="create",
    graph_scope="function",
    function_name="RandomTemp",
    node_params={"spawner_key": "KismetMathLibrary::RandomIntegerInRange"},
    position=[base_x + h_spacing, base_y]
)
# This node INPUTS: Min, Max
# This node OUTPUTS: ReturnValue

# 3. LAST: Return Node (RIGHT) - Has OUTPUT parameters as INPUT pins
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="create",
    graph_scope="function",
    function_name="RandomTemp",
    node_params={"spawner_key": "FunctionReturn"},
    position=[base_x + h_spacing * 2, base_y]
)
# This node INPUTS: NewTemp, execute (exec)
# Why? Because NewTemp is the output FROM the function, flowing INTO the return

# 4. Connect the nodes (after all are created)
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="connect_pins",
    graph_scope="function",
    function_name="RandomTemp",
    extra={
        "connections": [
            # Connect entry's output pins to random node's input pins
            {"source_node_id": "{ENTRY_GUID}", "source_pin_name": "Low", 
             "target_node_id": "{RANDOM_GUID}", "target_pin_name": "Min"},
            {"source_node_id": "{ENTRY_GUID}", "source_pin_name": "High",
             "target_node_id": "{RANDOM_GUID}", "target_pin_name": "Max"},
            # Connect random node's output to return node's input
            {"source_node_id": "{RANDOM_GUID}", "source_pin_name": "ReturnValue",
             "target_node_id": "{RETURN_GUID}", "target_pin_name": "NewTemp"}
        ]
    }
)
```

**Visual Result with Pin Flow:**
```
[Function Entry]──────→[Random Integer in Range]──────→[Return Node]
     (0,100)                  (300,100)                    (600,100)
  
  OUTPUT pins:            INPUT pins:                  INPUT pins:
  - Low ────────────────→ Min                          
  - High ───────────────→ Max                          
  - then (exec)                                      ← execute (exec)
                          OUTPUT pins:
                          - ReturnValue ──────────────→ NewTemp

REMEMBER: 
- Entry outputs the INPUTs (Low, High flow OUT to be used)
- Return inputs the OUTPUTs (NewTemp flows IN to be returned)
```

## Best Practices

1. ✅ Always use `get_available_blueprint_nodes()` before creating
2. ✅ Use `spawner_key` for exact node creation
3. ✅ Include `node_params` for variable/cast nodes
4. ✅ Use `describe` to verify structure before connecting
5. ✅ Connect nodes immediately after creation
6. ✅ Compile Blueprint after node changes
7. ✅ **Position nodes left-to-right with consistent Y-coordinates**
8. ✅ **Plan node layout before creating multiple nodes**
9. ✅ **Use 250-400 unit horizontal spacing for readability**
10. ✅ **CRITICAL: Function Entry (with INPUT parameters) FIRST, Return (with OUTPUT parameters) LAST**
11. ✅ **Remember: Entry node OUTPUTS the function inputs, Return node INPUTS the function outputs**
