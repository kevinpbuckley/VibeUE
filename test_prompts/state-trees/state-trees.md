# StateTree Skill — Test Prompts

A collection of prompts for testing the StateTree service layer. Each prompt exercises different parts of the API and validates end-to-end correctness.

---

## Basic Creation & Structure

### 1. Create a simple idle/walk/run StateTree

```
Create a StateTree at /Game/AI/BasicMovement with three states under Root:
Idle, Walking, and Running. Add a FStateTreeDelayTask to Idle.
Add transitions: Idle → Walking on OnStateCompleted, Walking → Running on OnStateCompleted,
Running → Idle on OnStateCompleted. Compile and save.
```

**Expected**: 1 StateTree asset, Root subtree, 3 child states, tasks & transitions wired up, compiled successfully. Default schema `StateTreeComponentSchema` used.

---

### 1b. Create a StateTree in a subfolder with explicit Component schema

```
Create a StateTree at /Game/StateTree/MyComponentTree with schema StateTreeComponentSchema.
Add a Root subtree and a single state called Idle under Root.
Compile and save.
```

**Expected**: `create_state_tree` called with `schemaClass: "StateTreeComponentSchema"`. Asset created at `/Game/StateTree/MyComponentTree`. `get_state_tree_info` shows `schema_class: "StateTreeComponentSchema"`.

---

### 1c. Create a StateTree with AI Component schema

```
Create a StateTree at /Game/StateTree/MyAITree using the AIComponent schema.
Add a Root subtree with two states: Patrol and Combat.
Compile and save. Then inspect it and confirm the schema is StateTreeAIComponentSchema.
```

**Expected**: `create_state_tree` called with `schemaClass: "AIComponent"` (shorthand). `get_state_tree_info` returns `schema_class: "StateTreeAIComponentSchema"`.

---

### 1d. Create a StateTree with shorthand schema name

```
Create a StateTree at /Game/StateTree/ShorthandTest using schema "Component".
Add a Root subtree. Compile and save.
```

**Expected**: Shorthand `"Component"` is expanded to `"StateTreeComponentSchema"` by the API. Asset created successfully.

---

### 2. List existing StateTrees

```
List all StateTree assets in /Game and show their names and compile status.
```

**Expected**: Uses `list_state_trees` + `get_state_tree_info` for each, prints structured results.

---

### 3. Inspect an existing StateTree

```
Show me the full structure of the StateTree at /Game/AI/BasicMovement —
list every state, its tasks, its transitions, and whether it's compiled.
```

**Expected**: Uses `get_state_tree_info`, prints each FStateTreeStateInfo with tasks and transitions.

---

## Intermediate: Nested Hierarchy

### 4. Hierarchical patrol AI

```
Create a StateTree at /Game/AI/PatrolAI.
Structure:
  Root
    Patrol (Group)
      Search
      MoveToPoint
    Combat (Group)
      Engage
      Retreat
Add transitions: Search → MoveToPoint on OnStateCompleted,
MoveToPoint → Search on OnStateSucceeded.
Compile and save.
```

**Expected**: Group states with nested children; transitions within Patrol group.

---

### 5. Add states to existing tree

```
The StateTree at /Game/AI/BasicMovement is missing a Sprinting state.
Add it as a child of Root, then add a transition from Running to Sprinting
on OnStateCompleted. Recompile and save.
```

**Expected**: Correct path navigation, new state added, transition added, recompiled.

---

## Evaluators & Global Tasks

### 6. Add an evaluator by discovered type

```
List all available evaluator types for StateTrees, then add the first one you find
to the StateTree at /Game/AI/BasicMovement. Compile and save.
```

**Expected**: Calls `get_available_evaluator_types()`, picks one, calls `add_evaluator`, compiles.

---

### 7. Add a global task

```
Add a global task of type FStateTreeDelayTask to /Game/AI/BasicMovement
so that it runs as long as the tree is active. Compile and save.
```

**Expected**: `add_global_task` called, shows up in `get_state_tree_info` globalTasks.

---

## Transitions

### 8. All transition types

```
In /Game/AI/BasicMovement, add the following transitions to the Idle state:
- OnStateSucceeded → Succeeded
- OnStateFailed → Failed
- OnStateCompleted → GotoState → Walking
Then compile and save.
```

**Expected**: Three transitions on Idle, each with correct type/trigger.

---

### 9. Priority transitions

```
Add two transitions to the Running state in /Game/AI/BasicMovement:
1. OnStateFailed → GotoState → Idle with Critical priority
2. OnStateCompleted → NextState with Normal priority
Compile and save.
```

**Expected**: Two transitions with different priorities on Running.

---

## Enable/Disable States

### 10. Disable a state

```
Disable the Running state in /Game/AI/BasicMovement.
Then verify with get_state_tree_info that it shows enabled = false (not b_enabled).
```

**Expected**: `set_state_enabled` called with false, verified via info query.

---

## Error Handling & Edge Cases

### 11. Compile failure detection

```
Create a StateTree at /Game/AI/BrokenTree with a Root state and a child
named Orphan that has a GotoState transition pointing to a non-existent state
called Ghost. Try to compile and report the errors.
```

**Expected**: `compile_state_tree` returns `b_success = false`, errors array populated.

---

### 12. List available task types

```
List all StateTree task types available in the current project,
filter for any containing "Delay" in the name, and show their full struct names.
```

