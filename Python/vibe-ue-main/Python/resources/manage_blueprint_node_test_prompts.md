# manage_blueprint_node Test Prompts

This document provides comprehensive test prompts for all 15 actions of the `manage_blueprint_node` tool.

## Overview

The test prompts demonstrate a complete workflow covering all node management operations, following the critical pattern: **discover → create → configure → connect → manipulate**.

## Test Prerequisites

- Blueprint: `/Game/Blueprints/BP_Player`
- Function: `TestFunction` (to be created)
- All tests assume Unreal Editor is running with VibeUE MCP server connected

## Complete Test Workflow

### 1. discover - Discover Available Node Types

**Purpose**: Find available node types with complete metadata including spawner_key

**Test Prompt**:
```
Use manage_blueprint_node to discover GetPlayerController node variants in BP_Player. 
Show me the different variants available with their spawner keys and expected pin counts.
```

**Expected Tool Call**:
```python
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="discover",
    graph_scope="event",
    extra={
        "search_term": "GetPlayerController",
        "return_descriptors": True
    }
)
```

**Expected Result**:
- Returns multiple variants (GameplayStatics::GetPlayerController, CheatManager::GetPlayerController, etc.)
- Each variant includes spawner_key, expected_pin_count, pins metadata
- Shows function_class and static/instance classification

---

### 2. create - Create New Nodes

**Purpose**: Create nodes using exact spawner_key from discover action

**Test Prompt**:
```
In BP_Player, create a new function called TestFunction. Then create the following nodes using their exact spawner keys:
1. A GameplayStatics::GetPlayerController node at position [200, 100]
2. A GET variable node for "Health" at position [500, 100]
3. A branch node at position [800, 100]

Use the discover action first to get the correct spawner_key for the GetPlayerController node.
```

**Expected Tool Calls**:
```python
# First discover to get spawner_key
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="discover",
    graph_scope="function",
    function_name="TestFunction",
    extra={
        "search_term": "GetPlayerController",
        "return_descriptors": True
    }
)

# Create node using exact spawner_key from discover
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="create",
    graph_scope="function",
    function_name="TestFunction",
    node_params={"spawner_key": "GameplayStatics::GetPlayerController"},
    position=[200, 100]
)

# Create variable get node
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="create",
    graph_scope="function",
    function_name="TestFunction",
    node_type="GET Health",
    node_params={"variable_name": "Health"},
    position=[500, 100]
)

# Create branch node
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="create",
    graph_scope="function",
    function_name="TestFunction",
    node_params={"spawner_key": "KismetSystemLibrary::Branch"},
    position=[800, 100]
)
```

**Expected Result**:
- Three nodes created in TestFunction
- Each node has correct pin count and configuration
- Nodes positioned at specified coordinates

---

### 3. connect_pins - Connect Pins Between Nodes

**Purpose**: Connect execution and data flow between nodes

**Test Prompt**:
```
In BP_Player's TestFunction, list all nodes to get their IDs, then describe them to get pin names. 
Connect the function entry's "then" pin to the GetPlayerController node's "execute" pin.
```

**Expected Tool Calls**:
```python
# List nodes to get node IDs
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="list",
    graph_scope="function",
    function_name="TestFunction"
)

# Describe nodes to get pin names
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="describe",
    graph_scope="function",
    function_name="TestFunction"
)

# Connect pins using exact node_ids and pin names
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="connect_pins",
    graph_scope="function",
    function_name="TestFunction",
    extra={
        "connections": [{
            "source_node_id": "ENTRY_NODE_GUID",
            "source_pin_name": "then",
            "target_node_id": "GETPLAYERCONTROLLER_NODE_GUID",
            "target_pin_name": "execute"
        }]
    }
)
```

**Expected Result**:
- Connection established between function entry and GetPlayerController
- Execution flow visible in graph

---

### 4. disconnect_pins - Disconnect Pins

**Purpose**: Break connections between pins

**Test Prompt**:
```
In BP_Player's TestFunction, disconnect the connection between the function entry and the GetPlayerController node.
```

**Expected Tool Call**:
```python
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="disconnect_pins",
    graph_scope="function",
    function_name="TestFunction",
    extra={
        "connections": [{
            "source_node_id": "ENTRY_NODE_GUID",
            "source_pin_name": "then",
            "target_node_id": "GETPLAYERCONTROLLER_NODE_GUID",
            "target_pin_name": "execute"
        }]
    }
)
```

