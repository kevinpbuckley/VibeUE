# Blueprint Node Positioning Best Practices

## Overview

Proper node positioning is critical for creating readable, maintainable Blueprint functions. Well-organized nodes improve comprehension, debugging, and collaboration.

## Core Positioning Principles

### 1. Left-to-Right Flow (CRITICAL)

**Blueprint execution flows left to right** - position nodes to match execution order:

```
[Entry] → [Operation 1] → [Operation 2] → [Operation 3] → [Return]
  X:0       X:300          X:600          X:900          X:1200
```

**Never position nodes in reverse execution order** - this creates confusion and makes logic hard to follow.

### 2. Consistent Horizontal Spacing

**Standard Spacing:**
- **Simple nodes** (Get, Set, Math): 250-300 units apart
- **Function calls**: 300-400 units apart
- **Flow control** (Branch, Switch): 400-500 units apart
- **Large nodes** (Cast, Complex functions): 500-600 units apart

**Example:**
```python
# Clean, consistent spacing
positions = {
    "Entry": [0, 100],
    "Get Health": [300, 100],
    "Greater Than": [600, 100],
    "Branch": [900, 100],
    "Print String": [1300, 50],   # True path
    "Return": [1300, 150]          # False path
}
```

### 3. Vertical Alignment by Type

**Group related operations vertically:**

```
Y:0    [Main execution line - primary flow]
Y:100  [Data fetching - variable gets]
Y:200  [Calculations - math operations]
Y:300  [Conditional branches - alternate paths]
```

**Example:**
```python
# Function Entry at Y:0
[0, 0]: Function Entry
[300, 0]: Main Operation 1
[600, 0]: Main Operation 2

# Variable Gets at Y:100
[50, 100]: Get Health
[350, 100]: Get MaxHealth

# Calculations at Y:200
[600, 200]: Divide (Health / MaxHealth)
[900, 200]: Multiply (Result * 100)
```

### 4. Branch Spacing (CRITICAL)

**True/False paths must be clearly separated:**

```
                [Branch]
                   |
        +----------+----------+
        |                     |
     Y:-100              Y:+100
   [True Path]        [False Path]
```

**Standard Branch Layout:**
- **Branch node**: Base Y position (e.g., Y:100)
- **True path**: Y - 100 (e.g., Y:0)
- **False path**: Y + 100 (e.g., Y:200)
- **Horizontal offset**: +400 from branch for both paths

**Example:**
```python
manage_blueprint_node(action="create", node_type="Branch", position=[900, 100])

# True path - Y offset -100
manage_blueprint_node(action="create", node_type="Print String", position=[1300, 0])

# False path - Y offset +100  
manage_blueprint_node(action="create", node_type="Return", position=[1300, 200])
```

### 5. Execution Pin Alignment

**Align execution pins horizontally** when nodes connect sequentially:

```
❌ WRONG - Misaligned:
[Node 1]    [Node 2]
  Y:0         Y:50      ← Different Y values create diagonal lines

✅ CORRECT - Aligned:
[Node 1]    [Node 2]
  Y:100       Y:100     ← Same Y creates clean horizontal line
```

### 6. Data Flow Organization

**Position data nodes near their consumers:**

```
[Get Variable]
      |
      ↓ (short connection)
[Function Call that uses variable]
```

**Not scattered across the graph:**

```
❌ WRONG:
[Get Variable] ← Far away, long messy connection line
      |
      |
      |--------------------------------------→ [Function Call]
```

## Standard Layouts by Pattern

### Simple Linear Function

```
Entry → Operation1 → Operation2 → Return

X: 0      300         600         900
Y: 100    100         100         100
```

**Code:**
```python
positions = {
    "entry": [0, 100],
    "operation1": [300, 100],
    "operation2": [600, 100],
    "return": [900, 100]
}
```

### Function with Variables

```
         Entry
           |
   +-------+-------+
   |               |
Get Var1      Get Var2
   |               |
   +-------+-------+
           |
      Operation
           |
         Return

Entry:     [0, 0]
Get Var1:  [50, 100]
Get Var2:  [250, 100]
Operation: [150, 200]
Return:    [150, 300]
```

**Code:**
```python
positions = {
    "entry": [0, 0],
    "get_var1": [50, 100],
    "get_var2": [250, 100],
    "operation": [150, 200],
    "return": [150, 300]
}
```

### Branch Pattern (CRITICAL)

```
Entry → GetValue → Branch → TruePath → Merge
  |                  |          |          |
  0,100          300,100    700,0      1100,100
                              |
                         FalsePath
                              |
                          700,200
```

**Code:**
```python
# Standard branch layout
positions = {
    "entry": [0, 100],
    "get_value": [300, 100],
    "branch": [600, 100],
    
    # True path - Y offset -100
    "true_action": [1000, 0],
    
    # False path - Y offset +100
    "false_action": [1000, 200],
    
    # Merge point (if needed)
    "merge": [1400, 100]
}
```

### Nested Branches

```
Entry → Branch1 → True1 → Branch2 → True2
         |          |                  |
       600,200    1000,100          1400,0
         |                             |
      False1                        False2
         |                             |
      1000,300                     1400,200
```

