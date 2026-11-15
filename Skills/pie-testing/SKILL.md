---
name: pie-testing
display_name: Play-In-Editor Testing
description: Start, stop, and query Play-In-Editor (PIE) sessions for runtime testing of Blueprints, gameplay logic, widgets, AI, and any in-game behavior, and drive the running game's UI - find the PIE window, snapshot it, click, drag, type, wait for and read live UMG widgets with Epic's SlateInspectorToolset and WidgetFinder. Use when the user asks you to "play", "test", "run", "PIE", "start/stop the game", "click the button", "drag the item", "type into the field", "playtest the menu", or otherwise needs a live game world to validate changes.
vibeue_classes:
  - WidgetService
  - InputService
  - PerformanceService
  - PIEActorService
unreal_classes:
  - UEditorEngine
  - FRequestPlaySessionParams
engine_toolsets:
  - EditorToolset.EditorAppToolset
  - AutomationTestToolset
  - ConsoleToolset
  - InputToolset
  - SceneCaptureToolset
  - SlateInspectorToolset
keywords:
  - pie
  - play in editor
  - play
  - run
  - test
  - testing
  - simulate
  - gameplay
  - runtime
  - start game
  - stop game
  - end play
  - click
  - drag
  - type
  - live ui
  - playtest
---

> 🧠 **Brains complement:** IF an `unreal-engine-skills-manager` tool (external MCP) exists in this session, call it with `{action: "load", skill: "automation-and-testing"}` for UE domain knowledge on this topic — correct APIs, architecture, best practices — and treat it as the rubric for any review / "best practices" question. If no such tool is available (e.g. running under Claude Code or Codex without that MCP), skip this line entirely and proceed with this skill alone — do NOT attempt the call.

# Play-In-Editor (PIE) Testing Skill

PIE is the only way to validate runtime behavior — Blueprint logic, AI ticking, animation, input, widget interaction, gameplay events. Without starting PIE, your "fix" is unverified.

**Tool names.** A tool is `<toolset>.<tool>`: over MCP that is `call_tool(toolset_name="SlateInspectorToolset.SlateInspectorToolset", tool_name="Click", arguments={"ref": "q_1"})`; inside `execute_python` it is one string, `await call_tool("SlateInspectorToolset.SlateInspectorToolset.Click", ref="q_1")`, which returns the tool's value directly. Arguments are the tool's camelCase schema names (`maxDepth`, `startRef`). A bare toolset name (`"SlateInspectorToolset"`, `"AutomationTestToolset"`) is not found; check a name with `get_symbol` or `search_symbols` (`kinds=["TOOL"]`).

## Drive a running UI: find, snapshot, click, drag, type, wait, read

Epic ships the pointer, keyboard and lookup surface for a live UI. Do not write your own PIE UI
driver or an OS-level mouse script first. `S` below is `SlateInspectorToolset.SlateInspectorToolset`
and `WF` is `WidgetFinder.WidgetFinderSubsystem`. SlateInspectorToolset is tools only (AICallable):
there is no `unreal.SlateInspectorToolset.click(...)`; call it as a tool. The same steps, with the
UMG side, are in `umg-widgets`.

Load Epic's two guides once (they are Python-registered skills, so their paths are object paths, not
`/Skills/...`; confirm them with `ToolsetRegistry.AgentSkillToolset.ListSkills`):

- `/EditorToolset/Python/editor_toolset/skills/debug_runtime_ui_bug_PY.DebugRuntimeUIBugSkill`:
  reproduce and classify a runtime UI bug (missing, layout, stale, focus, softlock, z-order, ...), with
  the recorders to arm first (`UMGEditor.UMGDebuggerSubsystem` widget lifetime log, CommonUI log, focus recording).
- `/SlateInspectorToolset/Python/slate_inspector_toolset/skills/input_simulation_PY.SlateInspectorInputSimulationSkill`:
  use the one-call tools (`Click`, `PressKey`, `Type`, `Drag`, `Scroll`), which leave nothing held; the
  split tools (`MouseDown`/`MouseUp`, `KeyDown`/`KeyUp`, `TouchStart`/`TouchEnd`) are for debugging one
  half of an interaction, and `ReleaseHeldInput` after an interrupted one.