**Expected Result**:
- Connection removed
- Nodes remain but are disconnected

---

### 5. delete - Remove Nodes

**Purpose**: Delete nodes from the graph

**Test Prompt**:
```
In BP_Player's TestFunction, delete the branch node that was created earlier.
First use list to find its node_id.
```

**Expected Tool Calls**:
```python
# List to find node ID
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="list",
    graph_scope="function",
    function_name="TestFunction"
)

# Delete the branch node
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="delete",
    graph_scope="function",
    function_name="TestFunction",
    node_id="BRANCH_NODE_GUID"
)
```

**Expected Result**:
- Branch node removed from graph
- Other nodes remain intact

---

### 6. move - Reposition Nodes

**Purpose**: Change node positions in the graph

**Test Prompt**:
```
In BP_Player's TestFunction, move the GetPlayerController node to position [400, 200].
```

**Expected Tool Call**:
```python
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="move",
    graph_scope="function",
    function_name="TestFunction",
    node_id="GETPLAYERCONTROLLER_NODE_GUID",
    position=[400, 200]
)
```

**Expected Result**:
- Node repositioned to new coordinates
- Connections maintained

---

### 7. list - List All Nodes in Graph

**Purpose**: Get inventory of all nodes with basic information

**Test Prompt**:
```
In BP_Player's TestFunction, list all nodes showing their IDs, types, and positions.
```

**Expected Tool Call**:
```python
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="list",
    graph_scope="function",
    function_name="TestFunction"
)
```

**Expected Result**:
- Returns array of all nodes
- Each entry includes node_id, node_type, display_name, position
- Shows current graph inventory

---

### 8. describe - Get Rich Node Metadata

**Purpose**: Get comprehensive node and pin information with deterministic ordering

**Test Prompt**:
```
In BP_Player's TestFunction, describe all nodes to get complete pin information including names, types, directions, and connection status.
```

**Expected Tool Call**:
```python
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="describe",
    graph_scope="function",
    function_name="TestFunction"
)
```

**Expected Result**:
- Complete node metadata for all nodes
- Pin arrays with pin_id, name, direction, type, connection status
- Linked pins information
- Deterministic pin ordering

---

### 9. get_details - Get Detailed Node Information

**Purpose**: Get detailed information for a specific node including default values

**Test Prompt**:
```
In BP_Player's TestFunction, get detailed information for the GetPlayerController node including all pin details and current default values.
```

**Expected Tool Call**:
```python
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="get_details",
    graph_scope="function",
    function_name="TestFunction",
    node_id="GETPLAYERCONTROLLER_NODE_GUID"
)
```

**Expected Result**:
- Detailed node information
- Complete pin metadata with default_value fields
- Current configuration state

---

### 10. configure - Set Pin Default Values

**Purpose**: Set default values on input pins

**Test Prompt**:
```
In BP_Player's TestFunction, create a RandomIntegerInRange node and configure its Min pin to 10 and Max pin to 100.
```

**Expected Tool Calls**:
```python
# Create node
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="create",
    graph_scope="function",
    function_name="TestFunction",
    node_params={"spawner_key": "KismetMathLibrary::RandomIntegerInRange"},
    position=[600, 300]
)

# Configure Min pin
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="configure",
    graph_scope="function",
    function_name="TestFunction",
    node_id="RANDOM_NODE_GUID",
    property_name="Min",
    property_value=10
)

# Configure Max pin
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="configure",
    graph_scope="function",
    function_name="TestFunction",
    node_id="RANDOM_NODE_GUID",
    property_name="Max",
    property_value=100
)
```

**Expected Result**:
- RandomIntegerInRange node created
- Min pin default value set to 10
- Max pin default value set to 100

---

### 11. split_pins - Split Struct Pins

**Purpose**: Split struct pins into sub-pins for granular access

**Test Prompt**:
```
In BP_Player's TestFunction, create a MakeTransform node and split its Location pin to access X, Y, Z components individually.
```