**Code:**
```python
positions = {
    "entry": [0, 200],
    "branch1": [600, 200],
    
    # First branch - True path
    "true1_action": [1000, 100],
    "branch2": [1400, 100],
    
    # Nested branch - True path
    "true2_action": [1800, 0],
    
    # Nested branch - False path
    "false2_action": [1800, 200],
    
    # First branch - False path
    "false1_action": [1000, 300]
}
```

### Function Call with Multiple Inputs

```
      GetVar1   GetVar2   GetVar3
         |         |         |
         +----+----+----+----+
              |
         FunctionCall
              |
            Return

GetVar1: [50, 0]
GetVar2: [250, 0]
GetVar3: [450, 0]
Function: [250, 100]
Return: [250, 200]
```

**Code:**
```python
positions = {
    "get_var1": [50, 0],
    "get_var2": [250, 0],
    "get_var3": [450, 0],
    "function_call": [250, 100],
    "return": [250, 200]
}
```

## Position Calculation Helpers

### X Position by Execution Order

```python
def calculate_x_position(execution_order: int, spacing: int = 300) -> int:
    """Calculate X position based on execution order."""
    return execution_order * spacing

# Example:
x_positions = {
    "entry": calculate_x_position(0),      # 0
    "operation1": calculate_x_position(1), # 300
    "operation2": calculate_x_position(2), # 600
    "return": calculate_x_position(3)      # 900
}
```

### Branch Y Offsets

```python
def calculate_branch_positions(branch_x: int, branch_y: int, offset: int = 100):
    """Calculate positions for branch true/false paths."""
    return {
        "branch": [branch_x, branch_y],
        "true_path": [branch_x + 400, branch_y - offset],
        "false_path": [branch_x + 400, branch_y + offset]
    }

# Example:
branch_positions = calculate_branch_positions(600, 100)
# Returns: {"branch": [600, 100], "true_path": [1000, 0], "false_path": [1000, 200]}
```

### Center Multiple Data Nodes

```python
def center_data_nodes(consumer_x: int, consumer_y: int, num_nodes: int, spacing: int = 200):
    """Center multiple data nodes above consumer."""
    total_width = (num_nodes - 1) * spacing
    start_x = consumer_x - (total_width // 2)
    
    return [
        [start_x + (i * spacing), consumer_y - 100]
        for i in range(num_nodes)
    ]

# Example: 3 Get Variable nodes feeding into function at [600, 200]
data_positions = center_data_nodes(600, 200, 3)
# Returns: [[400, 100], [600, 100], [800, 100]]
```

## Common Anti-Patterns to Avoid

### ❌ Reverse Flow (CRITICAL ERROR)

```
❌ WRONG - Execution flows right to left:
[Return] ← [Operation 2] ← [Operation 1] ← [Entry]
 X:900      X:600           X:300           X:0

✅ CORRECT - Execution flows left to right:
[Entry] → [Operation 1] → [Operation 2] → [Return]
 X:0       X:300           X:600           X:900
```

### ❌ Inconsistent Spacing

```
❌ WRONG - Random spacing:
[Entry] → [Op1] →     [Op2] →           [Return]
 X:0       X:50        X:400             X:1200

✅ CORRECT - Consistent spacing:
[Entry] → [Op1] → [Op2] → [Return]
 X:0       X:300   X:600   X:900
```

### ❌ Crossed Connections

```
❌ WRONG - Lines cross:
[Node1]     [Node3]
   \           /
    \         /
     X  X  X
    /         \
   /           \
[Node2]     [Node4]

✅ CORRECT - Clear paths:
[Node1] → [Node2]
[Node3] → [Node4]
```

### ❌ Vertical Execution Flow

```
❌ WRONG - Vertical flow is confusing:
[Entry]
   ↓
[Operation 1]
   ↓
[Operation 2]
   ↓
[Return]

✅ CORRECT - Horizontal flow is standard:
[Entry] → [Operation 1] → [Operation 2] → [Return]
```

### ❌ Misaligned Branches

```
❌ WRONG - Branches at same Y level:
                [Branch]
                   |
        +----------+----------+
        |                     |
      Y:100               Y:100  ← Same Y - paths overlap!
   [True Path]        [False Path]

✅ CORRECT - Branches separated vertically:
                [Branch]
                   |
        +----------+----------+
        |                     |
      Y:0                 Y:200  ← Different Y - clear separation
   [True Path]        [False Path]
```

## Grid System

**Use a consistent grid for alignment:**

- **X Grid**: 50-unit increments (0, 50, 100, 150, 200...)
- **Y Grid**: 50-unit increments (0, 50, 100, 150, 200...)
- **Major Grid**: 100-unit increments for primary alignment

**Snap positions to grid:**
```python
def snap_to_grid(x: int, y: int, grid_size: int = 50) -> tuple:
    """Snap position to grid."""
    return (
        round(x / grid_size) * grid_size,
        round(y / grid_size) * grid_size
    )

# Example:
position = snap_to_grid(347, 182)  # Returns (350, 200)
```

## Documentation Pattern

**Document your layout decisions:**

