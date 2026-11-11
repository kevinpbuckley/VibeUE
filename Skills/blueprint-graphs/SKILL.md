---
name: blueprint-graphs
display_name: Blueprint Graph Editing
description: Blueprint graph and node authoring is Epic's in UE 5.9 — `BlueprintToolsets.BlueprintDslToolset` is a whole-graph DSL (Docs/Schema, then Read → edit the text → Write or Edit against a revision token) and `BlueprintToolsets.BlueprintToolset` holds the ~77 per-node, pin, variable, function, event and layout tools. VibeUE keeps only Timelines, macro-graph creation (CreateMacroGraph) and read-only layout measurement (AnalyzeGraphLayout) plus RefreshBlueprintEditor. Use when the user asks to wire up Blueprint logic, add or connect nodes, add a loop/macro node, build an event graph, set up a timer/delay, broadcast a delegate, add or key a Timeline, or lay out and audit an existing graph.
vibeue_classes:
  - BlueprintService
unreal_classes:
  - EditorAssetLibrary
  - BlueprintDslToolset
  - BlueprintToolset
keywords:
  - node
  - graph
  - connect
  - wire
  - pin
  - event
  - function call
  - timer
  - delay
  - custom event
  - branch
  - math
  - execute
  - array
  - wildcard
related_skills:
  - blueprints
---

> 🧠 **Brains complement:** IF an `unreal-engine-skills-manager` tool (external MCP) exists in this session, call it with `{action: "load", skill: "blueprint-fundamentals"}` for UE domain knowledge on this topic — correct APIs, architecture, best practices — and treat it as the rubric for any review / "best practices" question. If no such tool is available (e.g. running under Claude Code or Codex without that MCP), skip this line entirely and proceed with this skill alone — do NOT attempt the call.

# Blueprint Graph Editing Skill

> **Also load `blueprints`** when creating a Blueprint asset, adding variables/components, or taking
> an override. This skill covers **graph bodies** — statements, wires, pin values, layout and
> Timelines.

## Who owns what in 5.9

Graph authoring moved out of VibeUE wholesale. `BlueprintService` went from 96 AI tools to **19**.

| Job | Tool |
|---|---|
| Author/rewrite a graph body as text | `BlueprintToolsets.BlueprintDslToolset` — `Docs`, `Schema`, `Read`, `Write`, `Edit`, `Diff`, `FindNodeReferences` |
| One node, one pin, one variable, one event — surgical | `BlueprintToolsets.BlueprintToolset` (~77 tools) |
| Arrange a graph | `BlueprintToolset.ArrangeGraphLayered` / `ArrangeNodes` |
| **Measure** a layout you just wrote | `unreal.BlueprintService.analyze_graph_layout` (VibeUE — nothing in Epic reads a layout back) |
| **Timelines** (node + `UTimelineTemplate` + tracks + keys) | `unreal.BlueprintService.*timeline*` (VibeUE — no engine tool touches a `UTimelineTemplate`) |
| **Create a macro graph** on a Macro Library | `unreal.BlueprintService.create_macro_graph` (VibeUE — the DSL refuses macro libraries) |
| Force an open Blueprint editor to redraw | `unreal.BlueprintService.refresh_blueprint_editor` (VibeUE) |
| What the user has selected / is looking at | `EditorToolset.EditorAppToolset.GetAssetEditorSelection`, `GetOpenAssets`, `OpenEditorForAsset` |

## Critical Rules

### ⚠️ The toolset name `editor_toolset.toolsets.blueprint.BlueprintTools` does not exist

Older revisions of this pack used that string. There is no such toolset. The real names are
**`BlueprintToolsets.BlueprintToolset`** and **`BlueprintToolsets.BlueprintDslToolset`**, and their
tool names are **PascalCase** (`CreateNode`, `ConnectPins`, `CompileBlueprint`, `Read`, `Write`) —
these are C++ toolsets, not Python ones. There is no lowercase `create` / `compile_blueprint` /
`add_variable` tool.

### ⚠️ Call styles — do not mix these up

**1. VibeUE's 19 survivors are Python**, inside `execute_python`:

```python
import unreal
unreal.BlueprintService.add_timeline("/Game/BP/BP_Door", "EventGraph", "SwingTimeline", 1.0)
```

**2. Epic's toolsets are `call_tool`** — one tool per MCP round-trip:

```
call_tool(tool_name="Read",
          toolset_name="BlueprintToolsets.BlueprintDslToolset",
          arguments={"assetPath": "/Game/BP/BP_Door.BP_Door", "scope": "EventGraph"})
```