**Expected Tool Calls**:
```python
# Create MakeTransform node
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="create",
    graph_scope="function",
    function_name="TestFunction",
    node_params={"spawner_key": "KismetMathLibrary::MakeTransform"},
    position=[300, 400]
)

# Split the Location pin
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="split_pins",
    graph_scope="function",
    function_name="TestFunction",
    node_id="TRANSFORM_NODE_GUID",
    extra={"pins": ["Location"]}
)
```

**Expected Result**:
- MakeTransform node created
- Location pin split into Location_X, Location_Y, Location_Z sub-pins
- Individual components accessible

---

### 12. recombine_pins - Recombine Split Pins

**Purpose**: Collapse split pins back into parent struct pin

**Test Prompt**:
```
In BP_Player's TestFunction, recombine the previously split Location pin on the MakeTransform node back into a single struct pin.
```

**Expected Tool Call**:
```python
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="recombine_pins",
    graph_scope="function",
    function_name="TestFunction",
    node_id="TRANSFORM_NODE_GUID",
    extra={"pins": ["Location"]}
)
```

**Expected Result**:
- Location sub-pins collapsed back into single Location pin
- Struct pin restored to original state

---

### 13. refresh_node - Refresh Single Node

**Purpose**: Reconstruct a single node (equivalent to right-click → Refresh Node)

**Test Prompt**:
```
In BP_Player's TestFunction, refresh the GetPlayerController node to clear any stale state and rebuild its pins.
```

**Expected Tool Call**:
```python
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="refresh_node",
    graph_scope="function",
    function_name="TestFunction",
    node_id="GETPLAYERCONTROLLER_NODE_GUID"
)
```

**Expected Result**:
- GetPlayerController node reconstructed
- Pins rebuilt from latest metadata
- Stale state cleared
- Blueprint auto-compiles (default behavior)

---

### 14. refresh_nodes - Refresh All Nodes

**Purpose**: Refresh every node in the Blueprint (mirrors Blueprint → Refresh All Nodes)

**Test Prompt**:
```
In BP_Player, refresh all nodes in the entire Blueprint to ensure all nodes are up-to-date with latest engine/class definitions.
```

**Expected Tool Call**:
```python
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="refresh_nodes"
)
```

**Expected Result**:
- All nodes in Blueprint refreshed
- All graphs updated
- Blueprint recompiled
- Any deprecated nodes or stale references updated

---

### 15. reset_pin_defaults - Reset Pin Defaults

**Purpose**: Restore pin default values to their autogenerated state

**Test Prompt**:
```
In BP_Player's TestFunction, reset the Min and Max pin defaults on the RandomIntegerInRange node back to their original autogenerated values.
```

**Expected Tool Call**:
```python
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="reset_pin_defaults",
    graph_scope="function",
    function_name="TestFunction",
    node_id="RANDOM_NODE_GUID",
    extra={"pins": ["Min", "Max"]}
)
```

**Alternative - Reset all pins**:
```python
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="reset_pin_defaults",
    graph_scope="function",
    function_name="TestFunction",
    node_id="RANDOM_NODE_GUID",
    extra={"reset_all": True, "compile": True}
)
```

**Expected Result**:
- Min and Max pins reset to default values
- Custom values removed
- Node restored to fresh state

---

## Complete Workflow Test Scenario

**Comprehensive Test Prompt**:
```
In BP_Player, create a new function called TestFunction and build a complete node graph that demonstrates all 15 manage_blueprint_node actions:

1. Discover GetPlayerController node variants and identify the GameplayStatics version
2. Create a function entry node
3. Create a GetPlayerController node using the exact spawner_key from discover
4. Create a GetHUD node  
5. Create a Cast to BP_MicrosubHUD node (assuming BP_MicrosubHUD exists)
6. Create a SET variable node for "MicrosubHUD"
7. List all nodes to verify creation
8. Describe all nodes to get pin information
9. Get detailed information for the GetPlayerController node
10. Connect the nodes in proper execution order
11. Configure the PlayerIndex pin on GetPlayerController to 0
12. Create a MakeVector node and split its X/Y/Z pins
13. Recombine the split pins
14. Refresh the GetPlayerController node
15. Move nodes to better positions
16. Reset pin defaults on configured nodes
17. Disconnect one connection
18. Delete an unused node
19. Refresh all nodes in the Blueprint

This workflow should demonstrate the complete discover → create → configure → connect → manipulate pattern.
```

