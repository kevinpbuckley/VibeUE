# StateTree Tests — Basics

Create, inspect, list, schema selection.

---

### 1. Create a simple three-state tree

```
Create a StateTree at /Game/AI/BasicMovement with three states under Root:
Idle, Walking, and Running. Add a delay task to Idle.
Add transitions: Idle → Walking on completion, Walking → Running on completion,
Running → Idle on completion. Compile and save.
```

---

### 2. Create with explicit schema

```
Create a StateTree at /Game/StateTree/MyComponentTree.
Add a Root subtree and a single Idle state. Compile and save.
```

---

### 3. Create for an AI character

```
Create a StateTree at /Game/StateTree/MyAITree for an AI-controlled character.
Add a Root subtree with Patrol and Combat states. Compile and save.
Then confirm what schema it's using.
```

---

### 4. Inspect an existing tree

```
Show me the full structure of /Game/AI/BasicMovement —
every state, its tasks, transitions, and whether it's compiled.
```

---

### 5. List all StateTrees

```
List all StateTree assets under /Game and show their compile status.
```

---

### 6. Compile failure detection

```
Create a StateTree at /Game/AI/BrokenTree with a Root state and a child named Orphan
that has a transition to a non-existent state called Ghost.
Try to compile and report the errors.
```

---

# StateTree Tests — State Properties

Hierarchy management, rename, selection behavior, task completion, enable/disable, description, weight.

---

### 1. Nested group hierarchy

```
Create a StateTree at /Game/AI/PatrolAI.
Structure:
  Root
    Patrol (group containing Search and MoveToPoint)
    Combat (group containing Engage and Retreat)
Add transitions: Search → MoveToPoint on success, MoveToPoint → Search on completion.
Compile and save.
```

---

### 2. Add a state to an existing tree

```
The StateTree at /Game/AI/BasicMovement is missing a Sprinting state.
Add it under Root, then add a transition from Running to Sprinting on completion.
Recompile and save.
```

---

### 3. Remove a state

```
Remove the Running state from /Game/AI/BasicMovement. Recompile and save.
```

---

### 4. Rename a state

```
Rename the Walking state in /Game/AI/BasicMovement to Jogging. Compile and save.
```

---

### 5. Set selection behavior

```
In /Game/AI/BasicMovement, make Root randomly pick among its children
instead of always choosing in order. Compile and save.
```

---

### 6. Set task completion mode

```
In /Game/AI/BasicMovement, change the Idle state so it only completes
when ALL of its tasks finish, not just any one of them. Compile and save.
```

---

### 7. Enable and disable states

```
Disable the Running state in /Game/AI/BasicMovement.
Verify it shows as disabled, then re-enable it.
```

---

### 8. State description

```
Add the description "Character is moving at a brisk pace" to the Walking state
in /Game/AI/BasicMovement.
```

---

### 9. State weight for utility selection

```
In /Game/AI/PatrolAI, set the Combat state weight to 3.0 and Patrol to 1.0
so Combat is three times more likely to be selected by utility. Compile and save.
```

---

### 10. Context actor class

```
Set the context actor class for /Game/AI/BasicMovement to BP_Cube.
Recompile and save.
```

---

# StateTree Tests — Parameters

Root parameter bag: inspect, add, update, remove, rename, bind.

---

### 1. Inspect existing parameters

```
What parameters does /Game/AI/BasicMovement have? Show each one's name, type, and default value.
```

---

### 2. Add parameters of different types

```
Add the following parameters to /Game/AI/BasicMovement:
- patrol_speed (float, default 300.0)
- is_aggressive (bool, default false)
- max_alerts (int, default 3)
- status_message (string, default "Idle")
Compile and save.
```

---

### 3. Update a parameter's default value

```
Change the default value of patrol_speed in /Game/AI/BasicMovement to 450.0.
Compile and save.
```

---

### 4. Remove a parameter

```
Remove the max_alerts parameter from /Game/AI/BasicMovement. Compile and save.
```

---

### 5. Rename a parameter

```
Rename the patrol_speed parameter to movement_speed in /Game/AI/BasicMovement.
Compile and save.
```

---

### 6. Bind a task property to a parameter

```
In /Game/AI/BasicMovement, make the delay task in Idle use the idling_time parameter
as its duration instead of a fixed value.
First add a float parameter idling_time with default 1.5 if it doesn't exist.
Compile and save.
```