**Expected**: `get_available_task_types()` called, results filtered in Python.

---

## Full End-to-End

### 13. Complete AI behavior tree

```
Build a complete AI behavior StateTree at /Game/AI/EnemyBehavior:

Root
  Idle
    - Task: FStateTreeDelayTask
    - Transition: OnStateCompleted → GotoState → Patrol
  Patrol (Group)
    MoveToWaypoint
      - Transition: OnStateSucceeded → GotoState → MoveToWaypoint (loop)
      - Transition: OnStateFailed → GotoState → Idle
  Combat
    Engage
      - Transition: OnStateSucceeded → GotoState → Idle
      - Transition: OnStateFailed → Failed

Compile and save. Then verify the structure with get_state_tree_info.
```

**Expected**: Full hierarchy created, all transitions wired, compiled successfully, structure verified.

---

### 14. Refactor an existing tree

```
Load the StateTree at /Game/AI/BasicMovement.
Remove the Running state entirely.
Add a new state called Dashing under Root.
Add a transition from Walking → Dashing on OnStateSucceeded.
Add a transition from Dashing → Idle on OnStateCompleted.
Compile and save.
```

**Expected**: `remove_state`, `add_state`, two `add_transition` calls, compile, save.

---

## Transition Triggers & Types

### 15. OnTick and OnEvent triggers

```
In /Game/AI/BasicMovement, add the following transitions to the Walking state:
- OnTick → GotoState → Running
- OnEvent → GotoState → Idle
Then compile and save.
```

**Expected**: Two transitions using `OnTick` and `OnEvent` triggers on Walking.

---

### 16. NextSelectableState transition type

```
In /Game/AI/BasicMovement, add a transition to the Idle state:
- OnStateCompleted → NextSelectableState
Compile and save, then verify with get_state_tree_info.
```

**Expected**: `add_transition` called with type `NextSelectableState` (no target_path needed).

---

### 17. All transition priorities

```
In /Game/AI/BasicMovement, add four transitions to the Idle state with different priorities:
1. OnStateFailed → GotoState → Walking with Low priority
2. OnStateSucceeded → GotoState → Running with Medium priority
3. OnStateCompleted → NextState with High priority
4. OnTick → GotoState → Idle with Critical priority
Compile and save.
```

**Expected**: Four transitions with Low, Medium, High, Critical priorities wired correctly.

---

## State Types

### 18. Linked and Subtree state types

```
Create a StateTree at /Game/AI/LinkedTreeTest.
Add a Root subtree.
Add a state named SubBehavior under Root with type "Subtree".
Add a state named LinkedState under Root with type "Linked".
Compile and save, then verify with get_state_tree_info that both show their correct state_type.
```

**Expected**: `add_state` called with explicit `"Subtree"` and `"Linked"` type arguments; `get_state_tree_info` confirms state types.

---

## Tasks

### 19. FStateTreeRunSubtreeTask

```
In /Game/AI/BasicMovement, add a FStateTreeRunSubtreeTask to the Root state.
Then compile and save.
```

**Expected**: `add_task` called with `"FStateTreeRunSubtreeTask"` on the Root state.

---

## Compile Warnings

### 20. Inspect compile warnings

```
Create a StateTree at /Game/AI/WarnTree. Add a Root subtree and a single state Idle under Root.
Add no transitions. Compile and check both result.errors and result.warnings — print each.
Save only if result.success is True.
```

**Expected**: `compile_state_tree` result checked for both `errors` and `warnings` fields; demonstrates correct Python property names (`result.success`, not `result.b_success`).

---

## Python API Direct Tests

### 21. Full Python workflow

```
Write a Python script using unreal.StateTreeService that:
1. Creates a StateTree at /Game/Test/PythonTree
2. Adds Root subtree
3. Adds states: StateA, StateB, StateC under Root
4. Adds FStateTreeDelayTask to StateA
5. Adds transition from StateA → StateB on OnStateCompleted
6. Adds transition from StateB → StateC on OnStateSucceeded
7. Adds transition from StateC → StateA on OnStateCompleted (loop)
8. Compiles and checks for errors and warnings
9. Saves if successful
10. Calls get_state_tree_info and prints state count
```

**Expected**: Complete working Python script, no hardcoded assumptions, uses correct snake_case property names (`result.success`, `state.enabled`, `info.is_compiled`).

---

## Notes for Testers

- All asset paths use the `/Game/...` format (content browser paths, NOT filesystem paths)
- State paths use `/`-separated names matching the `Name` property on each `UStateTreeState`
- UE Python strips the `b` prefix from bool UPROPERTY names: `bSuccess` → `result.success`, `bEnabled` → `state.enabled`, `bIsCompiled` → `info.is_compiled`
- **Always compile** before saving — uncompiled trees are invalid at runtime
- Task/evaluator struct names include the `F` prefix (e.g. `FStateTreeDelayTask`)
- Available state types: `"State"` (default), `"Group"`, `"Subtree"`, `"Linked"`, `"LinkedAsset"`
- Available transition triggers: `OnStateCompleted`, `OnStateSucceeded`, `OnStateFailed`, `OnTick`, `OnEvent`
- Available transition types: `GotoState`, `Succeeded`, `Failed`, `NextState`, `NextSelectableState`
- Available priorities: `Low`, `Normal` (default), `Medium`, `High`, `Critical`