**3. Inside `execute_python`, reach them with `vibeue.exec_tool`** — the same dispatch, in-process,
so a whole task is one round-trip. It fills missing optional args from the tool schema, unwraps
`returnValue` and raises `RuntimeError` on a tool error:

```python
import vibeue
DSL = "BlueprintToolsets.BlueprintDslToolset"
r = vibeue.exec_tool(DSL, "Read", {"assetPath": "/Game/BP/BP_Door.BP_Door", "scope": "EventGraph"})
```

`BlueprintDslToolset` is `AICallable` only — **`unreal.BlueprintDslToolset.read(...)` does not
exist.** Reach the DSL through `call_tool` / `vibeue.exec_tool`, never as a `unreal.*` API.

**Object arguments** are `{"refPath": "<full object path>"}` handles (or the bare object path
string): `/Game/Dir/BP_X.BP_X` for the asset, with the asset name repeated after the dot. A bare
package path (`/Game/Dir/BP_X`) is rejected. `AssetPath` on the DSL tools is a plain string.
`BlueprintToolset` takes live objects — a `UEdGraph` for `CreateNode`, a `UEdGraphNode` for
`DeleteNode` — so chain the handle a previous tool returned rather than rebuilding it:

```python
import vibeue
BT = "BlueprintToolsets.BlueprintToolset"

def ref(p):
    tail = p.rsplit("/", 1)[-1]
    return {"refPath": p if "." in tail else "{}.{}".format(p, tail)}

bp = ref("/Game/BP/BP_Door")
graph = vibeue.exec_tool(BT, "GetGraph", {"blueprint": bp, "graphName": "EventGraph"})
node  = vibeue.exec_tool(BT, "CreateNode",
                         {"graph": graph, "typeId": "Development|PrintString", "pos": [400, 0]})
```

### ⚠️ Nothing works during Play-In-Editor

Every `BlueprintService` entry point **refuses** while a PIE session is running and logs it with a
`PIE_ACTIVE:` prefix; the return is a `False` / empty string / empty array. Asset writes during PIE
are unsafe anyway. Check this first when a call returns nothing unexpectedly:

```python
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if les.is_in_play_in_editor():
    raise RuntimeError("Stop PIE first - Blueprint authoring refuses while PIE runs")
```

`EditorAppToolset.IsPIERunning` / `StopPIE` are the engine-side equivalents. Editor-world edits
issued immediately after stopping PIE can silently no-op — let the editor settle (poll) before
editing.

### ⚠️ Level Blueprints: pass the MAP path

Every `BlueprintService` `BlueprintPath` parameter also accepts a **map/world path**
(`/Game/Maps/MainMenu`) and resolves it to that level's Level Blueprint. Explicit subobject paths
(`/Game/Maps/MyMap.MyMap:PersistentLevel.MyMap`) work too. The **MAP** asset is what you save
afterwards.

---

## The DSL workflow — this is the main event

`BlueprintDslToolset` is **not** a like-for-like replacement for the old per-node creators. A graph
is a canonical text document. You read it, edit the text, and write it back against a revision
token. Follow this order:

1. **`Docs()` once per session, before your first `Write` or `Edit`.** It returns the common grammar
   plus the Blueprint dialect: entry forms, statements, expressions, struct values, metadata,
   scopes. You cannot reliably author the DSL without it, and it is one cheap call.
2. **`Schema(AssetPath, NodeType)` for any node type you are not sure of**, before writing it. It
   returns that one type's input pins, output pins, writable properties and the properties that
   change the pin set, as a DSL form. Use `FindNodeReferences(AssetPath, Query)` first when you only
   know the palette name — the **reference** it returns (not the palette name) is what goes in a call
   head.
3. **`Read` the scope you intend to change, and keep the `Revision`.** `Read` returns the canonical
   DSL text plus a `Revision` token. Pass `bIncludePositions=true` only when you actually need node
   positions.
4. **`Write` for a complete document or a large structural change; `Edit` for targeted exact-string
   replacements against the text you just read.** Both are transactional — a failure leaves the
   asset unchanged — and both return the canonical DSL plus a **new** `Revision`.
5. **Re-`Read` when a revision goes stale.** `Edit`'s `Revision` must come from the latest
   `Read`/`Write`/`Edit` for the *same asset and scope*. Anything else fails; call `Read` again
   rather than guessing.

### Scope syntax

| `Scope` | Selects |
|---|---|
| `""` | the whole asset — every event and function graph |
| `GraphName` | one graph |
| `GraphName/EntryName` | one event or function entry |