---

### 7. Bind a task property to context

```
In /Game/AI/BasicMovement, bind the debug text task's ReferenceActor property
to the Actor context so it follows the owning actor. Compile and save.
```

---

# StateTree Tests — Transitions

Add, edit, disable, delay, remove, reorder transitions.

---

### 1. All trigger types

```
In /Game/AI/BasicMovement, add the following transitions to the Idle state:
- fires when the state succeeds → go to Walking
- fires every tick → go to Running
- fires on an event → go to Idle
Compile and save.
```

---

### 2. All transition types

```
In /Game/AI/BasicMovement, add transitions to Walking:
- on completion → go to the next sibling state
- on failure → mark as Failed
- on success → mark as Succeeded
Compile and save.
```

---

### 3. Priority transitions

```
Add two transitions to the Running state in /Game/AI/BasicMovement:
- on failure → go to Idle with Critical priority
- on completion → next state with Normal priority
Compile and save.
```

---

### 4. Change a transition's trigger

```
The first transition on the Idle state in /Game/AI/BasicMovement fires on completion.
Change it to fire only on success instead. Compile and save.
```

---

### 5. Change a transition's target

```
In /Game/AI/BasicMovement, the transition from Walking that goes to Running
should now go to Idle instead. Fix it. Compile and save.
```

---

### 6. Disable a transition

```
Temporarily disable the second transition on the Running state
in /Game/AI/BasicMovement without removing it. Compile and save.
```

---

### 7. Add a delay to a transition

```
Make the transition from Idle to Walking in /Game/AI/BasicMovement
wait 2 seconds before firing, with a random variance of 0.5 seconds. Compile and save.
```

---

### 8. Remove a transition

```
Remove the last transition on the Idle state in /Game/AI/BasicMovement. Compile and save.
```

---

### 9. Reorder transitions

```
In /Game/AI/BasicMovement, move the third transition on Running to be first.
Compile and save.
```

---

# StateTree Tests — Tasks

Add, configure, remove, reorder, enable/disable tasks. Task property get/set. Considered-for-completion toggle.

---

### 1. Add a task and discover its properties

```
Add a delay task to the Idle state in /Game/AI/BasicMovement.
Show all editable properties on it. Compile and save.
```

---

### 2. Set a task property

```
In /Game/AI/BasicMovement, set the delay task on Idle to wait 3 seconds. Compile and save.
```

---

### 3. Remove a task

```
Remove the delay task from the Idle state in /Game/AI/BasicMovement. Compile and save.
```

---

### 4. Reorder tasks

```
In /Game/AI/BasicMovement, the Walking state has two tasks.
Move the second one to run first. Compile and save.
```

---

### 5. Enable / disable a task

```
Disable the delay task on Idle in /Game/AI/BasicMovement without removing it.
Verify it shows as disabled. Re-enable it. Compile and save.
```

---

### 6. Task completion toggle

```
In /Game/AI/BasicMovement, make the delay task on Idle run in the background —
it shouldn't count toward the state completing. Compile and save.
```

---

### 7. Add a global task and configure it

```
Add a delay task as a global task to /Game/AI/BasicMovement.
Set its duration to 0.1 seconds.
Compile and save.
```

---

### 8. Remove a global task

```
Remove the global delay task from /Game/AI/BasicMovement. Compile and save.
```

---

# StateTree Tests — Conditions

Enter conditions on states and conditions on transitions: add, configure, set operand, remove.

---

### 1. List available condition types

```
What kinds of conditions can I add to states in /Game/AI/BasicMovement?
```

---

### 2. Add an enter condition to a state

```
Add a random condition to the Idle state in /Game/AI/BasicMovement
so it doesn't always activate. Compile and save.
```

---

### 3. Add multiple conditions with And/Or

```
In /Game/AI/BasicMovement, add two enter conditions to Walking:
a float comparison and a random check.
The second one should be ANDed with the first. Compile and save.
```

---

### 4. Set enter condition properties

```
Show the properties on the first enter condition on Idle in /Game/AI/BasicMovement,
then set the first numeric property to 0.75. Compile and save.
```

---

### 5. Remove an enter condition

```
Remove the second enter condition from Walking in /Game/AI/BasicMovement. Compile and save.
```

---

### 6. Add a condition to a transition

