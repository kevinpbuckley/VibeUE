# Existence Check Methods - Design Document

**Status:** ✅ IMPLEMENTED
**Created:** 2026-01-16
**Implemented:** 2026-01-16
**Author:** AI/Human Collaboration
**Priority:** High - Prevents duplicate creation bugs

---

## Problem Statement

The AI assistant frequently creates duplicate assets, variables, components, nodes, and level actors because it doesn't check for existence before creation. This causes:

1. **Overwritten assets** - New assets replace existing ones
2. **Duplicate variables/functions** - Compilation errors in Blueprints
3. **Duplicate level actors** - Multiple actors with same label
4. **Duplicate graph nodes** - Messy blueprint graphs with redundant logic
5. **Script failures** - Assumptions that things don't exist when they do

---

## Solution: Service-Level Exists Methods

Add explicit `*_exists()` methods to each VibeUE service that creates things. These methods:
- Return simple `bool` (fast, unambiguous)
- Are cheaper than full search/list operations
- Give AI a clear "check first" pattern
- Enable idempotent script patterns

---

## Implementation Specification

### 1. BlueprintService

| Method | Signature | Description |
|--------|-----------|-------------|
| `blueprint_exists` | `(path: str) -> bool` | Check if Blueprint exists at path |
| `variable_exists` | `(blueprint_path: str, variable_name: str) -> bool` | Check if variable exists in BP |
| `function_exists` | `(blueprint_path: str, function_name: str) -> bool` | Check if function exists in BP |
| `component_exists` | `(blueprint_path: str, component_name: str) -> bool` | Check if component exists in BP |
| `local_variable_exists` | `(blueprint_path: str, function_name: str, variable_name: str) -> bool` | Check if local var exists in function |
| `node_exists` | `(blueprint_path: str, graph_name: str, node_name: str) -> bool` | Check if node exists in graph by name |
| `node_exists_by_type` | `(blueprint_path: str, graph_name: str, node_class: str) -> bool` | Check if node of type exists in graph |

#### Node Existence Details

Nodes are trickier because they don't always have unique names. Options:

```python
# By node name (custom/comment nodes)
unreal.BlueprintService.node_exists(bp_path, "EventGraph", "MyCustomEvent")

# By node class (function calls, events)
unreal.BlueprintService.node_exists_by_type(bp_path, "EventGraph", "K2Node_Event")

# By function reference (specific function call nodes)
unreal.BlueprintService.function_call_exists(bp_path, "EventGraph", "PrintString")
```

**Recommended approach:** Check by node title/name for custom events, by function reference for call nodes.

---

### 2. MaterialService

| Method | Signature | Description |
|--------|-----------|-------------|
| `material_exists` | `(path: str) -> bool` | Check if material exists |
| `material_instance_exists` | `(path: str) -> bool` | Check if material instance exists |
| `parameter_exists` | `(material_path: str, param_name: str) -> bool` | Check if parameter exists in material |
| `node_exists` | `(material_path: str, node_name: str) -> bool` | Check if material expression node exists |

---

### 3. DataTableService

| Method | Signature | Description |
|--------|-----------|-------------|
| `data_table_exists` | `(path: str) -> bool` | Check if DataTable exists |
| `row_exists` | `(table_path: str, row_name: str) -> bool` | Check if row exists in DataTable |
| `column_exists` | `(table_path: str, column_name: str) -> bool` | Check if column exists (struct property) |

---

### 4. DataAssetService

| Method | Signature | Description |
|--------|-----------|-------------|
| `data_asset_exists` | `(path: str) -> bool` | Check if DataAsset exists |
| `property_exists` | `(asset_path: str, property_name: str) -> bool` | Check if property exists on asset |

---

### 5. WidgetService

| Method | Signature | Description |
|--------|-----------|-------------|
| `widget_blueprint_exists` | `(path: str) -> bool` | Check if Widget Blueprint exists |
| `widget_exists` | `(blueprint_path: str, widget_name: str) -> bool` | Check if widget exists in hierarchy |
| `binding_exists` | `(blueprint_path: str, widget_name: str, property_name: str) -> bool` | Check if property binding exists |

---

### 6. InputService

| Method | Signature | Description |
|--------|-----------|-------------|
| `input_action_exists` | `(path: str) -> bool` | Check if Input Action exists |
| `mapping_context_exists` | `(path: str) -> bool` | Check if Input Mapping Context exists |
| `mapping_exists` | `(context_path: str, action_path: str) -> bool` | Check if action is mapped in context |
| `trigger_exists` | `(context_path: str, action_path: str, trigger_type: str) -> bool` | Check if trigger type exists on mapping |

