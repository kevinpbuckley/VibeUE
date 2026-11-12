---
name: animation-blueprint
display_name: Animation Blueprints
description: Navigate, inspect, and author Animation Blueprints — state machines, states, transitions, and transition rules (AnimGraphService). Use when the user asks to create or edit an Animation Blueprint/AnimBP, add a state machine or states, wire transitions between states, set a state's animation, or inspect AnimGraph structure.
vibeue_classes:
  - AnimGraphService
  - AssetDiscoveryService
  - BlueprintService
unreal_classes:
  - EditorAssetLibrary
keywords:
  - animation
  - animblueprint
  - abp
  - state machine
  - state
  - transition
  - transition rule
  - animgraph
  - locomotion
  - combat
  - authoring
---

> 🧠 **Brains complement:** IF an `unreal-engine-skills-manager` tool (external MCP) exists in this session, call it with `{action: "load", skill: "animation-system"}` for UE domain knowledge on this topic — correct APIs, architecture, best practices — and treat it as the rubric for any review / "best practices" question. If no such tool is available (e.g. running under Claude Code or Codex without that MCP), skip this line entirely and proceed with this skill alone — do NOT attempt the call.

# Animation Blueprint Skill

> **Loading skills:** skills load through the engine's `AgentSkillToolset` (`ListSkills`/`GetSkills`) —
> there is no `vibeue-skills-manager` tool. Run VibeUE services with `execute_python`
> (`unreal.AnimGraphService.<method>()`); reach engine toolsets with `call_tool`.

> An AnimBP **is a Blueprint**. The state-machine/AnimGraph authoring here is VibeUE
> `AnimGraphService` (40 AI tools). Its `get_parent_class` is gone -- use
> `BlueprintToolsets.BlueprintToolset.GetParent`. `add_blend_by_bool`, `add_blend_by_int`,
> `add_layered_blend`, `add_use_cached_pose`, `add_save_cached_pose`, `add_two_bone_ik_node` and `get_output_pose_node_id`
> are still callable from Python (they return the node GUIDs `connect_anim_nodes` takes) but are no
> longer AI tools: the engine's `BlueprintToolset.CreateNode` (palette type ids such as
> `Animation|Blends|BlendPosesbybool`, `Animation|Blends|BlendPosesbyint`,
> `Animation|Blends|Layeredblendperbone`, `Animation|SkeletalControls|TwoBoneIK`,
> `Animation|CachedPoses|Usecachedpose'<CacheName>'`, `Animation|CachedPoses|NewSavecachedpose...` then `await call_tool("EditorToolset.ObjectToolset.SetProperties", instance=<node ref from CreateNode>, values='{"CacheName": "..."}')` -- `values` is a JSON STRING, and `CreateNode` returns the node as an object reference you pass straight in as `instance`) and `FindNodes` (nodeClass
> `AnimGraphNode_Root`) cover them. Everything else about it as a Blueprint is the engine's:
> `BlueprintToolsets.BlueprintToolset` owns variables, components, interfaces, events, function
> graphs, node CRUD and compiling (`AddVariable`, `AddObjectVariable`, `AddStructVariable`,
> `ListVariables`, `GetVariableTooltip`/`SetVariableTooltip` and the other flag accessors,
> `AddFunctionGraph`, `AddEvent`, `CompileBlueprint`, ...), and
> `BlueprintToolsets.BlueprintDslToolset` reads and writes whole graphs as DSL. Both are C++
> toolsets, so their tool names are **PascalCase** and they are reached with `call_tool`, never as
> `unreal.BlueprintToolset.*`.
>
> VibeUE's `BlueprintService` is down to 19 tools: all the Timeline ones, plus `CreateMacroGraph`,
> `AnalyzeGraphLayout` and `RefreshBlueprintEditor`. It no longer has `list_variables`,
> `get_variable_info`, `set_variable_default_value`, `variable_exists`, components or overridable
> functions -- those are the engine's now.
>
> Bool fields on info structs drop the C++ `b` prefix in Python -- use `is_pure`, `is_output`,
> `already_overridden` (NOT `b_is_pure`). `PythonizePropertyName` strips a leading `b` before an
> uppercase letter, so a `bool bIsPure` UPROPERTY is read as `.is_pure`. Call `get_symbol` to
> confirm field names rather than guessing.

