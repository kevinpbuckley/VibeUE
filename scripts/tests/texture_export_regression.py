"""Regression check for AssetDiscoveryService.export_texture (#696): the requested file and extension are honoured, the
exporter is one whose SupportsObject accepts the texture, anything else is refused with a reason, and nothing reaches
the engine's check(SupportsTexture(Texture)) in UTextureExporterGeneric::ExportBinary (an editor crash).

Run from an execute_python script in any editor session. It writes source images and exports under
Saved/VibeUE/Tests/TextureExport and imports test textures into /Game/VibeUETests/TextureExport (not saved):

    import sys
    sys.path.insert(0, r"<VibeUE>/scripts/tests")
    import texture_export_regression as t
    t.run()

Cases: RGBA8 PNG, a BC4 (TC_Alpha) grey texture, G8 (TC_Grayscale), 16-bit RGBA, an HDR float texture (EXR import),
a Radiance .hdr (source BGRE8, imported as a long-lat cube), virtual textures (8-bit and float source), a texture with no source data, plus a missing asset, a non-texture asset, a
missing extension and an extension no exporter takes. Returns {"passed": [...], "failed": [...], "details": {...}}.
"""

import os
import shutil
import struct
import zlib

import unreal

ASSET_DIR = "/Game/VibeUETests/TextureExport"


def _saved(*parts):
    saved = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
    path = os.path.join(saved, "VibeUE", "Tests", "TextureExport", *parts)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    return path


def _check(results, name, ok, detail=None):
    (results["passed"] if ok else results["failed"]).append(name)
    if detail is not None:
        results["details"][name] = detail


def _png(path, width, height, channels, depth, pixel):
    color_type = {1: 0, 3: 2, 4: 6}[channels]
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        for x in range(width):
            for v in pixel(x, y)[:channels]:
                raw += struct.pack(">H", v) if depth == 16 else bytes((v,))

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    ihdr = struct.pack(">IIBBBBB", width, height, depth, color_type, 0, 0, 0)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(bytes(raw))) + chunk(b"IEND", b""))
    return path


def _hdr(path, width, height):
    """Radiance RGBE, flat scanlines (width < 8 must be flat; 16 is read flat too)."""
    data = bytearray()
    for y in range(height):
        for x in range(width):
            data += bytes((128 + x * 4, 64 + y * 4, 32, 129))  # exponent 129: values 1..2
    with open(path, "wb") as f:
        f.write(b"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y %d +X %d\n" % (height, width) + bytes(data))
    return path


def _exr(path, width, height):
    """OpenEXR, scanline, no compression, FLOAT B/G/R channels (imports as an HDR float Texture2D)."""
    def attr(name, typ, data):
        return name.encode() + b"\0" + typ.encode() + b"\0" + struct.pack("<i", len(data)) + data
    chlist = b"".join(c + b"\0" + struct.pack("<iBBBBii", 2, 0, 0, 0, 0, 1, 1) for c in (b"B", b"G", b"R")) + b"\0"
    box = struct.pack("<iiii", 0, 0, width - 1, height - 1)
    header = (b"\x76\x2f\x31\x01" + struct.pack("<i", 2)
              + attr("channels", "chlist", chlist) + attr("compression", "compression", b"\0")
              + attr("dataWindow", "box2i", box) + attr("displayWindow", "box2i", box)
              + attr("lineOrder", "lineOrder", b"\0") + attr("pixelAspectRatio", "float", struct.pack("<f", 1.0))
              + attr("screenWindowCenter", "v2f", struct.pack("<ff", 0, 0))
              + attr("screenWindowWidth", "float", struct.pack("<f", 1.0)) + b"\0")
    line_size = width * 4 * 3
    first = len(header) + 8 * height
    offsets = b"".join(struct.pack("<Q", first + y * (8 + line_size)) for y in range(height))
    lines = bytearray()
    for y in range(height):
        lines += struct.pack("<ii", y, line_size)
        for ch in range(3):  # B, G, R
            for x in range(width):
                lines += struct.pack("<f", (0.5 + x / width) * (4.0 if ch == 2 else 1.5) + y * 0.01)
    with open(path, "wb") as f:
        f.write(header + offsets + bytes(lines))
    return path


