---
name: blueprint-graphs
display_name: Blueprint Graph Editing
description: Add, connect, and configure nodes in Blueprint event graphs and function graphs — function-call/event/branch/timer/custom-event/delegate nodes, Standard Macro nodes (ForEachLoop/ForLoop/WhileLoop/DoOnce/Gate/IsValid/FlipFlop via add_macro_instance_node), macro-graph creation, pin wiring, node layout, arrays, comment boxes, and the batch build_graph API. Use when the user asks to wire up Blueprint logic, add or connect nodes, add a loop/macro node, build an event graph, set up a timer/delay, broadcast a delegate, or lay out an existing graph.
vibeue_classes:
  - BlueprintService
unreal_classes:
  - EditorAssetLibrary
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

> **Also load `blueprints` skill** when creating new blueprints, adding variables/components, or overriding functions.
> This skill covers **node-level graph editing** — adding nodes, wiring pins, setting values, and layout.

## Critical Rules

### ⚠️ Method Name Gotchas

| WRONG | CORRECT |
|-------|---------|
| `list_nodes()` | `get_nodes_in_graph()` |
| `add_node()` / `add_function_call_node()` / `add_event_node()` | **batch:** VibeUE `build_graph()`; **single node:** engine `BlueprintTools.create_node` via `call_tool`; **editor-style spawner:** `create_node_by_key()` |
| `disconnect_nodes()` | `disconnect_pin()` |
| `get_node_connections()` | `get_connections()` |
| `get_graphs()` | `list_graphs()` |
| `get_functions()` | `list_functions()` |
| `get_connected_nodes()` | `get_node_details()` (pins include connections) or `get_node_pins()` + `pin.is_connected` |
| `unreal.find_class()` | `unreal.load_class(None, "/Script/Module.ClassName")` |

> ⚠️ **Engine `BlueprintTools` `call_tool` args are `{refPath}` objects, not strings.** Every
> `arguments={"blueprint": path, ...}` / `{"blueprint": bp_path}` below is shorthand — the engine
> toolset needs `{"refPath": "/Game/Dir/BP.BP"}` (full object path, `.AssetName` suffix required) and
> returns `{"returnValue": null}` on success. See the **blueprints** skill for the `bp_ref_of()` helper.
> VibeUE's own `unreal.BlueprintService.*` methods are unaffected — they take plain string paths.

> **Node creation moved to the engine toolset.** The old per-node convenience creators
> (`add_function_call_node`, `add_event_node`, `add_get_variable_node`, `add_set_variable_node`,
> `add_member_get_node`, `add_validated_get_node`) and the basics (`create_blueprint`,
> `compile_blueprint`, `add_variable`, `create_function`) are **gone** from `unreal.BlueprintService`.
> Create nodes one of three ways:
>
> 1. **Batch (preferred for 2+ nodes):** `unreal.BlueprintService.build_graph(...)` — the VibeUE
>    delta. Node `type` values (`function_call`, `event`, `variable_get`, `variable_set`, `branch`,
>    `print_string`, `member_get`, `validated_get`, …) live inside `build_graph`. See `build-graph.md`.
> 2. **Single node:** the engine `BlueprintTools.create_node` tool via `call_tool` (args `graph`,
>    `type_id`, `pos`):
>    ```python
>    call_tool(
>        tool_name="create_node",
>        toolset_name="editor_toolset.toolsets.blueprint.BlueprintTools",
>        arguments={"blueprint": bp_path, "graph": "EventGraph",
>                   "type_id": "Development|PrintString", "pos": [400, 0]})
>    ```
>    `type_id` examples: `'Development|PrintString'`, `'AddEvent|EventBeginPlay'`,
>    `'AddEvent|Custom|MyEvent'`, a function `'Utilities|...'`, a macro
>    `'Utilities|FlowControl|ForLoop'`, a dispatcher handler `'Default|EventDispatcherOnDamaged'`.
> 3. **Editor-style spawner key:** `create_node_by_key()` still works for deterministic creation
>    after `discover_nodes()`.
>
> The surviving wiring/inspection methods (`connect_nodes`, `disconnect_pin`, `get_connections`,
> `get_node_pins`, `set_node_pin_value`, `add_custom_event_node`, `add_macro_instance_node`,
> the timeline + custom-event-input CRUD, `build_graph`, `auto_layout_*`, etc.) are the VibeUE
> focus and remain unchanged. Engine basics — `BlueprintTools.create`, `.compile_blueprint`,
> `.add_variable`, `.add_function_graph` — also go through `call_tool` against that same toolset.

### ⚠️ `disconnect_pin()` Signature — 4 Args Only

`disconnect_pin` breaks **all** connections from a single named pin. Do **not** call it like `connect_nodes` (which takes 6 args):

```python
# CORRECT — 4 args: path, graph, node_id, pin_name
unreal.BlueprintService.disconnect_pin(bp_path, "EventGraph", custom_event_id, "then")

# WRONG — 6 args crashes with "takes at most 4 arguments (6 given)"
# unreal.BlueprintService.disconnect_pin(bp_path, graph, src_id, src_pin, tgt_id, tgt_pin)
```