## ⚠️ #1 GOTCHA — Transitions need a RULE or they NEVER fire

`add_transition()` only draws the arrow between two states. The transition's rule graph
starts empty, which evaluates to **false**, so the transition is **inert** — the state
machine will never move. You MUST set a rule on every transition:

```python
import unreal
abp = "/Game/ABP_Character"
BT  = "BlueprintToolsets.BlueprintToolset"

# 1) Add the bool/float variable the rule reads, then COMPILE so it resolves.
#    Variable creation + compile are ENGINE BlueprintTools (call_tool), not VibeUE:
call_tool(BT, "add_variable", {"blueprint": {"refPath": abp}, "variable_name": "bIsDead", "variable_type": "bool"})
call_tool(BT, "compile_blueprint", {"blueprint": {"refPath": abp}})
# (Run describe_toolset(BT) for exact arg names.)

# 2) Draw the transition (VibeUE AnimGraphService)
unreal.AnimGraphService.add_transition(abp, "Locomotion", "Idle", "Dead", 0.2)

# 3) Give it a rule — THIS is what makes it fire
unreal.AnimGraphService.set_transition_rule_from_bool(abp, "Locomotion", "Idle", "Dead", "bIsDead")
```

Rule options:
- `set_transition_rule_from_bool(abp, machine, src, dst, bool_var, invert=False)`
- `set_transition_rule_comparison(abp, machine, src, dst, float_var, op, value)` — op ∈ `greater/less/greater_equal/less_equal/equal/not_equal`
- `set_transition_rule_automatic(abp, machine, src, dst, trigger_time=-1.0)` — fires when the source state's animation is (almost) finished; perfect for one-shots (attack→idle)
- `clear_transition_rule(...)` — non-destructively reset a rule

After authoring, **always** run `validate_state_machine()` (see below) and compile.

## Critical Rules

### Opening Animation States

Use `AnimGraphService` to navigate directly to states and graphs:

```python
import unreal

# Open an AnimBP's main AnimGraph
unreal.AnimGraphService.open_anim_graph("/Game/ABP_Character", "AnimGraph")

# Open a specific state inside a state machine
unreal.AnimGraphService.open_anim_state("/Game/ABP_Character", "Locomotion", "IdleLoop")

# Open a transition rule
unreal.AnimGraphService.open_transition("/Game/ABP_Character", "Locomotion", "Idle", "Walk")
```

### ⚠️ State Machine Names

State machine names come from the node title, not the graph name. Use `list_state_machines()` to get the correct names:

```python
machines = unreal.AnimGraphService.list_state_machines("/Game/ABP_Character")
for m in machines:
    print(f"Machine: {m.machine_name}")  # Use THIS name for open_anim_state
```

### ⚠️ Case Sensitivity

All name lookups are case-insensitive, but prefer using exact names from introspection:

```python
# Both work:
unreal.AnimGraphService.open_anim_state(path, "State Controller", "In Air Loop")
unreal.AnimGraphService.open_anim_state(path, "state controller", "in air loop")
```

---

## Workflows

### Create Animation Blueprint

**CRITICAL**: AnimBlueprints require a skeleton reference. Always find the skeleton first.

