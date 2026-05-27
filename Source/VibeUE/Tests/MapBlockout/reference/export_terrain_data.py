"""
export_terrain_data.py  --  STAGE 0 (runs INSIDE Unreal Engine, via VibeUE).

Exports everything map_designer.py needs from a VibeUE-generated landscape:
  * the landscape's world bounds + resolution           -> export_manifest.json
  * each paint (weight) layer as an 8-bit grayscale PNG  -> export_<Layer>.png
  * the heightmap as a 16-bit PNG                         -> export_height.png

It uses ONLY the Unreal Python API (unreal.LandscapeService) -- no numpy/PIL -- so it
runs in the engine's interpreter. The host script build_inputs.py then turns these files
into landcover_grid.json (+ optional river_world.json).

HOW TO RUN -- pick one:
  A) VibeUE MCP:  paste this file into  mcp__VibeUE__execute_python_code  (set LANDSCAPE/OUT_DIR below first), or
  B) UE Editor:   Tools > Execute Python Script... and select this file, or the Output Log Python prompt.

CONFIGURE these two lines, then run:
"""
LANDSCAPE = ""          # landscape actor name/label; "" = auto-pick the first one in the level
OUT_DIR   = ""          # where to write PNGs + manifest; "" = <Project>/Saved/Terrain/MapMakerExport

# ---------------------------------------------------------------------------
import unreal, os, json

svc = unreal.LandscapeService

def log(m): unreal.log("[MapMaker] " + str(m))

# ---- resolve output dir ----
if not OUT_DIR:
    proj = unreal.Paths.project_saved_dir()              # <Project>/Saved/
    OUT_DIR = os.path.join(os.path.abspath(proj), "Terrain", "MapMakerExport")
OUT_DIR = os.path.abspath(OUT_DIR)
os.makedirs(OUT_DIR, exist_ok=True)

# ---- resolve landscape ----
lands = svc.list_landscapes()
names = []
for L in lands:
    # list entries expose a name/label depending on engine version; be defensive
    nm = getattr(L, "name", None) or getattr(L, "label", None) or getattr(L, "actor_label", None) or str(L)
    names.append(nm)
log("Landscapes in level: %s" % names)
if not LANDSCAPE:
    if not names:
        raise RuntimeError("No landscape found in the level. Generate/import one with VibeUE first.")
    LANDSCAPE = names[0]
log("Using landscape: %s" % LANDSCAPE)

# ---- bounds + resolution (from the landscape actor's transform + bounds) ----
info = svc.get_landscape_info(LANDSCAPE)
log("get_landscape_info -> %s" % info)

# Robustly get world AABB from the actor (works across engine versions).
lo_x = lo_y = hi_x = hi_y = None
try:
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = None
    for a in eas.get_all_level_actors():
        if a.get_actor_label() == LANDSCAPE or a.get_name() == LANDSCAPE:
            actor = a; break
    if actor is None:                                    # fall back to first Landscape actor
        for a in eas.get_all_level_actors():
            if isinstance(a, unreal.Landscape): actor = a; LANDSCAPE = a.get_actor_label(); break
    origin, extent = actor.get_actor_bounds(False)       # world-space AABB
    lo_x, hi_x = origin.x - extent.x, origin.x + extent.x
    lo_y, hi_y = origin.y - extent.y, origin.y + extent.y
except Exception as e:
    log("bounds via actor failed (%s); trying info fields" % e)
    for kx in ("min_x", "world_min_x", "origin_x"):
        if hasattr(info, kx): lo_x = getattr(info, kx)
    # if this fails, set bounds manually in the manifest afterwards.

# map_designer expects a SQUARE, origin-centred world. Use the symmetric half-span.
half = max(abs(lo_x), abs(hi_x), abs(lo_y), abs(hi_y))
LO, HI = -half, half
log("World bounds (square, centred): lo=%.1f hi=%.1f  (span=%.1f cm = %.2f km)" % (LO, HI, HI-LO, (HI-LO)/100000.0))

# ---- layers ----
layers = svc.list_layers(LANDSCAPE)
layer_names = []
for L in layers:
    nm = getattr(L, "name", None) or getattr(L, "layer_name", None) or str(L)
    layer_names.append(nm)
log("Paint layers: %s" % layer_names)

exported = {}
for nm in layer_names:
    out = os.path.join(OUT_DIR, "export_%s.png" % nm)
    try:
        res = svc.export_weight_map(LANDSCAPE, nm, out)
        exported[nm] = os.path.basename(out)
        log("  exported layer %-16s -> %s  (%s)" % (nm, os.path.basename(out), res))
    except Exception as e:
        log("  FAILED to export layer %s: %s" % (nm, e))

# ---- heightmap ----
hm = os.path.join(OUT_DIR, "export_height.png")
try:
    svc.export_heightmap(LANDSCAPE, hm)
    log("  exported heightmap -> export_height.png")
    hm_name = "export_height.png"
except Exception as e:
    log("  heightmap export failed: %s" % e); hm_name = None

# ---- manifest ----
manifest = {
    "landscape": LANDSCAPE,
    "lo": LO, "hi": HI,
    "heightmap": hm_name,
    "layers": exported,                      # {layer_name: png_filename}
    "note": "Edit 'layers' mapping in config.json (crop/soil/flood/forest) to these layer names.",
}
with open(os.path.join(OUT_DIR, "export_manifest.json"), "w") as f:
    json.dump(manifest, f, indent=2)
log("Wrote export_manifest.json")
log("DONE. Next (on the host): python build_inputs.py  (point it at this OUT_DIR)")
print("[MapMaker] Export complete ->", OUT_DIR)
