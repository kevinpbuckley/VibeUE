# Blueprint Node Creation Patterns Guide

**Last Updated:** October 3, 2025  
**Status:** Validated & Production Ready

---

## Quick Reference

This guide contains **VALIDATED, WORKING PATTERNS** for creating Blueprint nodes with the VibeUE MCP tools. All patterns have been tested and confirmed working in the Blueprint Challenge (CastToMicrosubHUD function).

---

## Essential Workflow

### 1. Discovery → 2. Creation → 3. Validation → 4. Connection

```python
# STEP 1: Discover exact node type name
nodes = get_available_blueprint_nodes(
    blueprint_name="BP_Player2",
    category="Variables"  # or search_term="Set Health"
)

# STEP 2: Create with proper node_params
result = manage_blueprint_node(
    blueprint_name="BP_Player2",
    action="create",
    node_type="SET Health",
    node_params={"variable_name": "Health"},  # CRITICAL for variable nodes
    position=[100, 100],
    graph_scope="function",
    function_name="MyFunction"
)

# STEP 3: Validate pin count
if result.get("pin_count", 0) < 5:  # Variable set should have 5 pins
    print("⚠️ Node configuration failed - check node_params")

# STEP 4: Describe to see pins before connecting
details = manage_blueprint_node(
    blueprint_name="BP_Player2",
    action="describe",
    extra={"node_id": result["node_id"]},
    graph_scope="function",
    function_name="MyFunction"
)
```

---

## Variable Set Nodes ✅

### Pattern
```python
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player2",
    action="create",
    node_type="SET Microsub HUD",  # or "Set Microsub HUD"
    node_params={"variable_name": "Microsub HUD"},  # ← REQUIRED!
    position=[1100, 100],
    graph_scope="function",
    function_name="CastToMicrosubHUD"
)
```

### Expected Result
- **Pin Count:** 5
- **Pins:**
  - `execute` (input) - Execution flow in
  - `then` (output) - Execution flow out
  - `Microsub HUD` (input) - **VALUE INPUT** (the critical pin)
  - `Output_Get` (output) - Optional getter

### Common Mistakes
```python
# ❌ WRONG - Creates broken 2-pin node
manage_blueprint_node(
    action="create",
    node_type="Set Health"
    # Missing node_params!
)
# Result: Only execute and then pins, NO value input

# ✅ CORRECT
manage_blueprint_node(
    action="create",
    node_type="SET Health",
    node_params={"variable_name": "Health"}
)
```

### Validation
```python
result = manage_blueprint_node(...)
assert result.get("pin_count") == 5, "Variable set node missing pins!"
```

---

## Variable Get Nodes ✅

### Pattern
```python
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player2",
    action="create",
    node_type="GET Health",  # or "Get Health"
    node_params={"variable_name": "Health"},  # ← REQUIRED!
    position=[100, 50],
    graph_scope="function",
    function_name="CalculateHealth"
)
```

### Expected Result
- **Pin Count:** 2+ (depends on context)
- **Pins:**
  - `Health` (output) - Variable value output
  - Optional: `self` (input) - If context requires

### Common Mistakes
```python
# ❌ WRONG - Generic getter
manage_blueprint_node(
    action="create",
    node_type="Get Health"
    # Missing node_params!
)

# ✅ CORRECT
manage_blueprint_node(
    action="create",
    node_type="GET Health",
    node_params={"variable_name": "Health"}
)
```

---

## Cast Nodes ✅

### Pattern
```python
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player2",
    action="create",
    node_type="Cast To BP_MicrosubHUD",
    node_params={
        "cast_target": "/Game/Blueprints/HUD/BP_MicrosubHUD.BP_MicrosubHUD_C"
    },  # ← REQUIRED! Full path with _C suffix
    position=[800, 100],
    graph_scope="function",
    function_name="CastToMicrosubHUD"
)
```

### Expected Result
- **Pin Count:** 6
- **Pins:**
  - `execute` (input) - Execution flow in
  - `then` (output) - Success execution path
  - `CastFailed` (output) - Failure execution path
  - `Object` (input) - Object to cast
  - `AsBP Microsub HUD` (output) - **TYPED CAST RESULT** (the critical pin)

### Path Format Rules
```python
# Format: /Package/Path/BP_ClassName.BP_ClassName_C
#         └─ Package path ─┘ └─ Same name ─┘ └─ _C suffix

# Examples:
"/Game/Blueprints/HUD/BP_MicrosubHUD.BP_MicrosubHUD_C"
"/Game/Blueprints/Characters/BP_Player.BP_Player_C"
"/Game/UI/WBP_MainMenu.WBP_MainMenu_C"
```