```
Add a condition to the first transition on Idle in /Game/AI/BasicMovement
so it only fires when a random roll passes. Compile and save.
```

---

### 7. Set transition condition properties

```
Show the properties on the condition attached to the first transition on Idle
in /Game/AI/BasicMovement. Set the threshold to 0.5. Compile and save.
```

---

### 8. Remove a transition condition

```
Remove the first condition from the first transition on Idle in /Game/AI/BasicMovement. Compile and save.
```

---

# StateTree Tests — Evaluators

Global evaluators: add, inspect properties, set properties, remove.

---

### 1. List and add an evaluator

```
List the available evaluator types for /Game/AI/BasicMovement,
then add the first one you find. Compile and save.
```

---

### 2. Inspect evaluator properties

```
Show me all editable properties on the first evaluator in /Game/AI/BasicMovement.
```

---

### 3. Set an evaluator property

```
Set the first property on the first evaluator in /Game/AI/BasicMovement to 5.0.
Compile and save.
```

---

### 4. Remove an evaluator

```
Remove the first evaluator from /Game/AI/BasicMovement. Compile and save.
```

---

# StateTree Tests — Editor Presentation

Theme colors, expand/collapse.

---

### 1. List theme colors

```
What theme colors are defined on /Game/StateTree/ST_Cube?
Show each color's name and which states use it.
```

---

### 2. Rename a theme color

```
Rename the "Default Color" theme color to "Active" on /Game/StateTree/ST_Cube.
```

---

### 3. Set a state's theme color

```
Give the Root state in /Game/StateTree/ST_Cube a blue color called "Idle".
```

---

### 4. Collapse a state

```
Collapse the Root state in /Game/StateTree/ST_Cube.
```

---

### 5. Expand a state

```
Expand the Root/Walking state in /Game/StateTree/ST_Cube.
```

---

# StateTree Tests — End-to-End

Complex multi-step scenarios that exercise multiple API areas together.

---

### 1. Full AI behavior tree from scratch

```
Build a complete enemy AI StateTree at /Game/AI/EnemyBehavior:

Root
  Idle — delay task, transitions to Patrol on completion
  Patrol (group)
    MoveToWaypoint — loops on success, falls back to Idle on failure
  Combat
    Engage — succeeds to Idle, fails with Failed transition

Add a float parameter patrol_speed with default 300.
Make Root randomly pick among its children.
Compile and save.
```

---

### 2. Refactor a tree

```
In /Game/AI/BasicMovement:
1. Remove all transitions from the Idle state
2. Remove the delay task from Idle
3. Rename Idle to Resting
4. Add a float parameter rest_duration with default 2.0
5. Add a delay task to Resting with its duration bound to rest_duration
6. Add a transition from Resting to Walking on completion
Compile and save.
```

---

### 3. Parameter-driven multi-state tree

```
In /Game/AI/BasicMovement:
- Add parameters: idle_time (float 1.0), walk_time (float 2.0), run_time (float 0.5)
- Ensure Idle, Walking, Running each have a delay task
- Bind each delay duration to its matching parameter
- Wire transitions so they cycle: Idle → Walking → Running → Idle on completion
Compile and save.
```

---

### 4. Conditions + delayed fallback

```
In /Game/AI/BasicMovement:
- Add a random enter condition to Idle
- Add a condition to Walking's first transition
- Add a delayed transition (1.5s delay, 0.3s variance) from Running back to Idle as a safety net
Compile and save.
```

---

### 5. Full Python script

```
Write a Python script using unreal.StateTreeService that:
1. Creates a StateTree at /Game/Test/PythonTree
2. Adds Root, StateA, StateB, StateC
3. Adds a delay task to StateA
4. Wires transitions: A→B on completion, B→C on success, C→A on completion (loop)
5. Adds a float parameter loop_delay defaulting to 1.0
6. Binds StateA's delay duration to loop_delay
7. Compiles, prints any errors/warnings, saves if successful
8. Calls get_state_tree_info and prints the state count
```

---

# StateTree Tests — Considerations (Utility AI)

Considerations are the utility-AI scoring inputs StateTree uses to rank candidate
states when a parent's selection behavior is utility-based. Each consideration
contributes a scalar [0,1] weight that the engine multiplies into the state's
final utility score.

---

### 1. List available consideration types

```
Show me every consideration struct I can add to a state in /Game/AI/BasicMovement —
whatever's registered with the engine.
```

---

