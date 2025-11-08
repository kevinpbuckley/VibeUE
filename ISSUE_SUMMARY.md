# Issue Summary: Node Discovery Metadata Gap

**Date**: October 4, 2025  
**Status**: Root cause identified, solution designed

## The Core Problem

Our MCP tools have a **critical metadata gap** preventing reliable Blueprint node recreation:

### ❌ What's Broken:
```python
# When we describe existing nodes, we DON'T get spawner information
result = manage_blueprint_node(action="describe", node_id="{...}")
# Returns: display_name="Get Player Controller", class_path="/Script/..."
# Missing: spawner_key, function_class, target metadata
```

### ✅ What We Need:
```python
# We need spawner_key to create exact node variants
result = manage_blueprint_node(action="describe", node_id="{...}")
# Should return: spawner_key="GameplayStatics::GetPlayerController"
# Then we can: create(node_params={"spawner_key": "GameplayStatics::GetPlayerController"})
```

## Why This Matters

### Example: "Get Player Controller" has 8+ variants!

| Spawner Key | Function Class | Pin Count | Type |
|------------|----------------|-----------|------|
| `GameplayStatics::GetPlayerController` | GameplayStatics | 3 | Static ✅ **CORRECT** |
| `CheatManager::GetPlayerController` | CheatManager | 1 | Instance ❌ **WRONG** |
| `PlayerState::GetPlayerController` | PlayerState | 1 | Instance ❌ **WRONG** |

Without spawner_key from describe(), we can't tell which variant to create!

### Current Bug Example:
```python
# BP_Player has: GameplayStatics::GetPlayerController (3 pins, static)
nodes = describe(BP_Player, function="CastToMicrosubHUD")

# We try to recreate using display_name
create(node_type="Get Player Controller")  
# Result: Creates CheatManager::GetPlayerController (1 pin) ❌ WRONG!
```

## Impact on Your Blueprint Challenge

This is why we're stuck at 90% completion on CastToMicrosubHUD:

1. ✅ **Created Function Entry** - No ambiguity
2. ⚠️ **Created Get Player Controller** - WRONG VARIANT (CheatManager instead of GameplayStatics)
3. ✅ **Created Get HUD** - Pure function, less ambiguity
4. ⚠️ **Created Cast To Object** - Generic cast instead of typed BP_MicrosubHUD cast
5. ✅ **Created Set Microsub HUD** - Variable nodes work with variable_name parameter
6. ✅ **Connected 4/5 pins** - Compatible types connected
7. ❌ **Final connection blocked** - Type mismatch due to wrong cast node type

## The Solution

### C++ Plugin Change Required:

Add spawner metadata to the `describe` action response in `node_tools.cpp`:

```cpp
// For K2Node_CallFunction nodes
if (UK2Node_CallFunction* FuncNode = Cast<UK2Node_CallFunction>(Node)) {
    if (UFunction* Func = FuncNode->GetTargetFunction()) {
        NodeInfo.FunctionClass = Func->GetOwnerClass()->GetName();  // ✅ ADD
        NodeInfo.SpawnerKey = FString::Printf(TEXT("%s::%s"),       // ✅ ADD
            *NodeInfo.FunctionClass, *Func->GetName());
    }
}

// For K2Node_DynamicCast nodes
if (UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(Node)) {
    if (UClass* Target = CastNode->TargetType) {
        NodeInfo.CastTarget = Target->GetPathName();                // ✅ ADD
        NodeInfo.SpawnerKey = FString::Printf(TEXT("Cast To %s"),   // ✅ ADD
            *Target->GetName());
    }
}
```

## Immediate Workarounds

### Option 1: Manual Lookup (What I was doing)
```python
# For each node in source Blueprint
nodes = describe(BP_Player, "CastToMicrosubHUD")
for node in nodes:
    # Look up spawner for this specific node type
    variants = get_available_blueprint_nodes(search_term=node["display_name"])
    # AI guesses correct variant by pin count/type
    spawner = find_matching_variant(variants, node)
    create(node_params={"spawner_key": spawner})
```

**Drawback**: Requires 1 + N tool calls (slow, error-prone)

### Option 2: Pin-Based Heuristics (Unreliable)
```python
# Try to infer from pin types
if node["pins"][0]["pin_type_path"] == "/Script/Engine.PlayerController":
    # Guess it's GameplayStatics because it's static
    spawner = "GameplayStatics::GetPlayerController"
```

**Drawback**: Fails for nodes with identical pin structures

### Option 3: Accept Manual Completion in Unreal (Current State)
Complete CastToMicrosubHUD manually in Unreal Editor, then continue with other 4 functions using fuzzy node creation and manual fixes.

**Drawback**: Defeats the purpose of automated Blueprint recreation

## Recommended Next Steps

### Immediate (Your Decision):
1. **Accept 90% automation** - Complete CastToMicrosubHUD manually in Unreal
2. **Try Option 1 workaround** - I can implement the lookup workflow for remaining functions
3. **Wait for C++ fix** - Pause Blueprint recreation until describe() returns spawner_key

### Long Term (C++ Development):
1. Update `node_tools.cpp` `describe` action to return spawner metadata
2. Add `spawner_key` field to node descriptor JSON response
3. Add `function_class` and `cast_target` fields for function/cast nodes
4. Update Python tools documentation with new fields
5. Add validation tests for spawner_key accuracy

## Files to Review

1. **Gap Analysis**: `Plugins/VibeUE/Python/vibe-ue-main/Python/docs/node-discovery-gap-analysis.md`
2. **Node Tools Doc**: `node-tools-improvements.md` 
3. **C++ Source**: `Plugins/VibeUE/Source/VibeUE/Private/NodeTools.cpp`

## Questions for Discussion

1. Should we implement Option 1 workaround to finish the 4 remaining functions?
2. Priority for C++ fix - can you implement the spawner_key enhancement?
3. Should describe() return full descriptor format matching get_available_blueprint_nodes()?

---

**Bottom Line**: Our tools can discover exact node information via `get_available_blueprint_nodes()`, but `describe()` doesn't return that same information. This forces inefficient multi-call workflows and creates ambiguity in node recreation.

---

# Issue Summary: Missing Modules Dialog on Fresh Project Launch

**Date**: November 7, 2025  
**Status**: Needs investigation

## What Happened

When launching a brand-new Unreal project with the VibeUE plugin enabled, the editor displays a blocking dialog:

```
Missing Modules

The following modules are missing or built with a different engine version:
    VibeUE

Engine modules cannot be compiled at runtime. Please build through your IDE.
```

## Reproduction Steps

1. Create a clean UE 5.6 project.  
2. Enable or copy in the VibeUE plugin.  
3. Launch the project in the editor without performing a native code build.

## Impact

- Blocks artists/designers from opening the project because the engine refuses to hot-rebuild the plugin.  
- Creates confusion for new adopters who expect the plugin to "just work" after being copied into a fresh project.  
- Requires manual IDE build before the editor can run, which is easy to overlook.

## Open Questions / Next Actions

1. Do we need to ship prebuilt binaries for common engine versions, or detect the first-run scenario and prompt users with clearer instructions?  
2. Can BuildAndLaunch.ps1 be extended to detect the missing-binary state and compile automatically?  
3. Should documentation (README / quickstart) include an explicit "Run the build script before opening UE" call-out?

Attaching screenshot reference in repo history (see conversation attachment dated 2025-11-07).