Backtick-quote a segment containing spaces, syntax characters or `/`: `` `A/B` `` is **one**
segment. The `Scope` you pass to `Edit` must be the one the operation that returned your `Revision`
used.

### ⚠️ `Write` is upsert, and deletion needs a tombstone

> "Top-level entries use upsert semantics: omitted entries remain and produce a warning. Delete an
> entry with a tombstone such as ``(event `Foo` :!delete)``."

So a `Write` that omits an entry does **not** remove it — it leaves it and warns. To remove one,
write the tombstone. Deleting a missing entry only warns; deleting a *referenced* entry fails.
Writes create missing custom events, but a `(fn ...)` body still needs an existing function graph
(`BlueprintToolset.AddFunctionGraph`), and a macro library is refused outright.

`Write` and `Edit` carry `NonTransactableToolCall` — they manage their own transaction, so do not
wrap them in one of your own.

### Worked example — read, then write one entry

```python
import vibeue
DSL = "BlueprintToolsets.BlueprintDslToolset"
BP  = "/Game/BP/BP_Door.BP_Door"

# 0. Grammar, once.
print(vibeue.exec_tool(DSL, "Docs", {}))

# 1. What is there now, and the revision that proves we are current.
cur = vibeue.exec_tool(DSL, "Read", {"assetPath": BP, "scope": "EventGraph"})
print(cur["dsl"], cur["revision"])

# 2. Confirm the pins of anything unfamiliar before writing it.
print(vibeue.exec_tool(DSL, "Schema", {"assetPath": BP, "nodeType": "KismetSystemLibrary.PrintString"}))

# 3. Whole entry -> Write. Other entries are untouched (upsert), which is what we want here.
doc = """
(event `self.ReceiveBeginPlay`
  (bind loc (`Actor.K2_GetActorLocation`))
  (if (> (.z loc) 0.0)
    (`KismetSystemLibrary.PrintString` "above ground")
    (else (`KismetSystemLibrary.PrintString` "below ground"))))
"""
res = vibeue.exec_tool(DSL, "Write", {"assetPath": BP, "dsl": doc, "scope": "EventGraph"})
rev = res["revision"]          # keep it: the next Edit needs THIS token
```

Things the example shows that the grammar enforces:

- An engine override is `` (event `self.ReceiveBeginPlay` ...) `` — the implemented **member
  reference**, not a palette title.
- Members are spelled by their **authored** name as `Read` returns them: `K2_GetActorLocation`, not
  `SetActorLocation`/`GetActorLocation`.
- Control flow is a statement form (`if` / `for` / `while` / `switch` / `select`), not a Branch node
  you wire by hand. `elif` or `else` must end its enclosing branch.
- Numeric literals carry pin type: a real pin reads `0.0`, an integer pin `0`.
- A statement's data outputs come back through `(bind name (...))`; inside a continuation body they
  are implicit locals (`Return Value` → `_return_value`).

### Targeted change — `Edit`

```python
e = vibeue.exec_tool(DSL, "Edit", {
        "assetPath": BP,
        "edits": [{"oldString": '"above ground"', "newString": '"clear"'}],
        "revision": rev,
        "scope": "EventGraph"})
rev = e["revision"]
```

`Edit` fails — leaving the asset unchanged — when there is no matching session, when the `Revision`
is stale, when an `oldString` is absent or non-unique (unless `bReplaceAll`), or when the result does
not parse. On a stale-revision failure, `Read` again and rebuild the edit against the fresh text;
do not retry the same token.

### Delete an entry

```python
vibeue.exec_tool(DSL, "Write",
                 {"assetPath": BP, "dsl": "(event `OldHandler` :!delete)", "scope": "EventGraph"})
```

### Comparing two Blueprints

`Diff(OldAssetPath, NewAssetPath)` returns a line diff of two Blueprints' graphs as DSL plus their
class defaults, empty when they match. That is the cheap way to answer "what changed between these
two Blueprints" without reading both documents.

---

## When to reach for `BlueprintToolset` instead

The DSL rewrites whole entries. Use `BlueprintToolsets.BlueprintToolset` when the change is smaller
than an entry, or when it is not representable as graph text at all:

| Situation | Tool |
|---|---|
| Nudge one node, or fix a layout the DSL does not own | `SetNodePosition`, `ArrangeNodes`, `ArrangeGraphLayered` |
| One pin's literal value | `SetPinValue` / `GetPinValue` |
| One wire | `ConnectPins` / `DisconnectPins` |
| Inspect without a full document read | `ListGraphs`, `GetGraph`, `FindNodes`, `GetNodeInfos`, `GetConnectedSubgraph`, `GetNodeTypePins` |
| Discover what can be placed | `FindNodeTypes`, `FindNodeCategories` (palette **type ids**, `Category\|Name`) |
| Comment boxes | `AddCommentBox`, `ListCommentBoxes`, `Get`/`SetCommentBoxText`, `GetNodesInCommentBox`, `RemoveCommentBox` |
| Node note bubbles | `GetNodeNote`, `SetNodeNote`, `IsNodeNotePinned`, `SetNodeNotePinned` |
| Variables, their flags, categories, tooltips | `AddVariable` / `AddObjectVariable` / `AddStructVariable`, `ListVariables`, `Set*` |
| Function graphs and their params | `AddFunctionGraph`, `AddFunctionParam`, `Get`/`SetFunctionFlags` |
| Events, dispatchers, component-bound events, delegates | `AddEvent`, `ListEvents`, `AddEventDispatcher`, `AddComponentBoundEvent`, `AddMatchingDelegateEvent`/`Function`, `ListCompatibleEventFunctions` |
| Retype a node's class in place | `RetargetNodeClass` |
| Add/remove a dynamic pin (Switch, Sequence, Make Array, commutative Add) | `AddNodePin`, `RemoveNodePin` |
| Compile | `CompileBlueprint` (`bWarningsAsErrors` optional) |
| Collapsed graphs you do not want flattened | `BlueprintToolset` — a DSL `Write` that changes a collapsed graph's entry **permanently replaces the collapsed node with flat nodes** |

Two different naming systems, do not swap them:

- `BlueprintToolset` **type ids** are palette paths: `Development|PrintString`,
  `Utilities|Operators|Add`, `Utilities|FlowControl|ForLoop`, `AddEvent|EventBeginPlay`,
  `AddEvent|Custom|MyEvent`, `Default|EventDispatcherOnDamaged`, `|CallParentFunction`. Get them
  from `FindNodeTypes`.
- DSL **references** are `Owner.Member` (`KismetSystemLibrary.PrintString`, `self.UpdateScore`,
  `parent.ReceiveBeginPlay`) plus a few `|` built-ins (`|MakeArray`, `|FormatText`,
  `|CreateDelegate`, `Timeline|Name`, `AddComponent|Name`, `Key|F5`). Get them from
  `FindNodeReferences`. **Palette type ids are not DSL call heads.**

### Pin-name rules (still true, and they apply to `ConnectPins` / `SetPinValue`)

| Node | Pins |
|---|---|
| Branch | exec out is **`then`** and **`else`** — never `true`/`false`; condition pin is `Condition` |
| Any exec node | exec in is `execute`, exec out is `then` |
| Function Result | `execute` plus one pin per output |
| Validated Get | `execute`, `then` (valid), `else` (invalid), data pin named after the variable |
| Custom Event | `OutputDelegate`, `then`, plus one output per parameter |
| `K2Node_AddDelegate` (bind) | `execute`, `then`, `self` (target), `Delegate` (input) |
| Timeline node | exec in `Play`, `PlayFromStart`, `Stop`, `Reverse`, `ReverseFromEnd`, `SetNewTime` (+ `NewTime`); exec out `Update`, `Finished` + one per event track; data out `Direction` + one per float/vector/color track |
| Set Timer by Event (`KismetSystemLibrary.K2_SetTimerDelegate`) | `execute`, `Delegate`, `Time`, `bLooping`, `bMaxOncePerFrame` |

**Epic's toolset identifies a node by its object handle, not by a GUID string.** `FindNodes` returns
`UEdGraphNode` handles and `GetNodeInfos` returns `FBlueprintNodeInfo` — `TypeId` (`Category|Title`),
`Node` (the handle), `Position`, `InputPins`, `OutputPins`. There is no `node_id` field: chain the
handle a previous call returned into the next call. A node **GUID** string only turns up in VibeUE's
`analyze_graph_layout` report (`nodes[].id`); to act on one of those, find the node by title with
`FindNodes(graph, title)` and use the handle. Read the real pin list (`GetNodeTypePins`, or `Schema`
on the DSL side) rather than guessing pin names.

### ⚠️ The Result node's `execute` pin must be wired

Compile warning **`ReturnNode Exec pin has no connections`** means execution never reaches the Return
node, so the function's **output parameters are never set** — callers always get defaults. The
function is broken even though the compile "succeeded". In the DSL this is a `(return value ...)`
statement on the live exec path; with `BlueprintToolset` it is an `Entry.then → … → Result.execute`
chain. For a compute-only function, wire the entry **directly** to the result; connecting only the
data pin is not enough. Never report success with a `ReturnNode` warning outstanding. (If the
function has no side effects, making it pure is the cleaner fix.)

