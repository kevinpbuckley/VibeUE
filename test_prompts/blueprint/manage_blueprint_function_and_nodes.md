# manage_blueprint_function + manage_blueprint_node Combined Test

## Prerequisites
- ✅ Unreal Engine 5.6+ running
- ✅ VibeUE plugin loaded
- ✅ MCP connection active

## Setup: Create Test Assets

**Run these commands BEFORE starting tests:**

```
Create Blueprint "BP_FunctionNodeTest" with parent "Actor"
Create variable "Health" with type float in Blueprint "/Game/Blueprints/BP_FunctionNodeTest"
Open BP_FunctionNodeTest in editor to visualize changes
```

## Overview
Combined test suite for `manage_blueprint_function` (13 actions) and `manage_blueprint_node` (15 actions). Tests function creation, parameter management, and node creation/connection in a single integrated workflow.

**Critical Workflow**: Functions → Parameters → Nodes → Connections → Compile → User Verification → Cleanup

---

## Test 1: Function Lifecycle

**Purpose**: Create and manage custom functions

### Steps

1. **List Existing Functions**
   ```
   List all functions in BP_FunctionNodeTest (should show UserConstructionScript)
   ```

2. **Create Function**
   ```
   Create new function named "CalculateHealth" in BP_FunctionNodeTest
   ```

3. **Get Function Details**
   ```
   Get detailed information about CalculateHealth function
   ```

4. **List Functions Again**
   ```
   Verify CalculateHealth appears in function list
   ```

### Expected Outcomes
- ✅ Blueprint has UserConstructionScript by default
- ✅ CalculateHealth function created successfully
- ✅ Function details show graph_guid and node_count
- ✅ Function appears in list

---

## Test 2: Parameter Management

**Purpose**: Add inputs, outputs, and manage parameter types

### Steps

1. **List Initial Parameters**
   ```
   List parameters of CalculateHealth (should be empty)
   ```

2. **Add Input Parameters**
   ```
   Add input parameter "BaseHealth" of type float to CalculateHealth
   Add input parameter "Modifier" of type float to CalculateHealth
   ```

3. **Add Output Parameter**
   ```
   Add output parameter "ResultHealth" of type float with direction "out" to CalculateHealth
   ```

4. **List All Parameters**
   ```
   List params to verify all three were added (plus auto-generated execute pin)
   ```

5. **Update Parameter Type**
   ```
   Update Modifier parameter type from float to int
   ```

6. **Verify Update**
   ```
   List params again to confirm Modifier is now int
   ```

### Expected Outcomes
- ✅ Initial parameters list is empty
- ✅ Input parameters added with correct types
- ✅ Output parameter added with direction="out"
- ✅ list_params shows all parameters including execute pin
- ✅ Parameter type update works correctly

---

## Test 3: Local Variables

**Purpose**: Manage function-scoped local variables

### Steps

1. **List Initial Locals**
   ```
   List local variables in CalculateHealth (should be empty)
   ```

2. **Add Local Variable**
   ```
   Add local variable "TempResult" of type float to CalculateHealth
   ```

3. **Add Second Local**
   ```
   Add local variable "Multiplier" of type float
   ```

4. **List All Locals**
   ```
   Verify both local variables were added
   ```

5. **Update Local Type**
   ```
   Update TempResult type from float to int
   ```

6. **Remove Local**
   ```
   Remove Multiplier local variable
   ```

7. **Verify Changes**
   ```
   List locals to confirm TempResult is int and Multiplier is gone
   ```

### Expected Outcomes
- ✅ Initial locals list is empty
- ✅ Local variables added successfully
- ✅ Type update works for locals
- ✅ Local removal works correctly

---

## Test 4: Node Discovery (CRITICAL Workflow)

**Purpose**: Discover available nodes before creation

### Steps

1. **Discover Math Nodes**
   ```
   Discover nodes with search_term="Multiply" and return_descriptors=true
   ```

2. **Examine Results**
   ```
   Note the spawner_key values returned (e.g., "KismetMathLibrary::Multiply_FloatFloat")
   ```

3. **Discover Variable Nodes**
   ```
   Discover nodes with search_term="GET Health" to find variable getter
   ```

4. **Discover Function Nodes**
   ```
   Discover nodes with search_term="Print String"
   ```

### Expected Outcomes
- ✅ discover returns nodes with spawner_key, expected_pin_count, metadata
- ✅ Multiple variants may exist (use spawner_key to choose exact one)
- ✅ Variable getters found with correct variable name
- ✅ Descriptors include all info needed for creation

---

## Test 5: Node Creation in Function Graph

