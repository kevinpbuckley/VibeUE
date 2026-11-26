"""Regression checks for vibeue.measure_ab (issue #699).

Needs a RENDERING editor (-RenderOffScreen is fine, -nullrhi is not) with a lit level open (the lane
test level: a floor, coloured BasicShapes, a 20 x 20 grid of spheres tagged "Load", a sun, sky and
fog; the restore checks show and hide the "Load" actors, the A/B check doubles the screen
percentage, a cost every level has). Each phase is one execute_python
call well under a minute and starts and stops its own PIE session:

    import sys
    sys.path.insert(0, r"<VibeUE>/scripts/tests")
    import measure_ab_regression as t
    await t.phase_measure_aa()        # the same state against itself: difference within the spread
    await t.phase_measure_ab()        # 100 % / 200 % screen percentage: a significant GPU difference
    await t.phase_measure_restore()   # first state back after an exception, a cancel, a split job

Each returns {"passed": [...], "failed": [...], "details": {...}}; every check compares an expected
and an actual value. run() runs all three.
"""

import asyncio

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


def _load_actors(world):
    return list(unreal.GameplayStatics.get_all_actors_with_tag(world, "Load"))


def _set_load(state):
    world = vibeue.game_world()
    hidden = state == "off"
    for actor in _load_actors(world):
        actor.set_actor_hidden_in_game(hidden)


def _load_hidden(world):
    actors = _load_actors(world)
    return all(bool(a.get_editor_property("hidden")) for a in actors) if actors else None


async def phase_measure_aa():
    """The same state on both sides: every difference within its reported spread / noise."""
    results = _new_results()
    async with vibeue.pie_session(1280, 720) as session:
        world = session["world"]
        _set_load("on")
        await asyncio.sleep(1.0)
        r = await vibeue.measure_ab({"load_grid": POSES["load_grid"]}, lambda s: None, states=("a", "a2"),
                                    rounds=4, seconds=2.0, warmup_s=0.5, settle_s=0.5, budget_s=45,
                                    job="regression_aa", reset=True)
        _check(results, "A/A finished in one call", True, r.get("done"))
        diff = r["poses"]["load_grid"]["diff"]["a2 - a"]
        for key in ("gpu_ms", "game_ms"):
            d = diff[key]
            bound = max(d["noise"], d["spread"])
            _check(results, "A/A {} difference within the reported spread".format(key), True,
                   abs(d["median"]) <= bound)
            _check(results, "A/A {} not significant".format(key), False, d["significant"])
        results["details"]["result"] = r
    return results


def _set_percentage(state):
    """A state with a known GPU cost: the scene rendered at 100 % or 200 % per axis (4x the pixels)."""
    unreal.SystemLibrary.execute_console_command(vibeue.game_world(), "r.ScreenPercentage {}".format(state))


async def phase_measure_ab():
    """A known cost must come out as a significant GPU difference, and the resolution argument must
    really change what the GPU renders; the first state comes back afterwards."""
    results = _new_results()
    async with vibeue.pie_session(1280, 720) as session:
        world = session["world"]
        await asyncio.sleep(1.0)
        before = _view_state(world)
        r = await vibeue.measure_ab({"load_grid": POSES["load_grid"]}, _set_percentage, states=("100", "200"),
                                    rounds=3, seconds=2.0, resolution=None, warmup_s=0.5, settle_s=0.5,
                                    budget_s=45, job="regression_ab", reset=True)
        _check(results, "A/B finished", True, r.get("done"))
        gpu = r["poses"]["load_grid"]["diff"]["200 - 100"]["gpu_ms"]
        _check(results, "4x the pixels costs GPU time", True, gpu["median"] > 0)
        _check(results, "the GPU difference is significant", True, gpu["significant"])
        _check(results, "first state (100 %) restored", 100.0, vibeue._cvar_float("r.ScreenPercentage"))
        unreal.SystemLibrary.execute_console_command(world, "r.ScreenPercentage {}".format(
            before["r.ScreenPercentage"]))
        _check(results, "view target and camera count restored", before, _view_state(world))
        results["details"]["result"] = r

        # resolution= sets r.ScreenPercentage for the run: 2560x1440 must cost more GPU than 640x360.
        by_res = {}
        for res in ((640, 360), (2560, 1440)):
            m = await vibeue.measure_ab({"load_grid": POSES["load_grid"]}, lambda s: None, states=("x", "y"),
                                        rounds=1, seconds=1.5, resolution=res, warmup_s=0.3, settle_s=0.5,
                                        job="regression_res_{}".format(res[0]), reset=True)
            by_res[res[0]] = {"gpu_ms": m["poses"]["load_grid"]["states"]["x"]["gpu_ms"]["median"],
                              "render_resolution": m["environment"]["render_resolution"]}
        _check(results, "render height follows resolution", [360, 1440],
               [by_res[640]["render_resolution"][1], by_res[2560]["render_resolution"][1]])
        _check(results, "2560x1440 costs more GPU than 640x360", True,
               by_res[2560]["gpu_ms"] > by_res[640]["gpu_ms"])
        _check(results, "screen percentage restored after the resolution runs", before["r.ScreenPercentage"],
               vibeue._cvar_float("r.ScreenPercentage"))
        results["details"]["by_resolution"] = by_res
    return results