```python
# RandomInteger Function Layout
# Entry (0, 100) - Function entry point
# Get MinValue (300, 50) - Fetch minimum bound
# Get MaxValue (300, 150) - Fetch maximum bound  
# RandomIntegerInRange (600, 100) - Generate random number
# Set Result (900, 100) - Store output
# Return (1200, 100) - Exit function

positions = {
    "entry": [0, 100],
    "get_min": [300, 50],
    "get_max": [300, 150],
    "random": [600, 100],
    "set_result": [900, 100],
    "return": [1200, 100]
}
```

## AI Node Positioning Workflow

### Step 1: Plan the Layout

```python
# 1. List all nodes in execution order
execution_order = [
    "FunctionEntry",
    "GetHealth",
    "GetMaxHealth",
    "Divide",
    "Multiply",
    "GreaterThan",
    "Branch",
    "PrintSuccess",    # True path
    "PrintFailure",    # False path
    "ReturnNode"
]

# 2. Identify branches and data nodes
branches = ["Branch"]
data_nodes = ["GetHealth", "GetMaxHealth"]
```

### Step 2: Calculate Base Positions

```python
# 3. Calculate X positions (300 units apart)
base_x = 0
spacing = 300

positions = {}
for i, node in enumerate(execution_order):
    if node not in ["PrintSuccess", "PrintFailure"]:  # Handle branches separately
        positions[node] = [base_x + (i * spacing), 100]
```

### Step 3: Adjust for Branches

```python
# 4. Adjust branch paths
positions["Branch"] = [900, 100]
positions["PrintSuccess"] = [1300, 0]     # True: Y - 100
positions["PrintFailure"] = [1300, 200]   # False: Y + 100
positions["ReturnNode"] = [1700, 100]     # Merge point
```

### Step 4: Position Data Nodes

```python
# 5. Move data nodes above consumers
positions["GetHealth"] = [550, 0]      # Above Divide
positions["GetMaxHealth"] = [650, 0]   # Above Divide
```

### Step 5: Create Nodes with Positions

```python
# 6. Create all nodes with calculated positions
for node_name, position in positions.items():
    manage_blueprint_node(
        blueprint_name="/Game/Blueprints/BP_Player",
        action="create",
        node_type=node_name,
        position=position,
        graph_scope="function",
        function_name="CalculateHealthPercent"
    )
```

## Quality Checklist

Use this checklist before finalizing node positions:

- [ ] **Left-to-right flow**: Execution flows left to right
- [ ] **Consistent spacing**: Nodes evenly spaced (250-400 units)
- [ ] **Aligned execution**: Sequential nodes at same Y level
- [ ] **Branch separation**: True/False paths clearly separated (±100 Y)
- [ ] **Data proximity**: Get nodes near consumers
- [ ] **No crossings**: Connection lines don't cross
- [ ] **Grid alignment**: Positions snap to 50-unit grid
- [ ] **Readable**: Layout clear at default zoom level
- [ ] **Documented**: Layout decisions commented in code
- [ ] **Tested**: Function compiles without errors

## Real-World Example: RandomInteger Function

**From Challenge Documentation:**

```python
# CORRECT Layout (from correct_layout.md)
correct_positions = {
    "FunctionEntry": [0, 100],
    "GetMinValue": [300, 50],       # Above consumer
    "GetMaxValue": [300, 150],      # Above consumer
    "RandomInteger": [600, 100],    # Center alignment
    "SetResult": [900, 100],        # Sequential flow
    "ReturnNode": [1200, 100]       # End of flow
}

# Key Principles Applied:
# ✅ Left-to-right flow (0 → 300 → 600 → 900 → 1200)
# ✅ Consistent 300-unit spacing
# ✅ Data nodes (Get) positioned above consumer (Random)
# ✅ Main execution line at Y:100
# ✅ Grid-aligned positions
```

**WRONG Layout (from wrong_layout.md):**

```python
# ❌ Anti-patterns shown
wrong_positions = {
    "FunctionEntry": [0, 100],
    "GetMinValue": [300, 50],
    "GetMaxValue": [300, 150],
    "RandomInteger": [800, 100],     # ❌ Inconsistent spacing (500 vs 300)
    "SetResult": [1000, 100],        # ❌ Only 200 units (should be 300)
    "ReturnNode": [1400, 100]        # ❌ 400 units (should be 300)
}
```

## Pro Tips

1. **Start with Entry at [0, 100]**: Standard starting position
2. **Use Y:100 for main flow**: Leaves room above (Y:0+) and below (Y:200+)
3. **Document complex layouts**: Add comments explaining positioning logic
4. **Test at default zoom**: Layout should be clear without zooming
5. **Consistent branch offsets**: Always ±100 for true/false paths
6. **Round to 50s**: Use 50-unit increments for clean alignment
7. **Plan before creating**: Calculate all positions before creating nodes
8. **Follow execution order**: Position nodes in the order they execute

## See Also

- **get_help(topic="node-tools")** - Node creation and connections
- **get_help(topic="blueprint-workflow")** - Complete Blueprint workflow
- **get_help(topic="troubleshooting")** - Node-related issues