```python
import unreal

# Step 1: Find skeleton from reference blueprint
ref_bp_path = "/Game/Blueprints/SandboxCharacter_CMC_ABP"
if unreal.EditorAssetLibrary.does_asset_exist(ref_bp_path):
    ref_bp = unreal.load_asset(ref_bp_path)
    skeleton = ref_bp.get_editor_property('target_skeleton')
    skeleton_path = skeleton.get_path_name()
    print(f"Using skeleton: {skeleton_path}")
else:
    # Or load directly if you know the path
    skeleton_path = "/Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin"
    skeleton = unreal.load_asset(skeleton_path)

# Step 2: Delete existing if needed
asset_path = "/Game/Tests/ABP_TestCharacter"
if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
    unreal.EditorAssetLibrary.delete_asset(asset_path)

# Step 3: Create AnimBlueprint with skeleton
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
blueprint_factory = unreal.AnimBlueprintFactory()
blueprint_factory.set_editor_property("target_skeleton", skeleton)
blueprint_factory.set_editor_property("parent_class", unreal.AnimInstance)

animation_blueprint = asset_tools.create_asset(
    "ABP_TestCharacter",
    "/Game/Tests",
    unreal.AnimBlueprint,
    blueprint_factory
)

# Step 4: Save
if animation_blueprint:
    unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False)
    print(f"Created: {asset_path}")
```

**Opening in Editor**:
```python
# Use open_editor_for_assets (plural) with list
editor_subsystem = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
opened = editor_subsystem.open_editor_for_assets([animation_blueprint])
```

**⚠️ DO NOT** use `AssetEditorSubsystem.open_editor_for_asset()` (singular) - it will cause AttributeError.

### Discover Animation Blueprint Structure

```python
import unreal

abp_path = "/Game/Blueprints/SandboxCharacter_CMC_ABP"

# Get overview (parent class is the engine's BlueprintToolset.GetParent; object path form)
parent = await call_tool("BlueprintToolsets.BlueprintToolset.GetParent",
                         blueprint={"refPath": abp_path + ".SandboxCharacter_CMC_ABP"})
skeleton = unreal.AnimGraphService.get_skeleton(abp_path)
print(f"Parent: {parent}, Skeleton: {skeleton}")

# List all graphs
graphs = unreal.AnimGraphService.list_graphs(abp_path)
for g in graphs:
    print(f"Graph: {g.graph_name} ({g.graph_type}), Nodes: {g.node_count}")
```

### List State Machines and States

```python
import unreal

abp_path = "/Game/ABP_Character"

# Find all state machines
machines = unreal.AnimGraphService.list_state_machines(abp_path)
for machine in machines:
    print(f"\nState Machine: {machine.machine_name} ({machine.state_count} states)")

    # List states in this machine
    states = unreal.AnimGraphService.list_states_in_machine(abp_path, machine.machine_name)
    for state in states:
        end_marker = " [END]" if state.is_end_state else ""
        print(f"  - {state.state_name} ({state.state_type}){end_marker}")
```

### Navigate to Specific State

```python
import unreal

abp_path = "/Game/Blueprints/SandboxCharacter_CMC_ABP"

# Open the AnimBP and navigate to a specific state
unreal.AnimGraphService.open_anim_state(
    abp_path,
    "State Controller",  # State machine name
    "In Air Loop"        # State name
)
```

### Inspect Transitions

```python
import unreal

abp_path = "/Game/ABP_Character"
machine_name = "Locomotion"

# Get all transitions in a state machine
transitions = unreal.AnimGraphService.get_state_transitions(abp_path, machine_name)
for t in transitions:
    auto = " [AUTO]" if t.is_automatic else ""
    print(f"{t.source_state} -> {t.dest_state} (blend: {t.blend_duration}s){auto}")

# Get transitions for a specific state
idle_transitions = unreal.AnimGraphService.get_state_transitions(abp_path, machine_name, "Idle")
```

---

## Authoring State Machines (build / edit)

### ⭐ Fastest path: declarative `build_state_machine`

Build (or extend) a whole machine from one JSON spec in a single atomic, idempotent call.
Re-running it never duplicates existing states/transitions. It sets animations, rules,
the entry state, then compiles — and returns a JSON report.

