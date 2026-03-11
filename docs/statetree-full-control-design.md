# StateTree Full Control — Design Document

**Date**: 2026-03-10  
**Status**: Draft  
**Goal**: Enable VibeUE AI to fully manage every editable aspect of a StateTree asset through `UStateTreeService`, eliminating all reliance on `get_editor_property`/`set_editor_property` (which fails on protected EditorData).

---

## Problem Statement

The current `UStateTreeService` API has major gaps. The AI cannot:
1. **Edit existing transitions** — only `AddTransition` exists, so the AI creates duplicates when asked to "change" one
2. **Set selection behavior** — the AI enters a loop trying `get_editor_property("EditorData")` which is protected
3. **Manage parameters** — only `AddOrUpdateRootFloatParameter` exists (float only, no int/bool/string/struct/enum/vector, no remove, no list)
4. **Add/edit/remove conditions** — no condition management at all (enter conditions on states, conditions on transitions)
5. **Edit transition properties** — cannot modify trigger, target, priority, delay, enable/disable on existing transitions
6. **Edit task ordering or removal** — can add tasks but cannot remove or reorder them
7. **Edit evaluator/global task properties** — can add but cannot get/set properties, remove, or reorder
8. **Manage considerations (Utility AI)** — no consideration support at all
9. **Set state properties** — cannot set `Tag`, `Weight`, `TasksCompletion`, `CustomTickRate`, `RequiredEventToEnter`, `LinkedSubtree`, `LinkedAsset`
10. **Rename states** — no rename API

Below is a prioritized catalog of everything needed for "full control."

---

## Current API (What Exists)

### Discovery
| Method | Purpose |
|--------|---------|
| `ListStateTrees(DirectoryPath)` | List StateTree assets |
| `GetStateTreeInfo(AssetPath) → FStateTreeInfo` | Full structural inspection |
| `GetAvailableTaskTypes()` | List task struct names |
| `GetAvailableEvaluatorTypes()` | List evaluator struct names |
| `GetThemeColors(AssetPath)` | List color table entries |

### Asset Creation
| Method | Purpose |
|--------|---------|
| `CreateStateTree(AssetPath, SchemaClassName)` | Create new asset |

### State Management
| Method | Purpose |
|--------|---------|
| `AddState(AssetPath, ParentPath, StateName, StateType)` | Add a state |
| `RemoveState(AssetPath, StatePath)` | Remove a state + children |
| `SetStateEnabled(AssetPath, StatePath, bEnabled)` | Enable/disable |
| `SetStateDescription(AssetPath, StatePath, Description)` | Set description |
| `SetStateThemeColor(AssetPath, StatePath, ColorName, Color)` | Set color |
| `RenameThemeColor(AssetPath, OldColorName, NewColorName)` | Rename color entry |
| `SetStateExpanded(AssetPath, StatePath, bExpanded)` | Expand/collapse |
| `SetContextActorClass(AssetPath, ActorClassPath)` | Set schema context actor |

### Parameters
| Method | Purpose |
|--------|---------|
| `AddOrUpdateRootFloatParameter(AssetPath, Name, Default)` | Float parameter only |

### Tasks
| Method | Purpose |
|--------|---------|
| `AddTask(AssetPath, StatePath, TaskStructName)` | Add task to state |
| `GetTaskPropertyNames(AssetPath, StatePath, TaskStruct, MatchIndex)` | Discover properties |
| `GetTaskPropertyValue(AssetPath, StatePath, TaskStruct, PropertyPath, MatchIndex)` | Read property |
| `SetTaskPropertyValue(AssetPath, StatePath, TaskStruct, PropertyPath, Value, MatchIndex)` | Write property |
| `SetTaskPropertyValueDetailed(...)` | Write with error detail |
| `BindTaskPropertyToRootParameter(...)` | Bind to parameter |
| `BindTaskPropertyToContext(...)` | Bind to context |

### Evaluators / Global Tasks
| Method | Purpose |
|--------|---------|
| `AddEvaluator(AssetPath, EvaluatorStructName)` | Add evaluator |
| `AddGlobalTask(AssetPath, TaskStructName)` | Add global task |

### Transitions
| Method | Purpose |
|--------|---------|
| `AddTransition(AssetPath, StatePath, Trigger, Type, TargetPath, Priority)` | Add-only |