### 2. Add a constant consideration

```
In /Game/AI/BasicMovement, add a Constant consideration to the Walking state
with a value of 0.5. Compile and save.
```

---

### 3. Add multiple considerations on one state

```
In /Game/AI/PatrolAI, on the Combat state, add:
- a Constant consideration weighted at 0.7
- a Float Curve consideration on a parameter named "EnemyDistance"
Compile and save.
```

---

### 4. Inspect consideration properties

```
What properties can I set on the second consideration of the Combat state
in /Game/AI/PatrolAI? Return their names and types.
```

---

### 5. Set a consideration property

```
On the Constant consideration of the Walking state in /Game/AI/BasicMovement,
change its value from 0.5 to 0.85. Compile and save.
```

---

### 6. Bind a consideration property to a root parameter

```
In /Game/AI/PatrolAI, the Combat state has a Float Curve consideration.
Bind its input value to the root parameter "EnemyDistance" so the curve
reads from that parameter at runtime. Compile and save.
```

---

### 7. Bind a consideration property to context

```
In /Game/AI/PatrolAI, on the Combat state's first consideration, bind its
"Owner" input to the context actor. Compile and save.
```

---

### 8. Unbind a consideration property

```
In /Game/AI/PatrolAI, the Combat state's first consideration has a binding on
its "Owner" input. Remove that binding so it falls back to the literal default.
Compile and save.
```

---

### 9. Remove a consideration

```
In /Game/AI/PatrolAI, remove the second consideration from the Combat state.
Compile and save.
```

---

### 10. Utility-driven selection end-to-end

```
Build /Game/AI/UtilityDemo with three sibling states under Root: Idle, Patrol, Combat.
Set Root's selection behavior to utility-based. On each child:
- Idle: Constant consideration value 0.3
- Patrol: Constant consideration value 0.6
- Combat: Constant consideration value 0.9
Verify by inspection that Combat now wins selection. Compile and save.
```


---

# StateTree Tests — Linked Subtrees & Linked Assets

States can be promoted to special types that delegate execution to other parts
of the same tree (`LinkedSubtree`) or to a separate StateTree asset
(`LinkedAsset`). Useful for sharing behavior fragments without duplicating them.

---

### 1. Convert a state to a Group

```
In /Game/AI/BasicMovement, change the Idle state's type from a normal State
to a Group so it can hold child states without running its own tasks.
Compile and save.
```

---

### 2. Convert a state to a LinkedSubtree

```
In /Game/AI/PatrolAI, change the type of the Combat state to LinkedSubtree.
Then link it to the Patrol subtree (also under Root) so Combat re-uses Patrol's
states at runtime. Compile and save.
```

---

### 3. Convert a state to a LinkedAsset

```
Create /Game/AI/SharedCombat as a separate StateTree with a Root, Engage, and
Retreat. Then in /Game/AI/PatrolAI, change the Combat state to a LinkedAsset
referencing /Game/AI/SharedCombat. Compile both and save.
```

---

### 4. Re-point a LinkedSubtree

```
In /Game/AI/PatrolAI, the Combat state is currently linked to the Patrol subtree.
Re-point it to a different subtree under Root named Idle. Compile and save.
```

---

### 5. Re-point a LinkedAsset

```
In /Game/AI/PatrolAI, the Combat state is a LinkedAsset pointing at
/Game/AI/SharedCombat. Re-point it to /Game/AI/AlternateCombat instead.
Compile and save.
```

---

### 6. Verify linking is non-destructive

```
The Combat state in /Game/AI/PatrolAI has tasks, considerations, and
transitions configured. Convert it to a LinkedSubtree and confirm that none
of its existing data was destroyed (it should be preserved on the state object).
Then convert it back to a regular State.
```

---

### 7. Set a state tag on a linked state

```
In /Game/AI/PatrolAI, the Combat LinkedSubtree state should be tagged with
the gameplay tag "AI.Behavior.Combat". Set the tag and verify it was applied.
Compile and save.
```


---

# StateTree Tests — Component Parameter Overrides

When a placed actor has a `StateTreeComponent` that points at a tree, each
instance of that actor can override the tree's exposed root parameters
without forking the asset. These tests cover reading the link and the
per-instance override path.

---

### 1. Read which StateTree an actor uses

```
The level has an actor labeled "BP_Cube_1". What StateTree asset is its
StateTreeComponent referencing? Return the full path.
```

