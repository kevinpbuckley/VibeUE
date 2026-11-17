"""Regression check for VibeUE.FabPreviewService (contact sheets of Fab listing pictures and installed packs).

Run from an execute_python script (top-level await) in any editor session; it writes only under
Saved/VibeUE/Tests/FabPreview and changes no asset:

    import sys
    sys.path.insert(0, r"<VibeUE>/scripts/tests")
    import fab_preview_regression as t
    await t.run(call_tool)                       # offline checks (engine content + argument errors)
    await t.run(call_tool, urls=[...])           # also download real pictures, e.g. thumbnailUrl values
                                                 # from SearchFabMyLibrary / SearchFabListings
    await t.run(call_tool, blank_folder="/Game/MyPack", refresh=True)
                                                 # opt-in, CHANGES ASSETS: a folder whose meshes were saved with
                                                 # blank thumbnails; checks PackSheet renders them and
                                                 # RefreshThumbnails fixes and saves them

Returns {"passed": [...], "failed": [...], "details": {...}}.
"""

import base64
import json
import os
import struct

import unreal

OUT_DIR_PARTS = ("VibeUE", "Tests", "FabPreview")


def _out(name):
    saved = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
    folder = os.path.join(saved, *OUT_DIR_PARTS)
    os.makedirs(folder, exist_ok=True)
    return os.path.join(folder, name)


def _check(results, name, ok, detail=None):
    (results["passed"] if ok else results["failed"]).append(name)
    if detail is not None:
        results["details"][name] = detail


def _is_jpeg(b64):
    try:
        return base64.b64decode(b64)[:3] == b"\xff\xd8\xff"
    except Exception:  # noqa: BLE001 - a bad payload is a failed check
        return False


def _as_dict(value):
    return json.loads(value) if isinstance(value, str) else value


def _write_bmp(path, width, height, pixel):
    """24-bit BMP; pixel(x, y) -> (r, g, b). Stdlib only."""
    row_pad = (4 - (width * 3) % 4) % 4
    rows = []
    for y in range(height - 1, -1, -1):  # bottom-up
        row = bytearray()
        for x in range(width):
            r, g, b = pixel(x, y)
            row += bytes((b, g, r))
        rows.append(bytes(row) + bytes(1) * row_pad)
    data = b"".join(rows)
    header = b"BM" + struct.pack("<IHHI", 54 + len(data), 0, 0, 54)
    info = struct.pack("<IiiHHIIiiII", 40, width, height, 1, 24, 0, len(data), 2835, 2835, 0, 0)
    with open(path, "wb") as f:
        f.write(header + info + data)
    return path