### Compile / Save
| Method | Purpose |
|--------|---------|
| `CompileStateTree(AssetPath)` | Compile |
| `SaveStateTree(AssetPath)` | Save to disk |

---

## New API Required — Organized by Domain

### Priority 1 — Critical Gaps (AI loops/fails without these)

#### 1.1 State Properties

These properties exist on `UStateTreeState` but have no setter in the service.

| New Method | UE Property | Values |
|-----------|------------|--------|
| `SetSelectionBehavior(AssetPath, StatePath, Behavior)` | `SelectionBehavior` | `"None"`, `"TryEnterState"`, `"TrySelectChildrenInOrder"`, `"TrySelectChildrenAtRandom"`, `"TrySelectChildrenWithHighestUtility"`, `"TrySelectChildrenAtRandomWeightedByUtility"`, `"TryFollowTransitions"` |
| `SetTasksCompletion(AssetPath, StatePath, Completion)` | `TasksCompletion` | `"Any"`, `"All"` |
| `RenameState(AssetPath, StatePath, NewName)` | `Name` | FName |
| `SetStateTag(AssetPath, StatePath, GameplayTag)` | `Tag` | FGameplayTag string |
| `SetStateWeight(AssetPath, StatePath, Weight)` | `Weight` | float ≥ 0 |

**Implementation pattern**: Same as `SetStateEnabled` — `FindStateByPath` → set property → `MarkStateTreeDirty`.

#### 1.1b Task Completion Toggle

S2-L3 demonstrates the "clipboard" icon that toggles whether a task's completion contributes to the owning state's completion. This is `bConsideredForCompletion` on `FStateTreeTaskBase`.

| New Method | UE Property | Values |
|-----------|------------|--------|
| `SetTaskConsideredForCompletion(AssetPath, StatePath, TaskStructName, MatchIndex, bConsideredForCompletion)` | `bConsideredForCompletion` | `true` = task completion can trigger state completion; `false` = task runs in background |

**Implementation**: Find the task's `FStateTreeEditorNode`, access its `FStateTreeTaskBase` instance data, set `bConsideredForCompletion`. Also add `bConsideredForCompletion` field to `FStateTreeNodeInfo` so the AI can inspect the current toggle state.

#### 1.2 Transition Editing

Currently the AI can only add transitions. It needs CRUD for transitions.

**Transition identification**: By index within the state's `Transitions` array (0-based). The index is deterministic and matches the order returned by `GetStateTreeInfo`.

| New Method | Purpose |
|-----------|---------|
| `SetTransitionTrigger(AssetPath, StatePath, TransitionIndex, Trigger)` | Change trigger type |
| `SetTransitionType(AssetPath, StatePath, TransitionIndex, TransitionType, TargetPath)` | Change transition type and target |
| `SetTransitionPriority(AssetPath, StatePath, TransitionIndex, Priority)` | Change priority |
| `SetTransitionEnabled(AssetPath, StatePath, TransitionIndex, bEnabled)` | Enable/disable |
| `SetTransitionDelay(AssetPath, StatePath, TransitionIndex, bDelay, Duration, RandomVariance)` | Set delay |
| `RemoveTransition(AssetPath, StatePath, TransitionIndex)` | Remove by index |

**Alternative — single "UpdateTransition" method** (preferred for fewer API methods):

```cpp
// Updates specified fields on an existing transition. Empty/default strings = "don't change".
static bool UpdateTransition(
    const FString& AssetPath,
    const FString& StatePath,
    int32 TransitionIndex,
    const FString& Trigger = TEXT(""),           // empty = don't change
    const FString& TransitionType = TEXT(""),     // empty = don't change
    const FString& TargetPath = TEXT(""),         // empty = don't change (only for GotoState)
    const FString& Priority = TEXT(""),           // empty = don't change
    bool bSetEnabled = false,                     // whether to change enabled
    bool bEnabled = true,                         // new enabled value
    bool bSetDelay = false,                       // whether to change delay
    bool bDelayTransition = false,                // new delay enabled value
    float DelayDuration = 0.0f,
    float DelayRandomVariance = 0.0f
);
```

