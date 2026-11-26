"""Regression check for issue #717: every LandscapeService height writer updates BOTH stored copies of a vertex on a
component border, so a write that starts, ends or lies on a border changes the merged (rendered) heights there.

A border vertex is stored once in each component that shares it. Before the fix a rectangle starting or ending on a
border wrote only one copy, the edit-layer merge kept the other, and the border row of the write never showed.

Run from an execute_python script (top-level await) in a DISPOSABLE editor session with a renderer
(-RenderOffScreen, not -nullrhi: the edit-layer merge needs the GPU). It creates and saves a test map under
/Game/VibeUETests/ and writes PNGs under Saved/VibeUE/Tests/BorderWrite; never run it in a real project's editor.

    import sys
    sys.path.insert(0, r"<VibeUE>/scripts/tests")
    import landscape_border_write_regression as t
    await t.run()

Returns {"passed": [...], "failed": [...], "details": {...}}. Per write it checks, on a landscape with one edit layer:
  - the merged heights (get_height_in_region) inside the rectangle are the requested ones (set_height_in_region) or
    moved by the requested delta (raise_lower_region), border vertices included;
  - nothing outside the rectangle changed;
  - the merged view equals the edit layer everywhere (export_heightmap, then diff_heightmap: 0 differences), which
    also covers the brush writers (flatten, sculpt, smooth, noise, crater).
"""

import asyncio
import os

import unreal

ROOT = "/Game/VibeUETests"
MAP = ROOT + "/L_VibeUE_LandscapeBorderWrite"
LABEL = "VibeUEBorderLandscape"
COMPONENTS, QUADS, SECTIONS = 4, 15, 1
RES = COMPONENTS * QUADS * SECTIONS + 1          # 61 x 61 vertices; component borders at 0, 15, 30, 45, 60
STEP = 100.0 / 128.0                             # one 16-bit height step in cm at Z scale 100

# set_height_in_region rectangles: (name, x, y, width, height)
RECTS = [
    ("starts on two borders", 30, 30, 5, 4),
    ("ends on two borders", 10, 41, 6, 5),
    ("a column lying on a border", 45, 20, 1, 5),
    ("a row lying on a border", 3, 15, 7, 1),
    ("a single corner vertex", 15, 45, 1, 1),
    ("the landscape's bottom edge row", 20, 60, 4, 1),
]


def _check(results, name, expected, actual):
    ok = expected == actual
    (results["passed"] if ok else results["failed"]).append(name)
    results["details"][name] = {"expected": expected, "actual": actual}
    return ok


def _heights():
    return list(unreal.LandscapeService.get_height_in_region(LABEL, 0, 0, RES, RES))


def _layer_matches_merged(out):
    """Number of vertices whose edit-layer height differs from the merged view (0 = consistent)."""
    svc = unreal.LandscapeService
    path = os.path.join(out, "merged.png")
    svc.export_heightmap(LABEL, path)
    d = svc.diff_heightmap(LABEL, path, 10)
    return d.num_different, [(v.x, v.y) for v in d.vertices]


def _outside(before, after, x0, y0, x1, y1):
    return [(i % RES, i // RES) for i in range(RES * RES)
            if not (x0 <= i % RES <= x1 and y0 <= i // RES <= y1) and before[i] != after[i]]


async def run():
    results = {"passed": [], "failed": [], "details": {}}
    svc = unreal.LandscapeService
    out = os.path.join(unreal.Paths.project_saved_dir(), "VibeUE", "Tests", "BorderWrite")
    os.makedirs(out, exist_ok=True)

    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if unreal.EditorAssetLibrary.does_asset_exist(MAP):
        les.load_level(MAP)
    else:
        les.new_level(MAP)
    await asyncio.sleep(0.1)
    if svc.landscape_exists(LABEL):
        svc.delete_landscape(LABEL)
    created = svc.create_landscape(unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0), unreal.Vector(100, 100, 100),
                                   SECTIONS, QUADS, COMPONENTS, COMPONENTS, LABEL)
    if not _check(results, "fixture landscape created", True, created.success):
        return results
    base = [10.0 * (x % 7) + 6.0 * (y % 5) for y in range(RES) for x in range(RES)]
    svc.set_height_in_region(LABEL, 0, 0, RES, RES, base)
    _check(results, "fixture: merged view equals the edit layer", (0, []), _layer_matches_merged(out))

    for name, x, y, w, h in RECTS:
        before = _heights()
        wanted = [before[(y + j) * RES + x + i] + 250.0 for j in range(h) for i in range(w)]
        ok = svc.set_height_in_region(LABEL, x, y, w, h, wanted)
        after = _heights()
        got = [after[(y + j) * RES + x + i] for j in range(h) for i in range(w)]
        missed = [(x + k % w, y + k // w) for k in range(w * h) if abs(got[k] - wanted[k]) > STEP]
        _check(results, "set_height_in_region, %s: every vertex shows the new height" % name, (True, []), (ok, missed))
        _check(results, "set_height_in_region, %s: nothing outside changed" % name, [],
               _outside(before, after, x, y, x + w - 1, y + h - 1))
        _check(results, "set_height_in_region, %s: merged view equals the edit layer" % name, (0, []), _layer_matches_merged(out))

    # raise_lower_region with no falloff over x, y 30..45 (both ends on borders).
    before = _heights()
    ok = svc.raise_lower_region(LABEL, 3750.0, 3750.0, 1500.0, 1500.0, 300.0, 0.0)
    after = _heights()
    moved = {(i % RES, i // RES) for i in range(RES * RES) if abs(after[i] - before[i] - 300.0) <= STEP}
    border = [(30, 35), (45, 35), (35, 30), (35, 45), (30, 30), (45, 45)]
    _check(results, "raise_lower_region on borders: border vertices moved", (True, []), (ok, [p for p in border if p not in moved]))
    xs = [p[0] for p in moved] or [0]
    ys = [p[1] for p in moved] or [0]
    _check(results, "raise_lower_region on borders: nothing outside the raised rectangle changed", [],
           _outside(before, after, min(xs), min(ys), max(xs), max(ys)))
    _check(results, "raise_lower_region on borders: merged view equals the edit layer", (0, []), _layer_matches_merged(out))

    # Brush and semantic writers centred so their rectangles reach borders.
    brush_calls = [
        ("flatten_at_location", lambda: svc.flatten_at_location(LABEL, 1500.0, 1500.0, 1500.0, 400.0, 1.0, "Linear")),
        ("sculpt_at_location", lambda: svc.sculpt_at_location(LABEL, 4500.0, 1500.0, 1500.0, 0.8, "Linear")),
        ("smooth_at_location", lambda: svc.smooth_at_location(LABEL, 1500.0, 4500.0, 1500.0, 0.7)),
        ("apply_noise", lambda: svc.apply_noise(LABEL, 4500.0, 4500.0, 1500.0, 150.0, 0.01, 7, 3).success),
        ("create_crater", lambda: svc.create_crater(LABEL, 3000.0, 3000.0, 1500.0, 300.0, 80.0)),
    ]
    for name, call in brush_calls:
        ok = call()
        _check(results, name + ": merged view equals the edit layer", (True, 0, []), (bool(ok),) + _layer_matches_merged(out))

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    return results