### ⚠️ Compile after a structural change, before reading the new shape

A variable must exist on the compiled **skeleton** class before a getter/setter can resolve it, and
an added function/dispatcher/interface is only fully real after a compile. Order is:
add structure → `BlueprintToolset.CompileBlueprint` → author the graph → compile → save. Never save
on `BS_ERROR`.

### ⚠️ Event-style overrides live in `"EventGraph"` — not in a graph named after the function

Void/latent overrides (`ReceiveTick`, `ReceiveLatentEnterState`, `ReceiveStateCompleted`,
`ReceiveTreeStart`…) are **event nodes in the ubergraph**. There is no graph of that name, so a scope
of `ReceiveTreeStart` selects nothing and a `GetGraph` for it returns nothing. Only value-returning
overrides (`GetStaticDescription`…) get their own function graph.

| Raw function name | Scope / graph | Title in the graph |
|---|---|---|
| `ReceiveTreeStart` | `EventGraph` | `Event TreeStart` |
| `ReceiveTick` | `EventGraph` | `Event Tick` |
| `ReceiveTreeStop` | `EventGraph` | `Event TreeStop` |
| `ReceiveLatentEnterState` | `EventGraph` | `Event EnterState` |
| `ReceiveLatentTick` | `EventGraph` | `Event Tick` |
| `ReceiveStateCompleted` | `EventGraph` | `Event StateCompleted` |

In the DSL the entry is `` (event `self.ReceiveLatentEnterState` …) `` — the member reference, inside
the ubergraph scope. `BlueprintToolset.AddEvent(bp, "ReceiveLatentEnterState", pos)` is the
imperative equivalent and is idempotent: naming an inherited member *is* taking the override.

### ⚠️ Do not guess function or node names

Blueprint display titles and internal `UFunction` names routinely differ (`Get Actor Location` is
`K2_GetActorLocation`; `VInterp To` is `VInterpTo`). Resolve the name before you write it:
`FindNodeReferences` for a DSL reference, `FindNodeTypes` for a palette type id. If a lookup comes
back empty, make the search term **more** specific rather than raising the result cap.

### ⚠️ UE uses doubles for Blueprint math

| Wrong | Right |
|---|---|
| `Greater_FloatFloat` | `Greater_DoubleDouble` |
| `Add_FloatFloat` | `Add_DoubleDouble` |
| `MakeLiteralFloat` | `MakeLiteralDouble` |

`UKismetSystemLibrary::MakeLiteralFloat` is `UE_DEPRECATED(5.1)` and is not a `UFUNCTION` at all;
`MakeLiteralDouble` carries `DisplayName = "Make Literal Float"`, which is why the editor still shows
the old label. A reference to `KismetSystemLibrary.MakeLiteralFloat` resolves to nothing. In the DSL
you rarely need these by name — `(> a b)`, `(+ a b)` and a bare literal resolve the operator from the
wired operands — but a member call must use the real name.

### 💬 Comment boxes — write *informative* text

`BlueprintToolset.AddCommentBox` (and the DSL's comment-box headers, which cover the next contiguous
statement run) are documentation that lives next to the code. A future reader should learn **what
this group accomplishes and why** without re-reading every pin.

**Good:** `"Pick a random patrol point and cache its world location for the Move To task"`,
`"On Damage > 0: play hit reaction, flash material, decrement Health"`,
`"Debounce input — ignore re-presses within 0.25s of the last fire"`.

