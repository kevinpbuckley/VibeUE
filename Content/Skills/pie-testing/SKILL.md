---
name: pie-testing
display_name: Play-In-Editor Testing
description: Start, stop, and query Play-In-Editor (PIE) sessions for runtime testing
  of Blueprints, gameplay logic, widgets, AI, and any in-game behavior. Use when the
  user asks you to "play", "test", "run", "PIE", "start the game", "stop the game",
  or otherwise needs a live game world to validate changes.
vibeue_classes:
  - WidgetService
unreal_classes:
  - UEditorEngine
  - FRequestPlaySessionParams
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

# Play-In-Editor (PIE) Testing Skill

PIE is the only way to validate runtime behavior — Blueprint logic, AI ticking, animation, input, widget interaction, gameplay events. Without starting PIE, your "fix" is unverified.

## Methods

PIE control lives on `WidgetService` for historical reasons (it grew out of widget testing) but **applies to ALL runtime testing**, not just widgets.

| Method | Description |
|--------|-------------|
| `unreal.WidgetService.start_pie()` | Start PIE if not already running. Returns `True` on success or if already running. |
| `unreal.WidgetService.stop_pie()` | End the current PIE session. Returns `True` if stopped or already stopped. |
| `unreal.WidgetService.is_pie_running()` | `True` if PIE or Simulate-In-Editor is active. |

## Standard Test Loop

```python
# 1. Make sure you're starting from a clean state
if unreal.WidgetService.is_pie_running():
    unreal.WidgetService.stop_pie()

# 2. Start the session (uses the editor's current PIE settings — default map, viewport)
unreal.WidgetService.start_pie()

# 3. Let the test run / inspect log output / interact via other services
# 4. Stop when done
unreal.WidgetService.stop_pie()
```

## Gotchas

- **PIE start is asynchronous.** `start_pie()` returns immediately after `RequestPlaySession` is queued. The world isn't actually playing until the editor processes the request on its next tick. If you need to act inside the running world, give it a tick or poll `is_pie_running()`.
- **Already-running is treated as success.** `start_pie()` returns `True` if a PIE session already exists — it does NOT restart. Stop first if you need a fresh session.
- **`stop_pie()` cleans up tracked PIE widgets** (those spawned via `WidgetService.spawn_widget_in_pie`). Other widgets/actors are torn down by the engine as part of `RequestEndPlayMap`.
- **Save before starting.** Dirty asset changes are NOT picked up by PIE unless saved/compiled. Always `compile_blueprint(...)` before launching PIE to test Blueprint changes.
- **Don't leave PIE running between tasks.** Subsequent edits (recompiles, asset moves, hot reload) can fail or behave oddly while a PIE world is alive. Call `stop_pie()` before returning control to the user.

## When to use PIE

- Verifying a Blueprint event fires (combine with log inspection — see `LogsToolset`)
- Validating gameplay logic (damage, scoring, state transitions)
- Testing widgets in their real runtime context (combine with `umg-widgets` skill's `spawn_widget_in_pie`)
- Reproducing user-reported runtime bugs

## When NOT to use PIE

- Pure asset/editor validation (use `compile_blueprint`, `find_assets`, etc.)
- Static introspection (use `get_nodes_in_graph`, `get_node_pins`)
- Anything you can verify without a live world — PIE is slow, save it for genuine runtime checks.
