---
name: animation
display_name: Animation Blueprints
description: Navigate and inspect Animation Blueprints, state machines, states, and transitions
vibeue_classes:
  - AnimGraphService
  - AssetDiscoveryService
unreal_classes:
  - EditorAssetLibrary
keywords:
  - animation
  - animblueprint
  - abp
  - state machine
  - state
  - transition
  - animgraph
  - locomotion
---

# Animation Blueprint Skill

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

# Get overview
parent = unreal.AnimGraphService.get_parent_class(abp_path)
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

# If you have a node GUID from get_nodes_in_graph
node_id = "ABC123-DEF456-..."
unreal.AnimGraphService.focus_node(abp_path, node_id)
```

---

## Data Structures

### FAnimStateMachineInfo

| Property | Type | Description |
|----------|------|-------------|
| `machine_name` | string | Display name of the state machine |
| `node_id` | string | Node GUID |
| `state_count` | int | Number of states |
| `parent_graph_name` | string | Graph containing this machine |

### FAnimStateInfo

| Property | Type | Description |
|----------|------|-------------|
| `state_name` | string | Display name of the state |
| `node_id` | string | Node GUID |
| `state_type` | string | "State", "Conduit", "Entry" |
| `is_end_state` | bool | True if no outgoing transitions |
| `pos_x` | float | X position in graph |
| `pos_y` | float | Y position in graph |

### FAnimTransitionInfo

| Property | Type | Description |
|----------|------|-------------|
| `transition_name` | string | Display name |
| `node_id` | string | Node GUID |
| `source_state` | string | Source state name |
| `dest_state` | string | Destination state name |
| `priority` | int | Priority (lower = higher) |
| `blend_duration` | float | Crossfade time in seconds |
| `is_automatic` | bool | Auto-transition based on sequence |

---

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