---

### 7. AssetDiscoveryService

| Method | Signature | Description |
|--------|-----------|-------------|
| `asset_exists` | `(path: str) -> bool` | Check if any asset exists at path (generic fallback) |

---

### 8. LevelActorService (NEW SERVICE)

A new lightweight service specifically for level actor operations. Currently level actors are accessed via `LevelEditorSubsystem` which is verbose for common operations.

| Method | Signature | Description |
|--------|-----------|-------------|
| `actor_exists` | `(actor_label: str) -> bool` | Check if actor with label exists in current level |
| `actor_exists_by_name` | `(actor_name: str) -> bool` | Check by internal FName |
| `actor_exists_by_tag` | `(tag: str) -> bool` | Check if any actor has this tag |
| `actor_exists_at_location` | `(x: float, y: float, z: float, tolerance: float) -> bool` | Check if actor exists near location |

**Rationale for new service:**
- Level actors don't have asset paths (unlike other assets)
- Current subsystem requires iteration pattern
- Consistent API surface with other VibeUE services
- Single method call vs. multi-line existence check

---

## C++ Implementation Pattern

### Header Example (BlueprintService.h)

```cpp
UCLASS()
class VIBEUE_API UBlueprintService : public UObject
{
    GENERATED_BODY()

public:
    // Existence checks - fast boolean returns
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprint|Exists")
    static bool BlueprintExists(const FString& Path);
    
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprint|Exists")
    static bool VariableExists(const FString& BlueprintPath, const FString& VariableName);
    
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprint|Exists")
    static bool FunctionExists(const FString& BlueprintPath, const FString& FunctionName);
    
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprint|Exists")
    static bool ComponentExists(const FString& BlueprintPath, const FString& ComponentName);
    
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprint|Exists")
    static bool LocalVariableExists(const FString& BlueprintPath, const FString& FunctionName, const FString& VariableName);
    
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprint|Exists")
    static bool NodeExists(const FString& BlueprintPath, const FString& GraphName, const FString& NodeName);
    
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprint|Exists")
    static bool FunctionCallExists(const FString& BlueprintPath, const FString& GraphName, const FString& FunctionName);
    
    // ... existing methods ...
};
```

### Implementation Example (BlueprintService.cpp)

```cpp
bool UBlueprintService::VariableExists(const FString& BlueprintPath, const FString& VariableName)
{
    // Fast path - don't use TResult, just return bool
    UBlueprint* Blueprint = Cast<UBlueprint>(
        StaticLoadObject(UBlueprint::StaticClass(), nullptr, *BlueprintPath));
    
    if (!Blueprint)
    {
        return false; // Blueprint doesn't exist = variable doesn't exist
    }
    
    for (const FBPVariableDescription& Var : Blueprint->NewVariables)
    {
        if (Var.VarName.ToString().Equals(VariableName, ESearchCase::IgnoreCase))
        {
            return true;
        }
    }
    
    return false;
}

bool UBlueprintService::NodeExists(const FString& BlueprintPath, const FString& GraphName, const FString& NodeName)
{
    UBlueprint* Blueprint = Cast<UBlueprint>(
        StaticLoadObject(UBlueprint::StaticClass(), nullptr, *BlueprintPath));
    
    if (!Blueprint)
    {
        return false;
    }
    
    UEdGraph* Graph = FindGraph(Blueprint, GraphName);
    if (!Graph)
    {
        return false;
    }
    
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString().Equals(NodeName, ESearchCase::IgnoreCase))
        {
            return true;
        }
    }
    
    return false;
}

bool UBlueprintService::FunctionCallExists(const FString& BlueprintPath, const FString& GraphName, const FString& FunctionName)
{
    UBlueprint* Blueprint = Cast<UBlueprint>(
        StaticLoadObject(UBlueprint::StaticClass(), nullptr, *BlueprintPath));
    
    if (!Blueprint)
    {
        return false;
    }
    
    UEdGraph* Graph = FindGraph(Blueprint, GraphName);
    if (!Graph)
    {
        return false;
    }
    
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
        {
            if (CallNode->GetFunctionName().ToString().Equals(FunctionName, ESearchCase::IgnoreCase))
            {
                return true;
            }
        }
    }
    
    return false;
}
```

---

## Python Usage Patterns

### Idempotent Asset Creation

```python
import unreal

# PATTERN: Check before creating
if not unreal.BlueprintService.blueprint_exists("/Game/Blueprints/BP_Enemy"):
    path = unreal.BlueprintService.create_blueprint("BP_Enemy", "Actor", "/Game/Blueprints")
    print(f"Created: {path}")
else:
    print("BP_Enemy already exists, skipping creation")
```

