"""Regression check for issue #694: LandscapeService reads must not dirty the map or add an undo entry.

Run from execute_python scripts (top-level await) in a DISPOSABLE editor session with a renderer
(-RenderOffScreen, not -nullrhi: the edit-layer merge needs the GPU). It creates and saves a test map
under /Game/VibeUETests/, so never run it in a real project's editor.

    import sys
    sys.path.insert(0, r"<VibeUE>/scripts/tests")
    import landscape_read_only_regression as t
    await t.build_fixture()          # call 1: new map, landscape with two edit layers and a painted layer, saved
    await t.run(call_tool)           # call 2: every read, each inside a transaction like execute_python's own

run() returns {"passed": [...], "failed": [...], "details": {...}, "values": {...}}. "values" holds every value
read (floats as repr, exported files as sha256) so two builds can be compared to the bit: run the same fixture
on the old and the new build and compare the two "values" dicts.

What it checks, per read call, on a map that was just saved (nothing dirty):
  - get_dirty_map_packages() and get_dirty_content_packages() are still empty afterwards;
  - the read, made inside ScopedEditorTransaction (execute_python wraps a script without a top-level await in
    one), leaves no transaction in the undo history (the history's newest entry is unchanged).
Before the fix every read below dirtied the map and, inside a transaction, recorded the landscape's heightmap or
weightmap textures (property Source), because FLandscapeEditDataInterface calls Texture->Modify() on first access.
"""

import asyncio
import hashlib
import os

import unreal

ROOT = "/Game/VibeUETests"
MAP = ROOT + "/L_VibeUE_LandscapeReadOnly"
LABEL = "VibeUEReadOnlyLandscape"
LAYER = "VibeUEReadOnlyGrass"
EXTRA_EDIT_LAYER = "VibeUEReadOnlyDetail"
# 4 x 4 components of 15 quads, 1 section: 61 x 61 vertices, 6000 x 6000 cm at scale 100
COMPONENTS, QUADS, SECTIONS = 4, 15, 1
RES = COMPONENTS * QUADS * SECTIONS + 1


def _out_dir():
    path = os.path.join(unreal.Paths.project_saved_dir(), "VibeUE", "Tests", "LandscapeReadOnly")
    os.makedirs(path, exist_ok=True)
    return path


def _dirty():
    utils = unreal.EditorLoadingAndSavingUtils
    return ([p.get_name() for p in utils.get_dirty_map_packages()],
            [p.get_name() for p in utils.get_dirty_content_packages()])


async def _newest_transaction(call_tool):
    rows = await call_tool("EditorToolset.UndoHistoryToolset.GetRecentTransactions", count=1)
    return (rows[0]["label"], str(rows[0]["transactionId"])) if rows else ("", "")


def _sha(path):
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


async def build_fixture():
    """New map, a 61 x 61 landscape with varied heights in two edit layers and one painted layer, saved."""
    svc = unreal.LandscapeService
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if unreal.EditorAssetLibrary.does_asset_exist(MAP):
        les.load_level(MAP)
    else:
        les.new_level(MAP)
    await asyncio.sleep(0.1)
    if svc.landscape_exists(LABEL):
        svc.delete_landscape(LABEL)
    res = svc.create_landscape(unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0), unreal.Vector(100, 100, 100),
                               SECTIONS, QUADS, COMPONENTS, COMPONENTS, LABEL)
    assert res.success, res.error_message

    # Base layer: a slope plus a bump, written as world heights (cm).
    heights = []
    for y in range(RES):
        for x in range(RES):
            bump = 900.0 * max(0.0, 1.0 - ((x - 30) ** 2 + (y - 22) ** 2) / 150.0)
            heights.append(13.0 * x - 7.5 * y + bump)
    assert svc.set_height_in_region(LABEL, 0, 0, RES, RES, heights), "set_height_in_region failed"

    # A second edit layer with its own content, so the merged heightmap differs from layer 0.
    lsub = unreal.get_editor_subsystem(unreal.LandscapeEditorSubsystem)
    landscape = [a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
                 if isinstance(a, unreal.Landscape) and a.get_actor_label() == LABEL][0]
    names = [str(n) for n in lsub.get_edit_layer_names(landscape)]
    if EXTRA_EDIT_LAYER not in names:
        lsub.create_edit_layer(landscape, EXTRA_EDIT_LAYER)
    lsub.set_active_edit_layer(landscape, EXTRA_EDIT_LAYER)
    assert svc.raise_lower_region(LABEL, 4200.0, 4200.0, 800.0, 800.0, 350.0, 400.0), "raise_lower_region failed"

    # A painted layer.
    info = unreal.LandscapeMaterialService.create_layer_info_object(LAYER, ROOT, True)
    svc.add_layer(LABEL, info.asset_path)
    assert svc.paint_layer_at_location(LABEL, LAYER, 2000.0, 2500.0, 900.0, 1.0), "paint failed"

    for _ in range(10):
        await asyncio.sleep(0.1)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    svc.save_landscape(LABEL)
    return {"edit_layers": [str(n) for n in lsub.get_edit_layer_names(landscape)], "dirty_after_save": _dirty()}