### Finding Blueprint Paths
```python
# Use search_items to find package_path
search_result = search_items(
    search_term="BP_MicrosubHUD",
    asset_type="Blueprint"
)

# Get package_path from results
package_path = search_result["items"][0]["package_path"]
# Example: "/Game/Blueprints/HUD/BP_MicrosubHUD"

# Append .<ClassName>_C
class_name = "BP_MicrosubHUD"
cast_target = f"{package_path}.{class_name}_C"
# Result: "/Game/Blueprints/HUD/BP_MicrosubHUD.BP_MicrosubHUD_C"
```

### Common Mistakes
```python
# ❌ WRONG - Missing _C suffix
node_params={"cast_target": "/Game/Blueprints/HUD/BP_MicrosubHUD.BP_MicrosubHUD"}

# ❌ WRONG - Missing duplicate class name
node_params={"cast_target": "/Game/Blueprints/HUD/BP_MicrosubHUD_C"}

# ❌ WRONG - No node_params
manage_blueprint_node(
    action="create",
    node_type="Cast To BP_MicrosubHUD"
    # Missing node_params!
)

# ✅ CORRECT
node_params={
    "cast_target": "/Game/Blueprints/HUD/BP_MicrosubHUD.BP_MicrosubHUD_C"
}
```

---

## Function Call Nodes

### Standard Pattern (Most Functions)
```python
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player2",
    action="create",
    node_type="Get HUD",
    position=[500, 100],
    graph_scope="function",
    function_name="CastToMicrosubHUD"
)
# No node_params needed for most function calls
```

### Known Issue: Get Player Controller ⚠️

```python
# Creates node with CheatManager context warning
# BUT node works correctly - ReturnValue is proper PlayerController type
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player2",
    action="create",
    node_type="Get Player Controller",
    position=[200, 100]
)

# Workaround: Ignore compiler warning
# C++ plugin fix planned for future release
# Output connections work perfectly
```

**Status:** Known C++ plugin issue - functional workaround exists

---

## Connection Patterns ✅

### Basic Connection
```python
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player2",
    action="connect_pins",
    graph_scope="function",
    function_name="CastToMicrosubHUD",
    extra={
        "connections": [{
            "source_node_id": "F937A5914C523D1AB3532C8C4125C0B7",
            "source_pin_name": "ReturnValue",
            "target_node_id": "64DE8C1B47F7EEDA2B713B8604257954",
            "target_pin_name": "self"
        }]
    }
)
```

### Batch Connections
```python
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player2",
    action="connect_pins",
    graph_scope="function",
    function_name="CastToMicrosubHUD",
    extra={
        "connections": [
            # Connection 1: Entry → Cast
            {
                "source_node_id": "0F493DC2442F527C812CC29EBB510717",
                "source_pin_name": "then",
                "target_node_id": "09A757684371E8C55857BCA31870FF0B",
                "target_pin_name": "execute"
            },
            # Connection 2: Get HUD → Cast Object input
            {
                "source_node_id": "64DE8C1B47F7EEDA2B713B8604257954",
                "source_pin_name": "ReturnValue",
                "target_node_id": "09A757684371E8C55857BCA31870FF0B",
                "target_pin_name": "Object"
            },
            # Connection 3: Cast → Set Variable
            {
                "source_node_id": "09A757684371E8C55857BCA31870FF0B",
                "source_pin_name": "then",
                "target_node_id": "865C611B4E442957858D82B1794EA61F",
                "target_pin_name": "execute"
            },
            # Connection 4: Cast typed output → Set value input
            {
                "source_node_id": "09A757684371E8C55857BCA31870FF0B",
                "source_pin_name": "AsBP Microsub HUD",
                "target_node_id": "865C611B4E442957858D82B1794EA61F",
                "target_pin_name": "Microsub HUD"
            }
        ]
    }
)
```

---

## Complete Example: CastToMicrosubHUD Function

This is the **VALIDATED REFERENCE IMPLEMENTATION** from Blueprint Challenge Phase 4:

