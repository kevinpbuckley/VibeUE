"""Regression check for issue #703: import_heightmap refuses (or merges) when the landscape holds hand sculpting
the file does not know about; diff_heightmap; import_heightmap_region.

Run from an execute_python script (top-level await) in a DISPOSABLE editor session with a renderer
(-RenderOffScreen, not -nullrhi). It creates and saves a test map under /Game/VibeUETests/ and writes PNGs
under Saved/VibeUE/Tests/HeightmapGuard; never run it in a real project's editor.

    import sys
    sys.path.insert(0, r"<VibeUE>/scripts/tests")
    import landscape_heightmap_guard_regression as t
    await t.run()

Returns {"passed": [...], "failed": [...], "details": {...}}; every check compares an expected and an actual value.
"""

import asyncio
import os
import struct
import zlib

import unreal

ROOT = "/Game/VibeUETests"
MAP = ROOT + "/L_VibeUE_HeightmapGuard"
LABEL = "VibeUEGuardLandscape"
COMPONENTS, QUADS, SECTIONS = 4, 15, 1
RES = COMPONENTS * QUADS * SECTIONS + 1          # 61 x 61 vertices
HAND_EDITS = [(5, 7), (12, 40), (33, 33), (50, 9), (58, 55)]   # five "hand-moved" vertices
REGION = (20, 30, 10, 8)                          # start x, start y, width, height (starts on the y = 30 border)
REGION2 = (31, 1, 15, 14)                         # x 31..45, y 1..14: ends on the x = 45 border


def _check(results, name, expected, actual):
    ok = expected == actual
    (results["passed"] if ok else results["failed"]).append(name)
    results["details"][name] = {"expected": expected, "actual": actual}
    return ok


def _dir():
    path = os.path.join(unreal.Paths.project_saved_dir(), "VibeUE", "Tests", "HeightmapGuard")
    os.makedirs(path, exist_ok=True)
    return path


def write_png16(path, width, height, values):
    """A 16-bit greyscale PNG (big-endian samples), the format UE's landscape importer reads."""
    raw = b"".join(b"\x00" + struct.pack(">%dH" % width, *values[y * width:(y + 1) * width]) for y in range(height))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 16, 0, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))
    return path


def _field(fn):
    return [fn(x, y) for y in range(RES) for x in range(RES)]


def _dirty():
    utils = unreal.EditorLoadingAndSavingUtils
    return [p.get_name() for p in utils.get_dirty_map_packages()] + [p.get_name() for p in utils.get_dirty_content_packages()]


def _save():
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.LandscapeService.save_landscape(LABEL)


def _heights():
    return list(unreal.LandscapeService.get_height_in_region(LABEL, 0, 0, RES, RES))