def _blankness_cases(results, svc):
    """The blank-thumbnail test PackSheet uses (regression for AB#128: 13 wreck meshes saved with the dark
    empty-scene checker were shown as 'cached-thumbnail' cells with no picture)."""
    def dark_checker(x, y):  # the thumbnail the editor saved while the mesh was still building
        v = 18 if ((x // 16) + (y // 16)) % 2 == 0 else 30
        return (v, v, v)

    def noisy_checker(x, y):  # the same after lossy storage: +-2 levels of noise
        r, g, b = dark_checker(x, y)
        n = ((x * 7 + y * 13) % 5) - 2
        return (r + n, g + n, b + n)

    def picture(x, y):  # sky gradient + a lit shape, like a real mesh thumbnail
        if (x - 64) ** 2 + (y - 70) ** 2 < 900:
            return (min(255, 90 + x), 70 + (y % 50), 40)
        return (120 + y // 2, 150 + y // 3, 200)

    def thin_object(x, y):  # a thin shaded bar on the empty checker (a gun barrel, a flat cover plate):
        # a few percent of the cell, the false positive of the first cut (spread alone said "blank")
        if 30 <= x < 100 and abs(y - (40 + x // 3)) <= 1:
            return (60 + x, 55 + x // 2, 40 + (x % 17))
        return dark_checker(x, y)

    cases = (
        ("blank: uniform grey is blank", _write_bmp(_out("uniform.bmp"), 128, 128, lambda x, y: (128, 128, 128)), 1),
        ("blank: dark checker is blank", _write_bmp(_out("dark_checker.bmp"), 128, 128, dark_checker), 1),
        ("blank: noisy dark checker is blank", _write_bmp(_out("noisy_checker.bmp"), 128, 128, noisy_checker), 1),
        ("blank: a real picture is not blank", _write_bmp(_out("picture.bmp"), 128, 128, picture), 0),
        ("blank: a thin object on the checker is not blank", _write_bmp(_out("thin_object.bmp"), 128, 128, thin_object), 0),
    )
    for name, path, want in cases:
        got = svc.classify_image_file(path)
        _check(results, name, got == want, {"file": path, "got": got, "want": want})
    junk = _out("not_an_image.bmp")
    with open(junk, "wb") as f:
        f.write(b"not an image")
    _check(results, "blank: unreadable file is -1", svc.classify_image_file(junk) == -1)


async def run(call_tool, urls=None, blank_folder=None, refresh=False):
    results = {"passed": [], "failed": [], "details": {}}
    svc = unreal.FabPreviewService

    # 1. PackSheet on engine content that every project has
    path = _out("basic_shapes.png")
    if os.path.exists(path):
        os.remove(path)
    s = svc.pack_sheet("/Engine/BasicShapes", "StaticMesh", 12, 128, 0, True, path)
    _check(results, "pack_sheet: success", bool(s.success), s.summary)
    _check(results, "pack_sheet: cells for the meshes", len(s.cells) >= 5, len(s.cells))
    _check(results, "pack_sheet: every cell has a picture",
           all(str(c.image_origin) in ("cached-thumbnail", "rendered") for c in s.cells),
           [(c.label, str(c.image_origin), str(c.error)) for c in s.cells])
    _check(results, "pack_sheet: cells numbered 1..n", [c.index for c in s.cells] == list(range(1, len(s.cells) + 1)))
    _check(results, "pack_sheet: JPEG sheet", str(s.sheet.mime_type) == "image/jpeg" and _is_jpeg(str(s.sheet.data)))
    _check(results, "pack_sheet: file written", str(s.saved_path) != "" and os.path.getsize(str(s.saved_path)) > 1000,
           str(s.saved_path))
    _check(results, "pack_sheet: class counts", any(str(c.asset_class) == "StaticMesh" for c in s.class_counts))
    _check(results, "pack_sheet: bytes on disk", s.bytes_on_disk > 0, s.bytes_on_disk)
    _check(results, "pack_sheet: the sheet of real meshes is a picture",
           str(s.saved_path) != "" and svc.classify_image_file(str(s.saved_path)) == 0)

    # 1b. Blank-thumbnail detection (no assets touched)
    _blankness_cases(results, svc)

    # 2. PackSheet argument errors (no exception, success false, a reason)
    bad = svc.pack_sheet("Engine/BasicShapes")
    _check(results, "pack_sheet: relative path refused", not bad.success and "ContentPath" in str(bad.summary), str(bad.summary))
    none = svc.pack_sheet("/Game/__NoSuchFolder__")
    _check(results, "pack_sheet: empty folder refused", not none.success and "No assets" in str(none.summary), str(none.summary))
    cls = svc.pack_sheet("/Engine/BasicShapes", "NoSuchClassXyz")
    _check(results, "pack_sheet: unknown class refused", not cls.success and "Unknown asset class" in str(cls.summary), str(cls.summary))
    capped = svc.pack_sheet("/Engine/BasicShapes", "*", 2, 64)
    _check(results, "pack_sheet: MaxAssets caps cells", len(capped.cells) == 2 and capped.matched_assets > 2,
           str(capped.summary))

    ref = svc.refresh_thumbnails("Engine/BasicShapes")
    _check(results, "refresh: relative path refused", not ref.success and "ContentPath" in str(ref.summary), str(ref.summary))
    ref = svc.refresh_thumbnails("/Engine/BasicShapes", "StaticMesh", True, False)
    _check(results, "refresh: engine shapes need nothing (no blank thumbnails)",
           ref.success and len(ref.refreshed) == 0 and len(ref.failed) == 0, str(ref.summary))

    # 3. ListingImageSheet argument errors (answered at once, no download)
    for name, kwargs, needle in (
        ("listing: empty list refused", {"imageUrls": [], "labels": []}, "empty"),
        ("listing: http refused", {"imageUrls": ["http://example.com/a.jpg"], "labels": []}, "https"),
    ):
        try:
            r = await call_tool("VibeUE.FabPreviewService.ListingImageSheet", **kwargs)
            _check(results, name, False, r)
        except Exception as exc:  # noqa: BLE001 - the tool reports a refusal as an error
            _check(results, name, needle in str(exc), str(exc)[:200])

    # 4. Real downloads (optional: needs the network)
    if urls:
        r = _as_dict(await call_tool("VibeUE.FabPreviewService.ListingImageSheet", imageUrls=list(urls),
                                     labels=["#%d" % (i + 1) for i in range(len(urls))], cellSize=160,
                                     saveImagePath=_out("listing.jpg")))
        sheet = r.get("sheet") or {}
        cells = r.get("cells") or []
        _check(results, "listing: one cell per URL", len(cells) == len(urls), len(cells))
        _check(results, "listing: pictures decoded", any(c.get("imageOrigin") == "fab" for c in cells),
               [(c.get("index"), c.get("imageOrigin"), c.get("sourceWidth"), c.get("bLowDetail"), c.get("error")) for c in cells])
        _check(results, "listing: JPEG sheet", _is_jpeg(sheet.get("data", "")))
        _check(results, "listing: low-detail flag matches size",
               all(bool(c.get("bLowDetail")) == (max(c.get("sourceWidth", 0), c.get("sourceHeight", 0)) < 256)
                   for c in cells if c.get("imageOrigin") == "fab"))
        results["details"]["listing summary"] = r.get("summary")

    # 5. Opt-in: a real folder saved with blank thumbnails (changes assets when refresh=True)
    if blank_folder:
        before = svc.pack_sheet(blank_folder, "StaticMesh,SkeletalMesh", 64, 128, 0, False)
        blanks = [c for c in before.cells if "holds no picture" in str(c.error)]
        _check(results, "blank folder: blank saved thumbnails are not trusted",
               all(str(c.image_origin) == "none" for c in blanks), {"blank": len(blanks), "summary": str(before.summary)})
        shown = svc.pack_sheet(blank_folder, "StaticMesh,SkeletalMesh", 64, 128, 0, True, _out("blank_folder.jpg"))
        _check(results, "blank folder: every cell shows a picture when rendering",
               all(str(c.image_origin) in ("cached-thumbnail", "rendered") and "render is blank" not in str(c.error)
                   and "rendered picture is blank" not in str(c.error) for c in shown.cells),
               [(c.index, c.label, str(c.image_origin), str(c.error)) for c in shown.cells if str(c.error)])
        if refresh:
            ref = svc.refresh_thumbnails(blank_folder)
            _check(results, "blank folder: refresh succeeded", bool(ref.success), str(ref.summary))
            after = svc.pack_sheet(blank_folder, "StaticMesh,SkeletalMesh", 64, 128, 0, False, _out("blank_folder_after.jpg"))
            _check(results, "blank folder: no blank saved thumbnail left",
                   all(str(c.image_origin) == "cached-thumbnail" for c in after.cells),
                   [(c.index, c.label, str(c.image_origin), str(c.error)) for c in after.cells if str(c.image_origin) != "cached-thumbnail"])
            results["details"]["refresh summary"] = str(ref.summary)
    return results