**Recommendation**: Use the single `UpdateTransition` approach since it mirrors what users typically want ("change the trigger on transition 0"). Also add `RemoveTransition` as a separate method.

Also need:
- `MoveTransition(AssetPath, StatePath, FromIndex, ToIndex)` — S2-L6 demonstrates reordering transitions via drag handles ("we have the ability to reorder them"). Transitions are evaluated in order, so position matters.
- `FStateTreeTransitionInfo` to include: transition index, enabled, delay, delay duration, delay variance — so the AI can inspect before editing.

#### 1.3 Parameters (Root Property Bag)

Replace the single-type `AddOrUpdateRootFloatParameter` with a generic system.

| New Method | Purpose |
|-----------|---------|
| `GetRootParameters(AssetPath) → TArray<FStateTreeParameterInfo>` | List all parameters with name, type, default value |
| `AddOrUpdateRootParameter(AssetPath, Name, Type, DefaultValue)` | Add/update any type |
| `RemoveRootParameter(AssetPath, Name)` | Remove a parameter by name |
| `RenameRootParameter(AssetPath, OldName, NewName)` | Rename without breaking existing bindings |

**`Type` values**: `"Bool"`, `"Int32"`, `"Int64"`, `"Float"`, `"Double"`, `"Name"`, `"String"`, `"Text"`, `"Enum"`, `"Struct"`, `"Object"`, `"SoftObject"`, `"Class"`, `"SoftClass"`

These map to `EPropertyBagPropertyType` values.

**New struct**:
```cpp
USTRUCT(BlueprintType)
struct FStateTreeParameterInfo
{
    FString Name;
    FString Type;           // EPropertyBagPropertyType as string
    FString DefaultValue;   // Exported as text
};
```

**Implementation**: Uses `FInstancedPropertyBag::AddProperty()` / `RemovePropertyByName()`. For setting default values, use `SetValueFloat`, `SetValueInt32`, `SetValueBool`, etc. based on type, or the generic `SetValue` with text import.

### Priority 2 — Important for Common Workflows

#### 2.1 Conditions (Enter Conditions + Transition Conditions)

Conditions use the same `FStateTreeEditorNode` pattern as tasks. We need symmetric CRUD.

| New Method | Purpose |
|-----------|---------|
| `GetAvailableConditionTypes() → TArray<FString>` | List condition struct names |
| `AddEnterCondition(AssetPath, StatePath, ConditionStructName) → bool` | Add enter condition to a state |
| `RemoveEnterCondition(AssetPath, StatePath, ConditionIndex) → bool` | Remove by index |
| `SetEnterConditionOperand(AssetPath, StatePath, ConditionIndex, Operand)` | Set `"And"` / `"Or"` |
| `GetEnterConditionPropertyNames(AssetPath, StatePath, ConditionStructName, MatchIndex) → TArray<FStateTreePropertyInfo>` | Discover properties |
| `SetEnterConditionPropertyValue(AssetPath, StatePath, ConditionStructName, PropertyPath, Value, MatchIndex)` | Set property |
| `AddTransitionCondition(AssetPath, StatePath, TransitionIndex, ConditionStructName)` | Add condition to transition |
| `RemoveTransitionCondition(AssetPath, StatePath, TransitionIndex, ConditionIndex)` | Remove condition from transition |
| `SetTransitionConditionOperand(AssetPath, StatePath, TransitionIndex, ConditionIndex, Operand)` | Set `"And"` / `"Or"` |
| `GetTransitionConditionPropertyNames(AssetPath, StatePath, TransitionIndex, ConditionStructName, MatchIndex) → TArray<FStateTreePropertyInfo>` | Discover properties |
| `SetTransitionConditionPropertyValue(AssetPath, StatePath, TransitionIndex, ConditionStructName, PropertyPath, Value, MatchIndex)` | Set property |

**Condition types in UE 5.7**:
- `FStateTreeCompareIntCondition`
- `FStateTreeCompareFloatCondition`
- `FStateTreeCompareBoolCondition`
- `FStateTreeCompareEnumCondition`
- `FStateTreeCompareDistanceCondition`
- `FStateTreeCompareNameCondition`
- `FStateTreeRandomCondition`
- `FStateTreeObjectIsValidCondition`
- `FStateTreeObjectEqualsCondition`
- `FStateTreeObjectIsChildOfCondition`
- Blueprint-derived conditions