async def phase_measure_restore():
    """First state restored after an exception in set_state, after cancellation, and between the
    calls of a job split by budget_s."""
    results = _new_results()
    async with vibeue.pie_session(1280, 720) as session:
        world = session["world"]
        await asyncio.sleep(0.5)
        before = _view_state(world)

        def exploding(state):
            _set_load(state)
            if state == "on":
                raise ValueError("boom")

        try:
            await vibeue.measure_ab({"g": POSES["load_grid"]}, exploding, rounds=1, seconds=0.5,
                                    warmup_s=0.1, settle_s=0.1, job="regression_raise", reset=True)
            error = ""
        except ValueError as exc:
            error = str(exc)
        _check(results, "exception from set_state propagates", "boom", error)
        _check(results, "first state restored after the exception", True, _load_hidden(world))
        _check(results, "view and cvars restored after the exception", before, _view_state(world))

        task = asyncio.ensure_future(vibeue.measure_ab(
            {"g": POSES["load_grid"]}, _set_load, rounds=2, seconds=3.0, warmup_s=0.2, settle_s=0.2,
            job="regression_cancel", reset=True))
        # Round 0 runs "off" then "on": settle 0.2 + off (0.2 + 3.0) + on warmup 0.2 -> sampling "on"
        # from about 3.6 s. Cancel at 5 s, while the spheres are shown.
        await asyncio.sleep(5.0)
        shown_at_cancel = _load_hidden(world) is False
        task.cancel()
        try:
            await task
            cancelled = False
        except asyncio.CancelledError:
            cancelled = True
        _check(results, "the run was cancelled", True, cancelled)
        _check(results, "the second state was on when it was cancelled", True, shown_at_cancel)
        _check(results, "first state restored after cancellation", True, _load_hidden(world))
        _check(results, "view and cvars restored after cancellation", before, _view_state(world))

        kwargs = dict(rounds=2, seconds=1.0, warmup_s=0.2, settle_s=0.2, budget_s=1.0, job="regression_split")
        first = await vibeue.measure_ab({"g": POSES["load_grid"]}, _set_load, reset=True, **kwargs)
        _check(results, "a small budget returns a partial job", False, first.get("done"))
        _check(results, "first state restored between calls", True, _load_hidden(world))
        _check(results, "view and cvars restored between calls", before, _view_state(world))
        second = await vibeue.measure_ab({"g": POSES["load_grid"]}, _set_load, **kwargs)
        _check(results, "the next call finishes the job", True, second.get("done"))
        _check(results, "the job took two calls", 2, second.get("calls"))
        try:
            await vibeue.measure_ab({"g": POSES["overview"]}, _set_load, **kwargs)
            mismatch = ""
        except ValueError as exc:
            mismatch = str(exc)
        _check(results, "other poses under the same job id are refused", True, "reset=True" in mismatch)
        results["details"]["partial"] = first
    return results


async def run():
    results = _new_results()
    for phase in (phase_measure_aa(), phase_measure_ab(), phase_measure_restore()):
        part = await phase
        results["passed"] += part["passed"]
        results["failed"] += part["failed"]
        results["details"].update(part["details"])
    return results
