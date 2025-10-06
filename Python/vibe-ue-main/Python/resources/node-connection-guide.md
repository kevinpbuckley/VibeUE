# 🔌 Blueprint Node Connection Guide

## ✅ TESTED & VERIFIED Connection System

This guide documents the **correct** way to connect Blueprint nodes using the VibeUE MCP tools, based on actual testing and C++ backend implementation.

---

## 🎯 Quick Reference

### Connection Format (CORRECT)

```python
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player2",  # Full package path
    action="connect_pins",
    graph_scope="function",  # or "event"
    function_name="MyFunction",  # Required for function scope
    extra={
        "connections": [
            {
                "source_node_id": "NODE_GUID_WITHOUT_BRACES",
                "source_pin_name": "PinName",
                "target_node_id": "NODE_GUID_WITHOUT_BRACES",
                "target_pin_name": "PinName"
            }
        ]
    }
)
```

**Key Points:**
- ✅ Use `extra` parameter with `connections` array
- ✅ Node IDs are GUIDs without curly braces
- ✅ Pin names are case-sensitive exact matches
- ✅ Multiple connections can be made in one call

---

## 📋 Step-by-Step Workflow

### Step 1: Create Nodes

```python
# Create function call node
node1 = manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player2",
    action="create",
    graph_scope="function",
    function_name="CastToMicrosubHUD",
    node_type="Get Player Controller",
    position=[200, 100]
)

# Create another function call
node2 = manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player2",
    action="create",
    graph_scope="function",
    function_name="CastToMicrosubHUD",
    node_type="Get HUD",
    position=[500, 100]
)
```

### Step 2: Get Node Details

```python
# Get all nodes and their pins
details = manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player2",
    action="describe",
    graph_scope="function",
    function_name="CastToMicrosubHUD"
)

# Response contains:
# {
#   "nodes": [
#     {
#       "node_id": "{GUID}",
#       "display_name": "Get Player Controller",
#       "pins": [
#         {"pin_id": "GUID:PinName", "name": "ReturnValue", "direction": "output"},
#         ...
#       ]
#     }
#   ]
# }
```

### Step 3: Extract Node IDs and Pin Names

From the `describe` response:
- `node_id`: Use the GUID **without curly braces** for connections
- `pins[].name`: Use the exact pin name

Example:
- Node ID: `{F937A591-4C52-3D1A-B353-2C8C4125C0B7}` → Use: `F937A5914C523D1AB3532C8C4125C0B7`
- Pin name: `ReturnValue` → Use: `ReturnValue`

### Step 4: Connect the Pins

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

### Step 5: Verify Connection

The response will confirm the connection:

```json
{
    "success": true,
    "attempted": 1,
    "succeeded": 1,
    "failed": 0,
    "connections": [{
        "success": true,
        "source_node_id": "{F937A591-4C52-3D1A-B353-2C8C4125C0B7}",
        "target_node_id": "{64DE8C1B-47F7-EEDA-2B71-3B8604257954}",
        "created_links": [...]
    }]
}
```

---

## 🔍 Common Pin Names by Node Type

### Function Entry Node
- `then` - Execution output

### Function Call Nodes
- `self` - Target object input (for member functions)
- `ReturnValue` - Function return value output
- `execute` - Execution input (if not pure)
- Parameter names - Input parameters

### Cast Nodes
- `execute` - Execution input
- `then` - Execution output (success)
- `CastFailed` - Execution output (failed)
- `Object` - Object to cast (input)
- `As[ClassName]` - Cast result output (e.g., "AsBP Microsub HUD")

### Variable Nodes
**Get Variable:**
- Variable name - Variable value output

**Set Variable:**
- `execute` - Execution input
- `then` - Execution output
- Variable name - Variable value input
- `Output_Get` - Variable value output (passthrough)

### Branch Node
- `execute` - Execution input
- `Condition` - Boolean condition input
- `True` - Execution output (condition true)
- `False` - Execution output (condition false)

### Math/Logic Nodes
- Input parameter names (e.g., "A", "B")
- `ReturnValue` - Result output

---

## 💡 Real-World Example: CastToMicrosubHUD Function

This example recreates a complete function with 5 nodes and 4 connections:

### Nodes Created:
1. Function Entry (automatic)
2. Get Player Controller
3. Get HUD
4. Cast To BP_MicrosubHUD
5. Set Microsub HUD (variable setter)

### Complete Implementation:

```python
# Create Get Player Controller
node_getpc = manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player2",
    action="create",
    graph_scope="function",
    function_name="CastToMicrosubHUD",
    node_type="Get Player Controller",
    position=[200, 100]
)

# Create Get HUD
node_gethud = manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player2",
    action="create",
    graph_scope="function",
    function_name="CastToMicrosubHUD",
    node_type="Get HUD",
    position=[500, 100]
)

# Create Cast node (note: requires full Blueprint path)
node_cast = manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player2",
    action="create",
    graph_scope="function",
    function_name="CastToMicrosubHUD",
    node_type="Cast To BP_MicrosubHUD",
    node_params={"cast_target": "/Game/Blueprints/HUD/BP_MicrosubHUD.BP_MicrosubHUD_C"},
    position=[800, 100]
)

# Create Set Variable
node_set = manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player2",
    action="create",
    graph_scope="function",
    function_name="CastToMicrosubHUD",
    node_type="Set Microsub HUD",
    position=[1100, 100]
)

# Get all node details to find GUIDs
nodes = manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player2",
    action="describe",
    graph_scope="function",
    function_name="CastToMicrosubHUD"
)

# Extract node IDs (example - actual IDs from describe response)
entry_id = "0F493DC2442F527C812CC29EBB510717"
getpc_id = "F937A5914C523D1AB3532C8C4125C0B7"
gethud_id = "64DE8C1B47F7EEDA2B713B8604257954"
cast_id = "B3DC700344C6DA7C35FD48B872A57458"
set_id = "92EB08034D81329DE43F509D5FFFB7F3"

# Connect all nodes
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player2",
    action="connect_pins",
    graph_scope="function",
    function_name="CastToMicrosubHUD",
    extra={
        "connections": [
            # Connection 1: Get Player Controller → Get HUD
            {
                "source_node_id": getpc_id,
                "source_pin_name": "ReturnValue",
                "target_node_id": gethud_id,
                "target_pin_name": "self"
            },
            # Connection 2: Function Entry → Cast
            {
                "source_node_id": entry_id,
                "source_pin_name": "then",
                "target_node_id": cast_id,
                "target_pin_name": "execute"
            },
            # Connection 3: Get HUD → Cast (would fail if cast target invalid)
            {
                "source_node_id": gethud_id,
                "source_pin_name": "ReturnValue",
                "target_node_id": cast_id,
                "target_pin_name": "Object"
            },
            # Connection 4: Cast → Set Variable
            {
                "source_node_id": cast_id,
                "source_pin_name": "then",
                "target_node_id": set_id,
                "target_pin_name": "execute"
            }
        ]
    }
)
```

---

## ⚠️ Common Issues & Solutions

### Issue 1: "SOURCE_PIN_NOT_FOUND"
**Cause:** Node ID or pin name is incorrect

**Solution:**
1. Use `action="describe"` to get exact node IDs
2. Remove curly braces from GUIDs
3. Use exact pin names (case-sensitive)

### Issue 2: "CONNECTION_BLOCKED"
**Cause:** Cast node has invalid target class

**Solution:**
- Use full Blueprint class path with `_C` suffix
- Example: `/Game/Blueprints/HUD/BP_MicrosubHUD.BP_MicrosubHUD_C`

### Issue 3: Connection timeout
**Cause:** Trying to connect too many pins at once or invalid cast nodes

**Solution:**
- Connect pins one at a time or in smaller batches
- Verify all nodes are valid before connecting
- Fix any "Bad cast node" issues first

### Issue 4: "No pin identifier or node/pin name provided"
**Cause:** Missing required fields in connection object

**Solution:**
Ensure each connection has all 4 required fields:
- `source_node_id`
- `source_pin_name`
- `target_node_id`
- `target_pin_name`

---

## 🎯 Best Practices

1. **Always use `describe` first** to get exact node IDs and pin names
2. **Strip curly braces** from node GUIDs when using in connections
3. **Use exact pin names** - they are case-sensitive
4. **Test connections incrementally** - connect one or two at a time
5. **Check for "Bad cast node"** issues before connecting
6. **Use full Blueprint paths** for cast target parameters
7. **Compile after connections** to verify the graph is valid

---

## 📊 Connection Success Indicators

A successful connection response includes:

```json
{
    "success": true,
    "attempted": 1,
    "succeeded": 1,
    "failed": 0,
    "connections": [{
        "success": true,
        "already_connected": false,
        "created_links": [
            {
                "from_pin_id": "...",
                "to_pin_id": "...",
                "from_pin_role": "source",
                "to_node_id": "...",
                "to_pin_name": "..."
            }
        ]
    }],
    "modified_graphs": [
        {"graph_name": "FunctionName", "graph_guid": "..."}
    ]
}
```

Key fields:
- `success: true` - Overall operation succeeded
- `succeeded: 1` - Number of successful connections
- `failed: 0` - Number of failed connections
- `created_links` - Array of bidirectional pin links created

---

## 🧪 Testing Connections

After making connections, verify them:

```python
# Get updated node details
result = manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player2",
    action="describe",
    graph_scope="function",
    function_name="MyFunction"
)

# Check pins are connected
for node in result["nodes"]:
    for pin in node["pins"]:
        if pin["is_connected"]:
            print(f"{pin['name']} is connected to:")
            for link in pin.get("links", []):
                print(f"  - {link['pin_name']} on {link['node_id']}")
```

---

## 📝 Summary

**The correct connection workflow:**
1. Create all nodes with proper positions
2. Use `describe` to get node IDs and pin information
3. Extract GUIDs (remove braces) and pin names
4. Use `connect_pins` with `extra` parameter containing `connections` array
5. Verify connections succeeded
6. Compile Blueprint to validate

**Remember:** The connection system uses a **batch format** where you pass an array of connection objects via the `extra` parameter, not individual parameters!