def _import(filename, name, cls=unreal.Texture2D):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", filename)
    task.set_editor_property("destination_path", ASSET_DIR)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    tex = unreal.load_asset(ASSET_DIR + "/" + name)
    if not isinstance(tex, cls):
        raise RuntimeError("import failed: " + filename)
    return tex


def _export(path, out):
    """(written_path, error) - export_texture raises with the reason on failure."""
    try:
        return unreal.AssetDiscoveryService.export_texture(path, out), None
    except Exception as exc:  # noqa: BLE001 - the script error is the result under test
        return None, str(exc)


def _png_size(path):
    with open(path, "rb") as f:
        head = f.read(24)
    return struct.unpack(">II", head[16:24]) if head[:8] == b"\x89PNG\r\n\x1a\n" else None


def _expect_file(results, name, asset, ext, check_png=None):
    folder = _saved("out", name)
    shutil.rmtree(folder, ignore_errors=True)
    os.makedirs(folder)
    want = os.path.join(folder, "exactly_this_name." + ext)
    written, err = _export(asset, want)
    files = os.listdir(folder)
    ok = (err is None and written and os.path.normcase(os.path.normpath(written)) == os.path.normcase(os.path.normpath(want))
          and files == ["exactly_this_name." + ext] and os.path.getsize(want) > 0)
    detail = {"written": written, "error": err, "files": files}
    if ok and check_png:
        detail["png_size"] = _png_size(want)
        ok = detail["png_size"] == check_png
    _check(results, name, ok, detail)


def _expect_refusal(results, name, asset, out_name, must_contain):
    folder = _saved("out", name)
    shutil.rmtree(folder, ignore_errors=True)
    os.makedirs(folder)
    written, err = _export(asset, os.path.join(folder, out_name) if out_name else "")
    ok = written in (None, "") and err is not None and all(s in err for s in must_contain) and os.listdir(folder) == []
    _check(results, name, ok, {"error": err, "files": os.listdir(folder)})