```python
import unreal, json

abp = "/Game/ABP_Character"
BT  = "BlueprintToolsets.BlueprintToolset"

# Add the variables the rules read FIRST, then compile so the rules can resolve them.
# Variable creation + compile are ENGINE BlueprintTools (call_tool), not VibeUE:
call_tool(BT, "add_variable", {"blueprint": {"refPath": abp}, "variable_name": "Speed",   "variable_type": "float"})
call_tool(BT, "add_variable", {"blueprint": {"refPath": abp}, "variable_name": "bAttack", "variable_type": "bool"})
call_tool(BT, "compile_blueprint", {"blueprint": {"refPath": abp}})

spec = {
    "states": [
        {"name": "Idle",   "animation": "/Game/Anims/Idle",   "loop": True,  "pos": [0, 0]},
        {"name": "Walk",   "animation": "/Game/Anims/Walk",   "loop": True,  "pos": [350, 0]},
        {"name": "Attack", "animation": "/Game/Anims/Attack", "loop": False, "pos": [350, 250]},
    ],
    "transitions": [
        {"from": "Idle",   "to": "Walk",   "rule": {"type": "comparison", "variable": "Speed", "op": "greater", "value": 10}, "blend": 0.15},
        {"from": "Walk",   "to": "Idle",   "rule": {"type": "comparison", "variable": "Speed", "op": "less_equal", "value": 10}},
        {"from": "Idle",   "to": "Attack", "rule": {"type": "bool", "variable": "bAttack"}},
        {"from": "Attack", "to": "Idle",   "rule": {"type": "automatic"}},  # when the attack anim ends
    ],
    "entry": "Idle",
}

report = json.loads(unreal.AnimGraphService.build_state_machine(abp, "Locomotion", json.dumps(spec)))
print(report)  # {"success":true,"states_created":3,"transitions_created":4,"errors":[]}
```

`rule.type` ∈ `bool` (`variable`, optional `invert`), `comparison` (`variable`,`op`,`value`),
`automatic` (optional `trigger_time`), `always` (always-true), or omit `rule` to leave it inert.

### Manual / incremental authoring

```python
import unreal
abp = "/Game/ABP_Character"

# State machine + states
unreal.AnimGraphService.add_state_machine(abp, "Locomotion", 0, 0)
unreal.AnimGraphService.add_state(abp, "Locomotion", "Idle", 0, 0)
unreal.AnimGraphService.add_state(abp, "Locomotion", "Walk", 350, 0)

# One call: create+assign the sequence player inside the state AND wire it to Output Pose
unreal.AnimGraphService.set_state_animation(abp, "Locomotion", "Idle", "/Game/Anims/Idle", True)
unreal.AnimGraphService.set_state_animation(abp, "Locomotion", "Walk", "/Game/Anims/Walk", True)

# Default/entry state
unreal.AnimGraphService.set_entry_state(abp, "Locomotion", "Idle")

# Transitions + rules (rules are mandatory — see top gotcha)
unreal.AnimGraphService.add_transition(abp, "Locomotion", "Idle", "Walk", 0.15)
unreal.AnimGraphService.set_transition_rule_comparison(abp, "Locomotion", "Idle", "Walk", "Speed", "greater", 10.0)
unreal.AnimGraphService.set_transition_priority(abp, "Locomotion", "Idle", "Walk", 1)

# Compile via engine BlueprintTools:
call_tool("BlueprintToolsets.BlueprintToolset", "CompileBlueprint", {"blueprint": {"refPath": abp}})
```

### Verify before claiming success — `validate_state_machine`

```python
result = unreal.AnimGraphService.validate_state_machine(abp, "Locomotion")
print(f"valid={result.is_valid}  states={result.state_count}  transitions={result.transition_count}")
for e in result.errors:   print("ERROR:", e)    # inert transitions, no entry state
for w in result.warnings: print("WARN :", w)    # unreachable states, states with no animation
```

Treat any `errors` as a build failure. The most common error is an **inert transition**
(a transition with no rule) — fix it with one of the `set_transition_rule_*` calls.

---

