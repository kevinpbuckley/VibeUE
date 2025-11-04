# manage_blueprint_node Test Prompts

## Prerequisites
- ✅ Unreal Engine 5.6+ running
- ✅ VibeUE plugin loaded
- ✅ MCP connection active
- ✅ Test Blueprint with function created

## Setup: Create Test Assets

**Run these commands BEFORE starting tests:**

```
Create Blueprint "BP_NodeTest" with parent "Actor"
Create function "TestFunction" in Blueprint "/Game/Blueprints/BP_NodeTest"
Create variable "TestValue" with type float in Blueprint "/Game/Blueprints/BP_NodeTest"
```

## Overview
Tests all 15 actions of `manage_blueprint_node`. **CRITICAL**: Always use discover → create workflow with spawner_key for exact node creation.

## Test 1: Node Discovery Workflow (CRITICAL)

**Purpose**: Demonstrate proper discover → create pattern

### Steps

1. **Discover Node Variants**
   ```
   Discover available nodes with search_term="GetPlayerController" and return_descriptors=true
   ```

2. **Examine Spawner Keys**
   ```
   Note ALL variants returned, e.g.:
   - "GameplayStatics::GetPlayerController" (3 pins, static)
   - "CheatManager::GetPlayerController" (1 pin, instance method)
   ```

3. **Create Function**
   ```
   Create function "TestNodes" in BP_NodeTest
   ```

4. **Create Using Spawner Key**
   ```
   Create node with node_params={"spawner_key": "GameplayStatics::GetPlayerController"}
   at position [200, 100]
   ```

5. **Verify Exact Creation**
   ```
   Use describe to verify node has expected 3 pins
   ```

### Expected Outcomes
- ✅ discover returns multiple variants with complete metadata
- ✅ spawner_key uniquely identifies node variant
- ✅ create with spawner_key produces EXACT variant
- ✅ Pin count matches discover metadata
- ✅ No ambiguity in node creation

### Why This Matters
❌ **WITHOUT discover**: Creating "GetPlayerController" is ambiguous (which variant?)  
✅ **WITH discover + spawner_key**: Exact variant guaranteed, correct pin count

---

## Test 2: Node Creation and Positioning

**Purpose**: Create nodes with proper layout

### Steps

1. **Create Entry Node**
   ```
   Function entry node exists at [0, 0]
   ```

2. **Create GetPlayerController**
   ```
   Create at [300, 100] using spawner_key
   ```

3. **Create GetHUD**
   ```
   Create at [600, 100] using spawner_key
   ```

4. **Create Print String**
   ```
   Create at [900, 100] using spawner_key
   ```

5. **List All Nodes**
   ```
   Verify all nodes created at correct positions
   ```

### Expected Outcomes
- ✅ Nodes positioned left-to-right
- ✅ X coordinates increase (0 → 300 → 600 → 900)
- ✅ Y coordinates consistent for flow clarity
- ✅ list shows all nodes with positions

### Positioning Best Practices
- **Left-to-right**: Execution flows left to right (increasing X)
- **Spacing**: 250-400 units horizontal
- **Vertical**: 100-200 units for parallel branches
- ❌ **Avoid**: Decreasing X (creates backward flow)

---

## Test 3: Pin Connection Workflow

**Purpose**: Connect pins between nodes using describe → connect pattern

### Steps

1. **Describe All Nodes**
   ```
   Use describe action to get ALL node_ids and pin names
   ```

2. **Connect Execution Flow**
   ```
   Connect using extra parameter with connections array:
   {
     "connections": [{
       "source_node_id": "{FunctionEntry_GUID}",
       "source_pin_name": "then",
       "target_node_id": "{GetPlayerController_GUID}",
       "target_pin_name": "execute"
     }]
   }
   ```

3. **Connect Data Pins**
   ```
   Connect GetPlayerController "ReturnValue" to GetHUD "self"
   ```

4. **Connect Final Node**
   ```
   Connect GetHUD "ReturnValue" to PrintString "InString"
   ```

5. **Verify Connections**
   ```
   Use describe to see all pin connections
   ```

