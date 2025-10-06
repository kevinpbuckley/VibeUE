# Node Discovery Gap Analysis
**Date**: October 4, 2025  
**Issue**: Discovery tools don't return complete recreation metadata

## Problem Statement

When using `manage_blueprint_node(action="describe")` to inspect existing nodes, the returned metadata **does NOT include the spawner_key or function_class information** needed to recreate nodes exactly.

This creates a gap where AI must:
1. Describe nodes in source Blueprint
2. Extract node display names
3. Call `get_available_blueprint_nodes()` for EACH node type to find spawner_key
4. Finally create nodes with exact spawner_key

## Example: Get Player Controller

### Current describe() Output (Insufficient):
```json
{
  "node_id": "{596D4420-4FCF-DEE5-ECC4-B1983E780E85}",
  "display_name": "Get Player Controller",
  "class_path": "/Script/BlueprintGraph.K2Node_CallFunction",
  "pins": [...]
  // ❌ NO spawner_key
  // ❌ NO function_class
  // ❌ NO function_metadata
}
```

### What We Need from get_available_blueprint_nodes():
```json
{
  "spawner_key": "GameplayStatics::GetPlayerController",  // ✅ REQUIRED
  "function_metadata": {
    "function_name": "GetPlayerController",
    "function_class": "GameplayStatics",               // ✅ REQUIRED
    "function_class_path": "/Script/Engine.GameplayStatics",
    "is_static": true,
    "is_pure": true
  },
  "expected_pin_count": 3
}
```

## Example: Cast To BP_MicrosubHUD

### Current describe() Output:
```json
{
  "node_id": "{63C84559-479C-4079-F700-E5B824225770}",
  "display_name": "Cast To BP_MicrosubHUD",
  "class_path": "/Script/BlueprintGraph.K2Node_DynamicCast",
  "pins": [
    {
      "name": "AsBP Microsub HUD",
      "pin_type_path": "/Game/Blueprints/HUD/BP_MicrosubHUD.BP_MicrosubHUD_C"  
      // ✅ Has typed output path
    }
  ]
  // ❌ NO spawner_key
  // ❌ NO cast_target metadata
}
```

### What We Get from get_available_blueprint_nodes():
```json
{
  "spawner_key": "Cast To BP_MicrosubHUD",  // ✅ Exact spawner
  "node_class_name": "K2Node_DynamicCast"
}
```

## Ambiguity Examples

### "Get Player Controller" has 8+ variants:
- `CheatManager::GetPlayerController` (1 pin, instance method)
- `GameplayStatics::GetPlayerController` (3 pins, static) ✅ **CORRECT ONE**
- `PlayerState::GetPlayerController` (1 pin, instance method)
- `CheatManagerExtension::GetPlayerController` (1 pin)
- `InputDeviceLibrary::GetPlayerControllerFromPlatformUser` (2 pins)
- etc.

**Without spawner_key, we create the wrong variant!**

## Proposed Solutions

### Option 1: Enhance describe() Output (PREFERRED)
Add spawner metadata to `manage_blueprint_node(action="describe")` response:

```python
{
  "node_id": "{596D4420-4FCF-DEE5-ECC4-B1983E780E85}",
  "display_name": "Get Player Controller",
  "class_path": "/Script/BlueprintGraph.K2Node_CallFunction",
  
  # ✅ ADD THESE FIELDS:
  "spawner_key": "GameplayStatics::GetPlayerController",
  "function_metadata": {
    "function_class": "GameplayStatics",
    "function_class_path": "/Script/Engine.GameplayStatics",
    "function_name": "GetPlayerController",
    "is_static": true,
    "is_pure": true
  },
  
  "pins": [...]
}
```

For cast nodes:
```python
{
  "node_id": "{63C84559-479C-4079-F700-E5B824225770}",
  "display_name": "Cast To BP_MicrosubHUD",
  "class_path": "/Script/BlueprintGraph.K2Node_DynamicCast",
  
  # ✅ ADD THESE FIELDS:
  "spawner_key": "Cast To BP_MicrosubHUD",
  "cast_metadata": {
    "cast_target": "/Game/Blueprints/HUD/BP_MicrosubHUD.BP_MicrosubHUD_C"
  },
  
  "pins": [...]
}
```

### Option 2: AI Lookup Workflow (CURRENT WORKAROUND)
Keep describe() as-is, require AI to:
1. Describe source nodes
2. For each node, call `get_available_blueprint_nodes(search_term=display_name)`
3. Match node by pin count and type
4. Use spawner_key for creation