### Idempotent Variable Addition

```python
import unreal

bp_path = "/Game/Blueprints/BP_Enemy"

# Check before adding variable
if not unreal.BlueprintService.variable_exists(bp_path, "Health"):
    unreal.BlueprintService.add_variable(bp_path, "Health", "float")
    print("Added Health variable")
else:
    print("Health variable already exists")

# Compile regardless (ensures any changes are applied)
unreal.BlueprintService.compile_blueprint(bp_path)
```

### Idempotent Node Addition

```python
import unreal

bp_path = "/Game/Blueprints/BP_Enemy"
graph = "EventGraph"

# Check before adding event node
if not unreal.BlueprintService.node_exists(bp_path, graph, "Event BeginPlay"):
    unreal.BlueprintService.add_event_node(bp_path, graph, "BeginPlay")
    print("Added BeginPlay event")
else:
    print("BeginPlay event already exists")

# Check before adding function call
if not unreal.BlueprintService.function_call_exists(bp_path, graph, "PrintString"):
    unreal.BlueprintService.add_function_call_node(bp_path, graph, "PrintString", 400, 0)
    print("Added PrintString call")
else:
    print("PrintString call already exists in graph")
```

### Idempotent Level Actor Spawning

```python
import unreal

# Check before spawning actor
if not unreal.LevelActorService.actor_exists("TargetDummy_01"):
    actor = unreal.LevelActorService.spawn_actor(
        actor_class="StaticMeshActor",
        label="TargetDummy_01",
        location=(100, 200, 0)
    )
    print(f"Spawned: {actor.get_actor_label()}")
else:
    print("TargetDummy_01 already exists in level")
```

### Bulk Creation with Existence Checks

```python
import unreal

bp_path = "/Game/Blueprints/BP_Character"

# Define variables to add
variables = [
    ("Health", "float", "100.0"),
    ("MaxHealth", "float", "100.0"),
    ("Armor", "float", "0.0"),
    ("Name", "FString", "Player"),
]

# Add only missing variables
for var_name, var_type, default in variables:
    if not unreal.BlueprintService.variable_exists(bp_path, var_name):
        unreal.BlueprintService.add_variable(bp_path, var_name, var_type, default)
        print(f"Added: {var_name}")
    else:
        print(f"Exists: {var_name}")

unreal.BlueprintService.compile_blueprint(bp_path)
```

---

## Instruction File Updates

After implementation, update the following files:

### 1. Base Instructions (`vibeue.instructions.md`)

Add new Critical Rule section:

```markdown
### ⚠️ ALWAYS Check Before Creating (Idempotency Rule)

**NEVER create assets, variables, components, nodes, or actors without checking if they exist first.**

Use the appropriate `*_exists()` method before any creation operation:

| Creating | Check Method |
|----------|-------------|
| Blueprint | `BlueprintService.blueprint_exists(path)` |
| Variable | `BlueprintService.variable_exists(bp_path, name)` |
| Function | `BlueprintService.function_exists(bp_path, name)` |
| Component | `BlueprintService.component_exists(bp_path, name)` |
| Local Variable | `BlueprintService.local_variable_exists(bp_path, func_name, var_name)` |
| Graph Node | `BlueprintService.node_exists(bp_path, graph, node_name)` |
| Function Call | `BlueprintService.function_call_exists(bp_path, graph, func_name)` |
| Material | `MaterialService.material_exists(path)` |
| Material Instance | `MaterialService.material_instance_exists(path)` |
| Material Node | `MaterialService.node_exists(mat_path, node_name)` |
| Data Table | `DataTableService.data_table_exists(path)` |
| DT Row | `DataTableService.row_exists(table_path, row_name)` |
| Data Asset | `DataAssetService.data_asset_exists(path)` |
| Widget Blueprint | `WidgetService.widget_blueprint_exists(path)` |
| Widget Child | `WidgetService.widget_exists(wbp_path, name)` |
| Input Action | `InputService.input_action_exists(path)` |
| Mapping Context | `InputService.mapping_context_exists(path)` |
| Level Actor | `LevelActorService.actor_exists(label)` |
| Any Asset | `AssetDiscoveryService.asset_exists(path)` |
```

### 2. Each Skill's Workflow Files

Update workflow examples to include existence checks. Example for `blueprints/02-workflows.md`:

```markdown
## Adding a Variable (Idempotent)

Always check before adding:

\`\`\`python
if not unreal.BlueprintService.variable_exists(bp_path, "Health"):
    unreal.BlueprintService.add_variable(bp_path, "Health", "float")
\`\`\`
```

### 3. Skill API Sections

