# Function Call Node Creation Blocker

**Date:** October 3, 2025  
**Status:** 🚫 **CRITICAL BLOCKER** - Prevents Blueprint Challenge completion  
**Severity:** HIGH - Blocks 4 of 5 function implementations

---

## Problem Summary

**Issue:** Cannot reliably create standard Actor member function call nodes (K2_GetActorLocation, Set Input Mode, etc.) using current MCP tools.

**Impact:** Unable to complete SpawnDeathEffects, SetMouseToUIMode, SetMouseToGameMode, and HideLoadingScreen functions in Blueprint Challenge Phase 4.

---

## What Works ✅

1. **Variable Get Nodes** - Fully working with `node_params={"variable_name": "Name"}`
2. **Variable Set Nodes** - Fully working with `node_params={"variable_name": "Name"}`
3. **Cast Nodes** - Fully working with `node_params={"cast_target": "/Path/Class.Class_C"}`
4. **Get Player Controller** - Working despite CheatManager warning

---

## What Doesn't Work ❌

### Actor Member Function Calls
**Affected Nodes:**
- `K2_GetActorLocation` ("Get Actor Location")
- `K2_GetActorRotation` ("Get Actor Rotation")
- `SpawnSystemAtLocation` (Niagara)
- `PlaySoundAtLocation` (Audio)
- `SetInputModeGameAndUI` (PlayerController)
- `SetInputModeGameOnly` (PlayerController)

**Symptoms:**
When attempting to create these nodes with:
```python
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player2",
    action="create",
    node_params={"function_name": "K2_GetActorLocation"},
    node_type="FunctionCall",
    position=[100, -200],
    function_name="SpawnDeathEffects",
    graph_scope="function"
)
```

**Result:** Node created, but configured as wrong function:
- Display name: "Set Material Function"
- Target class: MaterialExpressionMaterialFunctionCall
- Pin count: 5 (wrong pins - should be 2+ for Get Actor Location)

---

## Root Cause Analysis

### Discovery Process
1. Attempted `node_params={"function_name": "K2_GetActorLocation"}` → Failed
2. Attempted `node_params={"function_class": "/Script/Engine.Actor", "function_name": "K2_GetActorLocation"}` → Same failure
3. Checked get_available_blueprint_nodes → Found nodes exist with type "variable" and "is_getter": true
4. Realized these aren't generic "FunctionCall" nodes - they're **variable accessor pattern nodes**

### Technical Issue
The C++ reflection system is trying to find a function to call, but:
- `K2_GetActorLocation` is implemented as a **variable accessor** not a standard function call
- The `function_name` parameter searches for callable functions, not K2 graph nodes
- The `function_class` hint doesn't override the node creation logic
- Without the right target, defaults to first available function (Material Function)

---

## Attempted Solutions

### ❌ Attempt 1: Standard Function Call
```python
manage_blueprint_node(
    action="create",
    node_type="FunctionCall",
    node_params={"function_name": "K2_GetActorLocation"}
)
```
**Result:** Wrong function (Material Function)

### ❌ Attempt 2: Function Class Specification
```python
manage_blueprint_node(
    action="create",
    node_type="FunctionCall",
    node_params={
        "function_class": "/Script/Engine.Actor",
        "function_name": "K2_GetActorLocation"
    }
)
```
**Result:** Same failure (class hint ignored)

### ❌ Attempt 3: Niagara Function Library
```python
manage_blueprint_node(
    action="create",
    node_type="FunctionCall",
    node_params={
        "function_class": "/Script/Niagara.NiagaraFunctionLibrary",
        "function_name": "SpawnSystemAtLocation"
    }
)
```
**Result:** Same "Set Material Function" misconfiguration

---

## Analysis: Variable vs Function Pattern

From `get_available_blueprint_nodes` results:
```json
{
    "name": "Get Actor Location",
    "category": "|Transformation",
    "type": "variable",  // ← NOT "node"!
    "is_getter": true,
    "variable_name": "Actor Location"
}
```

**Key Insight:** "Get Actor Location" is classified as a **variable accessor**, not a function call. This suggests it needs:
- `node_type`: "VariableGet" (not "FunctionCall")
- `node_params`: `{"variable_name": "Actor Location"}` (not "K2_GetActorLocation")

### But This Creates New Problems:
1. These aren't Blueprint variables - they're **Actor property accessors**
2. The variable_name pattern works for Blueprint variables, not class properties
3. No clear mapping between K2 function names and variable accessor names

---

## Required Function Nodes (Blueprint Challenge)

### SpawnDeathEffects (7 problematic nodes):
- ❌ K2_GetActorLocation (2x - different instances)
- ❌ K2_GetActorRotation  
- ❌ SpawnSystemAtLocation (Niagara)
- ❌ PlaySoundAtLocation (Audio)
- ✅ K2Node_SpawnActorFromClass (Works - special node type)
- ✅ K2Node_Knot (Reroute - Works)

