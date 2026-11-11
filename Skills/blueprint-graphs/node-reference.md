---
name: node-reference
description: Timeline CRUD on unreal.BlueprintService (the 16 surviving VibeUE tools - signatures, sentinels, node pins) plus the Graph DSL forms that replaced the old member_get / member_set / validated_get / custom-event-input node types
---

This sub-doc continues from SKILL.md → "VibeUE's surviving graph tools" and "The DSL workflow".

# Node Reference

Two halves. The first is **VibeUE's** — 16 of the 19 surviving `BlueprintService` tools are
Timelines, and no engine tool can do any of it. The second shows the **DSL spellings** that replaced
the old `build_graph` node types this file used to document.

---

## Timelines — full CRUD (`unreal.BlueprintService.*timeline*`)

A Timeline is a `UK2Node_Timeline` node **plus** a `UTimelineTemplate` on the Blueprint.
`add_timeline` creates both. Nothing in `BlueprintToolsets` or `EditorToolset` touches a
`UTimelineTemplate`, which is why this whole surface stayed in VibeUE. The DSL can **anchor** to a
timeline node that already exists (`` (`Timeline|Name` …) ``) but cannot create one.

All of these are plain Python inside `execute_python`, and every one refuses while PIE is running.

### Create the node, add tracks, add keys

```python
import unreal
bs = unreal.BlueprintService
bp, g = "/Game/Path/BP_Thing", "EventGraph"

# 1) The node + template. Length is seconds; use_last_key_frame=True auto-sizes to the last key.
#    add_timeline(bp, graph, name, length=5.0, use_last_key_frame=False, auto_play=False,
#                 loop=False, pos_x=0.0, pos_y=0.0, replace_existing=False)
tnode = bs.add_timeline(bp, g, "LookAtTimeline", 0.5, False, False, False, 600.0, 200.0)

# 2) Tracks. Each adds an output pin on the node, named after the track.
bs.add_timeline_float_track(bp, "LookAtTimeline", "Alpha")    # real out
bs.add_timeline_vector_track(bp, "LookAtTimeline", "Offset")  # vector out
bs.add_timeline_color_track(bp, "LookAtTimeline", "Tint")     # linear-color out
bs.add_timeline_event_track(bp, "LookAtTimeline", "Halfway")  # named EXEC out

# 3) Keys. interp_mode: "Auto" (cubic + auto tangents = smooth), "Linear", "Constant",
#    "CubicAuto", "CubicUser". Default "Auto".
bs.add_timeline_float_key(bp, "LookAtTimeline", "Alpha", 0.0, 0.0)
bs.add_timeline_float_key(bp, "LookAtTimeline", "Alpha", 0.5, 1.0)
bs.add_timeline_vector_key(bp, "LookAtTimeline", "Offset", 0.0, 0.0, 0.0, 0.0)
bs.add_timeline_color_key(bp, "LookAtTimeline", "Tint", 0.0, 1.0, 1.0, 1.0, 1.0)
bs.add_timeline_event_key(bp, "LookAtTimeline", "Halfway", 0.25)   # time only
```

### ⚠️ `add_timeline`'s three return cases

| Return | Meaning |
|---|---|
| a GUID string | success — the Timeline node's id |
| `""` | the Blueprint or the graph could not be loaded (PIE? wrong path? wrong graph name?) |
| a string starting with `"ERROR:"` | a `UTimelineTemplate` with that name already exists and `replace_existing` was `False` |

A **deleted Timeline node can leave its template behind**, which is exactly how you hit the third
case on a name you believe is free. Pass `replace_existing=True` to remove the orphan and re-add, or
call `remove_timeline(bp, name)` first. Check the string before treating it as a node id:

```python
tnode = bs.add_timeline(bp, g, "LookAtTimeline", 0.5)
assert tnode and not tnode.startswith("ERROR:"), tnode
```

### Timeline node pins

With the `Alpha` float track and the `Halfway` event track above:

- **exec in**: `Play`, `PlayFromStart`, `Stop`, `Reverse`, `ReverseFromEnd`, `SetNewTime` (plus data
  in `NewTime`)
