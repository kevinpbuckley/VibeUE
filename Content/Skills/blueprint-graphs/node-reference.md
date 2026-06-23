---
name: node-reference
description: Reference for specific Blueprint node APIs - member_get and validated_get build_graph node types, Custom Event Input Pin CRUD, and Timeline CRUD
---

This sub-doc continues from skill.md → reference subsections of "Critical Rules" (member getter, validated get, custom event inputs, timelines).

## Node Reference

### Accessing Members of Another Blueprint (`member_get` node)

> The standalone `add_member_get_node` method is **gone**. Create this node as the `build_graph`
> node type **`member_get`** (`params`: `member`, `class`). The resulting node and its pin names
> are unchanged.

Use `member_get` to read a property or component that belongs to another class
(not the current Blueprint). This creates a getter node with a **Target** input pin.

```
Target (input) — the object reference (e.g. your BP_Cube variable output)
MemberName (output) — the property value (e.g. the CubeMesh component)
```

```python
# Get the CubeMesh component from a "Cube" variable of type BP_Cube.
# build_graph member_get descriptor; ref_to_node_id maps the ref back to the GUID.
r = unreal.BlueprintService.build_graph(bp_path, graph,
    [{"ref": "Mesh", "type": "member_get",
      "params": {"member": "CubeMesh", "class": "BP_Cube_C"}}],
    [], [], False, False)
mesh_id = r.ref_to_node_id["Mesh"]

# Connect: Cube (from validated get) -> Target of the member getter
unreal.BlueprintService.connect_nodes(bp_path, graph, val_get_id, "Cube", mesh_id, "self")
# Output pin name matches the member name: "CubeMesh"
unreal.BlueprintService.connect_nodes(bp_path, graph, mesh_id, "CubeMesh", next_id, "Target")
```

The `class` param must be the generated class name (`BP_Cube_C`, not `BP_Cube`). It
resolves via the same 3-step class fallback as the engine `BlueprintTools.create`.

### Setting a Member of Another Class / Component (`member_set` node)

The write-side counterpart of `member_get`. Use it to SET a property that lives on
another class — most commonly a **component property** such as
`UCharacterMovementComponent::MaxWalkSpeed`. Plain `variable_set` only binds variables
on the Blueprint itself (`SetSelfMember`); for a component property it produces a blank
Set node with no value/Target pin. `member_set` uses `SetExternalMember` so the node
comes out fully bound.

```
self (input, Target)   — the owning object; wire a Get of the component here
<MemberName> (input)   — the value to set (e.g. "MaxWalkSpeed", real)
execute / then         — exec in/out
Output_Get (output)    — passthrough of the new value (use instead of a separate Get)
```

```python
# Hold-to-sprint: IA_Sprint Started -> MaxWalkSpeed = 1000, Completed -> 600.
# CharacterMovement is an inherited component, so variable_get resolves it on self.
unreal.BlueprintService.build_graph(bp, "EventGraph",
    [
        {"ref": "Sprint", "type": "input_action", "params": {"action": "/Game/Input/Actions/IA_Sprint"}},
        {"ref": "GetCMV1", "type": "variable_get", "params": {"variable": "CharacterMovement"}},
        {"ref": "SetRun",  "type": "member_set",   "params": {"member": "MaxWalkSpeed", "class": "CharacterMovementComponent"}},
        {"ref": "GetCMV2", "type": "variable_get", "params": {"variable": "CharacterMovement"}},
        {"ref": "SetWalk", "type": "member_set",   "params": {"member": "MaxWalkSpeed", "class": "CharacterMovementComponent"}},
    ],
    [
        {"from_": "Sprint.Started",            "to": "SetRun.execute"},
        {"from_": "GetCMV1.CharacterMovement", "to": "SetRun.self"},
        {"from_": "Sprint.Completed",          "to": "SetWalk.execute"},
        {"from_": "GetCMV2.CharacterMovement", "to": "SetWalk.self"},
    ],
    [
        {"node_ref": "SetRun",  "pin_name": "MaxWalkSpeed", "value": "1000.0"},
        {"node_ref": "SetWalk", "pin_name": "MaxWalkSpeed", "value": "600.0"},
    ],
    True, True)
```

The `class` param is the property's owning class name (`CharacterMovementComponent`,
not the `U`-prefixed C++ name). Round-trips: an external-member Set exported by
`get_graph_definition` comes back as `member_set` (self variables stay `variable_set`).

### Validated Get Nodes (`validated_get` node)