### Find Used Animation Sequences

```python
import unreal

abp_path = "/Game/ABP_Character"

sequences = unreal.AnimGraphService.get_used_anim_sequences(abp_path)
for seq in sequences:
    print(f"{seq.sequence_name}")
    print(f"  Path: {seq.sequence_path}")
    print(f"  Used in: {seq.used_in_graph}")
```

### Focus on Node by ID

```python
import unreal

abp_path = "/Game/ABP_Character"

# If you have a node GUID from BlueprintToolset.FindNodes / GetNodeInfos
node_id = "ABC123-DEF456-..."
unreal.AnimGraphService.focus_node(abp_path, node_id)
```

---

## Data Structures

> **Python Naming Convention**: C++ types like `FAnimStateMachineInfo` are exposed as `AnimStateMachineInfo` in Python (no `F` prefix).

### AnimStateMachineInfo

| Property | Type | Description |
|----------|------|-------------|
| `machine_name` | string | Display name of the state machine |
| `node_id` | string | Node GUID |
| `state_count` | int | Number of states |
| `parent_graph_name` | string | Graph containing this machine |

### AnimStateInfo

| Property | Type | Description |
|----------|------|-------------|
| `state_name` | string | Display name of the state |
| `node_id` | string | Node GUID |
| `state_type` | string | "State", "Conduit", "Entry" |
| `is_end_state` | bool | True if no outgoing transitions |
| `pos_x` | float | X position in graph |
| `pos_y` | float | Y position in graph |

### AnimTransitionInfo

| Property | Type | Description |
|----------|------|-------------|
| `transition_name` | string | Display name |
| `node_id` | string | Node GUID |
| `source_state` | string | Source state name |
| `dest_state` | string | Destination state name |
| `priority` | int | Priority (lower = higher) |
| `blend_duration` | float | Crossfade time in seconds |
| `is_automatic` | bool | Auto-transition based on sequence |
| `rule_type` | string | `None` (inert), `Bool`, `Comparison`, `Automatic`, `Custom` |
| `rule_variable` | string | Bound variable name (for Bool/Comparison rules) |
| `rule_summary` | string | Human-readable rule, e.g. `Speed > 150`, `bIsDead == true` |
| `has_rule` | bool | **False = inert (never fires).** True = transition can fire |

### AnimStateMachineValidationResult

Returned by `validate_state_machine()`.

| Property | Type | Description |
|----------|------|-------------|
| `is_valid` | bool | True when there are no blocking errors |
| `state_count` | int | Number of states |
| `transition_count` | int | Number of transitions |
| `errors` | [string] | Blocking problems (inert transitions, no entry state) |
| `warnings` | [string] | Non-blocking issues (unreachable states, states with no animation) |

---

## Driving bones directly — skeletal controls, no state machine

Not every AnimBP is a state machine. A spinning rotor, a windmill sail, a turret yaw, a rudder that
follows a variable — all are a **pose piped through Modify Bone nodes**, which is a different graph
shape and has its own traps.

The chain is always: a pose source → **Local To Component** → one Modify Bone per bone →
**Component To Local** → Output. Skeletal controls only operate in component space, and neither
`connect_anim_nodes` nor `BlueprintToolset.ConnectPins` inserts the space conversions for you.

VibeUE has no creator for the space converters, the ref pose or a variable getter, and its
connect tools address nodes by GUID, which the engine's node handles do not carry -- so build this
chain entirely with the engine's `BlueprintToolset` (one `execute_python` call):