**Purpose**: Create complete node graph for the function

**Function Logic**: CalculateHealth(BaseHealth, Modifier) → ResultHealth = BaseHealth * Modifier

### Steps

1. **List Existing Nodes**
   ```
   List all nodes in CalculateHealth function graph (should show entry and result nodes)
   ```

2. **Identify Entry and Result Nodes**
   ```
   Use list to get node IDs for the function entry (has BaseHealth, Modifier inputs) 
   and result node (has ResultHealth output)
   ```

3. **Create Convert Float to Int Node**
   ```
   Create node at position [200, 100] to convert Modifier (int) to float for multiplication
   Use spawner_key from discovery (search "convert int to float")
   ```

4. **Create Multiply Node** 
   ```
   Create multiply (float * float) node at position [400, 100]
   Use spawner_key from discovery (search "float * float" or just "*")
   ```

5. **List All Nodes**
   ```
   Verify all created nodes appear in list with correct IDs
   ```

6. **Describe All Nodes**
   ```
   Use describe to get complete pin information (names, types, IDs) for all nodes
   Note down: entry node pins, convert node pins, multiply node pins, result node pins
   ```

### Expected Outcomes
- ✅ Function entry node has: then (exec out), BaseHealth (float out), Modifier (int out)
- ✅ Convert node created with int input and float output
- ✅ Multiply node created with A (float in), B (float in), ReturnValue (float out)
- ✅ Result node has: ResultHealth (float in)
- ✅ All nodes positioned left-to-right with 200 unit spacing
- ✅ describe returns complete pin details for connection planning

---

## Test 6: Node Connections (COMPLETE GRAPH)

**Purpose**: Connect all pins to create fully functional graph

**CRITICAL**: Function must compile successfully after connections!

### Connection Map
Entry → Convert → Multiply → Result

**Data Flow:**
- Entry.BaseHealth → Multiply.A
- Entry.Modifier → Convert.IntValue  
- Convert.FloatValue → Multiply.B
- Multiply.ReturnValue → Result.ResultHealth

### Steps

1. **Get All Pin Details**
   ```
   Use describe to verify exact pin names on each node
   Entry node: "BaseHealth", "Modifier"
   Convert node: input pin name (e.g., "InInt"), output pin name (e.g., "ReturnValue")
   Multiply node: "A", "B", "ReturnValue"
   Result node: "ResultHealth"
   ```

2. **Connect BaseHealth to Multiply A**
   ```
   Use connect_pins with:
   - source_node_id: <entry_node_id>
   - source_pin_name: "BaseHealth"
   - target_node_id: <multiply_node_id>
   - target_pin_name: "A"
   ```

3. **Connect Modifier to Convert Input**
   ```
   Connect Entry.Modifier → Convert.(int input pin name)
   ```

4. **Connect Convert Output to Multiply B**
   ```
   Connect Convert.(float output) → Multiply.B
   ```

5. **Connect Multiply Result to Function Output**
   ```
   Connect Multiply.ReturnValue → Result.ResultHealth
   ```

6. **Verify All Connections**
   ```
   Use describe to confirm all data pins show as connected
   Check that no pins are highlighted in red/warning state
   ```

### Expected Outcomes
- ✅ All 4 data connections succeed
- ✅ describe shows "linked" status for all connected pins
- ✅ No disconnected pins remain
- ✅ Graph forms clean left-to-right flow
- ✅ No compilation errors about unconnected pins

---

## Test 7: Function Properties

**Purpose**: Update function metadata

### Steps

1. **Update Function Properties**
   ```
   Update CalculateHealth properties: BlueprintPure=true, Category="Health|Calculations"
   ```

2. **Verify Properties**
   ```
   Get function details to confirm properties were set
   ```

### Expected Outcomes
- ✅ Function properties updated successfully
- ✅ Category and pure flag reflected in function details

---

## Test 8: Compile and Verify (MUST SUCCEED)

**Purpose**: Ensure Blueprint compiles successfully with complete function

**CRITICAL**: This test MUST result in successful compilation!

### Steps

1. **Compile Blueprint**
   ```
   Compile BP_FunctionNodeTest Blueprint
   ```

2. **Check Compilation Result**
   ```
   Verify compilation succeeds with no errors
   If compilation fails, review error messages and fix connections
   ```

3. **Verify Function is Callable**
   ```
   In Event Graph, right-click and search for "CalculateHealth"
   Confirm function appears in context menu as callable node
   ```

4. **Final Node Graph Check**
   ```
   Use describe to get final state showing:
   - All nodes positioned correctly (left-to-right: Entry → Convert → Multiply → Result)
   - All pins connected (no red/warning indicators)
   - Clean execution flow
   ```