**Implementation**: The pattern already exists for tasks — `FindTaskNodeByStruct`, property enumeration, etc. Conditions are structurally identical (`FStateTreeEditorNode` array). Refactor existing task property helpers to be generic "EditorNode property" helpers.

#### 2.2 Task / Evaluator / Global Task Management (CRUD)

| New Method | Purpose |
|-----------|---------|
| `RemoveTask(AssetPath, StatePath, TaskStructName, MatchIndex)` | Remove a task |
| `MoveTask(AssetPath, StatePath, TaskStructName, MatchIndex, NewIndex)` | Reorder tasks |
| `SetTaskEnabled(AssetPath, StatePath, TaskStructName, MatchIndex, bEnabled)` | Per the existing `bEnabled` on `FStateTreeEditorNode` |
| `RemoveEvaluator(AssetPath, EvaluatorStructName, MatchIndex)` | Remove evaluator |
| `GetEvaluatorPropertyNames(AssetPath, EvaluatorStructName, MatchIndex)` | Like GetTaskPropertyNames |
| `SetEvaluatorPropertyValue(AssetPath, EvaluatorStructName, PropertyPath, Value, MatchIndex)` | Like SetTaskPropertyValue |
| `RemoveGlobalTask(AssetPath, TaskStructName, MatchIndex)` | Remove global task |
| `GetGlobalTaskPropertyNames(AssetPath, TaskStructName, MatchIndex)` | Like GetTaskPropertyNames |
| `SetGlobalTaskPropertyValue(AssetPath, TaskStructName, PropertyPath, Value, MatchIndex)` | Like SetTaskPropertyValue |

**Implementation note**: All evaluator/global task nodes live on `EditorData->Evaluators` and `EditorData->GlobalTasks` respectively — same `TArray<FStateTreeEditorNode>` pattern as state tasks.

#### 2.3 Custom Task Blueprint Creation & Configuration

S2-L5 demonstrates creating custom task BPs from `StateTreeTaskBlueprintBase` and configuring their variables. The AI needs to be able to scaffold these.

| New Method | Purpose |
|-----------|--------|
| `CreateCustomTaskBlueprint(AssetPath, TaskName)` | Create a new Blueprint subclass of `StateTreeTaskBlueprintBase`. Returns the asset path of the created BP. |
| `SetTaskBlueprintVariableCategory(TaskBlueprintPath, VariableName, Category)` | Set a BP variable's category to `"Input"`, `"Context"`, or `"Default"`. Input variables appear as bindable parameters on the task node in the state tree. Context variables auto-bind to matching context objects. |

**Why this matters**: S2-L5 shows the core pattern for extending StateTree behavior — create a custom task BP, add variables, set them as Input or Context so they participate in data binding. Without these APIs, the AI can only use built-in tasks.

**Implementation**: Uses `UEditorAssetLibrary` / `FKismetEditorUtilities` to create the BP asset, and property metadata APIs to set the variable category. The category is stored as metadata on the `FProperty` (`CPF_ExposeOnSpawn`-like flags or `FBlueprintEditorUtils::SetBlueprintVariableCategory`).

#### 2.4 Enhanced GetStateTreeInfo

Augment the return data for full AI visibility:

**`FStateTreeStateInfo` additions**:
| New Field | Type | Source |
|----------|------|--------|
| `SelectionBehavior` | FString | ✅ Already exists |
| `ThemeColor` | FString | ✅ Already added |
| `bExpanded` | bool | ✅ Already added |
| `Tag` | FString | `State->Tag.ToString()` |
| `Description` | FString | `State->Description` |
| `Weight` | float | `State->Weight` |
| `TasksCompletion` | FString | `"Any"` / `"All"` |
| `bHasCustomTickRate` | bool | `State->bHasCustomTickRate` |
| `CustomTickRate` | float | `State->CustomTickRate` |
| `RequiredEventTag` | FString | `State->RequiredEventToEnter.Tag.ToString()` |
| `Considerations` | TArray\<FStateTreeNodeInfo\> | Like Tasks |