```python
import unreal
BT = "BlueprintToolsets.BlueprintToolset."
ABP = {"refPath": "/Game/ABP_Windmill.ABP_Windmill"}           # object path: name after the dot
g = await call_tool(BT + "GetGraph", blueprint=ABP, graphName="AnimGraph")

async def make(fragment, x, y):
    """Create the one palette entry whose type id contains `fragment` (case-insensitive)."""
    ids = await call_tool(BT + "FindNodeTypes", graph=g, typeIdFilter=fragment)
    if len(ids) != 1:                                  # widen/narrow the fragment, don't guess
        raise RuntimeError(f"{fragment!r} matched {list(ids)}")
    return await call_tool(BT + "CreateNode", graph=g, typeId=ids[0], pos=[x, y])

async def pin(node, name, output):
    info = (await call_tool(BT + "GetNodeInfos", nodes=[node]))[0]
    pins = info["outputPins"] if output else info["inputPins"]
    return next(p["pinId"] for p in pins if p["name"] == name)

async def wire(src, src_pin, dst, dst_pin):
    await call_tool(BT + "ConnectPins", outputPin=await pin(src, src_pin, True),
                    inputPin=await pin(dst, dst_pin, False))

ref = await make("LocalSpaceRefPose", -900, -80)                # no sequence needed
l2c = await make("LocalToComponent", -640, -80)
mb  = await make("SkeletalControls|Transform", -340, -80)       # Transform (Modify) Bone
c2l = await make("ComponentToLocal", 0, -80)
get = await make("GetSailRotation", -400, 200)                  # see the getter-name gotcha below

# Bone and modes live on the node's inner `node` struct (see the Modify Bone gotcha below)
o = unreal.find_object(None, mb["refPath"])
n = o.get_editor_property("node")
bone = n.get_editor_property("bone_to_modify")                  # struct copies: set, then write back
bone.set_editor_property("bone_name", "sails")
n.set_editor_property("bone_to_modify", bone)
n.set_editor_property("rotation_mode", unreal.BoneModificationMode.BMM_ADDITIVE)
n.set_editor_property("rotation_space", unreal.BoneControlSpace.BCS_COMPONENT_SPACE)
o.set_editor_property("node", n)                                # write the struct back

await wire(ref, "Pose", l2c, "LocalPose")
await wire(l2c, "ComponentPose", mb, "ComponentPose")
await wire(get, "SailRotation", mb, "Rotation")
await wire(mb, "Pose", c2l, "ComponentPose")
root = (await call_tool(BT + "FindNodes", graph=g,
                        nodeClass="/Script/AnimGraph.AnimGraphNode_Root"))[0]
await wire(c2l, "Pose", root, "Result")
await call_tool(BT + "CompileBlueprint", blueprint=ABP)
```

`FindNodeTypes` filters on a substring of the palette type id (`Category|Title`, spaces dropped),
so a fragment that matches none or several entries raises with the candidates -- adjust it and
rerun. Verify the result with `BlueprintDslToolset.Read` or `GetNodeInfos` rather than trusting
the compile.

Pin names to remember: `LocalPose` in / `ComponentPose` out on Local To Component; `ComponentPose`
in / `Pose` out on Modify Bone and Component To Local.