To remove a specific edge, disconnect the output pin on the source node (e.g. `"then"`). Because the other end is the only connection on that exec pin, the single-pin disconnect is equivalent to removing the edge.

### ⚠️ Property Name Gotchas

| WRONG | CORRECT |
|-------|---------|
| `node.node_name` | `node.node_title` |
| `node.node_position_x` | `node.pos_x` |
| `node.node_position_y` | `node.pos_y` |
| `pin.direction` / `pin.is_output` | `pin.is_input` (bool) — the only direction field on `BlueprintPinInfo`; for outputs use `not pin.is_input` |
| `pin.is_linked` | `pin.is_connected` |
| `pin.current_value` | `pin.default_value` |
| `pin.sub_pins` | *(does not exist)* |

### ⚠️ `discover_nodes()` vs `get_nodes_in_graph()` — Different Object Types

`get_nodes_in_graph()` returns **`FBlueprintNodeInfo`** objects — these have `node_title`, `node_id`, `pos_x`, `pos_y`, `node_type`.

`discover_nodes()` returns **`FBlueprintNodeTypeInfo`** objects — these have **`display_name`** (NOT `node_title`), `spawner_key`, `category`, `tooltip`, `is_pure`, `is_latent`, `keywords`.

```python
# get_nodes_in_graph — use node_title
nodes = unreal.BlueprintService.get_nodes_in_graph(bp_path, graph)
for n in nodes:
    print(n.node_title, n.node_id)  # node_title is correct here

# discover_nodes — use display_name (NOT node_title)
matches = unreal.BlueprintService.discover_nodes(bp_path, "Broadcast")
for m in matches:
    print(m.display_name, m.spawner_key)  # display_name, NOT node_title
```

Also: search `discover_nodes` by **function name**, not variable type. To find the Broadcast Delegate node for a `StateTreeDelegate` variable, search `"Broadcast"` — NOT `"Dispatcher"` or `"StateTreeDelegate"`.

### 👀 Reading the User's Live Graph Selection — `get_selected_nodes()`

`get_nodes_in_graph()` returns **every** node in a graph. To act on just the nodes the user has highlighted in the open Blueprint editor, use `get_selected_nodes()` instead. Returned objects are the same `FBlueprintNodeInfo` shape as `get_nodes_in_graph()` — same `node_id`, `node_title`, `node_type`, `pos_x`, `pos_y`, `pin_names`, and `pins` fields — so you can pass `node.node_id` straight into `get_node_pins()`, `set_node_position()`, `delete_node()`, etc.

**The Blueprint MUST already be open in the editor.** Selection state lives on the Slate panel, not on the asset; closing the editor discards it. Calling `get_selected_nodes()` for a closed asset returns an empty array (and logs a warning).

```python
# Caller knows the asset
selected = unreal.BlueprintService.get_selected_nodes("/Game/BP_Player")

# Caller does NOT know which asset the user is looking at — empty path
# returns the selection from the first open Blueprint editor that has one.
selected = unreal.BlueprintService.get_selected_nodes("")

for n in selected:
    print(f"selected: {n.node_title} ({n.node_type}) @ ({n.pos_x}, {n.pos_y})")
    pins = unreal.BlueprintService.get_node_pins("/Game/BP_Player", "EventGraph", n.node_id)
```

Use this when the user says "this node", "the selected node(s)", "what I have highlighted", or "the one I'm looking at". An empty result means *nothing is selected in any open Blueprint editor* — ask the user to click a node in the graph rather than guessing.

### 🧭 "What am I looking at?" — `get_focused_graph_context()`

When the user says "this Blueprint", "the graph I have open", "the current material", or "what I'm looking at" **without naming the asset**, call `get_focused_graph_context()` first instead of guessing a path or chaining `list_graphs` → `get_selected_nodes`. It returns the asset **and** the focused graph in one shot — VibeUE's analogue of Epic's `GetDockedContext()`. It anchors on the globally-active editor tab (falling back to the first open supported editor).

It covers both **Blueprint-family editors** and the **Material editor**. Fields:

| Field | Notes |
|---|---|
| `found` | bool — `False` if no supported editor is open. **Note the stripped `b`** (UE Python: `bFound` → `found`). |
| `asset_path` | full object path — feed straight into `BlueprintPath` params |
| `asset_name` | short name, e.g. `"BP_Player"` |
| `editor_type` | `"BlueprintEditor"` / `"WidgetBlueprintEditor"` / `"AnimationBlueprintEditor"` / `"MaterialEditor"` |
| `graph_name` | focused graph tab, e.g. `"EventGraph"` — feed into `GraphName` params (Blueprints) |
| `graph_kind` | `"Ubergraph"` / `"Function"` / `"Macro"` / `"DelegateSignature"` / `"Material"` / `"Other"` |
| `graph_node_count` | node count of the focused graph |
| `selected_nodes` | same `FBlueprintNodeInfo` shape as `get_selected_nodes()` |

