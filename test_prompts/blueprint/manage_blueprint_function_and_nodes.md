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

**Purpose**: Create nodes using spawner_key from discovery

### Steps

1. **List Existing Nodes**
   ```
   List all nodes in CalculateHealth function graph
   ```

2. **Create Variable Getter**
   ```
   Create node in CalculateHealth at position [100, 100] using spawner_key from Health variable discovery
   ```

3. **Create Multiply Node**
   ```
   Create multiply node at position [400, 100] using spawner_key from discovery
   ```

4. **Create Set Variable Node**
   ```
   Create SET Health node at position [700, 100] with node_params={"variable_name": "Health"}
   ```

5. **List All Nodes**
   ```
   Verify all created nodes appear in list
   ```

6. **Describe Nodes**
   ```
   Use describe action to get detailed pin information for all nodes
   ```

### Expected Outcomes
- ✅ Initial node list shows function entry/result nodes
- ✅ Variable getter created with correct variable_name in node_params
- ✅ Math node created at specified position
- ✅ Variable setter created with proper pins (execute, then, value input, Output_Get)
- ✅ describe shows all pins with IDs and types

---

## Test 6: Node Connections

**Purpose**: Connect pins to create functional graph

### Steps

1. **Get Node Details**
   ```
   Describe all nodes to identify exact pin names and node IDs
   ```

2. **Connect Entry to Getter**
   ```
   Use connect_pins to connect function entry "then" pin to first node
   ```

3. **Connect Getter to Multiply**
   ```
   Connect Health getter output to Multiply input A
   ```

4. **Connect Multiply to Setter**
   ```
   Connect Multiply output to SET Health value input
   Connect execution pins as well
   ```

5. **List Connections**
   ```
   Use describe to verify all connections were made
   ```

### Expected Outcomes
- ✅ Pin connections succeed with proper source/target node_id and pin_name
- ✅ Execution flow connects (then pins)
- ✅ Data flow connects (value pins)
- ✅ describe shows linked pins

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

## Test 8: Compile and Verify

**Purpose**: Ensure Blueprint compiles and function is correct

### Steps

1. **Compile Blueprint**
   ```
   Compile BP_FunctionNodeTest Blueprint
   ```

2. **Check for Errors**
   ```
   If compilation fails, check error messages
   ```

3. **Verify Function Graph**
   ```
   Use describe to get final state of all nodes and connections
   ```

### Expected Outcomes
- ✅ Blueprint compiles successfully OR
- ⚠️ Compilation errors are clear and actionable
- ✅ Function graph is complete

---

## ⚠️ USER VERIFICATION CHECKPOINT

**STOP HERE - Do NOT proceed to cleanup until user confirms:**

### User Tasks:
1. **Open Blueprint in Unreal Editor** (should already be open)
   - Navigate to "My Blueprint" panel
   - Find "CalculateHealth" function
   - Double-click to open function graph

2. **Verify Function Signature**
   - Check input pins: BaseHealth (int), Modifier (int)
   - Check output pin: ResultHealth (float)
   - Check local variables: TempResult (int)

3. **Verify Node Graph**
   - Entry node connects to GET Health
   - GET Health connects to Multiply
   - Multiply connects to SET Health
   - All execution pins connected (white lines)
   - All data pins connected (colored lines)

4. **Verify Compilation**
   - Blueprint compiles without errors
   - No warning messages
   - Function appears in function list

5. **Test Function**
   - Right-click in Event Graph
   - Search for "CalculateHealth"
   - Verify function appears as callable node
   - Check that pin names match parameters

### User Confirmation Required:
```
Type "verified" when you confirm:
- Function exists and has correct signature
- Nodes are visible and connected
- Blueprint compiles successfully
- Function is callable from Event Graph
```

**Only proceed to cleanup after user types "verified"**

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