- **Reconnecting a pose input replaces the old link.** Pose inputs are 1:1, so `connect_anim_nodes`
  (and `connect_to_output_pose`) to an already-wired single-link input now breaks the existing link
  and wires the new source in its place — no need to `disconnect_anim_node(node, "Pose")` first — and
  returns `False` (logging a Warning with the schema's reason) when the schema refuses. Previously it
  stacked an illegal second link that compiled the AnimBP to `BS_ERROR` with an **empty error list**.

### ⚠️ Modify Bone ignores its Rotation pin until you set the mode

`add_modify_bone_node` creates the node with `rotation_mode`, `translation_mode` and `scale_mode` all
set to **Ignore**. You can connect the Rotation pin, compile clean, and see *absolutely nothing move* —
there is no warning. Set the mode on the node's inner `node` struct after creating it:

```python
for i in range(0, 12):
    o = unreal.find_object(None, f"{PATH}.{name}:{G}.AnimGraphNode_ModifyBone_{i}")
    if not o:
        continue
    n = o.get_editor_property("node")
    n.set_editor_property("rotation_mode", unreal.BoneModificationMode.BMM_ADDITIVE)
    n.set_editor_property("rotation_space", unreal.BoneControlSpace.BCS_COMPONENT_SPACE)
    n.set_editor_property("translation_mode", unreal.BoneModificationMode.BMM_IGNORE)
    n.set_editor_property("scale_mode", unreal.BoneModificationMode.BMM_IGNORE)
    o.set_editor_property("node", n)      # write the struct back, or the change is lost
```

`BMM_ADDITIVE` and `BMM_REPLACE` behave identically when the bone is at identity in the ref pose.

### ⚠️ Variable getter names differ between BP variables and C++ properties

A variable getter is addressed by the node's **display name**, and the two kinds of variable are
spelled differently -- which is the part that bites, whichever tool you create the node with:

| Variable lives on | Display name |
|---|---|
| The AnimBP itself (`BlueprintToolset.AddVariable`) | `Get SailRotation` |
| A C++ `UAnimInstance` subclass | `Get Sail Rotation` (spaced) |

Never hard-code either. Find it first with `BlueprintToolset.FindNodes` (or
`BlueprintDslToolset.FindNodeReferences`, which searches palette names and categories) and match on
what comes back -- that works for both spellings. VibeUE's `create_node_by_key` /
`add_member_variable` / `discover_nodes` were removed with the rest of the node surface.

### Continuous motion without an animation asset

Put the accumulator in the AnimBP's **EventGraph** on `Event Blueprint Update Animation` (its
`DeltaTimeX` pin), write a float, and convert it to the Rotator the Modify Bone reads:

```
DeltaTimeX ─┐
            ├─ float * float ─ float * float ─┐        (RPM → deg/sec is × 6)
SpinRPM ────┘                    6.0 ─────────┤
                                              ├─ float + float ─→ SET SailAngle
SailAngle (get) ──────────────────────────────┘
SailAngle (get) ─→ MakeRotator(Roll=) ─→ SET SailRotation
```

Float maths nodes are `FUNC KismetMathLibrary::Multiply_DoubleDouble` and `::Add_DoubleDouble`
(display names `float * float` / `float + float`) — Blueprint "float" is a double in UE5.
`MakeRotator` is `FUNC KismetMathLibrary::MakeRotator` with pins `Roll`, `Pitch`, `Yaw`.

Verify it is actually running rather than trusting the compile: in PIE read the variable off the live
instance, twice, in **separate** `execute_python` calls (see the pie-testing skill — Python
blocks the game thread, so a sampling loop inside one call returns the same value every time).

```python
comp = actor.get_components_by_class(unreal.SkeletalMeshComponent)[0]
ai = comp.get_anim_instance()
print(ai.get_editor_property("SailAngle"), comp.get_socket_rotation("sails").roll)
```

## Common Patterns

### Check if Asset is AnimBP

```python
import unreal

if unreal.AnimGraphService.is_anim_blueprint("/Game/SomeAsset"):
    # It's an AnimBP, safe to use AnimGraphService
    machines = unreal.AnimGraphService.list_state_machines("/Game/SomeAsset")
```

### Iterate All States Across All Machines

```python
import unreal

abp_path = "/Game/ABP_Character"

machines = unreal.AnimGraphService.list_state_machines(abp_path)
for machine in machines:
    states = unreal.AnimGraphService.list_states_in_machine(abp_path, machine.machine_name)
    for state in states:
        print(f"{machine.machine_name}/{state.state_name}")
```

## Sample scripts (run via `execute_python`)

- **`scripts/build_state_machine.txt`** — add a state machine with two states, set state animations, add a transition, compile.

## Additional gotchas

- `connect_anim_nodes` replaces an existing pose link on the target input rather than erroring; it returns False if the schema refuses the connection, so check the return instead of hunting for a `BS_ERROR` with an empty error list.

## Reference pages

Open these with the paths below; UE rewrites them to absolute paths when it serves this skill.

- [scripts/build_state_machine.txt](scripts/build_state_machine.txt) - runnable sample