**`FStateTreeNodeInfo` additions** (task/evaluator/condition nodes):
| New Field | Type | Source |
|----------|------|--------|
| `bConsideredForCompletion` | bool | `FStateTreeTaskBase::bConsideredForCompletion` — the "clipboard toggle" that controls whether a task's completion contributes to state completion |

**`FStateTreeTransitionInfo` additions**:
| New Field | Type | Source |
|----------|------|--------|
| `Index` | int32 | Position in the Transitions array |
| `bEnabled` | bool | `Trans.bTransitionEnabled` |
| `bDelayTransition` | bool | `Trans.bDelayTransition` |
| `DelayDuration` | float | `Trans.DelayDuration` |
| `DelayRandomVariance` | float | `Trans.DelayRandomVariance` |
| `RequiredEventTag` | FString | `Trans.RequiredEvent.Tag.ToString()` |
| `Conditions` | TArray\<FStateTreeNodeInfo\> | Transition conditions |
| `ConditionOperands` | TArray\<FString\> | `"Copy"`, `"And"`, `"Or"` per condition |

**`FStateTreeInfo` additions**:
| New Field | Type | Source |
|----------|------|--------|
| `RootParameters` | TArray\<FStateTreeParameterInfo\> | From `RootParameterPropertyBag` |
| `GlobalTasksCompletion` | FString | `"Any"` / `"All"` |

### Priority 3 — Advanced / Utility AI

#### 3.1 Considerations (Utility AI)

| New Method | Purpose |
|-----------|---------|
| `GetAvailableConsiderationTypes() → TArray<FString>` | List consideration struct names |
| `AddConsideration(AssetPath, StatePath, ConsiderationStructName)` | Add to a state |
| `RemoveConsideration(AssetPath, StatePath, ConsiderationIndex)` | Remove by index |
| `GetConsiderationPropertyNames(AssetPath, StatePath, ConsiderationStructName, MatchIndex)` | Discover properties |
| `SetConsiderationPropertyValue(AssetPath, StatePath, ConsiderationStructName, PropertyPath, Value, MatchIndex)` | Set property |

**Consideration types in UE 5.7**:
- `FStateTreeConstantConsideration`
- `FStateTreeFloatInputConsideration`
- `FStateTreeEnumInputConsideration`
- `FStateTreeConsiderationResponseCurve`
- Blueprint-derived considerations

#### 3.2 State Linking & Custom Tick Rate

| New Method | Purpose |
|-----------|---------|
| `SetLinkedSubtree(AssetPath, StatePath, LinkedStatePath)` | Link to subtree |
| `SetLinkedAsset(AssetPath, StatePath, LinkedAssetPath)` | Link to external StateTree asset |
| `SetCustomTickRate(AssetPath, StatePath, bEnabled, Rate)` | Set tick rate |
| `SetRequiredEventToEnter(AssetPath, StatePath, GameplayTag)` | Require event for state entry |

#### 3.3 Binding (Extended)

| New Method | Purpose |
|-----------|---------|
| `BindEnterConditionPropertyToRootParameter(...)` | Bind condition property to parameter |
| `BindEnterConditionPropertyToContext(...)` | Bind condition property to context |
| `BindEvaluatorPropertyToRootParameter(...)` | Bind evaluator property to parameter |
| `BindEvaluatorPropertyToContext(...)` | Bind evaluator property to context |

#### 3.4 State Parameters (Linked State Overrides)

States of type `Linked` or `LinkedAsset` have `FStateTreeStateParameters` that override the linked subtree's parameters.

| New Method | Purpose |
|-----------|---------|
| `GetStateParameters(AssetPath, StatePath) → TArray<FStateTreeParameterInfo>` | List state-level parameter overrides |
| `SetStateParameterOverride(AssetPath, StatePath, ParameterName, Value)` | Override a parameter value |

---

## Implementation Strategy

### Refactoring for Reuse

The existing task property helpers are tightly coupled to "state → task" navigation. Many of the new methods need the exact same property enumeration/setting logic but for different node targets:
- **State tasks** (existing)
- **State enter conditions** (new)
- **Transition conditions** (new)
- **Global evaluators** (new)
- **Global tasks** (new)
- **Considerations** (new)