- **exec out**: `Update`, `Finished`, plus one per **event** track (`Halfway`)
- **data out**: `Direction` (byte enum), plus one per float/vector/color track (`Alpha` real,
  `Offset` vector, `Tint` color)

Wire them from the DSL by anchoring to the node. The statement enters `Play` unless
``:!via `Pin` `` names another exec input, and any other arrival is a `goto` to its label with the
same `:!via`:

```
(event `self.ReceiveBeginPlay`
  (bind (_direction _alpha) (`Timeline|LookAtTimeline` :!via `PlayFromStart` :!label _look)
    (:`Update`
      (`KismetSystemLibrary.PrintString` "ticking"))
    (:`Finished`
      (`KismetSystemLibrary.PrintString` "done"))
    (:`Halfway`
      (`KismetSystemLibrary.PrintString` "halfway"))))

(event `Cancel`
  (goto _look :!via `Stop`))
```

Or imperatively: `BlueprintToolset.ConnectPins` with the node handle from
`FindNodes` / `GetNodeInfos`.

### Edit and remove

```python
# modify_timeline sentinels: "" for the name, a NEGATIVE number for length, -1 for the int flags
#   (1 = true, 0 = false, -1 = leave unchanged)
#   modify_timeline(bp, name, new_name="", length=-1.0, use_last_key_frame=-1, auto_play=-1,
#                   loop=-1, replicated=-1, ignore_time_dilation=-1)
bs.modify_timeline(bp, "LookAtTimeline", "", 0.75, -1, 1, -1, -1, -1)   # length 0.75, auto_play ON
bs.modify_timeline(bp, "LookAtTimeline", "RotateTimeline")              # rename only

bs.rename_timeline_track(bp, "RotateTimeline", "Alpha", "Blend")        # any track type
bs.remove_timeline_key(bp, "RotateTimeline", "Blend", 0.25, 0.001)      # by time (± tolerance)
bs.clear_timeline_track_keys(bp, "RotateTimeline", "Blend")
bs.remove_timeline_track(bp, "RotateTimeline", "Offset")                # also removes its node pin
bs.remove_timeline(bp, "RotateTimeline")                                # node + template

for t in bs.get_timelines(bp):
    # FBlueprintTimelineInfo: timeline_name, track_count (all types), length, loop, auto_play
    print(t.timeline_name, "| tracks:", t.track_count, "| len:", t.length,
          "| loop:", t.loop, "| auto_play:", t.auto_play)
```

Renaming a timeline also renames the component variable and its event-track functions.

### Timeline gotchas

- **Track names are unique within a timeline across all track types** — a float track and an event
  track cannot both be `Alpha`.
- **Adding, removing or renaming a track, or changing settings, reconstructs the node.** Any node
  handle, pin id or DSL `Revision` you were holding for that graph is stale afterwards: re-read.
- Compile (`BlueprintToolset.CompileBlueprint`) then verify with `get_timelines` and a fresh pin
  read before claiming the timeline is wired.
- Settings on the spawned `UTimelineComponent` are ordinary properties — use
  `EditorToolset.ObjectToolset.SetProperties`, not a timeline tool.

---

## DSL spellings for the node types this file used to document

The `build_graph` node types `member_get`, `member_set`, `validated_get` and the
`*_custom_event_input` CRUD are all gone. Each has a grammar form instead. Run
`BlueprintDslToolset.Schema(AssetPath, NodeType)` to confirm pin names before writing any of them.

### Reading a member of another object (was `member_get`)

A member read is a call head: `` (`Owner.Member` target) ``. `Owner` is a native class name or a
Blueprint class path; `target` is the object to read from. The authored name is what counts, exactly
as `Read` returns it.

```
(event `self.ReceiveBeginPlay`
  (bind mesh (`BP_Cube_C.CubeMesh` (`self.Cube`)))
  (`KismetSystemLibrary.PrintString` (`KismetSystemLibrary.GetDisplayName` mesh)))
```