**Drawback**: Requires 1 + N tool calls (1 describe + N node type lookups)

### Option 3: Batch Node Resolution Tool
Create new tool: `resolve_node_spawners(blueprint_name, node_ids[])`
- Takes array of node IDs from describe()
- Returns spawner_key for each
- Single call instead of N calls

## Impact on Blueprint Recreation

### Current Workflow (BROKEN):
```python
# 1. Describe source
nodes = describe("/Game/BP_Player", "CastToMicrosubHUD")

# 2. Create nodes - ❌ WRONG VARIANTS
for node in nodes:
    create(node_type=node["display_name"])  # Creates wrong Get Player Controller!
```

### Correct Workflow (TEDIOUS):
```python
# 1. Describe source
nodes = describe("/Game/BP_Player", "CastToMicrosubHUD")

# 2. Look up each node type - ⚠️ N additional calls!
for node in nodes:
    variants = get_available_blueprint_nodes(search_term=node["display_name"])
    spawner = match_by_pins(variants, node["pins"])  # AI must guess correct variant
    create(node_params={"spawner_key": spawner})
```

### Ideal Workflow (WITH FIX):
```python
# 1. Describe source - ✅ Returns spawner_key
nodes = describe("/Game/BP_Player", "CastToMicrosubHUD")

# 2. Create nodes directly - ✅ ONE call per node
for node in nodes:
    create(node_params={"spawner_key": node["spawner_key"]})  # Exact recreation!
```

## C++ Implementation Notes

The describe action queries `UK2Node` objects which have:
- `GetNodeTitle()` → display_name ✅ Already captured
- `GetClass()` → class_path ✅ Already captured
- `GetTargetFunction()` → For K2Node_CallFunction, returns UFunction*
  - `UFunction::GetName()` → function_name
  - `UFunction::GetOwnerClass()` → function_class ❌ NOT captured
- `GetTargetClass()` → For K2Node_DynamicCast, returns UClass* of cast target ❌ NOT captured

### Suggested C++ Changes:

```cpp
// In NodeDescriptor struct or equivalent
if (UK2Node_CallFunction* FuncNode = Cast<UK2Node_CallFunction>(Node)) {
    if (UFunction* Func = FuncNode->GetTargetFunction()) {
        Descriptor.FunctionName = Func->GetName();
        Descriptor.FunctionClass = Func->GetOwnerClass()->GetName();
        Descriptor.FunctionClassPath = Func->GetOwnerClass()->GetPathName();
        Descriptor.IsStatic = Func->HasAllFunctionFlags(FUNC_Static);
        Descriptor.IsPure = Func->HasAllFunctionFlags(FUNC_BlueprintPure);
        
        // Generate spawner_key
        Descriptor.SpawnerKey = FString::Printf(TEXT("%s::%s"), 
            *Descriptor.FunctionClass, *Descriptor.FunctionName);
    }
}

if (UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(Node)) {
    if (UClass* TargetClass = CastNode->TargetType) {
        Descriptor.CastTarget = TargetClass->GetPathName();
        Descriptor.SpawnerKey = FString::Printf(TEXT("Cast To %s"), 
            *TargetClass->GetName());
    }
}
```

## Validation

### Test Case 1: Get Player Controller
- **Source**: BP_Player, node ID {596D4420-4FCF-DEE5-ECC4-B1983E780E85}
- **Expected spawner_key**: "GameplayStatics::GetPlayerController"
- **Expected function_class**: "GameplayStatics"
- **Expected pin_count**: 3 (WorldContextObject hidden, PlayerIndex input, ReturnValue output)

### Test Case 2: Cast To BP_MicrosubHUD
- **Source**: BP_Player, node ID {63C84559-479C-4079-F700-E5B824225770}
- **Expected spawner_key**: "Cast To BP_MicrosubHUD"
- **Expected cast_target**: "/Game/Blueprints/HUD/BP_MicrosubHUD.BP_MicrosubHUD_C"
- **Expected pin_count**: 6

### Test Case 3: Variable Set
- **Source**: BP_Player, "Set Microsub HUD"
- **Expected spawner_key**: "SET Microsub HUD" or "Set Microsub HUD"
- **Expected variable_name**: "Microsub HUD"
- **Expected pin_count**: 5

## Priority: HIGH

This gap prevents reliable automated Blueprint recreation, which is a core use case for the VibeUE MCP system.

## Related Documentation
- `node-tools-improvements.md` - Spawner key system design
- `mcp_client_guide.py` - Node creation patterns
- GitHub issue: [Link when created]