**Recommended refactor**: Extract generic helpers:
```cpp
// Generic: enumerate properties on any FStateTreeEditorNode
static TArray<FStateTreePropertyInfo> GetEditorNodePropertyNames(const FStateTreeEditorNode& Node);

// Generic: set property on any FStateTreeEditorNode
static FStateTreeTaskPropertySetResult SetEditorNodePropertyValue(
    FStateTreeEditorNode& Node, const FString& PropertyPath, const FString& Value);

// Generic: find node by struct name in a TArray<FStateTreeEditorNode>
static FStateTreeEditorNode* FindEditorNodeByStruct(
    TArray<FStateTreeEditorNode>& Nodes, const FString& StructName, int32 MatchIndex = -1);
```

This eliminates code duplication across ~20+ new methods.

### Phased Rollout

**Phase 1** (Priority 1 — unblocks AI loops):
1. `SetSelectionBehavior` — direct cause of the logged loop
2. `UpdateTransition` + `RemoveTransition` + `MoveTransition` — direct cause of duplicate transitions + ordering matters (S2-L6)
3. `GetRootParameters` + `AddOrUpdateRootParameter` + `RemoveRootParameter`
4. `RenameState`
5. `SetTasksCompletion`, `SetStateTag`, `SetStateWeight`
6. Enhanced `FStateTreeTransitionInfo` and `FStateTreeStateInfo` fields
7. Skill docs update for all new methods

**Phase 2** (Priority 2 — common workflows):
1. Condition CRUD (enter conditions + transition conditions)
2. Refactor property helpers to generic EditorNode helpers
3. `RemoveTask`, `MoveTask`, `SetTaskEnabled`
4. Evaluator/global task property get/set + remove
5. `CreateCustomTaskBlueprint`, `SetTaskBlueprintVariableCategory` (custom task scaffolding)
6. Skill docs for conditions + custom tasks

**Phase 3** (Priority 3 — advanced):
1. Considerations (Utility AI)
2. Linking (subtree/asset linking, state parameters)
3. Custom tick rate, required events
4. Extended bindings for conditions/evaluators

---

## Enum Reference

### EStateTreeStateSelectionBehavior
| Value | Display Name |
|-------|-------------|
| `None` | None |
| `TryEnterState` | Try Enter |
| `TrySelectChildrenInOrder` | Try Select Children In Order |
| `TrySelectChildrenAtRandom` | Try Select Children At Random |
| `TrySelectChildrenWithHighestUtility` | Try Select Children With Highest Utility |
| `TrySelectChildrenAtRandomWeightedByUtility` | Try Select Children At Random Weighted By Utility |
| `TryFollowTransitions` | Try Follow Transitions |

### EStateTreeTransitionTrigger
| Value | Notes |
|-------|-------|
| `OnStateCompleted` | Succeeded or failed (0x1 \| 0x2) |
| `OnStateSucceeded` | Success only |
| `OnStateFailed` | Failure only |
| `OnTick` | Every tick |
| `OnEvent` | Specific event tag |
| `OnDelegate` | Specific delegate (new in 5.7) |

### EStateTreeTransitionType
| Value |
|-------|
| `None` |
| `Succeeded` |
| `Failed` |
| `GotoState` |
| `NextState` |
| `NextSelectableState` |

### EStateTreeTransitionPriority
| Value |
|-------|
| `Low` |
| `Normal` |
| `Medium` |
| `High` |
| `Critical` |

### EStateTreeTaskCompletionType
| Value |
|-------|
| `Any` |
| `All` |

### EStateTreeExpressionOperand (Condition operands)
| Value | Meaning |
|-------|---------|
| `Copy` | First condition (implicit) |
| `And` | AND with previous |
| `Or` | OR with previous |
| `Multiply` | Multiply (Considerations only) |

### EPropertyBagPropertyType (Parameter types)
| Value | C++ Type |
|-------|----------|
| `Bool` | bool |
| `Byte` | uint8 |
| `Int32` | int32 |
| `Int64` | int64 |
| `Float` | float |
| `Double` | double |
| `Name` | FName |
| `String` | FString |
| `Text` | FText |
| `Enum` | UEnum* |
| `Struct` | UScriptStruct* |
| `Object` | UObject* |
| `SoftObject` | TSoftObjectPtr |
| `Class` | TSubclassOf |
| `SoftClass` | TSoftClassPtr |

---

## UStateTreeState Full Property Map