> The standalone `add_validated_get_node` method is **gone**. Create this node as the
> `build_graph` node type **`validated_get`** (`params`: `variable`). The resulting node and its
> pin names are unchanged.

Use `validated_get` to create a **Validated Get** — a variable getter with execution
pins that only continues on the valid path if the object reference is non-null.

Pin names produced:

| Pin | Name | Direction |
|-----|------|-----------|
| Execution in | `"execute"` | input |
| Is Valid (object non-null) | `"then"` | output exec |
| Is Not Valid (object null) | `"else"` | output exec |
| Variable data | variable name e.g. `"MyObject"` | output data |

The whole BeginPlay → ValidatedGet → SomeFunction graph is a clean single `build_graph` call
(`event`, `validated_get`, and `function_call` are all build_graph node types; compile runs as
the final positional flag):

```python
import unreal

bp_path = "/Game/BP_MyActor"
graph = "EventGraph"

# Compile first so the variable type is resolved (engine BlueprintTools toolset)
call_tool(tool_name="compile_blueprint",
          toolset_name="editor_toolset.toolsets.blueprint.BlueprintTools",
          arguments={"blueprint": bp_path})

unreal.BlueprintService.build_graph(bp_path, graph,
    [
        {"ref": "Begin", "type": "event", "params": {"event": "ReceiveBeginPlay"}},
        {"ref": "ValGet", "type": "validated_get", "params": {"variable": "MyObject"}},
        {"ref": "Call", "type": "function_call",
         "params": {"class": "MyObject", "function": "SomeFunction"}},
    ],
    [
        # Execution flow: BeginPlay -> ValidatedGet (Is Valid path) -> SomeFunction
        {"from_": "Begin.then", "to": "ValGet.execute"},
        {"from_": "ValGet.then", "to": "Call.execute"},
        # Data flow: MyObject output -> function Target
        {"from_": "ValGet.MyObject", "to": "Call.self"},
    ],
    [], True, True)  # auto_layout, compile

unreal.EditorAssetLibrary.save_asset(bp_path)
```

> Only Object/Actor reference variables support Validated Get (`ValidatedObject` variation).
> Primitive types (int, float, bool) produce a Branch-style impure get instead.

---

### Custom Event Input Pins — Full CRUD (`*_custom_event_input`)

`add_function_parameter` **only works on real function graphs** (`UK2Node_FunctionEntry`).
They return `False` on a Custom Event node. To manage the input pins (parameters) of a Custom Event node,
use these (identify the node by GUID, as returned by `add_custom_event_node` / `get_nodes_in_graph`):

```python
# node_id = the Custom Event node's GUID
unreal.BlueprintService.add_custom_event_input(bp, "EventGraph", node_id, "Rotation", "FRotator")
unreal.BlueprintService.add_custom_event_input(bp, "EventGraph", node_id, "Speed", "float")

# rename and/or retype (keeps wires on rename; keeps wires on retype only if compatible)
unreal.BlueprintService.modify_custom_event_input(bp, "EventGraph", node_id, "Rotation", "TargetRotation")          # rename
unreal.BlueprintService.modify_custom_event_input(bp, "EventGraph", node_id, "Speed", "", "int")                    # retype
unreal.BlueprintService.modify_custom_event_input(bp, "EventGraph", node_id, "Speed", "MoveSpeed", "double")        # both

unreal.BlueprintService.remove_custom_event_input(bp, "EventGraph", node_id, "Speed")

for p in unreal.BlueprintService.get_custom_event_inputs(bp, "EventGraph", node_id):
    print(p.parameter_name, p.parameter_type)
```

- Type strings are the same format as `add_variable` (`"float"`, `"int"`, `"FRotator"`, `"FVector"`, `"AActor"`, `"BP_Foo"`, …).
- Custom Event input pins appear as **output** pins on the node (they flow out of the event). `get_node_pins` will show e.g. `Rotation | struct | input=False`.
- Empty `new_name` / `new_type` in `modify_custom_event_input` means "leave that part unchanged".
- Always compile afterwards (engine `BlueprintTools.compile_blueprint` via `call_tool`) and verify with `get_custom_event_inputs` / `get_node_pins`.

---

### Timelines — Full CRUD (`*_timeline*`)

There is no Timeline node in `discover_nodes` / `build_graph` / engine `create_node`; use the dedicated API. A Timeline is a
`UK2Node_Timeline` node **plus** a `UTimelineTemplate` on the blueprint — `add_timeline` creates both.

**Create the node + tracks + keys, then wire it:**