def run():
    results = {"passed": [], "failed": [], "details": {}}
    rgba = _import(_png(_saved("src", "rgba8.png"), 64, 32, 4, 8, lambda x, y: (x * 4, y * 8, 128, 200)), "T_RGBA8")
    bc4 = _import(_png(_saved("src", "grey_bc4.png"), 32, 32, 1, 8, lambda x, y: ((x * 8) & 255,)), "T_BC4")
    bc4.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_ALPHA)
    g8 = _import(_png(_saved("src", "grey_g8.png"), 32, 32, 1, 8, lambda x, y: ((y * 8) & 255,)), "T_G8")
    g8.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_GRAYSCALE)
    rgba16 = _import(_png(_saved("src", "rgba16.png"), 16, 16, 4, 16, lambda x, y: (x * 4000, y * 4000, 1000, 65535)), "T_RGBA16")
    hdr = _import(_hdr(_saved("src", "radiance.hdr"), 16, 16), "T_HDR", unreal.TextureCube)  # .hdr imports as a cube
    exr = _import(_exr(_saved("src", "float.exr"), 16, 16), "T_EXR")
    vt = _import(_png(_saved("src", "vt.png"), 256, 256, 4, 8, lambda x, y: (x, y, 64, 255)), "T_VT")
    vt.set_editor_property("virtual_texture_streaming", True)
    vt_hdr = _import(_exr(_saved("src", "vt_float.exr"), 128, 128), "T_VT_EXR")
    vt_hdr.set_editor_property("virtual_texture_streaming", True)
    no_source = unreal.new_object(unreal.Texture2D)

    p = lambda t: t.get_path_name()  # noqa: E731

    # RGBA8: every 8-bit format, and the PNG carries the texture's size
    _expect_file(results, "rgba8_png", p(rgba), "png", check_png=(64, 32))
    _expect_file(results, "rgba8_tga", p(rgba), "tga")
    _expect_file(results, "rgba8_bmp", p(rgba), "bmp")
    _expect_file(results, "rgba8_dds", p(rgba), "dds")
    _expect_file(results, "rgba8_png_upper_case_ext", p(rgba), "PNG")
    _expect_refusal(results, "rgba8_exr_refused", p(rgba), "x.exr", ["as .exr", "png"])
    _expect_refusal(results, "rgba8_hdr_refused", p(rgba), "x.hdr", ["as .hdr"])
    _expect_refusal(results, "rgba8_jpg_refused", p(rgba), "x.jpg", ["as .jpg"])

    # BC4 / alpha and G8: the source is G8 whatever the compression
    _expect_file(results, "bc4_png", p(bc4), "png", check_png=(32, 32))
    _expect_file(results, "bc4_tga", p(bc4), "tga")
    _expect_file(results, "g8_png", p(g8), "png", check_png=(32, 32))
    _expect_file(results, "g8_bmp", p(g8), "bmp")

    # 16-bit: PNG yes, TGA / BMP no
    _expect_file(results, "rgba16_png", p(rgba16), "png", check_png=(16, 16))
    _expect_refusal(results, "rgba16_tga_refused", p(rgba16), "x.tga", ["as .tga"])

    # HDR float source (EXR import): .exr and .dds yes; .png would have been check(SupportsTexture) in
    # AssetExportTask + TextureExporterPNG
    _expect_file(results, "float_exr", p(exr), "exr")
    _expect_file(results, "float_dds", p(exr), "dds")
    _expect_refusal(results, "float_png_refused", p(exr), "x.png", ["as .png", "exr"])
    _expect_refusal(results, "float_tga_refused", p(exr), "x.tga", ["as .tga"])

    # Radiance .hdr (source BGRE8; the importer makes a long-lat TextureCube of it)
    _expect_file(results, "hdr_cube_hdr", p(hdr), "hdr")
    _expect_refusal(results, "hdr_cube_png_refused", p(hdr), "x.png", ["as .png", "hdr"])

    # Virtual textures (8-bit and float source)
    _expect_file(results, "vt_png", p(vt), "png", check_png=(256, 256))
    _expect_file(results, "vt_float_exr", p(vt_hdr), "exr")
    _expect_refusal(results, "vt_float_png_refused", p(vt_hdr), "x.png", ["as .png", "virtual texture"])

    # No source data, bad arguments
    _expect_refusal(results, "no_source_refused", p(no_source), "x.png", ["no source data"])
    _expect_refusal(results, "missing_asset_refused", ASSET_DIR + "/T_DoesNotExist", "x.png", ["no texture at"])
    _expect_refusal(results, "not_a_texture_refused", "/Engine/BasicShapes/Cube", "x.png", ["not a texture"])
    _expect_refusal(results, "no_extension_refused", p(rgba), "no_extension", ["has no extension", "png"])
    _expect_refusal(results, "empty_path_refused", p(rgba), "", ["must both be given"])

    # An existing file is overwritten, not renamed
    folder = _saved("out", "overwrite")
    shutil.rmtree(folder, ignore_errors=True)
    os.makedirs(folder)
    target = os.path.join(folder, "same.png")
    with open(target, "wb") as f:
        f.write(b"old")
    written, err = _export(p(rgba), target)
    _check(results, "overwrites_existing_file", err is None and os.listdir(folder) == ["same.png"] and _png_size(target) == (64, 32),
           {"error": err, "files": os.listdir(folder)})

    # A relative path is taken relative to the project directory
    rel = "Saved/VibeUE/Tests/TextureExport/out/relative/rel.tga"
    shutil.rmtree(_saved("out", "relative"), ignore_errors=True)
    written, err = _export(p(rgba), rel)
    full = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), rel)
    _check(results, "relative_path_under_project", err is None and os.path.isfile(full), {"written": written, "error": err})
    return results