All UPROPERTY fields on `UStateTreeState` (UE 5.7) and their coverage status:

| Property | Type | Current API | Needed |
|----------|------|-------------|--------|
| `Name` | FName | ❌ | `RenameState` |
| `Description` | FString | ✅ `SetStateDescription` | — |
| `Tag` | FGameplayTag | ❌ | `SetStateTag` |
| `ColorRef` | FStateTreeEditorColorRef | ✅ `SetStateThemeColor` | — |
| `Type` | EStateTreeStateType | ✅ via `AddState` | — (set at creation) |
| `SelectionBehavior` | EStateTreeStateSelectionBehavior | ❌ | `SetSelectionBehavior` |
| `TasksCompletion` | EStateTreeTaskCompletionType | ❌ | `SetTasksCompletion` |
| `LinkedSubtree` | FStateTreeStateLink | ❌ | `SetLinkedSubtree` |
| `LinkedAsset` | UStateTree* | ❌ | `SetLinkedAsset` |
| `CustomTickRate` | float | ❌ | `SetCustomTickRate` |
| `bHasCustomTickRate` | bool | ❌ | `SetCustomTickRate` |
| `Parameters` | FStateTreeStateParameters | ❌ | `Get/SetStateParameterOverride` |
| `bCheckPrerequisitesWhenActivatingChildDirectly` | bool | ❌ | Low priority |
| `RequiredEventToEnter` | FStateTreeEventDesc | ❌ | `SetRequiredEventToEnter` |
| `Weight` | float | ❌ | `SetStateWeight` |
| `EnterConditions` | TArray\<FStateTreeEditorNode\> | ❌ | Condition CRUD |
| `Tasks` | TArray\<FStateTreeEditorNode\> | ✅ `AddTask` only | `RemoveTask`, `MoveTask` |
| `Considerations` | TArray\<FStateTreeEditorNode\> | ❌ | Consideration CRUD |
| `SingleTask` | FStateTreeEditorNode | ❌ | Covered by task methods |
| `Transitions` | TArray\<FStateTreeTransition\> | ✅ `AddTransition` only | Edit/Remove |
| `bExpanded` | bool | ✅ `SetStateExpanded` | — |
| `bEnabled` | bool | ✅ `SetStateEnabled` | — |

---

## FStateTreeTransition Full Property Map

| Property | Type | Current API | Needed |
|----------|------|-------------|--------|
| `Trigger` | EStateTreeTransitionTrigger | ✅ via `AddTransition` | `UpdateTransition` |
| `RequiredEvent` | FStateTreeEventDesc | ❌ | `UpdateTransition` |
| `State` (target) | FStateTreeStateLink | ✅ via `AddTransition` | `UpdateTransition` |
| `Priority` | EStateTreeTransitionPriority | ✅ via `AddTransition` | `UpdateTransition` |
| `bDelayTransition` | bool | ❌ | `UpdateTransition` |
| `DelayDuration` | float | ❌ | `UpdateTransition` |
| `DelayRandomVariance` | float | ❌ | `UpdateTransition` |
| `Conditions` | TArray\<FStateTreeEditorNode\> | ❌ | Condition CRUD |
| `bTransitionEnabled` | bool | ❌ | `UpdateTransition` |

---

## Summary: Total New Methods

| Priority | Count | Methods |
|----------|-------|---------|
| P1 Critical | ~13 | `SetSelectionBehavior`, `SetTasksCompletion`, `RenameState`, `SetStateTag`, `SetStateWeight`, `SetTaskConsideredForCompletion`, `UpdateTransition`, `RemoveTransition`, `MoveTransition`, `GetRootParameters`, `AddOrUpdateRootParameter`, `RemoveRootParameter`, `RenameRootParameter` |
| P2 Important | ~17 | Condition CRUD (enter + transition), `RemoveTask`, `MoveTask`, `SetTaskEnabled`, evaluator/global task property get/set/remove, `CreateCustomTaskBlueprint`, `SetTaskBlueprintVariableCategory` |
| P3 Advanced | ~10 | Considerations, linking, tick rate, events, extended bindings, state parameter overrides |
| **Total** | **~40** | |

Plus struct field additions to `FStateTreeStateInfo`, `FStateTreeTransitionInfo`, and `FStateTreeInfo`.