| Step | Call (camelCase arguments) | What to know |
|---|---|---|
| Start | `EditorToolset.EditorAppToolset.StartPIE` with `options={"playMode": "PlayMode_InEditorFloating", "warmupSeconds": 2}` | A floating window has a size you control and a findable title. Turn background throttling off first (below). Errors if a session is already active or queued; 30 s limit. |
| Find the window | `S.Windows` with `action="list"` | A JSON **string** of `{index, ref, title, active, regular, x, y, width, height}`; the PIE window's title contains `Preview`. `action="resize"` with `ref`, `width`, `height` resizes it (outer size, frame included). |
| Locate a widget | `WF.StartService` once, `WF.GetQuerySyntax` once (read the grammar), then `WF.Query` with `request={"query": "text(*PLAY*)"}` | The argument is `request`, not `query`. Text matches exactly unless it globs: `text(PLAY)` misses a "PLAY GAME" label, `text(*PLAY*)` finds it. Each result has `ref`, `type`, `name`, `bVisible`, `x`, `y`, `width`, `height`; check `bSuccess` / `errorMessage` first. |
| ... or snapshot | `S.Snapshot` with `ref=<window ref>`, `maxDepth=60` | Text, one widget per line: `[pos=x,y size=w,h]` in desktop pixels, `[umg=WBP_Title.Btn_Play(Button)]` naming the UMG widget and its Widget Blueprint, `[ref=...]`. Collapsed and Hidden widgets are left out: a missing line means not visible, not absent. Find a widget by its UMG name in the `umg=` tag. |
| Click | `S.Click` with `ref`, optional `params={"button": "left", "bDoubleClick": false, "position": {"x": 0.5, "y": 0.5}}` | `true` means delivered, not handled: confirm with a capture or a state read. On `false`, `S.LastRefusal` says why (covered, unarranged, stale ref). Clicking a label's ref presses the button under it. |
| Drag | `S.Drag` with `startRef`, `endRef`, optional `params` (`startPosition`, `endPosition` normalized 0..1 in each widget, `waypoints={"points": [{"x": .., "y": ..}]}`, `moveByX` / `moveByY`, `button`, `modifiers`) | Hit-tests both ends, then presses, moves in steps (so drag detection fires and the target gets `OnDragOver`) and releases. Empty `endRef` plus `moveByX`/`moveByY` drags the widget itself: sliders, splitters, scrollbars. Read in 5.9 source; not yet run against a UMG drag-and-drop. |
| Type, keys | `S.Type` with `ref`, `text`, `submit`; `S.PressKey` with `key` (`"Escape"`, `"Ctrl+A"`, `"Gamepad_FaceButton_Bottom"`); `S.Focus` with `ref` | `Type` focuses the field first. In CommonUI a gamepad key press is what switches to gamepad mode. |
| Hover, scroll | `S.Hover` with `ref`; `S.Scroll` with `ref`, `params={"ticks": -3}` | `Hover` moves the real cursor. |
| Is it clickable? | `S.HitTest` with `ref`, `params={"grid": 3}` | Says whether pointer input reaches the widget and what covers it, without changing anything. |
| Wait | `WF.WaitForQuery` with `request={"query": "...", "comparison": "GreaterOrEqual", "threshold": 1, "timeoutSeconds": 10}` (`"Equal"` + `0` waits for it to go) | Completes once, without polling; a timeout still counts as success (`bTimedOut`). `S.WaitFor` with `text` / `textGone` only checks once: poll it. |
| Read | `S.GetUMGInfo` with `ref`, then `EditorToolset.ObjectToolset.GetProperties` with `instance=<its widget>`, `properties=["Text", "Visibility"]` | `GetUMGInfo` returns `widget` (the live UWidget), `widgetName`, `widgetClass`, `asset`, `resolvedVia`, `outerUserWidgets`, `error`. |
| Look | `S.Screenshot` with `ref` (a widget or window); `SceneCaptureToolset.SceneCaptureToolset.CaptureGameViewport` with `world`, `maxResolution` | Over MCP both return an image block. In a script the capture is base64: write it to a PNG and return the path, never the base64. |
| Clean up | `S.ReleaseHeldInput`, `WF.StopService`, then `StopPIE` | Refs to widgets of an ended PIE session are purged and fail fast. |

What ran in a 2026-11 jam: `Windows` (list, resize), `Snapshot`, `Hover`, `Screenshot` from inside
`execute_python`; `WF.StartService` -> `GetQuerySyntax` -> `Query` -> `S.Click` over MCP (the click
opened the page). The other rows are read in the 5.9 source.

**`on_clicked.broadcast()` is not a click.** It runs the handlers but skips hit testing, focus and
visibility, so it "clicks" a covered button or one on a removed screen. Use it to move a test along
quickly; prove each interactive screen once with `S.Click`.

### What does not work from Python, and what to use instead