### SetMouseToUIMode (3 problematic nodes):
- ❌ GetPlayerController (GameplayStatics)
- ❌ SetInputModeGameAndUI (WidgetBlueprintLibrary)
- ❌ Set bShowMouseCursor (PlayerController property setter)

### SetMouseToGameMode (3 problematic nodes):
- ❌ GetPlayerController
- ❌ SetInputModeGameOnly  
- ❌ Set bShowMouseCursor

### HideLoadingScreen (nodes TBD):
- Expected to have similar function call issues

---

## Impact on Blueprint Challenge

**Current Status:**
- ✅ Phase 1: Variables (6/6) - COMPLETE
- ✅ Phase 2: Components (6/6) - COMPLETE
- ✅ Phase 3: Functions (6/6) - COMPLETE
- 🔄 Phase 4: Function Implementation
  - ✅ CastToMicrosubHUD (5 nodes, 5 connections) - COMPLETE
  - ❌ SpawnDeathEffects (BLOCKED)
  - ❌ SetMouseToUIMode (BLOCKED)
  - ❌ SetMouseToGameMode (BLOCKED)
  - ❌ HideLoadingScreen (BLOCKED)
- ⏸️ Phase 5: Event Graph (85 nodes) - DEPENDS ON PHASE 4

**Completion Estimate:** 20% of Phase 4 (1/5 functions)

---

## Workaround Attempts

### Manual Node Creation in Unreal
**Process:**
1. Open BP_Player2 in Unreal Editor
2. Manually add Get Actor Location nodes to functions
3. Save and close
4. Use MCP tools to connect pins

**Problems:**
- Defeats purpose of "100% MCP tool recreation" challenge
- Mixed manual/automated approach creates inconsistency
- Validation becomes complex (which parts were manual?)

### C++ Plugin Enhancement
**Potential Fix:**
- Modify node_tools.py C++ binding layer
- Add special handling for K2 accessor nodes
- Map `get_available_blueprint_nodes` results to proper node creation

**Timeline:** Unknown - requires C++ plugin development

---

## Recommendations

### Short-Term (Complete Current Challenge)
1. ✅ **Document limitation** (this file)
2. ⚠️ **Hybrid approach:**
   - Use MCP tools for all possible nodes (variables, casts, special nodes)
   - Document which function calls require manual creation
   - Complete challenge with documented manual steps
3. 📊 **Update challenge status** to reflect hybrid completion

### Medium-Term (Fix for Future)
1. **Investigate node_tools.py C++ code:**
   - How does it resolve function calls?
   - Why does `function_class` hint fail?
   - Can we add K2 accessor node support?

2. **Create mapping system:**
   - Map `get_available_blueprint_nodes` results to creation parameters
   - Document node type → creation pattern relationships
   - Build lookup table for common nodes

### Long-Term (Complete Solution)
1. **C++ Plugin Enhancement:**
   - Add reflection-based function call resolution
   - Support for K2 accessor pattern nodes
   - Proper class hierarchy traversal for member functions

2. **Python Layer Abstraction:**
   - High-level function call creation API
   - Auto-detect node patterns (variable vs function vs accessor)
   - Smart defaults based on target class

---

## Next Steps

**Immediate Actions:**
1. Report blocker to user
2. Get guidance on hybrid vs pure MCP completion
3. Document manual steps if hybrid approach approved

**Investigation Tasks:**
1. Examine successful Get Player Controller creation  
2. Compare working vs failing node creation parameters
3. Review C++ plugin code for function resolution logic

**Testing Ideas:**
1. Try creating Actor Location as "variable" type instead of "function"
2. Test different node_type values from get_available_blueprint_nodes
3. Experiment with K2Node_CallFunction vs K2Node_VariableGet

---

## Success Criteria for Resolution

**Fix Validated When:**
1. ✅ Can create K2_GetActorLocation nodes programmatically
2. ✅ Created nodes have correct pins (not Material Function pins)
3. ✅ Nodes connect successfully to other graph nodes
4. ✅ Functions compile without errors
5. ✅ All 4 blocked functions can be completed with MCP tools only

**Test Case:**
```python
# Should create properly configured Get Actor Location node
result = manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player2",
    action="create",
    node_type=TBD,  # Need to discover correct type
    node_params=TBD,  # Need to discover correct params
    function_name="SpawnDeathEffects",
    graph_scope="function"
)

# Validation
assert result["success"] == True
assert "Set Material Function" not in result.get("display_name", "")
assert result.get("pin_count", 0) >= 2  # At minimum: self + ReturnValue
```

---

## Related Documentation

- ✅ `node-creation-patterns.md` - Variable and Cast patterns (WORKING)
- ✅ `node-tools-improvements.md` - Phase 1 implementation summary
- ⏸️ `Phase 2 Investigation` - TBD for C++ plugin fixes

---

**Last Updated:** October 3, 2025 23:45 UTC  
**Next Review:** After user feedback on hybrid approach