async def run():
    results = {"passed": [], "failed": [], "details": {}}
    svc = unreal.LandscapeService
    out = _dir()

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

    baseline = write_png16(os.path.join(out, "baseline.png"), RES, RES,
                           _field(lambda x, y: 32768 + 40 * x + 25 * y))
    nxt = write_png16(os.path.join(out, "next.png"), RES, RES,
                      _field(lambda x, y: 33000 + 30 * x + 45 * y))

    # The "last import": the landscape's edit layer now equals baseline.png exactly.
    r = svc.import_heightmap(LABEL, baseline)
    _check(results, "plain import (old two-argument call) succeeds", (True, False, ""), (r.success, r.refused, r.error_message))
    _save()
    d = svc.diff_heightmap(LABEL, baseline)
    _check(results, "diff after the import: identical", (True, 0, RES * RES), (d.success, d.num_different, d.num_vertices_compared))
    _check(results, "diff does not dirty the map", [], _dirty())

    # A person moves five vertices by hand (+3 m each).
    for x, y in HAND_EDITS:
        z = svc.get_height_in_region(LABEL, x, y, 1, 1)[0]
        assert svc.set_height_in_region(LABEL, x, y, 1, 1, [z + 300.0])
    _save()

    d = svc.diff_heightmap(LABEL, baseline)
    listed = sorted((v.x, v.y) for v in d.vertices)
    _check(results, "diff lists exactly the five hand-moved vertices", (5, sorted(HAND_EDITS)), (d.num_different, listed))
    _check(results, "diff bounding box", (5, 7, 58, 55), (d.min_x, d.min_y, d.max_x, d.max_y))
    _check(results, "diff max delta is about 3 m", True, 299.0 < d.max_abs_delta_cm < 301.0)
    _check(results, "diff_heightmap leaves the map clean", [], _dirty())

    before = _heights()
    r = svc.import_heightmap(LABEL, nxt, baseline)
    _check(results, "guarded import refuses", (False, True, 5), (r.success, r.refused, r.num_hand_edited_vertices))
    _check(results, "refusal lists exactly the five vertices",
           sorted(HAND_EDITS), sorted((v.x, v.y) for v in r.hand_edited_vertices))
    _check(results, "refusal bounding box", (5, 7, 58, 55), (r.hand_edit_min_x, r.hand_edit_min_y, r.hand_edit_max_x, r.hand_edit_max_y))
    _check(results, "refusal message names the count", True, "5 vertices" in r.error_message)
    _check(results, "refused import changed no height", before, _heights())
    _check(results, "refused import left the map clean", [], _dirty())

    r = svc.import_heightmap(LABEL, nxt, baseline, True)
    _check(results, "keep_hand_edits import succeeds and keeps five", (True, False, 5, 5),
           (r.success, r.refused, r.num_hand_edited_vertices, r.num_hand_edits_kept))
    d = svc.diff_heightmap(LABEL, nxt)
    _check(results, "after the merge only the five hand edits differ from the new file",
           (5, sorted(HAND_EDITS)), (d.num_different, sorted((v.x, v.y) for v in d.vertices)))
    hand_kept = all(v.landscape_value - (v.file_value) != 0 for v in d.vertices)
    _check(results, "the hand edits kept their own heights", True, hand_kept)
    _save()

    # Region import: nothing outside the rectangle changes, every vertex inside takes the file's height. The first
    # rectangle starts on a component border (y = 30), the second ends on one (x = 45): a border vertex is stored in
    # both components, and both copies must be written.
    sx, sy, w, h = REGION
    region_values = [20000 + 97 * i for i in range(w * h)]
    region_png = write_png16(os.path.join(out, "region.png"), w, h, region_values)
    for name, (rx, ry, rw, rh), values, png in (
            ("region on a border", REGION, region_values, region_png),
            ("region ending on a border", REGION2, None, None)):
        if values is None:
            values = [41000 - 53 * i for i in range(rw * rh)]
            png = write_png16(os.path.join(out, "region2.png"), rw, rh, values)
        before = _heights()
        r = svc.import_heightmap_region(LABEL, png, rx, ry)
        _check(results, name + ": import succeeds", (True, "%d,%d-%d,%d" % (rx, ry, rx + rw - 1, ry + rh - 1)), (r.success, r.region))
        after = _heights()
        inside = lambda i: rx <= i % RES < rx + rw and ry <= i // RES < ry + rh
        _check(results, name + ": nothing outside the rectangle changed", [],
               [(i % RES, i // RES) for i in range(RES * RES) if not inside(i) and before[i] != after[i]])
        _check(results, name + ": every vertex inside has the file's height", [],
               [(rx + k % rw, ry + k // rw) for k in range(rw * rh)
                if abs(after[(ry + k // rw) * RES + rx + k % rw] - (values[k] - 32768) * 100.0 / 128.0) > 1e-3])
        _save()

    # Region guard: only vertices inside the rectangle count.
    current = os.path.join(out, "current.png")
    svc.export_heightmap(LABEL, current)
    for x, y, dz in ((sx + 2, sy + 3, 150.0), (2, 2, -150.0)):    # one hand edit inside the region, one outside
        z = svc.get_height_in_region(LABEL, x, y, 1, 1)[0]
        svc.set_height_in_region(LABEL, x, y, 1, 1, [z + dz])
    _save()
    before = _heights()
    r = svc.import_heightmap_region(LABEL, region_png, sx, sy, current)
    _check(results, "guarded region import refuses for the edit inside only",
           (False, True, 1, [(sx + 2, sy + 3)]),
           (r.success, r.refused, r.num_hand_edited_vertices, [(v.x, v.y) for v in r.hand_edited_vertices]))
    _check(results, "refused region import changed nothing", before, _heights())
    _check(results, "compares left the map clean", [], _dirty())

    # Argument errors.
    r = svc.import_heightmap(LABEL, nxt, "", True)
    _check(results, "keep_hand_edits without a baseline is an error", (False, True), (r.success, "needs ExpectedBaseline" in r.error_message))
    r = svc.import_heightmap_region(LABEL, region_png, RES - 3, 0)
    _check(results, "a region past the landscape edge is an error", (False, True), (r.success, "outside the landscape" in r.error_message))
    d = svc.diff_heightmap(LABEL, region_png)
    _check(results, "diff of a file of the wrong size is an error", False, d.success)
    return results
