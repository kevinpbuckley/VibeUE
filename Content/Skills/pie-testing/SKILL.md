---
name: pie-testing
display_name: Play-In-Editor Testing
description: Start, stop, and query Play-In-Editor (PIE) sessions for runtime testing of Blueprints, gameplay logic, widgets, AI, and any in-game behavior. Use when the user asks you to "play", "test", "run", "PIE", "start/stop the game", or otherwise needs a live game world to validate changes.
vibeue_classes:
  - WidgetService
unreal_classes:
  - UEditorEngine
  - FRequestPlaySessionParams
engine_toolsets:
  - EditorToolset.EditorAppToolset
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
---

> 🧠 **Brains complement:** IF an `unreal-engine-skills-manager` tool (external MCP) exists in this session, call it with `{action: "load", skill: "automation-and-testing"}` for UE domain knowledge on this topic — correct APIs, architecture, best practices — and treat it as the rubric for any review / "best practices" question. If no such tool is available (e.g. running under Claude Code or Codex without that MCP), skip this line entirely and proceed with this skill alone — do NOT attempt the call.

# Play-In-Editor (PIE) Testing Skill

PIE is the only way to validate runtime behavior — Blueprint logic, AI ticking, animation, input, widget interaction, gameplay events. Without starting PIE, your "fix" is unverified.

## PIE control — engine `EditorAppToolset`

Generic PIE start/stop/status is owned by Unreal 5.8's native **`EditorToolset.EditorAppToolset`**,
invoked through `call_tool` (run `describe_toolset` on it for exact action names/params):

| Action | Description |
|--------|-------------|
| `StartPIE` | Start PIE if not already running. Succeeds (no-op) if already running. |
| `StopPIE` | End the current PIE session. Succeeds if stopped or already stopped. |
| `IsPIERunning` | `True` if PIE or Simulate-In-Editor is active. |
| `CaptureViewport` | Screenshot the active viewport (use to visually verify runtime state). |

```
call_tool(toolset="EditorToolset.EditorAppToolset", tool="IsPIERunning")
call_tool(toolset="EditorToolset.EditorAppToolset", tool="StartPIE")
call_tool(toolset="EditorToolset.EditorAppToolset", tool="StopPIE")
```

> PIE start/stop/query is **not** on `WidgetService` anymore for general testing — use the engine
> toolset above. `WidgetService` still owns the widget-in-PIE validation helpers below.

## Standard Test Loop

```
# 1. Make sure you're starting from a clean state
call_tool(toolset="EditorToolset.EditorAppToolset", tool="StopPIE")   # no-op if not running

# 2. Start the session (uses the editor's current PIE settings — default map, viewport)
call_tool(toolset="EditorToolset.EditorAppToolset", tool="StartPIE")

# 3. Let the test run / inspect log output (LogsToolset) / interact via other services
# 4. Stop when done
call_tool(toolset="EditorToolset.EditorAppToolset", tool="StopPIE")
```

## Validating widgets in PIE — `WidgetService`

VibeUE keeps a small set of widget-in-PIE helpers on `unreal.WidgetService` (run them via
`execute_python_code`). These spawn and inspect live widget instances once PIE is running:

```python
import unreal

# After StartPIE, spawn a widget instance into the running viewport
handle = unreal.WidgetService.spawn_widget_in_pie("/Game/UI/WBP_HUD", 0)

# Read a live property off the running instance
val = unreal.WidgetService.get_live_property(handle, "HealthText", "Text")

# Tear it down before stopping PIE
unreal.WidgetService.remove_widget_from_pie(handle)
```

`unreal.WidgetService.is_pie_running()` also still exists and is handy from inside Python; for
tool-level control prefer the engine `EditorAppToolset` actions above.

## Gotchas

- **PIE start is asynchronous.** `StartPIE` returns immediately after `RequestPlaySession` is queued. The world isn't actually playing until the editor processes the request on its next tick. If you need to act inside the running world, give it a tick or poll `IsPIERunning`.
- **Already-running is treated as success.** `StartPIE` succeeds if a PIE session already exists — it does NOT restart. Stop first if you need a fresh session.
- **`StopPIE` tears down the world** via `RequestEndPlayMap`. Spawned PIE widget instances should be removed with `WidgetService.remove_widget_from_pie(handle)` before stopping.
- **Save before starting.** Dirty asset changes are NOT picked up by PIE unless saved/compiled. Always `compile_blueprint(...)` before launching PIE to test Blueprint changes.
- **Don't leave PIE running between tasks.** Subsequent edits (recompiles, asset moves, hot reload) can fail or behave oddly while a PIE world is alive. Call `StopPIE` before returning control to the user.

## When to use PIE

- Verifying a Blueprint event fires (combine with log inspection — see `LogsToolset`)
- Validating gameplay logic (damage, scoring, state transitions)
- Testing widgets in their real runtime context (combine with `umg-widgets` skill's `spawn_widget_in_pie`)
- Reproducing user-reported runtime bugs

## When NOT to use PIE

- Pure asset/editor validation (use `compile_blueprint`, `find_assets`, etc.)
- Static introspection (use `get_nodes_in_graph`, `get_node_pins`)
- Anything you can verify without a live world — PIE is slow, save it for genuine runtime checks.