### Expected Outcomes
- ✅ describe provides exact node_ids and pin names
- ✅ connect_pins uses extra parameter format
- ✅ Execution pins connect (then → execute)
- ✅ Data pins connect (ReturnValue → inputs)
- ✅ Type compatibility validated automatically

### Connection Format (CRITICAL)
```python
extra = {
  "connections": [{
    "source_node_id": "{GUID}",  # From describe
    "source_pin_name": "PinName",  # From describe
    "target_node_id": "{GUID}",
    "target_pin_name": "PinName"
  }]
}
```

---

## Test 4: Pin Configuration

**Purpose**: Set default values on input pins

### Steps

1. **Create RandomIntegerInRange Node**
   ```
   Create node, note node_id from result
   ```

2. **Configure Min Value**
   ```
   Use configure action:
   - node_id: {from step 1}
   - property_name: "Min"
   - property_value: 10
   ```

3. **Configure Max Value**
   ```
   Configure Max pin to 50
   ```

4. **Get Node Details**
   ```
   Verify pin default values were set
   ```

### Expected Outcomes
- ✅ configure sets pin default values
- ✅ Min and Max pins show configured values
- ✅ get_details confirms values
- ✅ Node functional with defaults

---

## Test 5: Struct Pin Splitting

**Purpose**: Split and recombine struct pins

### Steps

1. **Create Make Vector Node**
   ```
   Create node that has FVector output
   ```

2. **Split Vector Pin**
   ```
   Use split action with extra={"pins": ["ReturnValue"]}
   ```

3. **Verify Sub-Pins**
   ```
   Describe node - should show X, Y, Z sub-pins
   ```

4. **Recombine Pin**
   ```
   Use recombine action to collapse back to struct
   ```

5. **Verify Recombination**
   ```
   Describe - sub-pins should be gone
   ```

### Expected Outcomes
- ✅ split exposes struct sub-pins (X, Y, Z)
- ✅ Sub-pins accessible individually
- ✅ recombine restores struct pin
- ✅ Pin state persists correctly

---

## Test 6: Node Refresh Operations

**Purpose**: Test refresh_node and refresh_nodes

### Steps

1. **Modify Blueprint Structure**
   ```
   Add a variable to the Blueprint
   ```

2. **Refresh Single Node**
   ```
   Use refresh_node on a GET variable node
   ```

3. **Verify Node Updated**
   ```
   Check node reflects structural changes
   ```

4. **Refresh All Nodes**
   ```
   Use refresh_nodes to update entire Blueprint
   ```

5. **Compile**
   ```
   Compile Blueprint after refresh
   ```

### Expected Outcomes
- ✅ refresh_node updates single node
- ✅ Clears stale state
- ✅ refresh_nodes updates all nodes
- ✅ Equivalent to "Refresh All Nodes" menu command

---

## Test 7: Reset Pin Defaults

**Purpose**: Reset pin values to autogenerated defaults

### Steps

1. **Configure Pin Values**
   ```
   Set several pin defaults on a node
   ```

2. **Reset Single Pin**
   ```
   Use reset_pin_defaults with extra={"pins": ["PinName"]}
   ```

3. **Verify Reset**
   ```
   Check pin returned to default value
   ```

4. **Reset All Pins**
   ```
   Use reset_pin_defaults with extra={"reset_all": true}
   ```

### Expected Outcomes
- ✅ Single pin reset clears that pin only
- ✅ reset_all clears all pin defaults
- ✅ Pins return to autogenerated values
- ✅ Compile option available

---

## Test 8: Disconnect Pins

**Purpose**: Break pin connections

### Steps

1. **Create Connected Nodes**
   ```
   Create and connect several nodes
   ```

2. **Disconnect Single Connection**
   ```
   Use disconnect_pins to break one connection
   ```

3. **Verify Disconnection**
   ```
   Describe to confirm connection broken
   ```

4. **Clear Entire Pin**
   ```
   Use disconnect to clear all connections from a pin
   ```

### Expected Outcomes
- ✅ disconnect_pins breaks specific connection
- ✅ Other connections remain intact
- ✅ Can clear all connections from a pin
- ✅ Nodes remain valid after disconnect

