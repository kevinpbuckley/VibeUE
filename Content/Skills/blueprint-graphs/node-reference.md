---
name: node-reference
description: Reference for specific Blueprint node APIs - add_member_get_node, add_validated_get_node, Custom Event Input Pin CRUD, and Timeline CRUD
---

This sub-doc continues from skill.md → reference subsections of "Critical Rules" (member getter, validated get, custom event inputs, timelines).

## Node Reference

### Accessing Members of Another Blueprint (`add_member_get_node`)

Use `add_member_get_node` to read a property or component that belongs to another class
(not the current Blueprint). This creates a getter node with a **Target** input pin.

```
Target (input) — the object reference (e.g. your BP_Cube variable output)
MemberName (output) — the property value (e.g. the CubeMesh component)
```

```python
# Get the CubeMesh component from a "Cube" variable of type BP_Cube
mesh_id = unreal.BlueprintService.add_member_get_node(
    bp_path, graph, "BP_Cube_C", "CubeMesh", 400, 0)

# Connect: Cube (from validated get) -> Target of the member getter
unreal.BlueprintService.connect_nodes(bp_path, graph, val_get_id, "Cube", mesh_id, "self")
# Output pin name matches the member name: "CubeMesh"
unreal.BlueprintService.connect_nodes(bp_path, graph, mesh_id, "CubeMesh", next_id, "Target")
```

The `TargetClass` must be the generated class name (`BP_Cube_C`, not `BP_Cube`). The function
resolves it via the same 3-step fallback as `create_blueprint`.

### Validated Get Nodes (`add_validated_get_node`)

Use `add_validated_get_node` to create a **Validated Get** — a variable getter with execution
pins that only continues on the valid path if the object reference is non-null.

Pin names produced:

| Pin | Name | Direction |
|-----|------|-----------|
| Execution in | `"execute"` | input |
| Is Valid (object non-null) | `"then"` | output exec |
| Is Not Valid (object null) | `"else"` | output exec |
| Variable data | variable name e.g. `"MyObject"` | output data |

```python
import unreal

bp_path = "/Game/BP_MyActor"
graph = "EventGraph"

# Compile first so the variable type is resolved
unreal.BlueprintService.compile_blueprint(bp_path)

# Add BeginPlay + validated get + some function call
begin_id = unreal.BlueprintService.add_event_node(bp_path, graph, "ReceiveBeginPlay", 0, 0)
val_get_id = unreal.BlueprintService.add_validated_get_node(bp_path, graph, "MyObject", 300, 0)
call_id = unreal.BlueprintService.add_function_call_node(bp_path, graph, "MyObject", "SomeFunction", 700, 0)

# Execution flow: BeginPlay -> ValidatedGet (Is Valid path) -> SomeFunction
unreal.BlueprintService.connect_nodes(bp_path, graph, begin_id, "then", val_get_id, "execute")
unreal.BlueprintService.connect_nodes(bp_path, graph, val_get_id, "then", call_id, "execute")

# Data flow: MyObject output -> function Target
unreal.BlueprintService.connect_nodes(bp_path, graph, val_get_id, "MyObject", call_id, "self")

unreal.BlueprintService.compile_blueprint(bp_path)
unreal.EditorAssetLibrary.save_asset(bp_path)
```

> Only Object/Actor reference variables support Validated Get (`ValidatedObject` variation).
> Primitive types (int, float, bool) produce a Branch-style impure get instead.

---

### Custom Event Input Pins — Full CRUD (`*_custom_event_input`)

`add_function_parameter` / `add_function_input` **only work on real function graphs** (`UK2Node_FunctionEntry`).
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
- Always `compile_blueprint(bp)` afterwards and verify with `get_custom_event_inputs` / `get_node_pins`.

---

### Timelines — Full CRUD (`*_timeline*`)

There is no Timeline node in `discover_nodes` / `add_function_call_node`; use the dedicated API. A Timeline is a
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

unreal.BlueprintService.compile_blueprint(bp)
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
- Always `compile_blueprint(bp)` then verify with `get_timelines` and `get_node_pins(bp, "EventGraph", tnode)`.

---
