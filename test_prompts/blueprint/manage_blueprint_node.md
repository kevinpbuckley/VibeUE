# manage_blueprint_node Test Prompts

## Prerequisites
- ✅ Unreal Engine 5.6+ running
- ✅ VibeUE plugin loaded
- ✅ MCP connection active

## 🚨 IMPORTANT: Test Asset Management

**DO NOT delete test assets until after reviewing ALL test results!**

### Setup: Create Test Assets FIRST

**Run these commands at the START of testing:**

1. **Create Test Blueprint**
   ```
   Use manage_blueprint with action="create":
   - name: "BP_NodeTest"
   - parent_class: "Actor"
   - This creates: /Game/Blueprints/BP_NodeTest
   ```

2. **Open Blueprint in Editor**
   ```
   Use manage_asset with action="open_in_editor":
   - asset_path: "/Game/Blueprints/BP_NodeTest"
   - This opens the Blueprint Editor so you can see nodes being created
   ```

3. **Create Test Function with Parameters**
   ```
   Use manage_blueprint_function with action="create":
   - blueprint_name: "/Game/Blueprints/BP_NodeTest"
   - function_name: "GetRandomNumber"
   
   Then add parameters:
   - action="add_param", param_name="Low", direction="input", type="int"
   - action="add_param", param_name="High", direction="input", type="int"
   - action="add_param", param_name="Result", direction="out", type="int"
   
   Purpose: Create a function that takes in a high and low number, 
   selects a random number between the two and returns the new number.
   ```

4. **Create Test Variable**
   ```
   Use manage_blueprint_variable with action="create":
   - blueprint_name: "/Game/Blueprints/BP_NodeTest"
   - variable_name: "TestValue"
   - variable_config: {"type_path": "/Script/CoreUObject.FloatProperty"}
   ```

5. **Verify Assets Created**
   ```
   Use manage_blueprint with action="get_info":
   - blueprint_name: "/Game/Blueprints/BP_NodeTest"
   - Should show 1 function and 1 variable
   ```

**💡 TIP**: Keep the Blueprint Editor open throughout testing to watch nodes appear in real-time!

### After Testing: Review First, Then Cleanup

⚠️ **DO NOT run cleanup commands until you've reviewed:**
- All test results in Unreal Editor
- Node connections and layout
- Pin configurations
- Any error messages or issues

**Only after manual review**, proceed to cleanup section at end of document.

## Overview
Tests all major actions of `manage_blueprint_node`. **CRITICAL**: Always use discover → create workflow with spawner_key for exact node creation.

## Test 1: Complete Function Creation Workflow

**Purpose**: Create a complete function with parameters and nodes - demonstrates the full workflow

**Goal**: Create `GetRandomNumber` function that takes Low/High integer inputs and returns a random number between them.

### Steps

1. **Discover Random Integer Node**
   ```
   Discover with search_term="RandomIntegerInRange" and return_descriptors=true
   Note the spawner_key: "KismetMathLibrary::RandomIntegerInRange"
   ```

2. **Create the Function** (if not done in Setup)
   ```
   Create function "GetRandomNumber" with parameters:
   - Low (input, int)
   - High (input, int)
   - Result (output, int)
   ```

3. **Create Random Node**
   ```
   Create node with:
   - node_params={"spawner_key": "KismetMathLibrary::RandomIntegerInRange"}
   - position=[400, 100]
   - function_name="GetRandomNumber"
   - graph_scope="function"
   
   💡 In Unreal Editor: Navigate to the GetRandomNumber function graph to watch the node appear!
   ```

4. **Connect Function Parameters to Node**
   ```
   Use describe to get node_ids, then connect:
   - Function Entry "Low" → RandomIntegerInRange "Min"
   - Function Entry "High" → RandomIntegerInRange "Max"
   - RandomIntegerInRange "ReturnValue" → Return Node "Result"
   
   👀 Watch the wires appear in the Blueprint Editor as connections are made!
   ```

5. **Compile and Verify**
   ```
   Compile Blueprint
   Describe all nodes to verify connections
   ```

