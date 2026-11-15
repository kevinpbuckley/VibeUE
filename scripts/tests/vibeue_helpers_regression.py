"""Regression checks for the vibeue Python module: safe_load_level on a /Temp level (issue #680),
pie_session() and capture_game() (issue #679).

Run from an execute_python script (top-level await) in a DISPOSABLE editor session: it creates and
saves two maps under /Game/VibeUETests/, opens scratch /Temp levels and starts PIE, and leaves the
editor on one of the test maps.

    import sys
    sys.path.insert(0, r"<VibeUE>/scripts/tests")
    import vibeue_helpers_regression as t
    await t.run(r"<an output folder for the PNG>")

Returns {"passed": [...], "failed": [...], "details": {...}}; every check compares an expected and
an actual value. Run headless (no MCP client needed) with the engine's own execute_python tool from
a `-ExecCmds="py <driver>"` script on UnrealEditor.exe -RenderOffscreen; the PIE checks need a
renderer, so not -nullrhi.
"""

import os

import unreal
import vibeue

ROOT = "/Game/VibeUETests"
MAP_A = ROOT + "/L_VibeUE_TargetA"
MAP_B = ROOT + "/L_VibeUE_TargetB"


def _check(results, name, expected, actual):
    ok = expected == actual
    (results["passed"] if ok else results["failed"]).append(name)
    results["details"][name] = {"expected": expected, "actual": actual}
    return ok


def _new_temp_level():
    """A never-saved scratch level (/Temp/Untitled_N), made dirty with one actor."""
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(0, 0, 0))
    return world


def _make_saved_map(path):
    """LevelEditorSubsystem.new_level creates, saves and opens it. Not save_map on a /Temp world:
    that Save-As leaves the map resident, and safe_load_level would then refuse for that reason."""
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).new_level(path)


def _open_package():
    return vibeue._open_level_package()


def _check_safe_load_level(results):
    _make_saved_map(MAP_A)
    _make_saved_map(MAP_B)

    # 1. Without discard_temp the engine refuses a dirty /Temp level (the reported failure).
    _new_temp_level()
    temp_pkg = _open_package()
    _check(results, "temp level is under /Temp/", True, temp_pkg.startswith("/Temp/"))
    try:
        vibeue.safe_load_level(MAP_A, discard_temp=False)
        refused = False
    except Exception as exc:
        refused = "unsaved level changes" in str(exc)
    _check(results, "discard_temp=False still refuses a dirty /Temp level", True, refused)

    # 2. Default: the scratch level is discarded and the target opens.
    loaded = vibeue.safe_load_level(MAP_A)
    _check(results, "safe_load_level loads over a dirty /Temp level", True, bool(loaded))
    _check(results, "open level after the load", MAP_A, _open_package())

    # 3. A dirty REAL map is never discarded.
    unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(100, 0, 0))
    try:
        vibeue.safe_load_level(MAP_B)
        refused = False
    except Exception as exc:
        refused = "unsaved level changes" in str(exc)
    _check(results, "a dirty real map still refuses", True, refused)
    _check(results, "open level after the refusal", MAP_A, _open_package())
    # Leave MAP_A clean again for the PIE checks.
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(
        MAP_A, unreal.LevelEditorUnsavedChangesPolicy.DISCARD)


def _window_values():
    out = []
    for settings in vibeue._play_settings_objects():
        position = settings.get_editor_property("new_window_position")
        out.append([settings.get_editor_property("new_window_width"),
                    settings.get_editor_property("new_window_height"),
                    [position.x, position.y]])
    return out


def _throttling():
    import json
    return json.loads(unreal.PerformanceService.get_background_throttling()).get("throttling_enabled")


async def _check_pie_session(results, out_dir):
    window_before = _window_values()
    throttle_before = _throttling()
    _check(results, "two play-settings objects", 2, len(window_before))
    seen = {}
    async with vibeue.pie_session(800, 600) as session:
        seen["world"] = session["world"] is not None
        seen["pc"] = session["player_controller"] is not None
        seen["window_inside"] = _window_values()
        seen["throttle_inside"] = _throttling()
        size = unreal.WidgetLayoutLibrary.get_viewport_size(session["world"])
        seen["viewport"] = [int(round(size.x)), int(round(size.y))]
        path = await vibeue.capture_game(os.path.join(out_dir, "vibeue_capture_game.png"))
    _check(results, "pie_session yields the PIE world", True, seen["world"])
    _check(results, "pie_session yields player 0's controller", True, seen["pc"])
    _check(results, "window size set on both objects", [[800, 600]] * 2,
           [values[:2] for values in seen["window_inside"]])
    _check(results, "throttling off inside the session", False, seen["throttle_inside"])
    _check(results, "PIE stopped after the block", None, vibeue.game_world())
    _check(results, "window settings restored", window_before, _window_values())
    _check(results, "throttling restored", throttle_before, _throttling())
    with open(path, "rb") as handle:
        magic = handle.read(8)
    _check(results, "capture_game wrote a PNG", True, magic == b"\x89PNG\r\n\x1a\n")
    with open(path, "rb") as handle:
        header = handle.read(24)
    # IHDR width and height: with max_res=None the shot is 1:1 with the PIE viewport, and the
    # viewport is the 800 px wide window (its height came out 602 in one run). A blank test level
    # renders black; the size is what proves the window setting reached PIE.
    png_size = [int.from_bytes(header[16:20], "big"), int.from_bytes(header[20:24], "big")]
    _check(results, "capture is the viewport size", seen["viewport"], png_size)
    _check(results, "PIE viewport is the requested width", 800, seen["viewport"][0])
    results["details"]["png"] = {"path": path, "bytes": os.path.getsize(path)}

    # The block exits on an exception too, and the exception propagates.
    try:
        async with vibeue.pie_session(640, 480):
            raise ValueError("boom")
    except ValueError:
        pass
    _check(results, "PIE stopped after an exception in the block", None, vibeue.game_world())
    _check(results, "window settings restored after an exception", window_before, _window_values())

    # capture_game without a game world raises instead of writing an empty file.
    try:
        await vibeue.capture_game(os.path.join(out_dir, "none.png"))
        raised = False
    except RuntimeError:
        raised = True
    _check(results, "capture_game without PIE raises", True, raised)


async def run(out_dir, pie=True):
    results = {"passed": [], "failed": [], "details": {}}
    try:
        _check_safe_load_level(results)
    except Exception as exc:
        results["failed"].append("safe_load_level checks raised")
        results["details"]["safe_load_level error"] = repr(exc)
    if pie:
        try:
            await _check_pie_session(results, out_dir)
        except Exception as exc:
            results["failed"].append("pie_session checks raised")
            results["details"]["pie_session error"] = repr(exc)
    return results