```python
ctx = unreal.BlueprintService.get_focused_graph_context()
if ctx.found and ctx.graph_kind != "Material":
    # asset_path + graph_name drop straight into the other Blueprint tools
    nodes = unreal.BlueprintService.get_nodes_in_graph(ctx.asset_path, ctx.graph_name)
    sel = [n.node_id for n in ctx.selected_nodes]
```

> **Materials:** when `graph_kind == "Material"`, the Blueprint graph tools (`get_nodes_in_graph`, `build_graph`) do **not** apply — use `MaterialNodeService` keyed by `ctx.asset_path`. `selected_nodes` is still populated (material expression nodes), but `graph_name` is the internal material-graph name, not a Blueprint graph tab.

`found == False` means no Blueprint/Material editor is open — ask the user to open the asset rather than guessing a path.

### 💬 Comment Boxes — `add_comment_node()` / `add_comment_around_nodes()`

Comment boxes are the coloured bubbles that visually group nodes (the editor shortcut is `C` after selecting nodes). Two APIs:

- **`add_comment_node(blueprint_path, graph_name, comment_text, pos_x, pos_y, width, height, r, g, b, a)`** — explicit position and size. Use when you already know exactly where you want the box.
- **`add_comment_around_nodes(blueprint_path, graph_name, comment_text, node_ids, padding, r, g, b, a)`** — computes the bounding box of the supplied nodes (plus `padding`, default 40 px) and pads room for the title bar. This is the programmatic equivalent of select-then-press-`C`.

#### ✍️ Write *informative* comment text — this is the whole point

Comment boxes are documentation that lives next to the code. A future reader (human or LLM) should be able to glance at the comment and understand **what this group of nodes accomplishes and why**, without re‑reading every pin. Treat the comment text the same way you'd treat a function header comment.

**Good comments** explain intent and behaviour:

- `"Pick a random patrol point and cache its world location for the Move To task"`
- `"On Damage > 0: play hit reaction, flash material, decrement Health"`
- `"Debounce input — ignore re‑presses within 0.25s of the last fire"`
- `"Early‑out when the owning controller is not locally controlled (server‑auth path)"`

**Bad comments** restate node titles or are filler:

- `"Print String"` ❌ (just the node name)
- `"Logic"` / `"Stuff"` / `"TODO"` ❌ (says nothing)
- `"Branch + Set Variable"` ❌ (mechanical, not intent)
- The user's verbatim request like `"add a comment around the nodes"` ❌ — that's the *task*, not the *meaning*

**Workflow**: before calling `add_comment_around_nodes`, inspect the selected nodes (titles, pin connections, variables they read/write) and synthesise a one‑line description of what the group does. If the user gave you a hint ("this is the patrol picker"), build on it; if they didn't, derive it from the nodes themselves. Use multiple lines (`\n`) when the group has a non‑trivial flow worth narrating.

```python
sel = unreal.BlueprintService.get_selected_nodes("/Game/BP_Player")
ids = [n.node_id for n in sel]

# Derive intent from the selected nodes (event, variables read/written, called
# functions) and write a comment that documents WHY this group exists.
comment = (
    "Pick a random patrol point on EnterState\n"
    "Caches the chosen point's world location into PatrolPointLocation\n"
    "so the subsequent Move To task has a stable target."
)

unreal.BlueprintService.add_comment_around_nodes(
    "/Game/BP_Player", "EventGraph", comment, ids)
```

Both functions return the new comment node's `node_id` (a GUID string), or `""` on failure. Colour parameters are RGBA in 0–1 range; the default is the standard pale yellow. Unknown `node_ids` passed to `add_comment_around_nodes()` are skipped with a warning — if *all* IDs are invalid, the call returns `""`.

Standard editor behaviour applies: any K2 nodes whose bounds fall inside the comment box get picked up and dragged with it (GroupMovement mode).

### ⚠️ Branch Node Pin Names

Use **`then`** and **`else`**, NOT `true`/`false`:

```python
# WRONG
connect_nodes(path, func, branch_id, "true", target_id, "execute")

# CORRECT
connect_nodes(path, func, branch_id, "then", target_id, "execute")
connect_nodes(path, func, branch_id, "else", target_id, "execute")
```

### ⚠️ UE5.7 Uses Doubles for Math

| WRONG | CORRECT |
|-------|---------|
| `Greater_FloatFloat` | `Greater_DoubleDouble` |
| `Add_FloatFloat` | `Add_DoubleDouble` |
| `MakeLiteralFloat` | `MakeLiteralDouble` |

The editor still *displays* "Make Literal Float", but its spawner is `FUNC KismetSystemLibrary::MakeLiteralDouble`. A `build_graph` `function_call` node (or engine `create_node`) targeting `"KismetSystemLibrary", "MakeLiteralFloat"` resolves to nothing silently — use `MakeLiteralDouble`, or `discover_nodes(bp, "Make Literal Float")` and spawn via the returned `spawner_key`.