```python
bp = "/Game/Path/BP_Thing"; g = "EventGraph"

# 1) Timeline node. Length is in seconds; pass use_last_keyframe=True to auto-size to the last key instead.
#    add_timeline(bp, graph, name, length=5.0, use_last_keyframe=False, auto_play=False, loop=False, pos_x=0, pos_y=0)
tnode = unreal.BlueprintService.add_timeline(bp, g, "LookAtTimeline", 0.5, False, False, False, 600.0, 200.0)

# 2) Tracks (each adds an output pin on the node, named after the track)
unreal.BlueprintService.add_timeline_float_track(bp, "LookAtTimeline", "Alpha")
unreal.BlueprintService.add_timeline_vector_track(bp, "LookAtTimeline", "Offset")
unreal.BlueprintService.add_timeline_color_track(bp, "LookAtTimeline", "Tint")
unreal.BlueprintService.add_timeline_event_track(bp, "LookAtTimeline", "Halfway")   # adds a named exec-out pin

# 3) Keys. interp_mode: "Auto" (cubic + auto tangents = smooth), "Linear", "Constant", "CubicUser"
unreal.BlueprintService.add_timeline_float_key(bp, "LookAtTimeline", "Alpha", 0.0, 0.0)
unreal.BlueprintService.add_timeline_float_key(bp, "LookAtTimeline", "Alpha", 0.5, 1.0)
unreal.BlueprintService.add_timeline_vector_key(bp, "LookAtTimeline", "Offset", 0.0, 0,0,0)
unreal.BlueprintService.add_timeline_color_key(bp, "LookAtTimeline", "Tint", 0.0, 1,1,1,1)
unreal.BlueprintService.add_timeline_event_key(bp, "LookAtTimeline", "Halfway", 0.25)   # time only

# 4) Wire it: e.g. a Custom Event's exec out -> the timeline's "Play" input
unreal.BlueprintService.connect_nodes(bp, g, custom_event_id, "then", tnode, "Play")

# compile via the engine BlueprintTools toolset
call_tool(tool_name="compile_blueprint",
          toolset_name="editor_toolset.toolsets.blueprint.BlueprintTools",
          arguments={"blueprint": bp})
unreal.EditorAssetLibrary.save_asset(bp)
```

**Timeline node pins** (after adding the Alpha float + Halfway event tracks above):
- exec in: `Play`, `PlayFromStart`, `Stop`, `Reverse`, `ReverseFromEnd`, `SetNewTime` (+ data in `NewTime`)
- exec out: `Update`, `Finished`, plus one per **event track** (`Halfway`)
- data out: `Direction` (byte enum), plus one per float/vector/color track (`Alpha` real, `Offset` vector, `Tint` color)

**Edit / remove:**

```python
# settings — pass "" for name, a negative number for length, and -1 for the int flags to leave them unchanged
#   modify_timeline(bp, name, new_name="", length=-1, use_last_keyframe=-1, auto_play=-1, loop=-1, replicated=-1, ignore_time_dilation=-1)
unreal.BlueprintService.modify_timeline(bp, "LookAtTimeline", "", 0.75, -1, 1, -1, -1, -1)   # length 0.75, auto_play on
unreal.BlueprintService.modify_timeline(bp, "LookAtTimeline", "RotateTimeline")               # rename the timeline

unreal.BlueprintService.rename_timeline_track(bp, "RotateTimeline", "Alpha", "Blend")         # any track type
unreal.BlueprintService.remove_timeline_key(bp, "RotateTimeline", "Blend", 0.25, 0.001)       # by time (±tolerance)
unreal.BlueprintService.clear_timeline_track_keys(bp, "RotateTimeline", "Blend")
unreal.BlueprintService.remove_timeline_track(bp, "RotateTimeline", "Offset")                 # also removes its node pin
unreal.BlueprintService.remove_timeline(bp, "RotateTimeline")                                 # deletes node + template

for t in unreal.BlueprintService.get_timelines(bp):
    print(t.parameter_name, "| tracks:", t.parameter_type, "|", t.default_value)
    # parameter_type is "float:Name,vector:Name,color:Name,event:Name,..."; default_value has Length/LengthMode/AutoPlay/Loop/...
```

- Track names must be unique within a timeline (across all track types). Adding/removing/renaming a track or changing settings reconstructs the node, so re-read pins afterwards.
- Always compile (engine `BlueprintTools.compile_blueprint` via `call_tool`) then verify with `get_timelines` and `get_node_pins(bp, "EventGraph", tnode)`.

---