A member of **this** Blueprint's own class chain is `self.Member` — unless a node reaches it through
a target pin, in which case it is spelled with its class instead of `self`.

### Writing a member of another object (was `member_set`)

```
(set `Owner.Member` target value)
```

The classic case is a component property. `set` on `self.X` binds a variable on the Blueprint
itself; an external member needs the owner and a target:

```
(event `self.ReceiveBeginPlay`
  (set `CharacterMovementComponent.MaxWalkSpeed` (`self.CharacterMovement`) 1000.0))
```

`Owner` is the property's owning class, spelled without the C++ `U`/`A` prefix
(`CharacterMovementComponent`). To change only some fields of a struct member, use
`` (setmembers `Struct` :`StructRef` target :`Member` value …) `` — it touches only the members it
names.

### Validated Get (was `validated_get`)

There is no separate node type. **Execution continuations turn a pure variable getter into a
validated getter**: bind the variable and give the bind `` (:`then` …) `` / `` (:`else` …) ``
continuations.

```
(event `self.ReceiveBeginPlay`
  (bind _owner (`self.DslOwner`)
    (:`then`
      (`KismetSystemLibrary.PrintString` (`KismetSystemLibrary.GetDisplayName` _owner)))
    (:`else`)))
```

`` (:`else`) `` with an empty body leaves the invalid path unwired. Only object/actor reference
variables support this; a primitive produces an ordinary impure get. For a non-variable value, the
`IsValid` standard macro is the multi-exec equivalent, with continuation arms named after its pins:

```
(`/Engine/EditorBlueprintResources/StandardMacros.IsValid` some_object
  (:`Is Valid`    (`KismetSystemLibrary.PrintString` "valid"))
  (:`Is Not Valid` (`KismetSystemLibrary.PrintString` "not valid")))
```

### Custom event parameters (was `add_custom_event_input` & friends)

A custom event's input pins are its `(params …)` section. The section **is the exact set**: listed
items are added or updated, unlisted ones are removed, and `(params)` with nothing in it removes
every parameter. Items match by name, so a rename is a removal plus an addition, and the section's
order is the pin order.

```
(event `OnHit`
  (params (Damage :type Integer)
          (Instigator :type "Actor Object Reference"))
  (set `self.Health` (- (`self.Health`) Damage)))
```

- A new parameter needs `:type`. A listed one is retyped when `:type` names another type, and its
  `:default` is set when it differs.
- Pin types are spelled as the editor words them, with the kind suffixed: `Integer`,
  `Pawn Object Reference`, `Vector Structure`, `/Game/Modes/E_Mode Enum`,
  `Array of Actor Object Reference`.
- Custom-event parameters appear as **output** pins on the node — they flow out of the event.
- A handler whose parameters come from a function or dispatcher signature declares the source with
  ``:!signature `Owner.Member` `` and gets a **read-only** signature: naming a pin it lacks fails,
  and a mismatched `:type` warns while the graph wins.

`BlueprintToolset.AddFunctionParam(Graph, ParamName, ParamType, bInputParam, Options)` is the
imperative route for a **function or event-dispatcher** graph (dispatchers take inputs only). It does
not apply to a custom event node — that is the `(params …)` section's job.

### Make Struct / Make Instanced Struct (was `make_struct` / `instanced_struct`)

Struct construction is `` (make `Struct` …) ``, positionally or by field, and a struct **default** is
a literal:

```
(set `self.Stats` (make `/Game/Types/F_Stats.F_Stats` :`Health` 5 :`Label` "x"))
(set `self.Bounds` (struct :`Min` (1 2 3)))
(set `self.Spin` (struct :`Yaw` 90))
```

Vector-family values use positional tuples (`(1 2 3)`); other structs use
``(struct :`Field` value …)`` with default-valued fields omitted. A Rotator's fields are `Pitch`,
`Yaw`, `Roll`; colors use their named channels. An engine struct needs its real name or full path —
`Schema` and `FindNodeReferences` resolve it. An incompatible struct form fails the write outright,
so there is no "wildcard pin not yet resolved" trap the old `instanced_struct` node had.

---