---

## Test 9: Node Movement

**Purpose**: Reposition nodes in graph

### Steps

1. **Move Single Node**
   ```
   Use move action with new position [400, 200]
   ```

2. **Verify New Position**
   ```
   List or describe to confirm position changed
   ```

3. **Organize Layout**
   ```
   Move multiple nodes to create clean left-to-right flow
   ```

### Expected Outcomes
- ✅ move updates node position
- ✅ Position persists
- ✅ Visual layout improves
- ✅ Connections remain intact

---

## Test 10: Node Deletion

**Purpose**: Remove nodes from graph

### Steps

1. **Delete Single Node**
   ```
   Delete a disconnected node
   ```

2. **Verify Removal**
   ```
   List nodes - deleted node should be gone
   ```

3. **Delete Connected Node**
   ```
   Delete node with connections
   ```

4. **Verify Connections Broken**
   ```
   Connected nodes should show broken connections
   ```

### Expected Outcomes
- ✅ delete removes node
- ✅ Node no longer in list
- ✅ Connected pins become disconnected
- ✅ Graph remains valid

---

## Test 11: Variable Get/Set Nodes

**Purpose**: Create variable nodes with proper configuration

### Steps

1. **Create Variable**
   ```
   Use manage_blueprint_variable to create "Health" variable
   ```

2. **Create GET Node**
   ```
   Create node_type="GET Health" with node_params={"variable_name": "Health"}
   ```

3. **Verify GET Node**
   ```
   Describe - should have value output pin
   ```

4. **Create SET Node**
   ```
   Create node_type="SET Health" with node_params={"variable_name": "Health"}
   ```

5. **Verify SET Node**
   ```
   Describe - should have 5 pins (execute, then, Health input, Output_Get)
   ```

### Expected Outcomes
- ✅ node_params.variable_name REQUIRED for var nodes
- ✅ GET node has output pin
- ✅ SET node has 5 pins total
- ✅ Without node_params: broken 2-pin node

---

## Test 12: Cast Nodes

**Purpose**: Create Blueprint cast nodes

### Steps

1. **Create Cast Node**
   ```
   Create "Cast To BP_Enemy" with node_params={
     "cast_target": "/Game/Blueprints/BP_Enemy.BP_Enemy_C"
   }
   ```

2. **Verify Cast Node**
   ```
   Describe - should have 6 pins:
   - execute, then, CastFailed (exec)
   - Object (input)
   - AsBP Enemy (typed output)
   ```

### Expected Outcomes
- ✅ node_params.cast_target REQUIRED
- ✅ Format: full package path + _C suffix
- ✅ Creates 6-pin node
- ✅ Typed output pin available

---

## Reference: All Actions Summary

| Action | Purpose | Key Parameters |
|--------|---------|----------------|
| **discover** | Find node types | search_term, return_descriptors, category |
| **create** | Create node | node_params with spawner_key, position |
| **connect_pins** | Connect pins | extra with connections array |
| **disconnect_pins** | Break connections | extra with connections or pin_ids |
| **delete** | Remove node | node_id |
| **move** | Reposition node | node_id, position |
| **list** | List all nodes | graph_scope, function_name |
| **describe** | Get node metadata | node_id (optional) |
| **get_details** | Get node details | node_id |
| **configure** | Set pin defaults | node_id, property_name, property_value |
| **split_pins** | Split struct pins | node_id, extra with pins array |
| **recombine_pins** | Collapse pins | node_id, extra with pins array |
| **refresh_node** | Refresh single node | node_id |
| **refresh_nodes** | Refresh all nodes | blueprint_name |
| **reset_pin_defaults** | Reset pin values | node_id, extra with pins or reset_all |

---

## Cleanup: Delete Test Assets

**Run these commands AFTER completing all tests:**

```
Delete test Blueprint:
- Delete /Game/Blueprints/BP_NodeTest with force_delete=True and show_confirmation=False
```

---

**Test Coverage**: 15/15 actions tested ✅  
**Last Updated**: November 4, 2025  
**Related Issues**: #69, #74