### ⚠️ Compile Before Using Variables in Nodes

The variable must exist on the compiled skeleton class before a getter/setter can resolve it.
Add the variable and compile through the engine `BlueprintTools` toolset, then create the
`variable_get` node (here via `build_graph`):

```python
# Engine basics go through call_tool / BlueprintTools
call_tool(tool_name="add_variable",
          toolset_name="editor_toolset.toolsets.blueprint.BlueprintTools",
          arguments={"blueprint": path, "name": "Health", "type": "float", "default": "100.0"})
call_tool(tool_name="compile_blueprint",
          toolset_name="editor_toolset.toolsets.blueprint.BlueprintTools",
          arguments={"blueprint": path})  # REQUIRED before adding nodes

# Now the getter resolves — VibeUE build_graph
unreal.BlueprintService.build_graph(path, func,
    [{"ref": "GetHP", "type": "variable_get", "params": {"variable": "Health"}}],
    [], [], True, True)
```

### ⚠️ Verify Pins Immediately After Adding Variable Get/Set Nodes

A variable Get/Set node can occasionally be created with **zero pins** (the variable property
failed to resolve on the skeleton class at spawn time). The create call still returns a GUID, so
you won't notice until every `connect_nodes` against it returns `False`. After creating a
variable node, check its pins before wiring anything (create it via `build_graph`'s
`variable_get`; `ref_to_node_id` maps your ref back to the GUID):

```python
r = unreal.BlueprintService.build_graph(bp_path, graph,
    [{"ref": "Get", "type": "variable_get", "params": {"variable": "Armor"}}],
    [], [], False, False)
gid = r.ref_to_node_id["Get"]
pins = unreal.BlueprintService.get_node_pins(bp_path, graph, gid)
if not pins:  # corrupt node — recover
    unreal.BlueprintService.delete_node(bp_path, graph, gid)
    call_tool(tool_name="compile_blueprint",
              toolset_name="editor_toolset.toolsets.blueprint.BlueprintTools",
              arguments={"blueprint": bp_path})
    r = unreal.BlueprintService.build_graph(bp_path, graph,
        [{"ref": "Get", "type": "variable_get", "params": {"variable": "Armor"}}],
        [], [], False, False)
    gid = r.ref_to_node_id["Get"]
    assert unreal.BlueprintService.get_node_pins(bp_path, graph, gid)
```

Recovery notes:
- `delete_node` can return `False` here — confirm with a fresh `get_nodes_in_graph()` and don't
  leave a stray pin-less duplicate behind (two nodes with the same title, one never connected).
- Compiling between delete and re-add refreshes the skeleton class so the retry resolves.
- This is another reason to create **one node at a time** instead of batching 4–6 creates in one call.

### ⚠️ The Result Node's `execute` Pin MUST Be Wired — the Warning Is Not Minor

Compile warning **`ReturnNode Exec pin has no connections`** means the execution chain never
reaches the Return Node, so the function's **output parameters are never set** — callers always
get default values. The function is broken even though `compile.success` is `True`.

Every non-pure function needs an exec path `Entry.then → ... → Result.execute`. For a
"compute-only" function (e.g. read a variable, compare, return bool), wire the entry **directly**
to the result — connecting only the data pin is not enough:

```python
# CheckDeath: IsDead = (Health <= 0)
connect_nodes(bp, "CheckDeath", entry_id, "then", result_id, "execute")        # exec — REQUIRED
connect_nodes(bp, "CheckDeath", lessequal_id, "ReturnValue", result_id, "IsDead")  # data
```

Do not dismiss this warning in your summary or claim the function "works". Treat any
`ReturnNode` compile warning as a wiring bug and fix it before reporting success. (If the
function has no side effects, the cleaner fix is making it **pure**, but wiring the exec chain is
the minimal correct repair.)

### ⚠️ Node IDs Are GUID Strings, Not Small Integers

Do **not** assume Blueprint nodes have numeric IDs like `0` or `1`.
`get_nodes_in_graph()` returns **GUID strings** in `node.node_id`.

```python
nodes = unreal.BlueprintService.get_nodes_in_graph(bp_path, graph)
for node in nodes:
    print(node.node_id, node.node_title)
```

For function graphs, find nodes by `node_type` or `node_title`.
For event-style overrides, find nodes by the **display title Unreal shows in the graph**, not by the raw override function name.

### ⚠️ Event-Style Overrides Use `"EventGraph"` — NOT the Function Name

When an override is **event-style** (void/latent functions like `ReceiveTreeStart`, `ReceiveTick`,
`ReceiveLatentEnterState`, `ReceiveStateCompleted`), the event node lives inside `"EventGraph"`.
Do **NOT** use the function name as the graph name — that graph doesn't exist.