def _readers(out_dir):
    svc = unreal.LandscapeService
    return [
        ("get_height_at_location", lambda: svc.get_height_at_location(LABEL, 3010.0, 2230.0).height),
        ("get_height_at_location (between vertices)", lambda: svc.get_height_at_location(LABEL, 1234.5, 4321.5).height),
        ("get_height_in_region", lambda: list(svc.get_height_in_region(LABEL, 0, 0, RES, RES))),
        ("get_layer_weights_at_location",
         lambda: [(s.layer_name, s.weight) for s in svc.get_layer_weights_at_location(LABEL, 2000.0, 2500.0)]),
        ("get_weights_in_region", lambda: list(svc.get_weights_in_region(LABEL, LAYER, 0, 0, RES, RES))),
        ("export_heightmap", lambda: (svc.export_heightmap(LABEL, os.path.join(out_dir, "height.png")),
                                      _sha(os.path.join(out_dir, "height.png")))),
        ("export_weight_map", lambda: (svc.export_weight_map(LABEL, LAYER, os.path.join(out_dir, "weight.png")).success,
                                       _sha(os.path.join(out_dir, "weight.png")))),
        ("get_hole_at_location", lambda: svc.get_hole_at_location(LABEL, 1000.0, 1000.0)),
        ("analyze_terrain", lambda: (lambda a: (a.min_height, a.max_height, a.average_height, a.max_slope_degrees))(
            svc.analyze_terrain(LABEL, 3000.0, 3000.0, 2000.0))),
        ("get_slope_at_location", lambda: svc.get_slope_at_location(LABEL, 3000.0, 2200.0)),
        ("get_normal_at_location", lambda: (lambda n: (n.x, n.y, n.z))(svc.get_normal_at_location(LABEL, 3000.0, 2200.0))),
        ("get_slope_map", lambda: list(svc.get_slope_map(LABEL, 0.0, 0.0, 6000.0, 6000.0))),
        ("find_flat_areas", lambda: [(v.x, v.y, v.z) for v in svc.find_flat_areas(LABEL, 5.0, 100.0, 5)]),
    ]


def _repr(value):
    if isinstance(value, float):
        return repr(value)
    if isinstance(value, (list, tuple)):
        return [_repr(v) for v in value]
    return value if isinstance(value, (str, bool, int)) or value is None else repr(value)


async def run(call_tool):
    results = {"passed": [], "failed": [], "details": {}, "values": {}}
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if world.get_path_name().split(".")[0] != MAP:
        results["failed"].append("fixture map is not open (run build_fixture first)")
        return results
    start_dirty = _dirty()
    if start_dirty != ([], []):
        results["failed"].append("map was dirty before the reads")
        results["details"]["dirty before"] = start_dirty
        return results

    out_dir = _out_dir()
    for name, read in _readers(out_dir):
        before = await _newest_transaction(call_tool)
        label = "VibeUE read-only check: " + name
        try:
            with unreal.ScopedEditorTransaction(label):
                value = read()
        except Exception as exc:  # the check itself failed
            results["failed"].append(name + " raised")
            results["details"][name] = repr(exc)
            continue
        dirty = _dirty()
        after = await _newest_transaction(call_tool)
        results["values"][name] = _repr(value)
        ok = dirty == ([], []) and after == before
        (results["passed"] if ok else results["failed"]).append(name)
        results["details"][name] = {"dirty": dirty, "newest transaction before": before[0],
                                    "newest transaction after": after[0]}
        if dirty != ([], []):
            # leave the next read a clean map to judge
            unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    return results