---

### 2. List current overrides on an actor

```
For the actor "BP_Cube_1" in the level, list every parameter override
its StateTreeComponent has set, with values.
```

---

### 3. Set a numeric parameter override

```
On the actor labeled "BP_Cube_1", override the StateTreeComponent's
"IdlingTime" parameter to 4.5 — leaving every other instance using the
asset's default. Verify the override took.
```

---

### 4. Set a vector parameter override

```
On "BP_Cube_2", override the StateTreeComponent parameter "PatrolHome"
to (X=500, Y=200, Z=0). Verify by reading it back.
```

---

### 5. Override propagates only to the target actor

```
Confirm that setting "IdlingTime" to 4.5 on BP_Cube_1 has no effect on
BP_Cube_2's override or on the underlying asset's default. List both
overrides and the asset default to demonstrate isolation.
```

---

### 6. Clear an override (revert to asset default)

```
On BP_Cube_1, remove the per-instance override on "IdlingTime" so it falls
back to whatever the underlying StateTree asset defines. Verify the
override is gone and the runtime value matches the asset default.
```

---

### 7. Bulk-apply an override to many actors

```
Every actor in the level with a StateTreeComponent that references
/Game/AI/PatrolAI should have its "PatrolSpeed" parameter set to 250.0.
Apply this and report how many actors were updated.
```


---

# StateTree Tests — Delegate Transitions, Event Payloads, Cross-Bindings

Coverage for the "advanced wiring" surface of StateTreeService:

- Transitions triggered by delegates broadcast from tasks
- Transition conditions reading event payload fields
- Cross-bindings between tasks, global tasks, and conditions
- Unbind variants (mirror of every Bind* method)

---

### 1. Bind a transition to a task delegate

```
In /Game/AI/PatrolAI, the Patrol state has a task that exposes a
"OnPatrolComplete" delegate. Add a transition from Patrol to Idle that
fires when this delegate broadcasts. Compile and save.
```

---

### 2. Inspect event payload property names

```
The Patrol state in /Game/AI/PatrolAI has a transition that listens for an
event named "PatrolFinished". What property names are available on the
event's payload struct? Return them so I can bind a condition to one.
```

---

### 3. Bind a transition condition to an event payload field

```
On the Patrol → Idle transition in /Game/AI/PatrolAI, add a condition that
fires only when the "PatrolFinished" event's payload `bSuccess` field is true.
Compile and save.
```

---

### 4. Bind a task property to a global task property

```
In /Game/AI/PatrolAI, there's a global task "WorldStateTracker" with a
"CurrentTarget" property. The Combat state's task has an "Enemy" input.
Bind the Combat task's "Enemy" to the global task's "CurrentTarget" so it
reads the world state every tick. Compile and save.
```

---

### 5. Bind an enter-condition to a global task property

```
In /Game/AI/PatrolAI, the Combat state's enter condition checks whether
combat should start. Bind the condition's "Threshold" property to the
global task "WorldStateTracker"'s "AggroRadius" output. Compile and save.
```

---

### 6. Bind an evaluator property to context

```
In /Game/AI/PatrolAI, the "EnemyDetector" evaluator has a "Pawn" input.
Bind it to the context actor so it operates on whichever pawn owns the
StateTree at runtime. Compile and save.
```

---

### 7. Bind a global-task property to a root parameter

```
In /Game/AI/PatrolAI, the global task "WorldStateTracker" has an
"AggroRadius" input. Bind it to the root parameter of the same name so
designers can tune the radius from the placed actor. Compile and save.
```

---

### 8. Unbind every Bind* surface

For each binding made in tests 1–7, verify that an Unbind call removes
exactly that binding without disturbing the others.

```
List every binding currently configured on /Game/AI/PatrolAI. Then unbind
the Combat task's "Enemy" → global "CurrentTarget" link, and re-list to
confirm the others survive.
```

---

### 9. Detailed task property setter (typed write)

```
In /Game/AI/PatrolAI, set the Patrol task's "WaitTime" property to a strongly
typed Float value of 3.5 (not a stringified value). Use the detailed setter
that takes a value-type tag plus a value. Compile and save.
```

---

### 10. Read a task's current property value

```
What is the Patrol task's "WaitTime" set to right now in /Game/AI/PatrolAI?
Return the literal value plus, if it's a binding, the source path.
```

