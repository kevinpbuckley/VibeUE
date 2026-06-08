"""
build_inputs.py  --  HOST step (needs numpy + pillow). Turns the VibeUE export
(export_manifest.json + export_<Layer>.png + export_height.png produced by
export_terrain_data.py) into the single input map_designer.py needs:

    landcover_grid.json   { "N":120, "lo":..., "hi":..., "<LayerName>":[[...]], ... }

Usage:
    python build_inputs.py --indir <export_dir> [--out landcover_grid.json] [--grid 120]
                           [--flood-from-height] [--flip]

Notes:
  * Each exported weight layer becomes one key in the JSON (its layer name). Point
    config.json "layers" at these names (crop/soil/flood/forest -> layer names).
  * Water: map_designer derives water from your 'flood' layer automatically, so a
    separate river file is OPTIONAL. For precise river polylines + bridges, run
    reference/river_centerline_reference.py on your water layer to make river_world.json.
  * --flood-from-height adds a "FloodFromHeight" layer (lowest 8% of terrain) if you
    have no painted water layer. Map it as "flood" in config.json.
  * --flip mirrors rows north<->south. Use it only if your generated map comes out
    upside-down relative to the engine (depends on your export's row orientation).
"""
import argparse, json, os
import numpy as np
from PIL import Image

ap = argparse.ArgumentParser()
ap.add_argument("--indir", required=True, help="folder containing export_manifest.json + PNGs")
ap.add_argument("--out", default=None, help="output landcover_grid.json (default: <indir>/landcover_grid.json)")
ap.add_argument("--grid", type=int, default=120, help="grid resolution N x N (default 120)")
ap.add_argument("--flood-from-height", action="store_true", help="derive a flood layer from the heightmap")
ap.add_argument("--flood-percentile", type=float, default=8.0, help="lowest %% of terrain treated as flood (default 8)")
ap.add_argument("--flip", action="store_true", help="mirror rows N<->S if your output is upside down")
A = ap.parse_args()

indir = os.path.abspath(A.indir)
man = json.load(open(os.path.join(indir, "export_manifest.json")))
N, LO, HI = A.grid, float(man["lo"]), float(man["hi"])
out = A.out or os.path.join(indir, "landcover_grid.json")

def load_grid(path):
    im = Image.open(path).convert("L").resize((N, N), Image.BILINEAR)
    a = np.asarray(im, np.float32) / 255.0
    # landcover_grid convention: row 0 = SOUTH. export PNG row 0 = south (VibeUE weightmap),
    # so no flip by default; --flip handles exports with the opposite row order.
    if A.flip:
        a = np.flipud(a)
    return a

grid = {"N": N, "lo": LO, "hi": HI}
for layer_name, png in man.get("layers", {}).items():
    p = os.path.join(indir, png)
    if not os.path.exists(p):
        print("  WARN: missing %s, skipping" % png); continue
    grid[layer_name] = load_grid(p).round(3).tolist()
    print("  layer %-18s -> key '%s'  (%dx%d)" % (png, layer_name, N, N))

if A.flood_from_height and man.get("heightmap"):
    hp = os.path.join(indir, man["heightmap"])
    if os.path.exists(hp):
        h = np.asarray(Image.open(hp).resize((N, N), Image.BILINEAR), np.float32)
        if h.ndim == 3: h = h[..., 0]
        thr = np.percentile(h, A.flood_percentile)
        fl = (h <= thr).astype(np.float32)
        if A.flip: fl = np.flipud(fl)
        grid["FloodFromHeight"] = fl.round(3).tolist()
        print("  derived FloodFromHeight (<= %.0fth pct of terrain) -> key 'FloodFromHeight'" % A.flood_percentile)

json.dump(grid, open(out, "w"))
keys = [k for k in grid if k not in ("N", "lo", "hi")]
print("\nWrote %s" % out)
print("  N=%d  lo=%.1f hi=%.1f  span=%.2f km" % (N, LO, HI, (HI-LO)/100000.0))
print("  layer keys: %s" % keys)
print("\nNext: set config.json 'layers' to map crop/soil/flood/forest -> these keys, then run map_designer.py")