| Fails | Use instead |
|---|---|
| `widget_blueprint.widget_tree`, `widget_blueprint.parent_class` ("Failed to find property") | The design tree: `WidgetStaticAnalysisToolset.DescribeWidget` or `unreal.load_object(None, "/Game/UI/WBP_X.WBP_X:WidgetTree.Btn_Play")`. The parent class: `unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp)`. |
| `user_widget.get_widget_from_name("Btn_Play")` (no such attribute) | `user_widget.get_editor_property("Btn_Play")` for a widget that is a Blueprint variable; otherwise iterate `unreal.ObjectIterator(unreal.Widget)` and match `get_name()` under the instance's path (`<instance path>.WidgetTree.<Name>`); or a Slate snapshot (`[umg=...]` tag). |
| `unreal.WidgetBlueprintLibrary` | `unreal.WidgetLibrary` (the class's script name), e.g. `unreal.WidgetLibrary.get_all_widgets_of_class(world, widget_class, False)` for live instances; take the last one that `is_in_viewport()` or has a parent, since removed screens linger until garbage collection and still react to presses. |
| `unreal.SlateBlueprintLibrary` | `unreal.SlateLibrary`. But see geometry below. |
| `unreal.SubsystemBlueprintLibrary`, `game_instance.get_subsystem(cls)`, `MySubsystem.get()`, `unreal.get_game_instance(...)` | `gi = unreal.GameplayStatics.get_game_instance(world)`, then `unreal.get_game_instance_subsystem(gi, cls)`; also `unreal.get_world_subsystem(world, cls)`, `unreal.get_local_player_subsystem(local_player, cls)`, `unreal.get_editor_subsystem(cls)`, `unreal.get_engine_subsystem(cls)`. The getter of the wrong kind raises "Cannot nativize ... allowed Class type". |
| `widget.get_cached_geometry()` and `WidgetLayoutLibrary.get_viewport_widget_geometry()` read through `SlateLibrary` (all zeros from Python) | Positions from `S.Snapshot` (`[pos=... size=...]`, desktop pixels) or `WF.Query` results (`x`, `y`, `width`, `height`); the viewport size from `unreal.WidgetLayoutLibrary.get_viewport_size(world)`. |
| `str + text_block.get_text()` (TypeError) | `str(text_block.get_text())`; a text entry takes `entry.set_text("110")`. |

The PIE world is `unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()` (None
when no PIE). Do not look worlds up by name; with several clients use `vibeue.pie_worlds()`.

## PIE control — engine `EditorAppToolset`

Generic PIE start/stop/status is owned by the engine's **`EditorToolset.EditorAppToolset`**:

| Tool | Description |
|--------|-------------|
| `StartPIE` | Starts PIE (or Simulate), then waits for warmup. `options` is optional: `bSimulate`, `playMode` (`"PlayMode_InViewPort"` default, `"PlayMode_InEditorFloating"`), `startTransform`, `warmupSeconds` (default 2). **Errors** if a session is already active or queued; 30 s limit. |
| `StopPIE` | Stops the session and waits for shutdown. **Errors** if no session is active, so check `IsPIERunning` first. |
| `IsPIERunning` | `True` for active PIE or Simulate. A queued start alone returns `False`. |

Screenshots are not on this toolset: use `SceneCaptureToolset.SceneCaptureToolset.CaptureGameViewport`
(below).

```
call_tool(toolset_name="EditorToolset.EditorAppToolset", tool_name="IsPIERunning")
call_tool(toolset_name="EditorToolset.EditorAppToolset", tool_name="StartPIE",
          arguments={"options": {"playMode": "PlayMode_InEditorFloating", "warmupSeconds": 2}})
call_tool(toolset_name="EditorToolset.EditorAppToolset", tool_name="StopPIE")
```

Inside `execute_python`, `await call_tool("EditorToolset.EditorAppToolset.StartPIE", options={...})`
returns after warmup, so the world exists in the same script; still wait for the player controller
with `await asyncio.sleep(0.1)` in a loop before touching it (never `time.sleep`).

**Window size goes in two places.** Before `StartPIE`, set `new_window_width`, `new_window_height`
(and `new_window_position`, an `unreal.IntPoint`) on BOTH
`unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).get_play_settings()` and
`unreal.LevelEditorPlaySettings.get_default_object()`: they are different objects, and with only one
set the floating window comes back at the size it was last closed at.

## Session helpers in vibeue (pie_session, capture_game)

`async with vibeue.pie_session(width, height) as s:` and `await vibeue.capture_game(path)` wrap the
setup above (throttling, window size, start, wait for the player, screen messages, capture to PNG)
and restore everything on exit.

## Standard Test Loop

**`WorkflowService.run_scenario` / `get_scenario` / `cancel_scenario` are gone**, and with them the
declarative scenario spec: the preflight block, the step list, the assertion steps, the dependency
hashing and the `stale` / `verifiedCurrent` freshness verdicts. Nothing replaces the spec. **You
compose the loop yourself** from the calls below, which means you now own what the scenario engine
used to guarantee for you:

- wait for **real** PIE readiness: `StartPIE` returns after its warmup, but wait for the local
  player controller (or the state your test needs) before driving anything,
- scope log reads to this run yourself (nothing marks where your run began any more),
- declare and actually evaluate at least one assertion — input + wait + screenshot steps verify
  **nothing**; say what you expected, what you read, and in what unit,
- tear PIE down on every path, including failure and abandonment,
- and treat a pass as describing only the build you just ran: nothing hashes your inputs or tells
  you a previous pass went stale. Save and compile first, and re-run after every edit.

There are two shapes, and they are not interchangeable.

### 1. An authored automation test — run / poll / cancel is engine-native

If the check lives in a C++ or Python automation test, Epic's **`AutomationTestToolset`** owns the
async run/poll/cancel shape that `run_scenario` / `get_scenario` / `cancel_scenario` used to provide.
All of it is `call_tool` on the toolset `AutomationTestToolset.AutomationTestToolset` (the bare name
gives `ToolsetIsNotRegistered`); `ListTests` takes `nameFilter`, not `filter`:

| Need | Call |
|---|---|
| Discover the test list — **required once** before `ListTests`/`RunTests`, and it takes seconds | `AutomationTestToolset.DiscoverTests` |
| Find tests by name or tag | `AutomationTestToolset.ListTests` |
| Start a run (async; `RunTestsByFilter` for a large batch) | `AutomationTestToolset.RunTests` |
| Poll it — the `get_scenario` equivalent | `AutomationTestToolset.GetTestStatus` |
| Read per-test results once it finishes | `AutomationTestToolset.GetTestResults` |
| Cancel — the `cancel_scenario` equivalent | `AutomationTestToolset.StopTests` |

Poll `GetTestStatus` in separate tool calls, or in one script with `await asyncio.sleep(...)`
between polls. Never spin or `time.sleep()` on the game thread waiting for a run: the editor does
not tick while your script holds it, so the status can never change.

### 2. An ad-hoc runtime check — hand-compose it

Each old scenario step maps to a call you now make yourself. Note the style column: Epic's toolsets
are `call_tool`; VibeUE services are `unreal.<Service>.<method>()` inside `execute_python`.

| Old scenario step | What to call now |
|---|---|
| `start_pie` / `stop_pie` / `wait_for_pie` | `call_tool` → `EditorAppToolset.StartPIE` / `StopPIE` / `IsPIERunning` (`StartPIE` returns after warmup; then wait for the player controller) |
| click / drag / type on the UI | `SlateInspectorToolset.SlateInspectorToolset.Click` / `Drag` / `Type` — see "Drive a running UI" above |
| `inject_action_for` / `inject_key` | `call_tool` → `InputToolset.ApplyInput` (`entries` + `durationSeconds`). VibeUE keeps `unreal.InputService.start_injection(...)` / `stop_injection(...)` in `execute_python` for an **indefinite** hold released later, and `inject_action(...)` for a one-tick queued tap |
| `wait` | a later tool call, or a top-level `await asyncio.sleep(...)` in one `execute_python` script |
| `assert_log` | `call_tool` → `ConsoleToolset.LogsToolset.GetLogEntries` (it requires a `pattern`). Read the log before the run and compare, since nothing scopes it to your run now |
| `capture_game` | `call_tool` → `SceneCaptureToolset.SceneCaptureToolset.CaptureGameViewport`, or `await vibeue.capture_game(path)` in a script |
| `console_command` | `call_tool` → `ConsoleToolset.RunConsoleCommand`, or `unreal.PIEActorService.console_command(selector, cmd)` when it must run in one specific PIE world |
| `python_assert` / `python_assert_number` | your own comparison inside Epic's `execute_python`: read the live value, compare it, and print expected *and* actual as the evidence |
| `preflight: {save_dirty_assets, compile_blueprints}` | `unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)`, and `BlueprintToolsets.BlueprintToolset.CompileBlueprint` via `call_tool`, **before** you start PIE |
| `dependencies` freshness hashing | nothing — no tool hashes inputs or reports `stale`. Save, compile, re-run after changes |
| the run report under `Saved/VibeUE` | write your own: `FileSystemToolset.WriteTextFile` / `ReadTextFile` (it mounts `/Saved`), plus `UndoHistoryToolset.GetRecentTransactions` / `GetTransactionDetails` for which assets the run actually touched |

The loop itself:

```
# 1. Start from a clean state: StopPIE ERRORS when nothing runs, so check first
call_tool(toolset_name="EditorToolset.EditorAppToolset", tool_name="IsPIERunning")
call_tool(toolset_name="EditorToolset.EditorAppToolset", tool_name="StopPIE")      # only if it was True

# 2. Save + compile what you are about to test (execute_python), THEN start the session
#    (StartPIE uses the editor's current PIE settings; it returns after its warmup)
call_tool(toolset_name="EditorToolset.EditorAppToolset", tool_name="StartPIE",
          arguments={"options": {"playMode": "PlayMode_InEditorFloating", "warmupSeconds": 2}})

# 3. Wait for the player controller, then drive input / read logs / capture / assert
# 4. Stop when done — on failure too
call_tool(toolset_name="EditorToolset.EditorAppToolset", tool_name="StopPIE")
```

## Net mode from Python — the surviving PIE-settings helpers

`StartPIE` uses the editor's saved PIE settings, so set the net mode BEFORE starting the session.
`EngineSettingsService.get_pie_settings` / `set_pie_settings` survive as Python-only helpers (not MCP tools) and read/write `ULevelEditorPlaySettings` directly and persists the
change, so a dedicated-server PIE gate no longer means quitting the editor to hand-edit
`EditorPerProjectUserSettings.ini` and relaunching:

```python
import unreal

# Read the current PIE multiplayer settings
info = unreal.EngineSettingsService.get_pie_settings()
print(info.net_mode, info.num_clients, info.run_under_one_process)

# Set dedicated-server PIE: 1 client, all windows in one process
ok = unreal.EngineSettingsService.set_pie_settings("Client", 1, True)   # returns True only on verified write
# Verify by readback, never by return value alone
assert unreal.EngineSettingsService.get_pie_settings().net_mode == "Client"
```

- `net_mode` maps `EPlayNetMode`: `"Standalone"` | `"ListenServer"` | `"Client"`. **`"Client"` is
  dedicated-server PIE** — the editor's "Play As Client", where a windowless dedicated server is
  spawned behind the scenes and PIE instance 0 is that server (`"DedicatedServer"` is accepted as an
  alias for `"Client"`).
- `set_pie_settings` refuses an unknown net mode or a `num_clients` outside 1-10 (returns `False`,
  logs a Warning), and returns `False` if any written value fails to read back.
- **These are per-USER config, not project config** — they live in
  `Saved/Config/<Platform>/EditorPerProjectUserSettings.ini` and follow the machine, not the repo.
  Do not expect a teammate or CI to inherit them; set them from the gate itself.

## Validating widgets in PIE — `WidgetService`

VibeUE keeps two widget-in-PIE helpers on `unreal.WidgetService` (run them via `execute_python`).
They spawn and tear down a live widget instance once PIE is running:

```python
import unreal

# After StartPIE, spawn a widget instance into the running viewport
handle = unreal.WidgetService.spawn_widget_in_pie("/Game/UI/WBP_HUD", 0)

# Tear it down before stopping PIE
unreal.WidgetService.remove_widget_from_pie(handle)
```

**Reading a live widget property is engine-native now.** `WidgetService.get_live_property` was
removed; walk Slate → UMG → properties with `call_tool` instead (full table in "Drive a running UI"):

1. `SlateInspectorToolset.Snapshot(ref)` — the live Slate tree under that ref, to find the widget.
2. `SlateInspectorToolset.GetUMGInfo(ref)` — the UMG `UWidget` behind that Slate ref, plus a live
   handle to it.
3. `ObjectToolset.GetProperties` on that handle — the actual property values (`Text`, `Visibility`,
   whatever you need).

`WidgetService.is_pie_running` / `start_pie` / `stop_pie` are also gone: PIE state and control are
`EditorAppToolset.IsPIERunning` / `StartPIE` / `StopPIE` via `call_tool`, as above.

## Spawning test actors in PIE — `PIEActorService`

**Why this exists:** you cannot spawn an actor into a running PIE world from Python any other way.
`unreal.World` / `unreal.GameplayStatics` expose no spawn (the deferred-spawn entry points are
`BlueprintInternalUseOnly`), `EditorActorSubsystem.spawn_actor_from_class` targets the **editor**
world, and the engine `SceneTools` toolset refuses with "Cannot create actors while PIE is active".
`PIEActorService` resolves the live PIE world by role and spawns a **transient** actor into it — a
blocker, a test dummy, a trigger volume — with no Blueprint and no editor-world contamination. Run it
via `execute_python`.

**Selector grammar** (strict — anything else returns `INVALID_SELECTOR`):

| Selector | Resolves to |
|----------|-------------|
| `"server"` | PIE instance 0 — the standalone world, the listen host, or the dedicated server |
| `"client"` | the single client world (instance ≥ 1); `AMBIGUOUS_WORLD` if >1, `WORLD_NOT_FOUND` if none |
| `"client:N"` | client instance N (N ≥ 1) |
| `"instance:N"` | the exact PIE instance N (N ≥ 0) |

Worlds come from the PIE world contexts only; the editor world and the ~100 stale `/Memory/UEDPIE_*`
shells are never used. No PIE at all → `PIE_NOT_RUNNING`.

**Authority rule:** a spawn into a **client** world is cosmetic and never replicates, so it is refused
with `CLIENT_REQUIRES_OPT_IN` unless you pass `allow_client_local=True`. Spawn on `"server"` for
anything that must exist for every player.

```python
import unreal, json

# Drop a static-mesh actor as a blocker in front of the host, then verify and clean up.
t = unreal.Transform(location=unreal.Vector(500, 0, 100))
res = json.loads(unreal.PIEActorService.spawn_actor(
    "server", "/Script/Engine.StaticMeshActor", t))
assert res["success"], res
handle = res["handle"]          # "<session_serial>:<guid>" — dies with this PIE session
print(res["actor_path"], res["net_mode"], res["is_authority"])

# ... run whatever movement / collision check needs the blocker ...

# Idempotent teardown before StopPIE.
print(unreal.PIEActorService.destroy_all())     # {"success": true, "destroyed": 1, "already_gone": 0}
```

`resolve_world("server")` returns `{success, world_path, net_mode, pie_instance, is_authority,
actor_count}` and is the cheap way to confirm a session's role before spawning. `destroy_actor(handle)`
is idempotent per handle: it destroys the actor and **keeps the record**, so calling it again returns
`success: true, already_gone: true` (never `UNKNOWN_HANDLE`). A consumed handle stays in
`list_spawned()` as `alive: false` until PIE ends — `list_spawned()` is the full spawn ledger for the
session, not just the live actors — and `destroy_all()` counts an already-consumed actor under
`already_gone` rather than `destroyed`.

**Error codes:** `INVALID_SELECTOR`, `PIE_NOT_RUNNING`, `WORLD_NOT_FOUND`, `AMBIGUOUS_WORLD`,
`CLASS_NOT_FOUND`, `NOT_AN_ACTOR_CLASS`, `CLASS_ABSTRACT`, `CLASS_DEPRECATED`, `CLASS_REINSTANCED`,
`INVALID_TRANSFORM`, `INVALID_COLLISION_HANDLING`, `CLIENT_REQUIRES_OPT_IN`, `SPAWN_REJECTED`
(the placement collided under the chosen collision policy), `UNSUPPORTED_CLASS`, `UNKNOWN_HANDLE`,
`STALE_HANDLE`.

**Landscapes are refused** (`UNSUPPORTED_CLASS` for `ALandscape`, `ALandscapeStreamingProxy` and every
other `ALandscapeProxy`): a landscape spawned this way has no landscape GUID, so destroying it never
unregisters it from `ULandscapeSubsystem`, and the subsystem's next tick crashes the editor. The class
check runs after the selector and PIE-world checks, so with no PIE you still get `PIE_NOT_RUNNING`.

> **Gotcha — handles die with the PIE session.** `EndPIE` bumps an internal session serial and clears
> the registry, so a handle kept across a stop/start reports `STALE_HANDLE`. And the same
> Python-globals rule as everywhere else applies with teeth here: **release any global holding a
> spawned actor before `StopPIE`** (`del blocker` / set to `None`, or `vibeue.release_globals`), or
> the editor asserts on PIE teardown ("Object from PIE level still referenced"). The returned JSON is
> a string, so keeping `res`/`handle` is safe — only a live actor reference is the hazard.

## Calling functions with real RPC routing — `PIEActorService.call_function`

**Why Python-called RPCs misroute:** the Python plugin runs every call under
`FEditorScriptExecutionGuard`, which sets `GAllowActorScriptExecutionInEditor = true`, and while that
global is true `AActor::GetFunctionCallspace` answers **Local** for every function. So a `Server_` RPC
called from Python on a PIE client runs on the client and sends no packet, and a `Client_` RPC the
server sends inside a flow Python started runs on the server instead of the owning client.

`call_function` lowers that global for the one call (restored on return), computes the real callspace,
and calls `ProcessEvent`, so every RPC inside the call routes exactly as in a real game.
`console_command` does the same around `GEngine->Exec` for the chosen PIE world (console variables and
engine/world commands; PlayerController exec functions and cheats are not routed there, so call those
with `call_function` on the controller).

**When to use which:**
- **Input injection** (`InputService`, below) when you are testing the player-facing path: the key or
  action, the input binding, and the code that decides to send the RPC.
- **`call_function`** when you need one specific function or RPC with chosen arguments: a server RPC from
  a client, a server-side flow that sends client RPCs (respawn, possession, travel), or a gate fixture.
  It also covers plain non-RPC UFUNCTIONs with typed return values.

**The `callspace` field** says what the routing decided: `"local"` (ran here), `"remote"` (the RPC
was handed to the net driver and nothing ran here, so `return`/`out_params` are empty),
`"local+remote"` (a multicast on the server: ran here and was handed to the driver), or `"absorbed"`
(dropped before the driver: an authority-only call on a client, or, on the SERVER side, an RPC on an
actor with no owning connection). `executed_locally` is the same thing as a bool.

**`"remote"` means handed to the net driver, not delivered.** On a client, an actor without authority
skips the ownership check entirely and gets `"remote"` whenever it replicates. So a `Server_` call on
another player's pawn, or on a placed lever the client does not own, also reports `"remote"`, and the
net driver then DROPS it with a LogNet "No owning connection" warning. With the guard lowered, a
client-side `Server_` call never reports `"local"`. To confirm delivery, run `log LogNetTraffic Log`
first and look for `Sent RPC: ...::Server_X` in the client frame (and the handler's effect on the
server), and watch for "No owning connection".

```python
import unreal, json

# Server RPC from client 1's own pawn: "remote" = handed to the net driver (confirm with LogNetTraffic).
res = json.loads(unreal.PIEActorService.call_function(
    "client:1", "BP_Test_C_0", "Server_BeginInteract", '{"Target": "Lever_0"}'))
assert res["success"] and res["callspace"] == "remote", res

# A normal UFUNCTION with a return value, on a component ("<actor>.<component>").
hp = json.loads(unreal.PIEActorService.call_function("server", "BP_Test_C_0.HealthComp", "GetHealth", "{}"))
print(hp["return"])

# A console variable on the server world.
print(unreal.PIEActorService.console_command("server", "r.VSync 0"))
```

The object is resolved **inside the selected PIE world only**: a full object path (it must belong to
that world), a `spawn_actor` handle, an actor name or editor label, `"<actor>.<component>"`, or
`"<actor>:<subobject>"`. Arguments are a JSON object keyed by parameter name (case-insensitive).
Numbers and bools fill numeric and bool params; strings fill string, name and text params and enums
(by name); object and class params take a PIE-world name or label, or a full object or asset path
(`""`/`"None"` is null; an object from another PIE world or the editor world is refused); anything else
(structs, arrays, soft references) takes its ImportText form, e.g. `"(X=1,Y=2,Z=3)"`. **Object
references nested inside a struct, array or map argument are resolved by the engine's global object
lookup, not confined to the PIE world**, so pass full PIE-world paths there. An object or class path
that points into a map package the editor has not loaded is refused with `MAP_PATH_REFUSED` and is
never loaded (a map loaded as an asset stays resident and makes the next `load_level` fatal); other
assets outside the PIE world are found first and loaded only when they are not maps. Integers are
range-checked against the parameter's size (`int8`, `uint16`, `int32`, ...; `uint64` caps at 2^63-1),
numbers given as strings must be whole numbers (`"12abc"` is refused), and enum integers or names must
name a real enumerator (never `_MAX`). JSON arrays and objects and delegate params are rejected. A missing input uses the UFUNCTION's C++ default when one is
declared (BlueprintCallable functions carry it), else `MISSING_ARG`; out-only params need no input and
come back in `out_params`.

Reply: `{success, function, object_path, world_path, net_mode, pie_instance, callspace,
executed_locally, return, out_params}`. **Error codes:** the selector and world codes above, plus
`OBJECT_NOT_FOUND`, `AMBIGUOUS_OBJECT` (two actors share the label), `OBJECT_NOT_IN_WORLD`,
`FUNCTION_NOT_FOUND`, `INVALID_ARGS_JSON`, `UNKNOWN_ARG` (a key that names no parameter),
`MISSING_ARG`, `INVALID_ARG_VALUE`, `UNSUPPORTED_ARG_TYPE`, `MAP_PATH_REFUSED`. `console_command` replies `{success,
command, handled, output, world_path, net_mode, pie_instance}` and fails with `EMPTY_COMMAND` or
`COMMAND_NOT_HANDLED`; `output` is only what the handler wrote to the command's output device, not the
log.

> **Why this exists (ZombieGame, 2026-10-22/23):** a gate respawned a dead client's player through a
> C++ fixture called from Python. The server ran `Possess`, but under the raised guard `ClientRestart`
> and `ClientSetRotation` executed **on the server**, so the owning client never acknowledged its new
> pawn: its movement component stayed `MOVE_None`, it sent no moves, and its copy froze while the
> train rode on. It looked like a product regression for a whole day; the log tell was
> `ClientRestart_Implementation` printed in the server's frame. Scoping the guard to false around the
> flow fixed it, and `call_function` does exactly that for you.

## Driving gameplay input in PIE — `InputService` (issue #550)

Never remap game input assets or send OS keystrokes (SendKeys/AppActivate) to test a mechanic —
inject input directly; no OS window focus needed:

```python
import unreal

# VibeUE keeps two of these for an INDEFINITE hold you release later (X/Y/Z map onto the
# action's value type, Boolean uses X != 0): hold an Enhanced Input action down ...
print(unreal.InputService.start_injection("/Game/Input/IA_Fire_Secondary"))
# ... and release it, in a LATER call, whenever the test says to:
print(unreal.InputService.stop_injection("/Game/Input/IA_Fire_Secondary"))
# inject_action(...) is different: a single press QUEUED for the next input tick and released on
# the one after.
```

**Every bounded press or hold is Epic's now** — `inject_action_for` and `inject_key` are gone. Use
`InputToolset.ApplyInput` via `call_tool`, which takes `entries` plus a `durationSeconds`
(`0` = a tap: down then up; `> 0` = held for that long, then released):

```
call_tool InputToolset.ApplyInput {"entries": [{"action": {"refPath": "/Game/Input/IA_Block.IA_Block"},
                                               "value": {"x": 1, "y": 0, "z": 0}}], "durationSeconds": 1.5}
call_tool InputToolset.ApplyInput {"entries": [{"key": {"keyName": "SpaceBar"}, "value": {"x": 1, "y": 0, "z": 0}}],
                                   "durationSeconds": 0}
```

A Python call blocks the game thread, so PIE does not tick while it runs: inject in one call and
read the game state in a LATER call, two or more frames on, or in the same call after a top-level
`await asyncio.sleep(...)`, which lets the editor tick. While a hold is active the editor stays
off its background frame rate, so an unfocused editor does not starve the hold. The action calls
take `pie_instance` to pick a PIE world: -1, the default, is the first PIE world with a local player
(the lowest instance number — the listen server's window, or client 1 under a dedicated server), and
replies report the instance actually used. A hold is one hold per action and PIE world whatever the
path spelling; starting a held action again restarts it; `StopPIE` drops every hold, so nothing from an
ended session is released into the editor or reported active in the next one.

All return JSON with `success` and an `error_code` naming the problem (PIE not running,
`NO_LOCAL_PLAYER` / `NO_PLAYER_CONTROLLER` while PIE or a client's join is still starting — retry on
a later call —, unknown key, ...). Epic `ApplyInput` additionally rejects a **Simulate In Editor** session
with `SIMULATE_NOT_PLAY`: Simulate has no player game viewport, so the Slate key events would be
dropped. Use `StartPIE` (Play), not Simulate, when driving keys. Its `handled_down` / `handled_up`
fields report whether a Slate widget *consumed* the event, not whether the key reached the player — a
key the game polls with `WasInputKeyJustPressed` lands with `handled_down: false`. Verify from game
state, not from those flags.

## Seeing the game — `CaptureGameViewport` (engine-native in 5.9)

`SceneCaptureToolset.CaptureGameViewport` screenshots the Game/PIE viewport **as Slate draws it,
with UMG and any other UI over the viewport**, and returns a real MCP image block you can look at.
In PIE it redraws the window holding the viewport; if that fails you get the scene only, plus a
warning. VibeUE's old `capture_image` is gone — 5.9 does this natively, so there is no VibeUE
screenshot tool any more.

**Multi-player PIE: pick the window with `World`.** Call `SceneCaptureToolset.ListWorlds` to see the
PIE/Game worlds, then pass the one you want as `World`. Leaving `World` unset resolves it
automatically (the editor's current game viewport).

```
call_tool ListWorlds            → the PIE/Game worlds available
call_tool CaptureGameViewport   {"world": {"refPath": "<world from ListWorlds>"}}
call_tool CaptureGameViewport   {"maxResolution": 1600}   # longest edge, aspect preserved
```

`MaxResolution` is capped by the toolset's configured maximum (reported in `Warnings`). A
dedicated-server instance has no viewport, and the call raises a script error if no readable
Game/PIE framebuffer exists.

**"window is minimized"?** A minimized editor also freezes PIE (world time stays 0). Restore it from
`execute_python` with `unreal.ViewportService.set_editor_window_state("restore")`.

## Full-rate unfocused verification — `PerformanceService` (issue #549)

An unfocused editor throttles to ~3 FPS, wrecking timed PIE runs. Disable throttling for the
session instead of focus-hacking the window:

```python
unreal.PerformanceService.set_background_throttling(False)   # before the run
unreal.PerformanceService.set_background_throttling(True)    # after
```

## Gotchas

- **Python blocks the game thread, so a value only changes across an `await` or a later call.** A loop with `time.sleep()` between reads (rejected by `execute_python` anyway; inside a module it freezes PIE) returns the *same* number every iteration — the world never ticks while your script holds the thread. This makes a working animation look frozen and a stuck one look identical to a healthy one. Yield between samples with top-level `await asyncio.sleep(...)`, or take each sample in a separate call:

  ```python
  # WRONG - three identical readings, proves nothing
  readings = []
  for i in range(3):
      readings.append(ai.get_editor_property("SailAngle")); time.sleep(0.4)

  # CORRECT - the editor ticks during each await, so the samples are real tick time apart
  readings = []
  for i in range(3):
      readings.append(ai.get_editor_property("SailAngle"))
      await asyncio.sleep(0.4)
  readings
  ```
  Convert the difference into the engineering unit you expect and check it: 306° → 709° across ~8 s is 48°/s, which is exactly the 8 RPM that was configured — that is a pass, "the number changed" is not.
- **PIE start is asynchronous, but the tool waits.** `StartPIE` is an async tool: awaited (`await call_tool(...)` in a script, or an MCP `call_tool`), it completes after `PostPIEStarted` plus `warmupSeconds`. A raw `unreal.ToolsetRegistry.execute_tool` or `vibeue.exec_tool` returns before that. Even after warmup the pawn may not be possessed yet: wait for `unreal.GameplayStatics.get_player_controller(world, 0)` with `await asyncio.sleep(0.1)` between checks.
- **An active session is an error.** `StartPIE` errors if a session is already active or queued, and `StopPIE` errors if none is. Check `IsPIERunning` first; stop, wait for `get_game_world()` to return None, then start for a fresh session.
- **`StopPIE` tears down the world** via `RequestEndPlayMap`. Spawned PIE widget instances should be removed with `WidgetService.remove_widget_from_pie(handle)` before stopping.
- **Save before starting.** Dirty asset changes are NOT picked up by PIE unless saved/compiled. Compile (`BlueprintToolsets.BlueprintToolset.CompileBlueprint`) before launching PIE to test Blueprint changes; during PIE the editor refuses widget compiles ("Not supported during PIE").
- **Don't leave PIE running between tasks.** Subsequent edits (recompiles, asset moves, hot reload) can fail or behave oddly while a PIE world is alive. Call `StopPIE` before returning control to the user.
- **Map-load delegates never fire on PIE start.** `FCoreUObjectDelegates::PreLoadMap` / `PostLoadMapWithWorld` (loading screens, post-load hooks, subsystem map handlers) are skipped because the PIE world is *duplicated* from the editor world, not loaded. To exercise them, trigger a real in-game transition once PIE is up: `unreal.GameplayStatics.open_level(pie_world, "MapName")`.
- **Python references holding PIE objects CRASH the editor on PIE end.** Each `execute_python` call gets a fresh script namespace, but anything stored in an imported module (a module-level variable, a cache, a pending `vibeue.wait_for` predicate or result) lives on between calls and is a GC root (`FPyReferenceCollector`). If any still reference a PIE object (pawn, subsystem instance, quest/objective, widget) when PIE tears down, the engine asserts in `PlayLevel.cpp` ("Object from PIE level still referenced") and the editor dies. Before stopping PIE — or as the last statement of any call that inspected PIE objects — release them: set those module attributes to `None` (`mymod.pawn = None`), and run `vibeue.cancel_all_waits()`. Also holds for a `world` pointer cached across an in-game `open <map>` travel: it goes stale and using it raises "WorldContext requested with invalid context object"; re-fetch `get_game_world()` in the same call that uses it.

## When to use PIE

- Verifying a Blueprint event fires (combine with log inspection — see `LogsToolset`)
- Validating gameplay logic (damage, scoring, state transitions)
- Testing widgets in their real runtime context (combine with `umg-widgets` skill's `spawn_widget_in_pie`)
- Reproducing user-reported runtime bugs

## When NOT to use PIE

- Pure asset/editor validation (use `BlueprintToolset.CompileBlueprint`, `AssetToolset.FindAssets` / `ValidateAssets`, etc.)
- Static introspection (use `BlueprintDslToolset.Read`, `BlueprintToolset.FindNodes` / `GetNodeInfos`, `WidgetStaticAnalysisToolset.DescribeWidget`)
- Anything you can verify without a live world — PIE is slow, save it for genuine runtime checks.

## Additional gotchas

- `StopPIE` then `StartPIE` back to back fails with "A play session is already running" unless the editor ticks between them. Await the `StopPIE` call, then wait (`await asyncio.sleep(0.1)` in a loop) until `get_game_world()` is None before starting again.
- `EditorAssetLibrary.load_asset` returns None while PIE runs, and downstream calls then return 0/None instead of raising (a fake negative). Load assets before `StartPIE`, or find live objects with `ObjectIterator`.
- Match PIE worlds by their `/Game/<path>/UEDPIE_0_<Map>` (server/host) and `UEDPIE_1_` (client) object paths, not by name — roughly 100 stale `/Memory/UEDPIE_*` shells match by name. `vibeue.pie_worlds()` returns them keyed by role, and `UnrealEditorSubsystem.get_game_world()` gives the local one.
- A two-client run is Standalone by default: both worlds read `ROLE_AUTHORITY`, so confirm `get_editor_property("role")` (or `vibeue.role(actor)`) before claiming a host/client result. Read and set the PIE net mode with `get_pie_settings`/`set_pie_settings` (dedicated server = PlayNetMode 2).
- A Server RPC invoked from Python runs LOCALLY and is silently dropped; never Start/StopFire a client world's weapon from Python (it crashes the editor). Move a client pawn through its server copy; control rotation is client-owned.
- `slomo 0.1` stretches a 2 s timer to 20 s of wall time so a transient state survives between calls; restore `slomo 1` after. PIE time restarts each run.
- Never leave an automation test run queued before starting PIE — it wakes on PIE activity and tears the session down. That includes a run started with `AutomationTestToolset.RunTests`: `StopTests` it (and confirm with `GetTestStatus`) before you drive your own PIE session.
- Don't hand-roll waits with `time.sleep()` or your own post-tick callback. Inside one script, loop on `await asyncio.sleep(0.1)` with a deadline. Across calls, `vibeue.wait_for(pred, timeout_s)` and `vibeue.wait_for_players(n)` poll between calls against a wall-clock deadline (PIE world time restarts each session), and `vibeue.cancel_all_waits()` purges their handles. `with vibeue.gate_session(clients=3, net_mode="ListenServer"):` turns background throttling off, sets the PIE settings and restores both afterwards. See the `vibeue` skill.
- Script globals do NOT persist across `execute_python` calls (each call has a fresh namespace): repeat your imports in every script, keep state that must span calls in a module of your own (it must do nothing at import), and release PIE object references held there before `StopPIE`. Prefer one script with top-level `await` over state spread across calls.

## Reference pages

Open these with the paths below; UE rewrites them to absolute paths when it serves this skill.

- [scripts/pie_test_loop.txt](scripts/pie_test_loop.txt) - runnable sample
