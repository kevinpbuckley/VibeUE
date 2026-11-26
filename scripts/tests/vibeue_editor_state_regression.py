"""Regression checks for vibeue.editor_state() and vibeue.preserve_editor() (#701). Needs a live UE 5.9 editor with
VibeUE and a renderer (not -nullrhi: the person's edit is typed into the Details panel through Slate).

Run from an execute_python script (top-level await) in a DISPOSABLE editor session: it creates and saves
/Game/VibeUETests/L_D1_State, opens it, spawns actors and leaves the editor on that map.

    import sys
    sys.path.insert(0, r"<VibeUE>/scripts/tests")
    import vibeue_editor_state_regression as t
    await t.run(call_tool) # {"passed": [...], "failed": [...], "details": {...}}

How a person's edit is made headless: the test selects an actor and types a new Location X into the Details panel
with SlateInspectorToolset.Type (focus, characters, Enter): Slate dispatches those as key events, exactly as for a
person typing, so the "Set Location" transaction begins inside Slate's input processing. That is the same rule a
gizmo drag meets (its transaction begins on the mouse-down event). A gizmo drag itself is not driven: it needs the
pixel position of a gizmo handle.
"""

import asyncio

import unreal
import vibeue

MAP = "/Game/VibeUETests/L_D1_State"
SLATE = "SlateInspectorToolset.SlateInspectorToolset."


def _check(results, name, expected, actual):
    ok = expected == actual
    (results["passed"] if ok else results["failed"]).append(name)
    results["details"][name] = {"expected": expected, "actual": actual}
    return ok


async def _type_location_x(call_tool, value):
    """Type into the selected actor's Location X field in the Details panel (Slate key events)."""
    await call_tool(SLATE + "Windows", action="list")
    snap = await call_tool(SLATE + "Snapshot", ref="w_1", maxDepth=60)
    lines = snap.splitlines()
    starts = [i for i, line in enumerate(lines) if 'text "Location"' in line]
    if not starts:
        return False
    slider = [line for line in lines[starts[0]:starts[0] + 6] if "slider" in line][0]
    ref = slider.split("[ref=")[1].rstrip("]")
    return bool(await call_tool(SLATE + "Type", ref=ref, text=str(value), submit=True))


async def run(call_tool):
    results = {"passed": [], "failed": [], "details": {}}
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if vibeue._open_level_package() != MAP:
        if unreal.EditorAssetLibrary.does_asset_exist(MAP):
            vibeue.safe_load_level(MAP)
        else:
            les.new_level(MAP)
    _check(results, "test map open", MAP, vibeue._open_level_package())
    _check(results, "origins are being recorded", True, bool(unreal.TransactionService.is_tracking_origins()))

    cube = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(100, 200, 50))
    cube.set_actor_label("D1_StateCube")
    eas.set_selected_level_actors([cube])
    await asyncio.sleep(0.3)              # let the Details panel show the selection

    # 1. A script-only change: no user edits.
    s0 = vibeue.editor_state()
    with unreal.ScopedEditorTransaction("D1 script move"):
        cube.modify()
        cube.set_actor_location(unreal.Vector(150, 200, 50), False, False)
    s1 = vibeue.editor_state(since=s0)
    _check(results, "script-only change: user_edits empty", [], s1["user_edits"])
    _check(results, "script-only change: listed from scripts", True,
           "D1 script move" in [e["label"] for e in s1["undo"]["from_scripts"]])
    _check(results, "level is dirty", True, s1["level_dirty"])

    # 2. A person's edit (Details panel, Slate key input): named in user_edits.
    typed = await _type_location_x(call_tool, 350)
    await asyncio.sleep(0.3)
    s2 = vibeue.editor_state(since=s1)
    _check(results, "Details panel edit typed", True, typed)
    _check(results, "the edit moved the actor", 350.0, round(cube.get_actor_location().x, 1))
    _check(results, "user edit named", ["Set Location"], [e["label"] for e in s2["user_edits"]])
    _check(results, "user edit names the object", True,
           bool(s2["user_edits"]) and any("D1_State" in o or "StaticMeshActor" in o for o in s2["user_edits"][0]["objects"]))

    # 3. Undone, it is no longer a pending user edit.
    unreal.TransactionService.undo()
    s3 = vibeue.editor_state(since=s1)
    _check(results, "undone user edit is not pending", [], s3["user_edits"])
    _check(results, "undone user edit still listed", True, any(e["undone"] for e in s3["undo"]["from_user"]))

    # 4. preserve_editor puts camera, throttling, PIE settings and selection back.
    before = vibeue.editor_state()
    keeper = vibeue.preserve_editor()
    keeper.enter()
    vibeue.exec_tool("EditorToolset.EditorAppToolset", "SetCameraTransform",
                     {"translation": {"x": 5000, "y": -3000, "z": 2000}, "rotation": {"pitch": -45, "yaw": 30, "roll": 0}})
    unreal.ViewportService.set_fov(60.0)
    if before["background_throttling"] is not None:
        unreal.PerformanceService.set_background_throttling(not before["background_throttling"])
    pie_before = unreal.EngineSettingsService.get_pie_settings()
    unreal.EngineSettingsService.set_pie_settings("ListenServer", 3, True)
    eas.select_nothing()
    out = keeper.exit()
    after = vibeue.editor_state()
    pie_after = unreal.EngineSettingsService.get_pie_settings()
    _check(results, "preserve_editor restored", ["camera", "throttling", "pie_settings", "selection"], out["restored"])
    for key in ("viewport", "background_throttling", "selection"):
        _check(results, "preserve_editor: {} back".format(key), before[key], after[key])
    _check(results, "preserve_editor: PIE settings back", [pie_before.net_mode, pie_before.num_clients],
           [pie_after.net_mode, pie_after.num_clients])
    return results