The `vibeue_apis` auto-discovery will pick up new methods automatically. No manual updates needed.

---

## Implementation Phases

### Phase 1: Core ✅ COMPLETED (2026-01-16)
- [x] `AssetDiscoveryService.asset_exists` - generic fallback
- [x] `BlueprintService.blueprint_exists`
- [x] `BlueprintService.variable_exists`
- [x] `BlueprintService.function_exists`
- [x] `BlueprintService.component_exists`
- [x] `BlueprintService.local_variable_exists`
- [x] `BlueprintService.node_exists`
- [x] `BlueprintService.function_call_exists`
- [x] Update base instructions with idempotency rule

### Phase 2: Data Services ✅ COMPLETED (2026-01-16)
- [x] `DataTableService.data_table_exists`
- [x] `DataTableService.row_exists`
- [ ] `DataTableService.column_exists` - DEFERRED (columns defined by struct, not runtime)
- [x] `DataAssetService.data_asset_exists`
- [ ] `DataAssetService.property_exists` - DEFERRED (use reflection if needed)
- [ ] Update data-tables and data-assets skills - PENDING skill file updates

### Phase 3: UI & Materials ✅ COMPLETED (2026-01-16)
- [x] `WidgetService.widget_blueprint_exists`
- [x] `WidgetService.widget_exists`
- [ ] `WidgetService.binding_exists` - DEFERRED (complex binding system)
- [x] `MaterialService.material_exists`
- [x] `MaterialService.material_instance_exists`
- [x] `MaterialService.parameter_exists`
- [ ] `MaterialService.node_exists` - DEFERRED (material node graphs are complex)
- [ ] Update umg-widgets and materials skills - PENDING skill file updates

### Phase 4: Input & Actors ✅ COMPLETED (2026-01-16)
- [x] `InputService.input_action_exists`
- [x] `InputService.mapping_context_exists`
- [x] `InputService.key_mapping_exists` (renamed from mapping_exists)
- [ ] `InputService.trigger_exists` - DEFERRED (can be added if needed)
- [x] `ActorService.actor_exists` (added to existing ActorService, not new service)
- [x] `ActorService.actor_exists_by_tag`
- [ ] `ActorService.actor_exists_at_location` - DEFERRED (can be added if needed)
- [ ] Update enhanced-input and level-actors skills - PENDING skill file updates

### Phase 5: Testing & Polish
- [ ] Add test prompts for idempotency testing
- [ ] Verify all exists methods appear in skill vibeue_apis
- [ ] Final documentation review
- [ ] Update CONTRIBUTING.md if needed

---

## Testing Strategy

### Test Prompts to Add

Create `test_prompts/idempotency/` directory with:

1. **01-duplicate-variable.md**
   ```
   Add a Health variable to BP_Player_Test, then add it again.
   The second addition should be skipped, not cause an error or duplicate.
   ```

2. **02-duplicate-actor.md**
   ```
   Spawn a cube at 0,0,0 named "TestCube", then run the same command again.
   The second spawn should be skipped.
   ```

3. **03-duplicate-node.md**
   ```
   Add a PrintString node to BP_Player_Test's EventGraph.
   Run the same command again - should skip if already present.
   ```

4. **04-bulk-creation.md**
   ```
   Add variables Health, MaxHealth, Armor, Name to BP_Player_Test.
   Run again - should report which already exist and skip them.
   ```

---

## Open Questions

1. **Case sensitivity**: Should existence checks be case-insensitive?
   - **Recommendation**: Yes, use `ESearchCase::IgnoreCase` - Unreal is generally case-insensitive

2. **Return type**: `bool` vs `TOptional<bool>` vs `TResult<bool>`?
   - **Recommendation**: Simple `bool` - false covers both "doesn't exist" and "error loading parent"

3. **Node identification**: By name, by class, by GUID, or by function reference?
   - **Recommendation**: Multiple methods - `node_exists` by name, `function_call_exists` by function reference

4. **Level context**: Current level only, or all loaded levels?
   - **Recommendation**: Current level (persistent + sublevels) - matches typical use case

---

## Success Criteria

1. AI stops creating duplicate assets when asked to "add X" multiple times
2. Scripts become idempotent - can be re-run without side effects
3. Error messages improve: "Health already exists" instead of compilation error
4. Level actor spawning respects existing actor labels
5. Graph nodes aren't duplicated when scripts run multiple times

---

## Related Documents

- [vibeue.instructions.md](../instructions/vibeue.instructions.md) - Base AI instructions
- [Skills System](../Skills/) - Domain-specific workflow documentation
- [Test Prompts](../../test_prompts/) - Integration test scenarios