**Expected Actions**:
1. ✅ **discover** - Find GetPlayerController variants with spawner_key
2. ✅ **create** - Create multiple nodes using exact spawner_keys
3. ✅ **list** - Get node inventory with IDs
4. ✅ **describe** - Get comprehensive pin metadata
5. ✅ **get_details** - Get specific node details with defaults
6. ✅ **connect_pins** - Establish execution and data flow
7. ✅ **configure** - Set pin default values
8. ✅ **split_pins** - Split struct pin
9. ✅ **recombine_pins** - Recombine split pin
10. ✅ **refresh_node** - Refresh single node
11. ✅ **move** - Reposition nodes
12. ✅ **reset_pin_defaults** - Reset configured values
13. ✅ **disconnect_pins** - Break connections
14. ✅ **delete** - Remove nodes
15. ✅ **refresh_nodes** - Refresh entire Blueprint

---

## Key Testing Patterns

### Pattern 1: Discover → Create Workflow (CRITICAL)
```
1. Use discover with search_term to find exact variants
2. Examine spawner_key, expected_pin_count, function_class
3. Use exact spawner_key in create action
4. Verify correct pin count in result
```

### Pattern 2: List → Describe → Connect Workflow
```
1. Use list to get all node IDs
2. Use describe to get exact pin names
3. Use connect_pins with exact node_ids and pin names
4. Verify connections in describe output
```

### Pattern 3: Configure → Get Details → Reset Workflow
```
1. Use configure to set pin defaults
2. Use get_details to verify values were set
3. Use reset_pin_defaults to restore original values
4. Use get_details again to confirm reset
```

### Pattern 4: Split → Connect → Recombine Workflow
```
1. Create node with struct pins
2. Use split_pins to access components
3. Connect to split sub-pins
4. Use recombine_pins when done
```

### Pattern 5: Create → Refresh → Verify Workflow
```
1. Create nodes with specific configuration
2. Use refresh_node to rebuild from metadata
3. Use describe to verify pins are correct
4. Use refresh_nodes for Blueprint-wide refresh
```

---

## Validation Checklist

After running test prompts, verify:

- ✅ All 15 actions executed successfully
- ✅ Discover returned spawner_key metadata
- ✅ Create used exact spawner_key from discover
- ✅ List returned complete node inventory
- ✅ Describe provided pin names and types
- ✅ Connect_pins established proper connections
- ✅ Configure set pin defaults correctly
- ✅ Split_pins expanded struct pins
- ✅ Recombine_pins collapsed sub-pins
- ✅ Refresh_node rebuilt single node
- ✅ Refresh_nodes updated entire Blueprint
- ✅ Move repositioned nodes
- ✅ Get_details returned default values
- ✅ Reset_pin_defaults restored original values
- ✅ Disconnect_pins broke connections
- ✅ Delete removed nodes

---

## Common Issues and Solutions

### Issue: "Node created with wrong variant"
**Solution**: Always use discover first to get exact spawner_key

### Issue: "Cannot find pin to connect"
**Solution**: Use describe to get exact pin names (case-sensitive)

### Issue: "Cast node has no output pin"
**Solution**: Provide full Blueprint path with _C suffix in cast_target

### Issue: "Variable node missing pins"
**Solution**: Include variable_name in node_params for GET/SET nodes

### Issue: "Connection fails with type mismatch"
**Solution**: Use get_details to verify pin types, use conversion nodes if needed

### Issue: "Refresh doesn't update node"
**Solution**: Ensure compile flag is True (default), or manually compile after refresh

---

## Notes

- All node GUIDs should be without curly braces in connection payloads
- Pin names are case-sensitive
- Position coordinates: X increases right, Y increases down
- Recommended spacing: 250-400 units horizontal, 100-200 units vertical
- Always verify node creation with list or describe before connecting
- Use get_details to inspect current pin default values
- Split/recombine operations require exact struct pin names

---

## Dependencies

- Requires Unreal Engine with VibeUE plugin installed
- Requires MCP server running and connected
- Requires BP_Player Blueprint to exist
- Some tests assume BP_MicrosubHUD Blueprint exists for cast node testing
- Some tests assume Health and MicrosubHUD variables exist in BP_Player

---

*Generated for Phase 5, Task 6: Create manage_blueprint_node test prompts*
*Covers all 15 actions with discover → create workflow demonstration*