### Expected Outcomes
- ✅ **Compilation SUCCEEDS** (this is MANDATORY!)
- ✅ No errors in compiler results
- ✅ No warnings about unconnected pins
- ✅ CalculateHealth appears as callable function
- ✅ Function signature shows: CalculateHealth(BaseHealth: float, Modifier: int) → ResultHealth: float
- ✅ Graph is visually clean and properly connected

**If compilation fails:**
- Review all pin connections
- Check that data types match (int→float conversion node exists)
- Verify no dangling/unconnected pins
- Fix issues and recompile until SUCCESS

---

## ⚠️ USER VERIFICATION CHECKPOINT

**STOP HERE - Do NOT proceed to cleanup until user confirms:**

### User Tasks:
1. **Open Blueprint in Unreal Editor** (should already be open)
   - Navigate to "My Blueprint" panel
   - Find "CalculateHealth" function
   - Double-click to open function graph

2. **Verify Function Signature**
   - Check input pins: BaseHealth (float), Modifier (int)
   - Check output pin: ResultHealth (float)

3. **Verify Node Graph is COMPLETE and CONNECTED**
   - Function Entry node at left
   - Convert (int to float) node connected to Modifier
   - Multiply node receiving BaseHealth and converted Modifier
   - Function Result node receiving multiply output
   - All data pins connected (blue/green/orange lines visible)
   - Nodes aligned left-to-right

4. **Verify Compilation SUCCESS**
   - Blueprint shows green checkmark (compiled successfully)
   - No errors in compiler results panel
   - No warning messages about unconnected pins

5. **Test Function is Callable**
   - Open Event Graph
   - Right-click in empty space
   - Search for "CalculateHealth"
   - Verify function appears with correct signature
   - Check that calling the function shows BaseHealth and Modifier inputs, ResultHealth output

### User Confirmation Required:
```
Type "verified" when you confirm ALL of the following:
✅ Function exists with correct signature (BaseHealth: float, Modifier: int → ResultHealth: float)
✅ All nodes are visible and positioned left-to-right
✅ ALL data pins are connected (no red/disconnected pins)
✅ Blueprint compiles successfully (green checkmark, no errors)
✅ Function is callable from Event Graph
✅ Function logic is correct: ResultHealth = BaseHealth * (Modifier converted to float)
```

**DO NOT type "verified" if:**
- ❌ Any pins are disconnected
- ❌ Blueprint shows compilation errors
- ❌ Nodes are misaligned or overlapping
- ❌ Function cannot be called from Event Graph

**Only proceed to cleanup after user types "verified" confirming COMPLETE, WORKING function!**

---

## Cleanup: Delete Test Assets

**ONLY RUN AFTER USER VERIFICATION:**

```
Delete Blueprint "/Game/Blueprints/BP_FunctionNodeTest" with force_delete=true and show_confirmation=false
```

### Expected Outcomes
- ✅ Blueprint deleted successfully
- ✅ No assets left behind

---

## Test Summary

**manage_blueprint_function Actions Tested:**
1. ✅ list - List all functions
2. ✅ create - Create new function
3. ✅ get - Get function details
4. ✅ list_params - List function parameters
5. ✅ add_param - Add input/output parameters
6. ✅ update_param - Update parameter types
7. ✅ list_locals - List local variables
8. ✅ add_local - Add local variable
9. ✅ update_local - Update local variable type
10. ✅ remove_local - Remove local variable
11. ✅ update_properties - Set function metadata

**manage_blueprint_node Actions Tested:**
1. ✅ discover - Discover available nodes with descriptors
2. ✅ list - List nodes in function graph
3. ✅ create - Create nodes using spawner_key
4. ✅ describe - Get detailed node/pin information
5. ✅ connect_pins - Connect pins between nodes
6. ✅ move - Position nodes in graph (via position parameter)

**Integration Points Validated:**
- ✅ Functions create entry/result nodes automatically
- ✅ Parameters become pins on entry/result nodes
- ✅ Variables accessible as GET/SET nodes
- ✅ Node discovery required for exact node creation
- ✅ spawner_key eliminates ambiguity
- ✅ Pin connections require exact node_id and pin_name
- ✅ Compilation validates complete graph

**Key Learnings:**
1. Always use discover → spawner_key workflow for node creation
2. Variable nodes require node_params with variable_name
3. Parameters automatically create pins (no manual node creation needed)
4. Compilation requires complete execution and data flow
5. User verification ensures visual correctness before cleanup