**Bad:** `"Print String"` (just the node name), `"Logic"` / `"Stuff"` / `"TODO"` (says nothing),
`"Branch + Set Variable"` (mechanical, not intent), or the user's verbatim request like
`"add a comment around the nodes"` (that's the task, not the meaning).

Derive the text from the nodes — the event, the variables read/written, the functions called —
before you place the box. Use `\n` when the group's flow is worth narrating. `AddCommentBox` picks up
and drags any K2 nodes whose bounds fall inside it, exactly like the editor's `C` shortcut.

### ⚠️ Verification is mandatory before claiming success

After any graph edit, prove it from live data:

1. **Shape** — `BlueprintDslToolset.Read` the scope back and check the statements and wires you
   intended are in the canonical text (wires are explicit in the DSL). Or `FindNodes` +
   `GetNodeInfos` for a per-node check.
2. **Pins** — if something did not connect, read the real pin list (`GetNodeTypePins`, `Schema`) and
   use the actual names.
3. **Compile** — `BlueprintToolset.CompileBlueprint` and read the result. Treat a `ReturnNode`
   warning as a bug, not a nit.
4. **Layout** — `unreal.BlueprintService.analyze_graph_layout` and confirm `issues` is empty.

Never print `MODIFIED` or describe a graph as complete until the expected statements are present and
the compile is clean. Never treat compile success alone as proof the graph matches the request.

---

## VibeUE's surviving graph tools

All 19 are `unreal.BlueprintService.<snake_name>()` from inside `execute_python`. Sixteen are
Timelines; the other three are below. **Nothing in Epic's toolsets replaces any of them.**

### `create_macro_graph(blueprint_path, macro_name)` → bool

Adds a macro graph to a Blueprint, typically a Macro Library. It survives because **the engine has
no path to it**: `BlueprintToolset.AddFunctionGraph` makes *function* graphs (and adding one to a
Macro Library asserts in the K2 schema), and `BlueprintDslToolset` **refuses a macro library
outright** — "macro graphs have no Graph DSL representation"
(`BlueprintDslDomain.cpp:51-55`). The DSL can place a macro *instance* but cannot author a macro.

Idempotent — returns `True` if the macro already exists.

```python
import unreal

# The library asset itself is a factory call (BlueprintMacroFactory, NOT BlueprintFactory):
f = unreal.BlueprintMacroFactory()
f.set_editor_property("parent_class", unreal.Actor.static_class())
unreal.AssetToolsHelpers.get_asset_tools().create_asset("ML_Doors", "/Game/Macros", unreal.Blueprint, f)

# The macro graph inside it — this is the VibeUE tool.
unreal.BlueprintService.create_macro_graph("/Game/Macros/ML_Doors", "SwingOpen")
```

Place an instance from the DSL by **owner reference** — the library asset path, dot, the macro name
— the same shape the engine's own StandardMacros use:

```
(`/Game/Macros/ML_Doors.SwingOpen` door)
(`/Engine/EditorBlueprintResources/StandardMacros.Gate` :!via `Open` :!label _gate)
(`self.MyLocalMacro`)
```

Standard macros (`ForEachLoop`, `ForLoop`, `WhileLoop`, `DoOnce`, `DoN`, `Gate`, `IsValid`,
`FlipFlop`, `MultiGate`…) need no macro authoring at all — write `(for …)` / `(while …)` /
`(if …)` where the grammar has a statement for it, reference
`/Engine/EditorBlueprintResources/StandardMacros.<Name>` where it does not, or place one with
`BlueprintToolset.CreateNode` and the palette type id (`Utilities|FlowControl|ForLoop`). `IsValid`
is one node with both `Is Valid` and `Is Not Valid` exec outputs; there is no separate
`IsNotValid` macro.

### `analyze_graph_layout(blueprint_path, graph_name)` → (report_json, error)

Measures a graph's current layout **without changing it**. Epic's layout tools only *write* a
layout; nothing there reads one back, which is why this is still VibeUE's. The bool return is folded
away by UE Python, so the two out-strings come back as a tuple.

```python
import json, unreal
report, err = unreal.BlueprintService.analyze_graph_layout("/Game/BP/BP_Door", "EventGraph")
data = json.loads(report)
if data["issues"]:
    print(data["issues"])   # e.g. ["2 overlapping node pair(s)", "1 backward exec wire(s) — ..."]
```

The intended loop: author or arrange (`BlueprintDslToolset.Write`, or
`BlueprintToolset.ArrangeGraphLayered`) → `analyze_graph_layout` → fix the offenders
(`BlueprintToolset.ArrangeNodes` on just those nodes, or `SetNodePosition`) → re-check. Don't
hand-place coordinates first. Details and metric meanings: `node-layout.md`.

### `refresh_blueprint_editor(blueprint_path)` → bool

Forces the open Blueprint editor to re-run the construction script in its SCS viewport and redraw its
graphs. Use it after authoring changes the open editor does not reflect live. Returns `False` when no
editor is open for that Blueprint — which is not an error, there is simply nothing to refresh; the
on-disk asset is already current.

### Timelines (16 tools)

A Timeline is a `UK2Node_Timeline` node **plus** a `UTimelineTemplate` on the Blueprint, and no
engine tool touches a `UTimelineTemplate`. The full CRUD — `add_timeline`, `modify_timeline`,
`remove_timeline`, `get_timelines`, the four `add_timeline_*_track`, the four `add_timeline_*_key`,
`remove_timeline_key`, `remove_timeline_track`, `rename_timeline_track`,
`clear_timeline_track_keys` — is documented with signatures, pin names and sentinels in
**`node-reference.md`**.

Settings on the spawned `UTimelineComponent` are ordinary properties:
`EditorToolset.ObjectToolset.SetProperties`.

---

## Migration table — removed VibeUE tool → replacement

| Removed | Replacement |
|---|---|
| `build_graph` | `BlueprintDslToolset.Write` (whole document) or `.Edit` (targeted). `Docs()` first. |
| `get_graph_definition`, `get_graph_summary` | `BlueprintDslToolset.Read` (with a graph `Scope` for a summary) |
| `get_connections` | `BlueprintDslToolset.Read` — wires are explicit in the DSL |
| `get_nodes_in_graph` | `BlueprintToolset.FindNodes` + `GetNodeInfos`, or `BlueprintDslToolset.Read` |
| `discover_nodes` | `BlueprintToolset.FindNodes` / `FindNodeTypes` / `FindNodeCategories`, or `BlueprintDslToolset.FindNodeReferences` |
| `create_node_by_key`, `add_macro_instance_node`, `add_create_event_node`, `add_call_delegate_node` | `BlueprintToolset.CreateNode` with a palette type id, or a node form in a DSL `Write` |
| `delete_node` | `BlueprintToolset.DeleteNode` |
| `connect_nodes` | `BlueprintToolset.ConnectPins` |
| `disconnect_pin` | `BlueprintToolset.DisconnectPins` |
| `set_node_pin_value` / `get_node_pins` | `BlueprintToolset.SetPinValue` / `GetPinValue`, `GetNodeTypePins` |
| `set_node_position` | `BlueprintToolset.SetNodePosition` |
| `auto_layout_graph` | `BlueprintToolset.ArrangeGraphLayered` |
| `auto_layout_selected_nodes` | `BlueprintToolset.ArrangeNodes` |
| `get_selected_nodes`, `get_focused_graph_context` | the DSL is scope-addressed — select a `Scope` instead. For live editor selection, `EditorToolset.EditorAppToolset.GetAssetEditorSelection` (+ `GetOpenAssets` / `OpenEditorForAsset`). |
| `add_comment_node` / `add_comment_around_nodes` | `BlueprintToolset.AddCommentBox`, `ListCommentBoxes`, `GetCommentBoxText`, `SetCommentBoxText` |
| `add_custom_event_node`, `create_component_bound_event` | `BlueprintToolset.AddEvent` / `AddComponentBoundEvent`, or an `(event …)` entry in a DSL `Write` |
| `add_custom_event_input` / `modify_` / `remove_` / `get_custom_event_inputs` | the `(params …)` section of the DSL `(event …)` entry |
| `add_delegate_bind_on_variable`, `add_delegate_bind_node` | `BlueprintToolset.AddComponentBoundEvent` / `AddMatchingDelegateEvent`, or a DSL `add` statement with a `CreateDelegate` handler (see `graph-dsl.md`) |
| `add_function_parameter`, `create_function_graph` | `BlueprintToolset.AddFunctionParam` / `AddFunctionGraph`, or the DSL `(fn …)` `(params …)`/`(returns …)`/`(locals …)` sections |
| `list_overridable_functions` | `BlueprintToolset.ListEvents` / `ListFunctions` — both include inheritable members, with `is_implemented` as the old `already_overridden` |
| `override_function` | `BlueprintToolset.AddEvent` (event-style) / `AddFunctionGraph` (function-style), named after the inherited member |
| `list_variables`, `get_variable_info`, `add_member_variable`, `remove_member_variable` | `BlueprintToolset.ListVariables` / `AddVariable` / `AddObjectVariable` / `AddStructVariable` / `RemoveVariable` + the per-variable getters/setters |
| `set_variable_default_value`, `get_property` / `set_property` | `EditorToolset.ObjectToolset.SetProperties` / `GetProperties` on the CDO (`BlueprintToolset.GetDefaultObject`) |
| `compile_blueprint` | `BlueprintToolset.CompileBlueprint` |
| `get_blueprint_info` | `BlueprintToolset.ListGraphs` / `ListVariables` / `ListFunctions` / `GetParent` |
| `list_graphs`, `list_functions` | `BlueprintToolset.ListGraphs` / `ListFunctions` / `ListEvents` |
| `search_variable_types` | type strings are documented on `BlueprintToolset.AddFunctionParam` / `AddVariable`; `ObjectToolset.GetPropertySchema` for a live object's shape |

---

## Sample scripts (run via `execute_python`)

- **`scripts/build_event_graph.txt`** — the full DSL loop on a tiny graph: `Docs` → `Read` →
  `Write` → `Read`-back → compile → layout audit.
- **`scripts/timer_with_custom_event.txt`** — non-blocking Set Timer by Event with a custom-event
  callback, written as one DSL document, plus the stale-revision `Edit` retry.

## Sub-docs

Load the matching file from this same folder:

- **`graph-dsl.md`** — the DSL in depth: entries, statements, expressions, binds and multi-exec
  continuations, struct values, scopes and revisions, `Edit` recipes, error handling, `Diff`.
- **`node-reference.md`** — Timelines CRUD (the VibeUE survivors, with signatures and sentinels) and
  the DSL forms that replaced the old `member_get` / `member_set` / `validated_get` /
  custom-event-input node types.
- **`node-layout.md`** — arrange with Epic, measure with `analyze_graph_layout`, what each metric
  means, when to hand-place.
- **`function-classes.md`** — the owner names you put in a DSL `Owner.Member` reference
  (KismetMathLibrary, KismetSystemLibrary, KismetArrayLibrary, GameplayStatics…).
- **`array-operations.md`** — arrays in the DSL: `|MakeArray`, `(for item array …)`, the
  `KismetArrayLibrary` functions, element-type metadata on wildcard pins.
- **`workflows.md`** — taking an override, authoring a function with a body, Enhanced Input action
  events.

## Additional gotchas

- A DSL `Write` or `Edit` that **fails leaves the asset untouched** — there is no half-applied state
  to clean up. That is the opposite of the old batch builder, which left successful nodes behind on
  partial failure. Fix the document and resubmit rather than inspecting for strays.
- `(stale name)` reads a bound value from another exec scope in the same entry, and
  `` (stale `Entry` Name) `` from another entry. It changes no execution order and cannot be
  rebound — it is how `Read` represents a cross-chain data read, so preserve it on a round-trip.
- Preserve emitted metadata on a round-trip, especially `:!type` / `:!valuetype` container types and
  `:!label` goto targets. Values alone may not replay to the same type, and labels are graph-unique.
- `_` is a no-op statement; as an argument it leaves that pin unchanged, including an unspellable
  struct default or a by-reference parameter. Use it rather than inventing a value you cannot spell.
- An interface function spawns as a plain `K2Node_CallFunction` with an interface-typed self pin —
  feed self through a cast, or use the DSL's `` (message `Interface.Function` target args…) `` for a
  runtime-checked call. Some query functions are pure (no exec pins).
- Class, object, soft and interface pins take an **object path** (`/Script/Engine.StaticMeshActor`,
  `/Game/Meshes/SM_Cube.SM_Cube`); quote it in the DSL, because every bare non-literal atom is a
  local reference. `null` clears such a pin.
- Script-baked BP-CDO `TArray<TSubclassOf<...>>` defaults can be LOST on editor reload — read them
  back after a restart to confirm they persisted.
- `ListGraphs` legitimately returns `[]` for Anim Blueprints — use `AnimGraphService.list_graphs`.
  A Blueprint can hold several graph objects with the same `GraphName`; name-keyed code must reject
  the ambiguity rather than overwrite, and `FBlueprintGraphInfo.GraphPath` disambiguates.
- Everything returning empty across a long session is usually a stale editor session, not a
  per-asset fact — restart before diagnosing.

## Reference pages

Open these with the paths below; UE rewrites them to absolute paths when it serves this skill.

- [array-operations](array-operations.md) - Arrays in the Blueprint Graph DSL - MakeArray, for-each over an array, KismetArrayLibrary functions, element-type metadata on wildcard pins
- [function-classes](function-classes.md) - Owner names for a DSL `Owner.Member` reference - KismetMathLibrary, KismetSystemLibrary, KismetArrayLibrary, GameplayStatics and the other UE libraries
- [graph-dsl](graph-dsl.md) - The Blueprint Graph DSL in depth - entries, statements, expressions, binds, scopes, revisions, Write vs Edit, tombstones, Diff, error handling
- [node-layout](node-layout.md) - Blueprint graph layout - arrange with BlueprintToolset, measure with analyze_graph_layout, metric meanings, when to hand-place
- [node-reference](node-reference.md) - Timeline CRUD (VibeUE survivors) and the DSL forms that replaced member_get / member_set / validated_get / custom event input pins
- [workflows](workflows.md) - Step-by-step Blueprint graph workflows - taking an override, authoring a function body, Enhanced Input action events
- [scripts/build_event_graph.txt](scripts/build_event_graph.txt) - runnable sample
- [scripts/timer_with_custom_event.txt](scripts/timer_with_custom_event.txt) - runnable sample
