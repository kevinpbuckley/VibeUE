"""Regression checks for vibeue.capture_poses / contact_sheet / PoseCamera (issue #698).

Needs a RENDERING editor (-RenderOffScreen is fine, -nullrhi is not) with a level open that has
something to look at (the lane test level: a floor, coloured BasicShapes, a 20 x 20 grid of spheres
tagged "Load", a sun, sky and fog; any lit level works if POSES is adjusted). Each phase is one
execute_python call well under a minute and starts and stops its own PIE session:

    import sys
    sys.path.insert(0, r"<VibeUE>/scripts/tests")
    import capture_poses_regression as t
    await t.phase_capture(r"<an output folder>")        # 8 poses at 1920x1080 from 1280x720, sheet
    await t.phase_capture_errors(r"<an output folder>") # no PIE; exception half-way

Each returns {"passed": [...], "failed": [...], "details": {...}}; every check compares an expected
and an actual value. run(out_dir) runs both. Look at the PNGs and the sheet afterwards.
"""

import asyncio
import os

import unreal
import vibeue

POSES = {
    "overview": ((-1600, 0, 700), 0, -18),
    "red_cube": ((150, -650, 260), -20, -12),
    "blue_sphere": ((150, 550, 300), 40, -10),
    "green_cylinder": ((-200, 1200, 350), -40, -15),
    "yellow_cone": ((-900, -1100, 400), 40, -15),
    "load_grid": ((900, 0, 900), 0, -35),
    "wall": ((0, 0, 300), 180, -5, 60),
    "sky": ((0, 0, 200), 90, 25),
}


def _check(results, name, expected, actual):
    ok = expected == actual
    (results["passed"] if ok else results["failed"]).append(name)
    results["details"][name] = {"expected": expected, "actual": actual}
    return ok


def _new_results():
    return {"passed": [], "failed": [], "details": {}}


def _view_state(world):
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    hud = pc.get_hud()
    return {
        "view_target": pc.get_view_target().get_name(),
        "show_hud": bool(hud.get_editor_property("show_hud")) if hud else None,
        "r.ScreenPercentage": vibeue._cvar_float("r.ScreenPercentage"),
        "r.HighResScreenshotDelay": vibeue._cvar_int("r.HighResScreenshotDelay"),
        "cameras": len(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)),
    }


async def phase_capture(out_dir):
    """Eight poses at 1920x1080 from a 1280x720 window, one with buffers, then a contact sheet."""
    results = _new_results()
    folder = os.path.join(out_dir, "poses")
    async with vibeue.pie_session(1280, 720) as session:
        world = session["world"]
        await asyncio.sleep(1.0)
        size = unreal.WidgetLayoutLibrary.get_viewport_size(world)
        # A 1280x720 floating PIE window has a 1280x688 viewport (title bar) at 100 % DPI.
        _check(results, "the viewport is smaller than the 1920x1080 output", True,
               size.x <= 1280 and size.y <= 720)
        results["details"]["viewport"] = [int(size.x), int(size.y)]
        # Non-default values first, so "restored" cannot pass by never having been changed.
        unreal.SystemLibrary.execute_console_command(world, "r.ScreenPercentage 80")
        unreal.SystemLibrary.execute_console_command(world, "r.HighResScreenshotDelay 3")
        before = _view_state(world)
        during = []

        async def watch():
            while True:
                during.append(_view_state(world))
                await asyncio.sleep(0.25)

        watcher = asyncio.ensure_future(watch())
        try:
            paths = await vibeue.capture_poses(POSES, folder, size=(1920, 1080), settle=1.0)
        finally:
            watcher.cancel()
        mid = during[len(during) // 2] if during else {}
        _check(results, "during the run: pose camera is the view target", "CameraActor",
               mid.get("view_target", "")[:len("CameraActor")])
        _check(results, "during the run: HUD hidden, 100 %, 8 run-up frames, one camera",
               [False, 100.0, 8, 1], [mid.get("show_hud"), mid.get("r.ScreenPercentage"),
                                      mid.get("r.HighResScreenshotDelay"), mid.get("cameras")])
        _check(results, "eight lit PNGs", 8, len(paths))
        _check(results, "every PNG is 1920x1080", [(1920, 1080)] * 8, [vibeue._png_size(p) for p in paths])
        buffers = await vibeue.capture_poses({"overview": POSES["overview"]}, folder, settle=0.5,
                                             buffers=("BaseColor", "WorldNormal", "Roughness"))
        _check(results, "lit + three buffers", 4, len(buffers))
        _check(results, "buffer PNGs are 1920x1080", [(1920, 1080)] * 4,
               [vibeue._png_size(p) for p in buffers])
        after = _view_state(world)
        _check(results, "view target, HUD, cvars and camera count restored", before, after)
        results["details"]["state"] = before
        unreal.SystemLibrary.execute_console_command(world, "r.ScreenPercentage 0")
        unreal.SystemLibrary.execute_console_command(world, "r.HighResScreenshotDelay 4")
    sheet = vibeue.contact_sheet(paths, os.path.join(out_dir, "sheet.png"))
    _check(results, "contact sheet written", True, vibeue._png_size(sheet) is not None)
    results["details"]["paths"] = paths + buffers + [sheet]
    results["details"]["sheet_size"] = vibeue._png_size(sheet)
    return results


async def phase_capture_errors(out_dir):
    """No PIE -> a clear error; an exception half-way -> everything restored."""
    results = _new_results()
    try:
        await vibeue.capture_poses(POSES, out_dir)
        raised = ""
    except vibeue.PoseCaptureError as exc:
        raised = str(exc)
    _check(results, "no PIE raises PoseCaptureError naming PIE", True, "PIE is not running" in raised)
    async with vibeue.pie_session(1280, 720) as session:
        world = session["world"]
        await asyncio.sleep(0.5)
        unreal.SystemLibrary.execute_console_command(world, "r.ScreenPercentage 80")
        before = _view_state(world)
        try:
            await vibeue.capture_poses({"a": POSES["overview"]}, os.path.join(out_dir, "err"),
                                       settle=0.2, buffers=("NoSuchBuffer",))
            error = ""
        except Exception as exc:
            error = type(exc).__name__
        _check(results, "a bad buffer mode raises", True, bool(error))
        _check(results, "state restored after the exception", before, _view_state(world))
        unreal.SystemLibrary.execute_console_command(world, "r.ScreenPercentage 0")
    return results


async def run(out_dir):
    results = _new_results()
    for phase in (phase_capture(out_dir), phase_capture_errors(out_dir)):
        part = await phase
        results["passed"] += part["passed"]
        results["failed"] += part["failed"]
        results["details"].update(part["details"])
    return results