```python
# WRONG — returns 0 nodes, creates nothing
nodes = unreal.BlueprintService.get_nodes_in_graph(bp_path, "ReceiveTreeStart")
# ...and creating into "ReceiveTreeStart" (build_graph or create_node) goes nowhere

# CORRECT — event-style overrides are in EventGraph
nodes = unreal.BlueprintService.get_nodes_in_graph(bp_path, "EventGraph")
unreal.BlueprintService.build_graph(bp_path, "EventGraph",
    [{"ref": "Call", "type": "function_call",
      "params": {"class": "KismetSystemLibrary", "function": "PrintString"}}],
    [], [], True, True)
```

Common event-style overrides and their **graph name → node title**:

| Raw Function Name | Graph Name | Node Title in Graph |
|---|---|---|
| `ReceiveTreeStart` | `"EventGraph"` | `Event TreeStart` |
| `ReceiveTick` | `"EventGraph"` | `Event Tick` |
| `ReceiveTreeStop` | `"EventGraph"` | `Event TreeStop` |
| `ReceiveLatentEnterState` | `"EventGraph"` | `Event EnterState` |
| `ReceiveLatentTick` | `"EventGraph"` | `Event Tick` |
| `ReceiveStateCompleted` | `"EventGraph"` | `Event StateCompleted` |

Only **function-style** overrides (non-void, like `GetStaticDescription`) create a separate function graph.

### ⚠️ Do Not Guess Blueprint Function Node Names

Blueprint display titles and internal UFunction names are often **not the same**.

Common examples:

| Display title in graph | Internal callable name may be |
|---|---|
| `Get Actor Location` | `K2_GetActorLocation` |
| `Set Actor Location` | `K2_SetActorLocation` |
| `VInterp To` | `VInterpTo` |

If you do not already know the exact callable name, do **not** guess and do **not** batch multiple node creations into one shot. Use one of these two patterns:

1. `discover_nodes()` + `create_node_by_key()` for deterministic editor-style creation.
2. A `build_graph` `function_call` node (or engine `BlueprintTools.create_node`) only after discovery, or when you already know the exact function name.

```python
import unreal

bp_path = "/Game/StateTree/Tasks/STT_Chase"
graph = "EventGraph"

matches = unreal.BlueprintService.discover_nodes(bp_path, "Get Actor Location")
actor_get_location = next(
    node for node in matches
    if node.spawner_key.startswith("FUNC AActor::") or node.spawner_key.startswith("FUNC Actor::")
)

node_id = unreal.BlueprintService.create_node_by_key(
    bp_path, graph, actor_get_location.spawner_key, 400, 0)
assert node_id, actor_get_location.spawner_key
```

If a node-create call returns an empty ID, stop immediately. Re-read the graph, inspect the error output, and fix the lookup before creating anything else.

### ⚠️ Standard Macro nodes (ForEachLoop, etc.) — do NOT use `create_node_by_key`

Macro instances (`K2Node_MacroInstance`) have **no spawner key**, so `discover_nodes()` won't find them and `create_node_by_key()` **fails silently** (returns empty, no error). Use the dedicated method instead:

```python
import unreal
node_id = unreal.BlueprintService.add_macro_instance_node(bp_path, graph, "ForEachLoop", x, y)
```

Supported shorthands:

| Shorthand | Engine macro graph |
|---|---|
| `ForEachLoop` | StandardMacros:ForEachLoop |
| `ForEachLoopWithBreak` | StandardMacros:ForEachLoopWithBreak |
| `ReverseForEachLoop` | StandardMacros:ReverseForEachLoop |
| `ForLoop` | StandardMacros:ForLoop |
| `ForLoopWithBreak` | StandardMacros:ForLoopWithBreak |
| `WhileLoop` | StandardMacros:WhileLoop |
| `IsValid` | StandardMacros:IsValid |
| `Gate` | StandardMacros:Gate |
| `DoOnce` | StandardMacros:DoOnce |
| `DoN` | StandardMacros:Do N |
| `FlipFlop` | StandardMacros:FlipFlop |

A full `AssetPath:GraphName` string is also accepted for user-defined macro libraries (e.g. `/Game/Macros/ML_X.ML_X:MyMacro`).