```python
# Function structure:
# Entry → Get Player Controller → Get HUD → Cast → Set Variable

# 1. Function Entry (auto-created, no action needed)

# 2. Get Player Controller (has known warning, but works)
pc_result = manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player2",
    action="create",
    node_type="Get Player Controller",
    position=[200, 100],
    graph_scope="function",
    function_name="CastToMicrosubHUD"
)

# 3. Get HUD (standard function call)
hud_result = manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player2",
    action="create",
    node_type="Get HUD",
    position=[500, 100],
    graph_scope="function",
    function_name="CastToMicrosubHUD"
)

# 4. Cast To BP_MicrosubHUD (needs node_params)
cast_result = manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player2",
    action="create",
    node_type="Cast To BP_MicrosubHUD",
    node_params={
        "cast_target": "/Game/Blueprints/HUD/BP_MicrosubHUD.BP_MicrosubHUD_C"
    },
    position=[800, 100],
    graph_scope="function",
    function_name="CastToMicrosubHUD"
)

# 5. Set Microsub HUD (needs node_params)
set_result = manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player2",
    action="create",
    node_type="SET Microsub HUD",
    node_params={"variable_name": "Microsub HUD"},
    position=[1100, 100],
    graph_scope="function",
    function_name="CastToMicrosubHUD"
)

# 6. Validate all nodes created correctly
assert pc_result.get("pin_count") == 2   # Get Player Controller
assert hud_result.get("pin_count") >= 2  # Get HUD  
assert cast_result.get("pin_count") == 6 # Cast node
assert set_result.get("pin_count") == 5  # Set variable

# 7. Connect all nodes (get node_ids from results first)
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player2",
    action="connect_pins",
    graph_scope="function",
    function_name="CastToMicrosubHUD",
    extra={
        "connections": [
            # Get Player Controller → Get HUD
            {
                "source_node_id": pc_result["node_id"].replace("{", "").replace("}", "").replace("-", ""),
                "source_pin_name": "ReturnValue",
                "target_node_id": hud_result["node_id"].replace("{", "").replace("}", "").replace("-", ""),
                "target_pin_name": "self"
            },
            # Get HUD → Cast
            {
                "source_node_id": hud_result["node_id"].replace("{", "").replace("}", "").replace("-", ""),
                "source_pin_name": "ReturnValue",
                "target_node_id": cast_result["node_id"].replace("{", "").replace("}", "").replace("-", ""),
                "target_pin_name": "Object"
            },
            # Cast execution → Set execution
            {
                "source_node_id": cast_result["node_id"].replace("{", "").replace("}", "").replace("-", ""),
                "source_pin_name": "then",
                "target_node_id": set_result["node_id"].replace("{", "").replace("}", "").replace("-", ""),
                "target_pin_name": "execute"
            },
            # Cast typed output → Set value input
            {
                "source_node_id": cast_result["node_id"].replace("{", "").replace("}", "").replace("-", ""),
                "source_pin_name": "AsBP Microsub HUD",
                "target_node_id": set_result["node_id"].replace("{", "").replace("}", "").replace("-", ""),
                "target_pin_name": "Microsub HUD"
            }
        ]
    }
)

print("✅ CastToMicrosubHUD function created successfully!")
```

---

## Troubleshooting

### Problem: Variable Set Node Has Only 2 Pins

**Cause:** Missing `node_params.variable_name`

**Solution:**
```python
# Add node_params with variable_name
node_params={"variable_name": "VariableName"}
```

### Problem: Cast Node Shows Generic Object Type

**Cause:** Missing `node_params.cast_target` or wrong path format

**Solution:**
```python
# Use full path with _C suffix
node_params={
    "cast_target": "/Game/Blueprints/ClassName.ClassName_C"
}
```

### Problem: Function Shows Wrong Target Context

**Cause:** Known C++ plugin issue (e.g., Get Player Controller)

**Solution:**
- Use node anyway - output type is correct
- Ignore compiler warning
- C++ fix planned for future release

### Problem: Connections Fail with "Pin Not Found"

**Cause:** Node created without proper node_params

**Solution:**
1. Delete broken node
2. Recreate with node_params
3. Verify pin count before connecting
4. Use `describe` action to inspect pins

---

## Validation Checklist

Before connecting nodes, verify:

- [ ] Variable Set nodes have 5 pins
- [ ] Variable Get nodes have value output pin
- [ ] Cast nodes have 6 pins with typed output
- [ ] Node GUIDs obtained from creation results
- [ ] Pin names verified with `describe` action
- [ ] All required `node_params` included

---

## See Also

- `node_tools.py` - Tool implementation
- `node-tools-improvements.md` - Design document and investigation
- `help.md` - Complete MCP tools reference
- Blueprint Challenge Phase 4 - Working implementation example

---

**END OF GUIDE**