### Expected Outcomes
- ✅ Function created with correct parameters
- ✅ RandomIntegerInRange node created with spawner_key
- ✅ All parameters connected correctly
- ✅ Function is callable with Low/High inputs
- ✅ Returns random number in specified range

### Why This Matters
This demonstrates the **complete workflow** for creating functional Blueprint logic:
1. **Discover** → Find exact node variant needed
2. **Create Function** → Define inputs/outputs
3. **Create Nodes** → Use spawner_key for exact creation
4. **Connect** → Wire parameters to node pins
5. **Compile** → Validate the logic works

---

## Test 2: Node Discovery Workflow (CRITICAL)

**Purpose**: Demonstrate proper discover → create pattern for different node types

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

## Test 3: Node Creation and Positioning

**Purpose**: Create multiple nodes with proper layout

### Steps

1. **Create Additional Function**
   ```
   Create function "TestNodeLayout" in BP_NodeTest
   ```

2. **Create GetPlayerController**
   ```
   Create at [300, 100] using spawner_key from discover
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

## Test 4: Pin Connection Workflow

**Purpose**: Connect pins between nodes using describe → connect pattern

### Steps

1. **Describe All Nodes**
   ```
   Use describe action to get ALL node_ids and pin names
   ```

2. **Connect Execution Flow**
   ```
   Connect using extra parameter with connections array:

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

## Test 6: Node Deletion

**Purpose**: Remove nodes from Blueprint graph

### Steps

1. **Create Temporary Node**
   ```
   Create a test node to delete
   ```

2. **Delete Node**
   ```
   Use delete action with node_id
   ```

3. **Verify Deletion**
   ```
   List nodes - deleted node should not appear
   ```

4. **Verify Disconnection**
   ```
   Connected pins should auto-disconnect
   ```

### Expected Outcomes
- ✅ Node removed from graph
- ✅ Pins automatically disconnected
- ✅ Graph remains valid
- ✅ Safety checks prevent deleting protected nodes

---

## Test 7: Node Refresh Operations

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

## Test 8: Reset Pin Defaults

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

## Test 9: Disconnect Pins

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

## Test 10: Node Movement

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

## Test 11: Variable Get/Set Nodes

**Purpose**: Create variable getter and setter nodes

### Steps

1. **Discover Variable Nodes**
   ```
   Search for TestValue variable nodes with discover action
   ```

2. **Create GET Node**
   ```
   Use spawner_key from discover: "SKEL_BP_NodeTest_C::GET TestValue"
   ```

3. **Verify GET Node**
   ```
   Describe - should have value output pin
   ```

4. **Create SET Node**
   ```
   Use spawner_key: "SKEL_BP_NodeTest_C::SET TestValue"
   ```

5. **Verify SET Node**
   ```
   Describe - should have execute, then, value input pins
   ```

### Expected Outcomes
- ✅ spawner_key identifies exact variable node variant
- ✅ GET node has output pin
- ✅ SET node has execution and value pins
- ✅ Variable nodes properly configured

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

## 🧹 Cleanup: Delete Test Assets (After Review Only!)

**⚠️ STOP! Before running cleanup:**

1. ✅ Have you reviewed all test results in Unreal Editor?
2. ✅ Have you verified node connections and layouts?
3. ✅ Have you checked for any unexpected behavior?
4. ✅ Have you documented any issues found?

**Only proceed if all above are YES.**

### Cleanup Commands

```
Delete test Blueprint:
Use manage_blueprint with action="delete" (if supported) OR
Manually delete /Game/Blueprints/BP_NodeTest in Content Browser

⚠️ Warning: This will permanently delete:
- BP_NodeTest Blueprint
- GetRandomNumber function (with Low/High parameters)
- TestNodeLayout function (if created)
- TestValue variable
- All test nodes created during testing
```

### Manual Cleanup Alternative

If you prefer to keep test assets for future reference:
1. Open BP_NodeTest in Blueprint Editor
2. Review the TestFunction graph
3. Save as a different name if you want to preserve it
4. Delete when no longer needed

---

**Test Coverage**: 15/15 actions tested ✅  
**Last Updated**: November 5, 2025  
**Related Issues**: #69, #74