Notes:
- `IsValid` places **one** node exposing both `Is Valid` and `Is Not Valid` exec outputs — there is no separate `IsNotValid` macro.
- `MultiGate` is `K2Node_MultiGate` (a distinct node type), not a StandardMacro — it is not in this list.
- To create a macro graph on a `MacroLibrary` Blueprint, use `create_macro_graph(lib_path, "MacroName")` (the engine `BlueprintTools.add_function_graph` path asserts on MacroLibrary BPs). To create the **library asset** itself from Python (issue #449):
  ```python
  f = unreal.BlueprintMacroFactory()   # NOT BlueprintFactory
  unreal.AssetToolsHelpers.get_asset_tools().create_asset("ML_X", "/Game/Macros", unreal.Blueprint, f)
  unreal.BlueprintService.create_macro_graph("/Game/Macros/ML_X", "MyMacro")
  # reference it from a graph: add_macro_instance_node(bp, graph, "/Game/Macros/ML_X.ML_X:MyMacro", x, y)
  ```

### 🔎 Calling a function (self / parent / library) — discover the spawner key (#449)

`build_graph`'s `function_call` works for library statics, but the **most reliable** way to add a
call to a **self** function, a **parent-class** function (Character/Pawn/Actor…), or any discoverable
function is `discover_nodes` → `create_node_by_key`:

```python
# Find the function — self functions show category "Self Functions"; parents show "FUNC Parent::Func"
hits = unreal.BlueprintService.discover_nodes(bp, "Jump")        # search_term (NOT a graph name)
key  = hits[0].spawner_key                                       # e.g. "FUNC Character::Jump"
node = unreal.BlueprintService.create_node_by_key(bp, "EventGraph", key, 400, 200)
```

`discover_nodes(blueprint_path, search_term, category="", max_results=20)` surfaces the blueprint's
OWN functions (your custom functions, callable on self) plus the full parent hierarchy and library
functions. The returned `spawner_key` (`"FUNC <Class>::<Func>"`) feeds straight into
`create_node_by_key` — this is how you add a **self-call** node deterministically.

#### Full node coverage — every action-menu node, not just functions/events

`discover_nodes` enumerates the **entire** Blueprint action database, so it also returns template/custom
K2 nodes and variable get/set on other classes — e.g. **Get Subsystem** (`Get EnhancedInputLocalPlayerSubsystem`,
`Get GameInstanceSubsystem`, …), **Cast To** nodes, etc. These come back with a faithful
**`SPAWN <NodeClass>|<MenuName>`** key:

```python
# Get Enhanced Input Local Player Subsystem, fully bound (ReturnValue typed to the subsystem)
hits = unreal.BlueprintService.discover_nodes(bp, "EnhancedInputLocalPlayerSubsystem", "", 100)
key  = next(h.spawner_key for h in hits if h.node_class == "K2Node_GetSubsystem")
nid  = unreal.BlueprintService.create_node_by_key(bp, "EventGraph", key, 0, 0)
```

`create_node_by_key` resolves a `SPAWN` key back to the exact action-database spawner and **invokes**
it — the same path the editor's Add-Node menu uses — so the node is created fully bound (Get Subsystem's
`CustomClass`, a variable's member reference, …), not as a blank node.

> **Search specifically.** Node menu names are **spaceless** (`EnhancedInputLocalPlayerSubsystem`, not
> "Enhanced Input …"), and a broad term like `"Subsystem"` matches hundreds of function nodes that can
> crowd out the template nodes under `max_results`. Search the exact spaceless name (or raise
> `max_results`) and filter by `node_class` (`K2Node_GetSubsystem`, `K2Node_GetSubsystemFromPC`,
> `K2Node_GetEngineSubsystem`, `K2Node_GetEditorSubsystem`).

### ⚠️ Complex Graphs: Create and Verify One Node at a Time

For fragile graph work such as `STT_*` task Blueprints, timers, delegate workflows, or any graph being built from a screenshot:

1. Create **one** node.
2. Re-read the graph and confirm the returned GUID exists.
3. Read that node's pins.
4. Only then create the next node.
5. After all required nodes exist, connect **one** edge at a time and verify the new connection appears in `get_connections()`.

Do **not** create 4-6 nodes in one tool call when some of them have unresolved function names. That makes cleanup harder and leaves duplicate nodes behind on retry.

### ⚠️ Recovery After a Failed Graph Attempt

If a graph-edit attempt fails partway through:

1. Call `get_nodes_in_graph()` and `get_connections()` to see the true post-failure state.
2. Remove orphaned nodes with `delete_node()`.
3. Remove stale links with `disconnect_pin()`.
4. Retry from a clean baseline.

Do **not** invent helper names like `disconnect_all_pins()`. Use the real APIs that exist.

Examples for StateTree task blueprints:

| Override created | Typical graph title |
|---|---|
| `ReceiveLatentEnterState` | `Event EnterState` |
| `ReceiveLatentTick` | `Event Tick` |
| `ReceiveStateCompleted` | `Event StateCompleted` |

### ⚠️ Delay vs Set Timer by Event

When a user asks for a "timer" or "timed delay" in a Blueprint, **use `Set Timer by Event`**, NOT `Delay`:

| Node | Behavior | Use When |
|------|----------|----------|
| `Delay` | **Latent action** — blocks execution flow on that path | Simple linear sequences where nothing else runs during the wait |
| `Set Timer by Event` | **Non-blocking** — fires a delegate callback after the duration | The Blueprint must keep ticking/running during the wait (e.g. State Tree tasks that rotate while waiting, animation tasks, any gameplay that shouldn't freeze) |

The pattern for `Set Timer by Event` requires a **Custom Event**:

Important: after `add_custom_event_node(...)`, immediately re-read the graph and re-find that callback by the returned `node_id`. Do **not** assume the title will be exactly the event name you passed in. Unreal can display custom event titles as multi-line labels such as `OnTimerFinished` + `Custom Event`.

Do **not** silently substitute a `Create Event` / `Create Delegate` node plus a separate function graph when the user asked for a `Custom Event` node or when the target graph should visibly contain the callback in `EventGraph`. A `Create Event` delegate can compile and still be the wrong implementation for the requested graph shape.

```python
import unreal

bp_path = "/Game/MyBlueprint"
graph = "EventGraph"

# 1. Add Set Timer by Event node (build_graph function_call; ref_to_node_id gives the GUID)
r = unreal.BlueprintService.build_graph(bp_path, graph,
    [{"ref": "Timer", "type": "function_call",
      "params": {"class": "KismetSystemLibrary", "function": "K2_SetTimerDelegate"}}],
    [], [], False, False)
timer_id = r.ref_to_node_id["Timer"]

# 2. Set the Time pin
unreal.BlueprintService.set_node_pin_value(bp_path, graph, timer_id, "Time", "1.0")

# 3. Add a Custom Event node — this is the callback
custom_event_id = unreal.BlueprintService.add_custom_event_node(
    bp_path, graph, "OnTimerFinished", 300, 300)

# 4. Connect the Custom Event's delegate output to the timer's Delegate pin
unreal.BlueprintService.connect_nodes(
    bp_path, graph, custom_event_id, "OutputDelegate",
    timer_id, "Delegate")

# 5. Wire: EnterState → Set Timer by Event
unreal.BlueprintService.connect_nodes(
    bp_path, graph, enter_state_id, "then", timer_id, "execute")

# 6. Wire: Custom Event → Finish Task (or whatever follows)
unreal.BlueprintService.connect_nodes(
    bp_path, graph, custom_event_id, "then", finish_id, "execute")
```

### ⚠️ StateTree Task Timer Workflow: Always Discover Real Titles and Pins First

For `STT_*` Blueprint tasks, do **not** guess event titles or pin names. The stable pattern is:

1. `override_function(bp, "ReceiveLatentEnterState")`
2. `get_nodes_in_graph(bp, "EventGraph")` and locate the event by title `Event EnterState`
3. After `add_custom_event_node(...)`, call `get_nodes_in_graph()` again and re-find the callback by the returned GUID
4. Treat the callback title as display-only evidence; custom event titles can be multi-line and should not be your primary lookup key
5. `get_node_pins()` on every newly created node
6. Wire using the **actual pin names returned by the graph**

Current UE 5.7 / VibeUE graph details for this workflow:

| Node | Pin to use |
|---|---|
| `Event EnterState` | `then` |
| `Custom Event` | `OutputDelegate`, `then` |
| `Set Timer by Event` | `execute`, `Delegate`, `Time`, `bLooping`, `bMaxOncePerFrame` |
| `Finish Task` | `execute`, `bSucceeded` |

Success for this workflow also requires the callback node in `EventGraph` to be a real custom event node, typically `K2Node_CustomEvent` in a fresh node listing. `K2Node_CreateDelegate` is a different node type and should not be treated as equivalent when the requested outcome is a visible custom event callback in the graph.

```python
import unreal

bp_path = "/Game/StateTree/STT_Rotate"
graph = "EventGraph"

unreal.BlueprintService.override_function(bp_path, "ReceiveLatentEnterState")

# Timer + Finish Task are function_call nodes — create them via build_graph;
# the Custom Event callback uses the surviving add_custom_event_node.
r = unreal.BlueprintService.build_graph(bp_path, graph,
    [{"ref": "Timer", "type": "function_call",
      "params": {"class": "KismetSystemLibrary", "function": "K2_SetTimerDelegate"}},
     {"ref": "Finish", "type": "function_call",
      "params": {"class": "StateTreeTaskBlueprintBase", "function": "FinishTask"}}],
    [], [], False, False)
timer_id = r.ref_to_node_id["Timer"]
finish_id = r.ref_to_node_id["Finish"]
custom_id = unreal.BlueprintService.add_custom_event_node(
    bp_path, graph, "OnTimerFinished", 0, 420)

nodes = unreal.BlueprintService.get_nodes_in_graph(bp_path, graph)
enter_node = next((n for n in nodes if n.node_title == "Event EnterState"), None)
custom_node = next((n for n in nodes if n.node_id == custom_id), None)
assert enter_node, "Event EnterState not found"
assert custom_node, f"Custom event {custom_id} not found after create"

custom_pins = unreal.BlueprintService.get_node_pins(bp_path, graph, custom_node.node_id)
print("CUSTOM EVENT TITLE:", custom_node.node_title)
print("CUSTOM EVENT PINS:", [p.pin_name for p in custom_pins])

unreal.BlueprintService.set_node_pin_value(bp_path, graph, timer_id, "Time", "1.0")
unreal.BlueprintService.set_node_pin_value(bp_path, graph, timer_id, "bLooping", "false")
unreal.BlueprintService.set_node_pin_value(bp_path, graph, finish_id, "bSucceeded", "true")

assert unreal.BlueprintService.connect_nodes(bp_path, graph, enter_node.node_id, "then", timer_id, "execute")
assert unreal.BlueprintService.connect_nodes(bp_path, graph, custom_node.node_id, "OutputDelegate", timer_id, "Delegate")
assert unreal.BlueprintService.connect_nodes(bp_path, graph, custom_node.node_id, "then", finish_id, "execute")
```

Use `add_create_event_node()` only when you need a **Create Event / Create Delegate** node. For `Set Timer by Event`, a `Custom Event` node is the simpler callback source and matches the Blueprint editor workflow shown in screenshots.

If a previous attempt already inserted a `Create Event` node for this timer callback, treat that as the wrong graph shape when the request is for a custom event. Remove the wrong node, remove any stale callback function graph that only existed to support that delegate node, then create the real `Custom Event` node and verify it by node ID and node type.

### ⚠️ Verification Is Mandatory Before Claiming Success

After any graph edit, verify all three layers:

1. **Connections**: call `get_connections()` and confirm the exact expected wiring.
2. **Pins**: if a connection fails, call `get_node_pins()` and use the real pin names.
3. **Compile**: inspect the engine `BlueprintTools.compile_blueprint` result's `success`, `num_errors`, and `errors`.

For any node you claim you created, also re-read the graph with `get_nodes_in_graph()` and confirm that node actually exists in the graph after the edit. A returned node ID from a create call is not enough.

For `Custom Event` timer callbacks, verify both of these before wiring:

1. The returned GUID appears in a fresh `get_nodes_in_graph()` result.
2. `get_node_pins()` on that exact node shows the pins you intend to use, typically `OutputDelegate` and `then` for the callback path.

Also verify that the node type is the expected custom event form rather than `K2Node_CreateDelegate`.

```python
# compile goes through the engine BlueprintTools toolset
result = call_tool(tool_name="compile_blueprint",
                   toolset_name="editor_toolset.toolsets.blueprint.BlueprintTools",
                   arguments={"blueprint": bp_path})
assert result["success"], result.get("errors")

nodes = unreal.BlueprintService.get_nodes_in_graph(bp_path, graph)
for node in nodes:
    print(f"NODE {node.node_id} {node.node_title}")

connections = unreal.BlueprintService.get_connections(bp_path, graph)
for conn in connections:
    print(f"{conn.source_node_title}.{conn.source_pin_name} -> {conn.target_node_title}.{conn.target_pin_name}")

unreal.EditorAssetLibrary.save_asset(bp_path)
```

Never print `MODIFIED` or describe the graph as complete until the expected connections are present and compile succeeds.
Never describe a node as present until it appears in a fresh `get_nodes_in_graph()` result.
Never treat compile success alone as proof that the graph matches the requested design.

### Required Success Gate For Graph Edits

If you edit a graph, your success check must answer all of these from live asset data:

1. Which required node titles are present?
2. Which exact connection lines prove the intended wiring exists?
3. Did compile succeed with zero relevant errors?

If you cannot answer those three questions from tool output, the task is not complete yet.

For complex graph tasks, also answer these two questions before claiming completion:

4. Which node IDs were created successfully, one by one?
5. What cleanup was done after any failed intermediate attempt?

**Common mistake**: Using `Delay` when the user says "timer". `Delay` is a latent action that
pauses the execution chain. `Set Timer by Event` is non-blocking and fires a separate event —
this is critical for State Tree tasks, animation blueprints, and any actor that must keep
ticking during the wait period.

---

## Sample scripts (run via `execute_python_code`)

- **`scripts/build_event_graph.txt`** — wire Event BeginPlay → Print String, verify connection, compile.
- **`scripts/timer_with_custom_event.txt`** — non-blocking Set Timer by Event + Custom Event callback.

## Sub-docs Available

This skill index contains the frontmatter, intro, and the **Critical Rules / gotchas** section. For everything else, load the matching sub-doc by reading its file under this same folder (`Plugins/VibeUE/Content/Skills/blueprint-graphs/`):

- **`node-reference.md`** — Reference for specific node APIs that are part of Critical Rules but read as reference material: the `member_get` / `validated_get` build_graph node types (pin tables), Custom Event Input Pins CRUD (`*_custom_event_input`), and Timelines CRUD (`*_timeline*`).
- **`workflows.md`** — Step-by-step workflows: overriding a parent function (`override_function`), creating a function with logic, adding an Enhanced Input Action node.
- **`node-layout.md`** — Node layout best practices: layout constants, execution flow, data flow above execution, branch layout, repositioning Entry/Result nodes.
- **`function-classes.md`** — Quick reference for the common class names you pass to a `build_graph` `function_call` node / engine `create_node` (KismetMathLibrary, KismetSystemLibrary, KismetArrayLibrary, GameplayStatics, etc.).
- **`array-operations.md`** — Array operations on wildcard pins: `Array_Random`, available array functions, `K2Node_GetArrayItem`, wildcard pin type propagation, common array mistakes.
- **`build-graph.md`** — The batch `build_graph` API: when to use it, node types, connection format, examples (BeginPlay → PrintString, Branch with Math, StateTreeDelegate Broadcast), round-trip export/rebuild, auto-layout, Make Struct / Make Instanced Struct, error handling.
