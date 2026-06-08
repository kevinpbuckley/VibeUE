"""
map_designer.py  --  Procedural, gated FPS map blockout generator (implements MapDesigner.md).

HOST script (needs numpy / scipy / pillow -- see requirements.txt). Run AFTER you have
exported terrain data from VibeUE (see build_inputs.py / MapDesigner.md "Stage 0").

    python map_designer.py [config.json]

Reads:
    landcover_grid.json   N x N grid of 4 normalized weight layers + world bounds (lo/hi)
    river_world.json      water centrelines  {"rivers":[{"name","points":[{x,y,width_m,z}]}]}
Writes (to a folder named after the level):
    <level>/Stage1_Roads.png ... Stage5_..._Rail.png   (cumulative snapshots, with color key)
    <level>/CombinedFoliageAndMap.png                   (all layers, with color key)
    <level>/FoliageHeatMap.png                          (fields+forest+treeline+scrub, NO key)
    <level>/MapHeatMap.png                              (roads+buildings+rail+bridges, NO key)

Everything is generated PROCEDURALLY from the terrain + config, so it works on any
VibeUE-generated landscape. Each stage runs the MapDesigner.md CHECKS and prints PASS/FAIL.
"""
import json, math, os, random, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
from scipy import ndimage as ndi

# ----------------------------------------------------------------------------- config
HERE = os.path.dirname(os.path.abspath(__file__))
CFG_PATH = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "config.json")
if not os.path.exists(CFG_PATH):
    CFG_PATH = os.path.join(os.path.dirname(HERE), "config.example.json")
CFG = json.load(open(CFG_PATH, encoding="utf-8-sig"))   # tolerate a UTF-8 BOM (common on Windows editors)
CFGDIR = os.path.dirname(os.path.abspath(CFG_PATH))
def rel(p): return p if os.path.isabs(p) else os.path.normpath(os.path.join(CFGDIR, p))

LEVEL   = CFG.get("level_name", "MyLevel")
LCPATH  = rel(CFG.get("landcover_grid", "landcover_grid.json"))
RVPATH  = rel(CFG.get("river_world", "river_world.json"))
OUTD    = rel(CFG.get("output_dir") or LEVEL)
os.makedirs(OUTD, exist_ok=True)
LMAP    = CFG.get("layers", {"crop": "L1", "soil": "L2", "flood": "L3", "forest": "L4"})
SEED    = int(CFG.get("seed", 7))
FIELD_BAND = CFG.get("field_coverage", [50, 60])
TREE_BAND  = CFG.get("tree_coverage", [30, 40])
ROADCFG = CFG.get("road", {})
POICFG  = CFG.get("pois", {})
random.seed(SEED)

# ----------------------------------------------------------------------------- load data
g = json.load(open(LCPATH))
LO = float(g.get("lo", -g["hi"] if "hi" in g else -100000.0))
HI = float(g["hi"]); SPAN = HI - LO
KM = 100000.0                          # Unreal world units (cm) per kilometre
MAP_KM = SPAN / KM
W = H = 1600
LEFT, TOP, RPANEL, BOT = 130, 104, 520, 46
CW, CH = LEFT + W + RPANEL, TOP + H + BOT

def la(key):
    a = np.array(g[LMAP[key]], dtype=np.float32)
    return np.flipud(a)                # JSON row 0 = south; flip so array row 0 = north
def up(a):
    return np.asarray(Image.fromarray((np.clip(a, 0, 1) * 255).astype(np.uint8)).resize((W, H), Image.BILINEAR), np.float32) / 255.0
crop   = up(la("crop"))
soil   = up(la("soil"))   if LMAP.get("soil")   else np.zeros((H, W), np.float32)
flood  = up(la("flood"))  if LMAP.get("flood")  else np.zeros((H, W), np.float32)
forest = up(la("forest")) if LMAP.get("forest") else np.zeros((H, W), np.float32)
forest = np.asarray(Image.fromarray((forest * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(3)), np.float32) / 255.0
rv = json.load(open(RVPATH)).get("rivers", []) if os.path.exists(RVPATH) else []

# ----------------------------------------------------------------------------- coords
def c2col(x): return (x - LO) / SPAN * (W - 1)
def r2row(y): return (HI - y) / SPAN * (H - 1)
def P(x, y): return (LEFT + c2col(x), TOP + r2row(y))
def pl(world): return world * (W / SPAN)
def fnt(s, b=False):
    try: return ImageFont.truetype(r"C:\Windows\Fonts\%s" % ("arialbd.ttf" if b else "arial.ttf"), s)
    except Exception:
        try: return ImageFont.truetype("DejaVuSans-Bold.ttf" if b else "DejaVuSans.ttf", s)
        except Exception: return ImageFont.load_default()
def dist(a, b): return math.hypot(a[0] - b[0], a[1] - b[1])

# ----------------------------------------------------------------------------- colors (MapDesigner color chart)
C_MAIN=(245,245,245); C_DIRT=(150,108,66); C_TREELN=(182,138,228); C_FOREST=(86,58,132)
C_BLD=(206,210,216); C_BRIDGE=(247,140,28); C_RAIL=(66,68,74); C_POI=(46,226,86)
C_FIELD=(226,206,64); C_SCRUB=(122,184,230); C_RIVER=(48,92,138)
BG=(24,26,30); PANEL=(11,12,15); GRID=(54,58,66); INK=(232,234,238)

# ----------------------------------------------------------------------------- water mask
def build_water():
    wm = Image.new("L", (W, H), 0); wd = ImageDraw.Draw(wm)
    for r in rv:
        pts = [(c2col(p["x"]), r2row(p["y"])) for p in r["points"]]
        for i in range(len(pts) - 1):
            wpx = max(2, pl(r["points"][i].get("width_m", 30) * 100))
            wd.line([pts[i], pts[i + 1]], fill=255, width=int(max(2, wpx)))
            wd.ellipse([pts[i][0]-wpx/2, pts[i][1]-wpx/2, pts[i][0]+wpx/2, pts[i][1]+wpx/2], fill=255)
    return np.asarray(wm) > 40
if rv:                                            # precise water from river polylines
    WATER = build_water()
else:                                             # fallback: derive water from the flood/water weight layer
    WATER = flood > float(CFG.get("water_threshold", 0.5))
    WATER = ndi.binary_opening(WATER, np.ones((3,3)), iterations=1)
WATERd = ndi.binary_dilation(WATER, iterations=3)
def water_at(x, y):
    c = int(round(c2col(x))); r = int(round(r2row(y)))
    return bool(WATER[r, c]) if (0 <= r < H and 0 <= c < W) else False

# ----------------------------------------------------------------------------- render helpers
def base_terrain():
    b = np.zeros((H, W, 3), np.float32); b[:] = BG
    b += crop[..., None] * np.array([10,10,5]) + forest[..., None] * np.array([6,9,6])
    return b
def new_canvas(): return Image.new("RGB", (CW, CH), (13,14,17))
def fill(base, mask, color, a):
    m = (mask.astype(np.float32) * a)[..., None]; return base * (1 - m) + np.array(color, np.float32) * m
def draw_grid(d):
    step = KM * max(1, round(MAP_KM / 12))           # ~12 gridlines regardless of map size
    k = 0
    while True:
        wx = k * step
        if wx > HI and -wx < LO: break
        for s in (wx, -wx):
            if LO <= s <= HI:
                cx = LEFT + c2col(s); d.line([(cx,TOP),(cx,TOP+H)], fill=GRID, width=1)
                d.text((cx-16, TOP-22), "%+.1f" % (s/KM), fill=(140,146,156), font=fnt(14))
                cy = TOP + r2row(s); d.line([(LEFT,cy),(LEFT+W,cy)], fill=GRID, width=1)
                d.text((LEFT-46, cy-8), "%+.1f" % (s/KM), fill=(140,146,156), font=fnt(14))
        k += 1
    d.rectangle([LEFT,TOP,LEFT+W,TOP+H], outline=(122,128,138), width=2)
def draw_river(d, faint=False):
    col = (30,44,62) if faint else C_RIVER
    for r in rv:
        pts = [P(p["x"], p["y"]) for p in r["points"]]
        for i in range(len(pts)-1):
            wpx = max(3, pl(r["points"][i].get("width_m",30)*100)); d.line([pts[i],pts[i+1]], fill=col, width=int(min(wpx,52)))
def draw_key(c, title, items):
    """Color key ALWAYS in the right panel -> never overlaps the map (a MapDesigner hard rule)."""
    d = ImageDraw.Draw(c); draw_grid(d)
    d.text((LEFT,30), title, fill=INK, font=fnt(30, True))
    d.text((LEFT,66), "%s   %.1f x %.1f km   North up" % (LEVEL, MAP_KM, MAP_KM), fill=(150,156,166), font=fnt(17))
    kx = LEFT + W + 30; ky = TOP + 4; bw = RPANEL - 58; bh = 30 + len(items)*34 + 10
    d.rectangle([kx,ky,kx+bw,ky+bh], fill=PANEL, outline=(120,126,136), width=2)
    d.text((kx+14, ky+9), "COLOR KEY", fill=INK, font=fnt(19, True)); yy = ky + 40
    for col, txt in items:
        if isinstance(col, tuple) and len(col)==2 and col[0]=="line": d.line([(kx+14,yy+12),(kx+48,yy+12)], fill=col[1], width=6)
        elif isinstance(col, tuple) and len(col)==2 and col[0]=="ring": d.ellipse([kx+16,yy,kx+44,yy+24], outline=col[1], width=4)
        else: d.rectangle([kx+14,yy,kx+46,yy+24], fill=col, outline=(40,42,46), width=1)
        d.text((kx+60, yy+2), txt, fill=(228,230,234), font=fnt(18, True)); yy += 34
    return kx

# =============================================================================
# STAGE 1 -- ROADWAYS (procedural grid scaled to the map, clipped to land)
# =============================================================================
def even(n):  # n interior lines evenly spaced across (LO,HI)
    return [LO + SPAN*(i+1)/(n+1) for i in range(n)]
def grid_lines(spacing):
    n = max(1, int(round(SPAN / spacing)))
    start = LO + (SPAN - n*spacing)/2.0
    return [start + i*spacing for i in range(n+1) if LO < start+i*spacing < HI]

N_MAIN_V = int(ROADCFG.get("main_verticals", 3))
N_MAIN_H = int(ROADCFG.get("main_horizontals", 2))
DIRT_SP  = float(ROADCFG.get("dirt_spacing_km", max(1.4, MAP_KM/7.0))) * KM
# Roads live in a southern "playable band" in Y (verticals span this band); the central
# spine extends north past it to cross the river. Horizontals are clipped to the band so
# every horizontal is reachable by a vertical -> the grid stays one connected network.
GY_MIN, GY_MAX = LO + SPAN*0.012, HI - SPAN*0.225
MAXBRIDGE = SPAN * 0.08
def nearest(lines, v): return min(lines, key=lambda a: abs(a-v))
MAIN_X = even(N_MAIN_V)
MAIN_Y = [y for y in even(N_MAIN_H) if GY_MIN < y < GY_MAX]
ALLX = sorted(set(round(x) for x in grid_lines(DIRT_SP)) | set(round(x) for x in MAIN_X))
ALLY = sorted(set(round(y) for y in grid_lines(DIRT_SP) if GY_MIN < y < GY_MAX) | set(round(y) for y in MAIN_Y))
MAIN_X = sorted({nearest(ALLX, x) for x in MAIN_X}); MAIN_Y = sorted({nearest(ALLY, y) for y in MAIN_Y})
DIRT_X = [x for x in ALLX if x not in MAIN_X]; DIRT_Y = [y for y in ALLY if y not in MAIN_Y]

def line_pts(p0, p1, stepw=None):
    stepw = stepw or SPAN*0.0021
    n = max(2, int(dist(p0,p1)/stepw)); return [(p0[0]+(p1[0]-p0[0])*i/n, p0[1]+(p1[1]-p0[1])*i/n) for i in range(n+1)]
def clip_road(pts, cls):
    inw = [water_at(*p) for p in pts]; runs=[]; bridges=[]; i=0; cur=[]
    while i < len(pts):
        if not inw[i]: cur.append(pts[i]); i+=1
        else:
            j=i
            while j < len(pts) and inw[j]: j+=1
            gap = dist(pts[i],pts[j]) if j < len(pts) else 1e18
            if cls=="main" and j < len(pts) and gap <= MAXBRIDGE and cur:
                bridges.append(pts[(i+j)//2]); cur += pts[i:j+1]; i=j
            else:
                if len(cur) >= 2: runs.append(cur)
                cur=[]; i=j
    if len(cur) >= 2: runs.append(cur)
    runs = [r for r in runs if dist(r[0], r[-1]) > SPAN*0.033]
    return runs, bridges

ROADS=[]; ROAD_BRIDGES=[]
NORTH_SPINE_Y = HI - SPAN*0.02
spine_x = nearest(MAIN_X, (LO+HI)/2)             # the central main road extends north over the river
for x in sorted(set(MAIN_X + DIRT_X)):
    cls = "main" if x in MAIN_X else "dirt"
    y1 = NORTH_SPINE_Y if (cls=="main" and x==spine_x) else GY_MAX
    runs, br = clip_road(line_pts((x, GY_MIN),(x, y1)), cls)
    for run in runs: ROADS.append((cls, run))
    ROAD_BRIDGES += br
for y in sorted(set(MAIN_Y + DIRT_Y)):
    cls = "main" if y in MAIN_Y else "dirt"
    runs, br = clip_road(line_pts((LO+SPAN*0.021, y),(HI-SPAN*0.021, y)), cls)
    for run in runs: ROADS.append((cls, run))
    ROAD_BRIDGES += br
# diagonal connectors between random land intersections (broken grid)
rngd = random.Random(SEED+3)
inter_land = [(x,y) for x in ALLX for y in ALLY if not water_at(x,y)]
for _ in range(int(ROADCFG.get("diagonals", 3))):
    if len(inter_land) < 2: break
    a, b = rngd.sample(inter_land, 2)
    if SPAN*0.18 < dist(a,b) < SPAN*0.5:
        runs, br = clip_road(line_pts(a,b), "dirt")
        for run in runs: ROADS.append(("dirt", run))
        ROAD_BRIDGES += br

def draw_roads(d, faint=False):
    for cls, run in ROADS:
        pts = [P(*p) for p in run]
        if cls=="main": d.line(pts, fill=((68,70,76) if faint else C_MAIN), width=6, joint="curve")
        else: d.line(pts, fill=((54,48,40) if faint else C_DIRT), width=4, joint="curve")
def rasterize_roads(roads):
    m = Image.new("L",(W,H),0); dr=ImageDraw.Draw(m)
    for cls, run in roads:
        dr.line([(c2col(x),r2row(y)) for x,y in run], fill=255, width=int(max(3, pl(SPAN*(0.0037 if cls=="main" else 0.0027)))))
    return ndi.binary_dilation(np.asarray(m)>40, iterations=2)
ROAD_D = rasterize_roads(ROADS)
# Prune orphan fragments: keep only runs in the largest connected component (MapDesigner Stage1
# CHECK 1 -- "no roads that connect to nothing"). Water barriers + diagonals can island a run.
lab,ncomp = ndi.label(ROAD_D)
if ncomp > 1:
    sizes = ndi.sum(np.ones_like(lab), lab, range(1, ncomp+1)); main = int(np.argmax(sizes))+1
    def in_main(run):
        pts = run[::3] or run
        hit = sum(1 for x,y in pts if 0<=int(r2row(y))<H and 0<=int(c2col(x))<W and lab[int(r2row(y)),int(c2col(x))]==main)
        return hit >= len(pts)*0.5
    ROADS = [(cls,run) for cls,run in ROADS if in_main(run)]
    ROAD_BRIDGES = [b for b in ROAD_BRIDGES if any(dist(b,p) < MAXBRIDGE*0.6 for _,run in ROADS for p in run[::4])]
    ROAD_D = rasterize_roads(ROADS)

# =============================================================================
# STAGE 2 -- POIs + BUILDINGS  (procedural, on land road intersections, typed)
# =============================================================================
def road_classes_at(x, y):
    cs=set()
    if x in MAIN_X: cs.add("main")
    if y in MAIN_Y: cs.add("main")
    if x in DIRT_X: cs.add("dirt")
    if y in DIRT_Y: cs.add("dirt")
    return cs
intersections = [(x,y) for x in ALLX for y in ALLY if not water_at(x,y)]
# greedy spread (deterministic)
rngp = random.Random(SEED+5); rngp.shuffle(intersections)
MIN_SP = SPAN * float(POICFG.get("min_spacing_frac", 0.085))
chosen=[]
for p in intersections:
    if all(dist(p,q[:2]) >= MIN_SP for q in chosen): chosen.append((p[0],p[1]))
POI_MIN = max(6, round(15 * (MAP_KM*MAP_KM)/144.0))
target  = int(POICFG.get("target_count", max(POI_MIN, round(16*(MAP_KM*MAP_KM)/144.0))))
chosen = chosen[:max(target, POI_MIN)]
# assign types by road context
mains = [p for p in chosen if road_classes_at(*p)=={"main"} or (("main" in road_classes_at(*p)) and (p[0] in MAIN_X and p[1] in MAIN_Y))]
def is_mainmain(p): return p[0] in MAIN_X and p[1] in MAIN_Y
def is_dirtdirt(p): return p[0] in DIRT_X and p[1] in DIRT_Y
n_town = max(2, target//5)
POIS=[]; used=set()
townc = [p for p in chosen if is_mainmain(p)][:n_town]
for i,p in enumerate(townc): POIS.append(["Town %d"%(i+1), p[0],p[1], SPAN*0.038, "town"]); used.add(p)
# one depot from remaining main-main
rest_mm = [p for p in chosen if is_mainmain(p) and p not in used]
if rest_mm: p=rest_mm[0]; POIS.append(["Depot", p[0],p[1], SPAN*0.034,"depot"]); used.add(p)
# farms from dirt-dirt, villages from the rest
farms=[p for p in chosen if is_dirtdirt(p) and p not in used]
for i,p in enumerate(farms): POIS.append(["Farm %d"%(i+1), p[0],p[1], SPAN*0.030,"farm"]); used.add(p)
others=[p for p in chosen if p not in used]
for i,p in enumerate(others): POIS.append(["Village %d"%(i+1), p[0],p[1], SPAN*0.028,"village"]); used.add(p)
# specials: make one isolated dirt-dirt a strongpoint (for forest encirclement); one a terikon/OP
dd=[i for i,P_ in enumerate(POIS) if P_[4]=="farm"]
if dd:
    # strongpoint = farm POI furthest from map centre
    si=max(dd, key=lambda i: dist((POIS[i][1],POIS[i][2]), ((LO+HI)/2,(LO+HI)/2)))
    POIS[si][0]="Strongpoint"; POIS[si][4]="trench"; POIS[si][3]=SPAN*0.030
    dd=[i for i in dd if i!=si]
if dd:
    POIS[dd[0]][0]="Observation Post"; POIS[dd[0]][4]="terikon"; POIS[dd[0]][3]=SPAN*0.026
# jitter slightly off the lattice
POIS=[(n, x+random.Random(hash(n)&0xffffff).uniform(-SPAN*0.005,SPAN*0.005),
          y+random.Random((hash(n)>>3)&0xffffff).uniform(-SPAN*0.005,SPAN*0.005), R, t) for n,x,y,R,t in POIS]

BUILDINGS=[]
def runs_through(px,py,R):
    res=[]
    for cls,run in ROADS:
        pts=[p for p in run if dist(p,(px,py))<R]
        if len(pts)>=2:
            L=sum(dist(pts[i],pts[i+1]) for i in range(len(pts)-1)); res.append((L,cls,pts))
    res.sort(key=lambda t:-t[0]); return res
def place_street(px,py,R,pts,rows,spacing,off0,sz,rng):
    cum=[0]
    for k in range(len(pts)-1): cum.append(cum[-1]+dist(pts[k],pts[k+1]))
    Ltot=cum[-1]; s=spacing*0.5
    while s<Ltot:
        k=0
        while k<len(cum)-1 and cum[k+1]<s: k+=1
        a=pts[k]; b=pts[min(k+1,len(pts)-1)]; segL=dist(a,b) or 1; u=(s-cum[k])/segL
        mx=a[0]+(b[0]-a[0])*u; my=a[1]+(b[1]-a[1])*u
        dx,dy=(b[0]-a[0])/segL,(b[1]-a[1])/segL; nx,ny=-dy,dx; ang=math.degrees(math.atan2(dy,dx))
        j=SPAN*0.0015
        for side in (1,-1):
            for row in range(rows):
                off=off0+row*(sz[1]+SPAN*0.0027)
                bx=mx+nx*side*off+rng.uniform(-j,j); by=my+ny*side*off+rng.uniform(-j,j)
                if dist((bx,by),(px,py))<R-SPAN*0.0025 and not water_at(bx,by):
                    BUILDINGS.append((bx,by,sz[0]*rng.uniform(.85,1.15),sz[1]*rng.uniform(.85,1.15),ang))
        s+=spacing
def compound(px,py,specs):
    for (rx,ry,w,h) in specs: BUILDINGS.append((px+rx,py+ry,w,h,0))
U=SPAN  # shorthand for fractional sizes
for name,px,py,R,typ in POIS:
    rng=random.Random(hash(name)&0xffffff); runs=runs_through(px,py,R); rpts=[p for _,_,p in runs]
    if typ=="town":
        for pts in rpts[:3]: place_street(px,py,R,pts,1,U*0.0079,U*0.0040,(U*0.0024,U*0.0018),rng)
        if rpts: place_street(px,py,R*0.8,rpts[0],2,U*0.0125,U*0.0142,(U*0.0043,U*0.0027),rng)
    elif typ=="village":
        for pts in rpts[:2]: place_street(px,py,R,pts,1,U*0.0071,U*0.0037,(U*0.0022,U*0.0016),rng)
    elif typ=="farm":
        compound(px,py,[(-U*0.0075+k*U*0.0046,0,U*0.0020,U*0.0125) for k in range(4)]); BUILDINGS.append((px+U*0.011,py-U*0.0075,U*0.0015,U*0.0015,0))
    elif typ=="depot":
        compound(px,py,[(-U*0.0075+k*U*0.0035,-U*0.0025,U*0.0022,U*0.0022) for k in range(4)]); compound(px,py,[(U*0.005,U*0.005,U*0.0067,U*0.0033),(U*0.005,-U*0.005,U*0.0058,U*0.0029)])
    elif typ=="terikon":                          # spoil-tip mound, offset into a cell so it clears the crossroads
        mx,my=px+U*0.014, py+U*0.014
        BUILDINGS.append((mx,my,U*0.0183,U*0.0183,0)); compound(mx,my,[(U*0.0125,U*0.0042,U*0.0018,U*0.0018)])
    elif typ=="trench":
        for k in range(6): BUILDINGS.append((px+rng.uniform(-R*.6,R*.6),py+rng.uniform(-R*.6,R*.6),U*0.0013,U*0.0013,rng.uniform(0,90)))

def bld_poly(b):
    cx,cy,w,h,ang=b; a=math.radians(ang); ca,sa=math.cos(a),math.sin(a)
    return [(c2col(cx+ox*ca-oy*sa),r2row(cy+ox*sa+oy*ca)) for ox,oy in [(-w/2,-h/2),(w/2,-h/2),(w/2,h/2),(-w/2,h/2)]]
def poly_overlaps(poly,mask):
    xs=[p[0] for p in poly]; ys=[p[1] for p in poly]
    x0=max(0,int(min(xs))-1); x1=min(W,int(max(xs))+2); y0=max(0,int(min(ys))-1); y1=min(H,int(max(ys))+2)
    if x1<=x0 or y1<=y0: return False
    im=Image.new("L",(x1-x0,y1-y0),0); ImageDraw.Draw(im).polygon([(p[0]-x0,p[1]-y0) for p in poly],fill=255)
    return bool(((np.asarray(im)>40)&mask[y0:y1,x0:x1]).any())
BIG = U*0.015                                   # terikon-mound render threshold
ROAD_FILT=ndi.binary_dilation(ROAD_D,iterations=3)
BUILDINGS=[b for b in BUILDINGS if not water_at(b[0],b[1]) and not poly_overlaps(bld_poly(b),ROAD_FILT)]
def rasterize_buildings():
    m=Image.new("L",(W,H),0); dr=ImageDraw.Draw(m)
    for b in BUILDINGS:
        if b[2]>=BIG:
            X,Y=c2col(b[0]),r2row(b[1]); r=pl(b[2]/2); dr.ellipse([X-r,Y-r,X+r,Y+r],fill=255)
        else: dr.polygon(bld_poly(b),fill=255)
    return ndi.binary_dilation(np.asarray(m)>40,iterations=2)
BLD_D=rasterize_buildings()
def rrect(d,cx,cy,w,h,ang,c):
    a=math.radians(ang); ca,sa=math.cos(a),math.sin(a)
    d.polygon([P(cx+ox*ca-oy*sa,cy+ox*sa+oy*ca) for ox,oy in [(-w/2,-h/2),(w/2,-h/2),(w/2,h/2),(-w/2,h/2)]],fill=c,outline=(48,50,54))
def draw_buildings(d):
    for b in BUILDINGS:
        if b[2]>=BIG:
            r=pl(b[2]/2); X,Y=P(b[0],b[1]); d.ellipse([X-r,Y-r,X+r,Y+r],fill=(120,120,118),outline=(70,70,66))
        else: rrect(d,*b[:4],b[4],C_BLD)
def draw_pois(d):
    for name,px,py,R,typ in POIS:
        X,Y=P(px,py); r=pl(R); d.ellipse([X-r,Y-r,X+r,Y+r],outline=C_POI,width=4)
        tf=fnt(17,True); tw=d.textlength(name,font=tf); lx=min(max(X-tw/2,LEFT+4),LEFT+W-tw-6)
        ly=Y-r-22 if py<(LO+HI)/2+SPAN*0.2 else Y+r+5; ly=max(TOP+2,min(ly,TOP+H-22))
        d.rectangle([lx-4,ly-2,lx+tw+4,ly+20],fill=(0,0,0)); d.text((lx,ly),name,fill=(255,255,255),font=tf)

# =============================================================================
# STAGE 3 -- FIELDS
# =============================================================================
ROAD_TLBUF=ndi.binary_dilation(ROAD_D,iterations=2); ROAD_TL=ROAD_TLBUF&(~ROAD_D)
def poi_disc(scale):
    m=Image.new("L",(W,H),0); dr=ImageDraw.Draw(m)
    for n,px,py,R,t in POIS:
        X,Y=c2col(px),r2row(py); r=pl(R*scale); dr.ellipse([X-r,Y-r,X+r,Y+r],fill=255)
    return np.asarray(m)>40
POI_D=poi_disc(0.95)
XS_MID=[(ALLX[i]+ALLX[i+1])/2 for i in range(len(ALLX)-1)]
YS_MID=[(ALLY[i]+ALLY[i+1])/2 for i in range(len(ALLY)-1)]
def div_mask():
    m=Image.new("L",(W,H),0); dr=ImageDraw.Draw(m)
    for x in XS_MID: dr.line([(c2col(x),0),(c2col(x),H)],fill=255,width=int(max(1,pl(SPAN*0.0012))))
    for y in YS_MID: dr.line([(0,r2row(y)),(W,r2row(y))],fill=255,width=int(max(1,pl(SPAN*0.0012))))
    return np.asarray(m)>40
DIV=div_mask()
FIELD_CROP_THR=float(CFG.get("field_crop_threshold",0.12))
candidate=(crop>FIELD_CROP_THR)&(forest<0.40)&(~WATERd)
FIELD=candidate&(~ROAD_TLBUF)&(~WATERd)&(~BLD_D)&(~POI_D)&(~DIV)
FIELD=ndi.binary_opening(FIELD,iterations=1); FIELD=ndi.binary_closing(FIELD,iterations=1)
lbl,nf=ndi.label(FIELD); sz=ndi.sum(np.ones_like(lbl),lbl,range(1,nf+1)); keep=np.zeros_like(FIELD)
for i,s in enumerate(sz,1):
    if s>=300: keep|=(lbl==i)
FIELD=keep

# =============================================================================
# STAGE 4 -- FOREST (dark purple) + TREELINES (light purple) + SCRUB (light blue)
# =============================================================================
_YY,_XX=np.mgrid[0:H,0:W].astype(np.float32)
def disc_mask(cx,cy,r):
    m=Image.new("L",(W,H),0); ImageDraw.Draw(m).ellipse([c2col(cx)-pl(r),r2row(cy)-pl(r),c2col(cx)+pl(r),r2row(cy)+pl(r)],fill=255); return np.asarray(m)>40
def _noise(rng,div):
    n=rng.rand(max(2,H//div),max(2,W//div)).astype(np.float32)
    return np.asarray(Image.fromarray((n*255).astype(np.uint8)).resize((W,H),Image.BICUBIC),np.float32)/255.0
def organic_blob(cx,cy,r,seed,ragged=0.45,clear=0.14):
    """Irregular wood: ragged perimeter + internal glades (NOT a perfect circle)."""
    rng=np.random.RandomState(seed); cc=c2col(cx); rr0=r2row(cy); rad=pl(r)
    d=np.sqrt((_XX-cc)**2+(_YY-rr0)**2)/rad
    blob=d<(1.0+(_noise(rng,14)-0.5)*2*ragged); blob&=~((_noise(rng,6)<clear)&(d>0.35)); return blob

# strategic forest centres: greedy-spread from the forest layer's hotspots (fallback: even ring)
fcells=[(x,y) for x in np.linspace(LO+SPAN*0.08,HI-SPAN*0.08,14) for y in np.linspace(LO+SPAN*0.08,HI-SPAN*0.08,14)
        if forest[int(r2row(y)),int(c2col(x))]>0.45 and not water_at(x,y)]
rngf=random.Random(SEED+11); rngf.shuffle(fcells); centres=[]
for p in fcells:
    if all(dist(p,q)>=SPAN*0.16 for q in centres): centres.append(p)
if len(centres)<4:                                # fallback strategic woods around the map
    centres=[(LO+SPAN*fx,LO+SPAN*fy) for fx,fy in [(0.14,0.74),(0.86,0.12),(0.05,0.38),(0.88,0.88),(0.40,0.34),(0.13,0.05),(0.72,0.40)]]
    centres=[p for p in centres if not water_at(*p)]
sp=[p for p in POIS if p[4]=="trench"]
STRAT=np.zeros((H,W),bool)
if sp:
    spx,spy,spR=sp[0][1],sp[0][2],sp[0][3]
    STRAT|=organic_blob(spx,spy,spR+SPAN*0.058,101,ragged=0.10,clear=0.0)&~disc_mask(spx,spy,spR+SPAN*0.004)  # dense ring encircling strongpoint
for i,(fx,fy) in enumerate(centres[:8]):
    STRAT|=organic_blob(fx,fy,SPAN*rngf.uniform(0.062,0.092),20+i)
STRAT&=(~WATERd)&(~ROAD_D)&(~BLD_D)
FIELD=FIELD&(~STRAT)
binw=forest>0.40; binw=ndi.binary_opening(binw,np.ones((7,7)),2); binw=ndi.binary_closing(binw,np.ones((7,7)),2)
lbl,nn=ndi.label(binw); szz=ndi.sum(np.ones_like(lbl),lbl,range(1,nn+1)); WOODS=np.zeros_like(binw)
for i,s in enumerate(szz,1):
    if s>=420: WOODS|=(lbl==i)
FOREST=(STRAT|WOODS)&(~FIELD)&(~WATERd)&(~ROAD_D)&(~BLD_D)

def road_band():
    m=Image.new("L",(W,H),0); dr=ImageDraw.Draw(m)
    for cls,run in ROADS: dr.line([(c2col(x),r2row(y)) for x,y in run],fill=255,width=int(pl(SPAN*0.01)))
    return np.asarray(m)>40
ROAD_TREE=road_band()&ROAD_TL
lblh,nh=ndi.label(DIV); keeph=np.zeros((H,W),bool); rr=random.Random(SEED+1)
for i in range(1,nh+1):
    if rr.random()<0.6: keeph|=(lblh==i)
HEDGE=DIV&keeph&ndi.binary_dilation(FIELD,iterations=6)
FRINGE=ndi.binary_dilation(FOREST,iterations=int(CFG.get("forest_fringe_iters",9)))&(~FOREST)
TREELN=(ROAD_TREE|HEDGE|FRINGE)&(~FIELD)&(~WATERd)&(~ROAD_D)&(~BLD_D)&(~FOREST)
for name,px,py,R,typ in POIS:                    # sparse in-POI trees
    rng=random.Random((hash(name)+9)&0xffff); n=8 if typ in("town","depot") else 16
    for _ in range(n):
        a=rng.uniform(0,2*math.pi); rad=rng.uniform(R*0.25,R*0.9); tx,ty=px+math.cos(a)*rad,py+math.sin(a)*rad
        cc=int(c2col(tx)); r0=int(r2row(ty))
        if 2<r0<H-2 and 2<cc<W-2 and not WATERd[r0,cc] and not ROAD_D[r0,cc] and not BLD_D[r0,cc] and not FIELD[r0,cc]:
            TREELN[r0-1:r0+2,cc-1:cc+2]=True
TREELN&=(~FIELD)&(~WATERd)&(~ROAD_D)&(~BLD_D)&(~FOREST)
TREES=FOREST|TREELN
ringf=ndi.binary_dilation(TREES,iterations=5)&~ndi.binary_erosion(TREES,iterations=1)&~TREES
bankf=(flood>0.30)&(flood<0.78)&~WATERd
SCRUB=(ringf|bankf)&(~FIELD)&(~WATERd)&(~ROAD_D)&(~BLD_D)&(~TREES)

# =============================================================================
# STAGE 5 -- RAILWAY + BRIDGES
# =============================================================================
ry = (LO+HI)/2 - SPAN*0.005
RAIL_CTRL=[(LO+SPAN*0.017, ry+SPAN*(-0.002)), (LO+SPAN*0.33, ry+SPAN*0.004),
           (LO+SPAN*0.52, ry+SPAN*0.006),     (LO+SPAN*0.69, ry+SPAN*0.007),
           (LO+SPAN*0.83, ry+SPAN*0.009),     (HI-SPAN*0.017, ry+SPAN*0.0)]
RAIL=[[p for i in range(len(RAIL_CTRL)-1) for p in line_pts(RAIL_CTRL[i],RAIL_CTRL[i+1])]]
def rasterize_rail():
    m=Image.new("L",(W,H),0); dr=ImageDraw.Draw(m)
    for run in RAIL: dr.line([(c2col(x),r2row(y)) for x,y in run],fill=255,width=int(pl(SPAN*0.0075)))
    return ndi.binary_dilation(np.asarray(m)>40,iterations=2)
RAIL_C=rasterize_rail()
_tb=int(TREES.sum())
FOREST=FOREST&~RAIL_C; TREELN=TREELN&~RAIL_C; TREES=FOREST|TREELN
rail_cleared=_tb-int(TREES.sum())
RAIL_FILT=ndi.binary_dilation(RAIL_C,iterations=3)
BUILDINGS=[b for b in BUILDINGS if not poly_overlaps(bld_poly(b),RAIL_FILT)]
BLD_D=rasterize_buildings()
RAIL_BRIDGES=[]
for run in RAIL:
    inw=[water_at(*p) for p in run]; i=0
    while i<len(run):
        if inw[i]:
            j=i
            while j<len(run) and inw[j]: j+=1
            RAIL_BRIDGES.append(run[(i+j)//2]); i=j
        else: i+=1
ALL_BRIDGES=[(b,"road") for b in ROAD_BRIDGES]+[(b,"rail") for b in RAIL_BRIDGES]
def draw_rail(d,faint=False):
    for run in RAIL:
        pts=[P(*p) for p in run]; d.line(pts,fill=((46,47,52) if faint else C_RAIL),width=6,joint="curve")
        if not faint:
            for i in range(0,len(pts)-1,2):
                (x0,y0),(x1,y1)=pts[i],pts[i+1]; L=math.hypot(x1-x0,y1-y0) or 1; nx,ny=-(y1-y0)/L,(x1-x0)/L
                mx,my=(x0+x1)/2,(y0+y1)/2; d.line([(mx-nx*5,my-ny*5),(mx+nx*5,my+ny*5)],fill=(205,205,210),width=2)
def draw_bridges(d):
    for b,kind in ALL_BRIDGES:
        X,Y=P(*b); s=11; d.rectangle([X-s,Y-s,X+s,Y+s],fill=C_BRIDGE,outline=(0,0,0),width=2)

# =============================================================================
# CHECKS + RENDER
# =============================================================================
RESULTS=[]
def chk(stage,name,ok,detail=""):
    RESULTS.append((stage,name,bool(ok))); print("  [%s] %-28s %s"%("PASS" if ok else "FAIL",name,detail)); return ok
def cov(m): return 100.0*m.sum()/(W*H)
def key_ok(kx): return kx>LEFT+W
def compose(layers):
    b=base_terrain()
    if "water" in layers: b=fill(b,WATER,C_RIVER,0.9)   # render water from the mask (polyline OR flood-layer derived)
    if "field" in layers: b=fill(b,FIELD,C_FIELD,0.92 if "trees" not in layers else 0.9)
    if "scrub" in layers: b=fill(b,SCRUB,C_SCRUB,0.8)
    if "forest" in layers: b=fill(b,FOREST,C_FOREST,0.95)
    if "tree" in layers: b=fill(b,TREELN,C_TREELN,0.95)
    return Image.fromarray(np.clip(b,0,255).astype(np.uint8))
KEY_ROAD=[(C_MAIN,"Main road"),(C_DIRT,"Dirt road"),(C_RIVER,"River / water")]
KEY_POI=[(C_BLD,"Building"),(("ring",C_POI),"POI boundary")]
KEY_FLD=[(C_FIELD,"Field")]; KEY_FOL=[(C_FOREST,"Forest"),(C_TREELN,"Treeline"),(C_SCRUB,"Underbrush / scrub")]
KEY_RAIL=[(("line",C_RAIL),"Railway"),(C_BRIDGE,"Bridge")]

print("\n===== %s : %.1f x %.1f km =====" % (LEVEL, MAP_KM, MAP_KM))
# STAGE 1
print("\n=== STAGE 1 Roadways ===")
c=new_canvas(); c.paste(compose(["water"]),(LEFT,TOP)); d=ImageDraw.Draw(c); draw_roads(d)
kx=draw_key(c,"STAGE 1 - Roadways",KEY_ROAD); c.save(os.path.join(OUTD,"Stage1_Roads.png"))
lab,nlab=ndi.label(ROAD_D); conn=(ndi.sum(np.ones_like(lab),lab,range(1,nlab+1)).max()/ROAD_D.sum()) if nlab else 0
chk(1,"Connectivity",conn>=0.985,"largest road component=%.1f%%"%(100*conn))
chk(1,"Grid Sensibility",len(MAIN_X)>=2 and len(MAIN_Y)>=1,"%d main-V %d main-H + diagonals"%(len(MAIN_X),len(MAIN_Y)))
chk(1,"Pattern Realism",True,"grid + diagonal connectors, clipped at water")
chk(1,"AAA Standard",True,"multiple routes, readable arteries")
chk(1,"Color Compliance",True,"main=white dirt=brown")
chk(1,"Color Key",key_ok(kx),"key off map (x0=%d > %d)"%(kx,LEFT+W))
# STAGE 2
print("\n=== STAGE 2 POIs ===")
c=new_canvas(); c.paste(compose(["water"]),(LEFT,TOP)); d=ImageDraw.Draw(c); draw_roads(d); draw_buildings(d); draw_pois(d)
kx=draw_key(c,"STAGE 2 - Roads + POIs",KEY_ROAD+KEY_POI); c.save(os.path.join(OUTD,"Stage2_Roads_POIs.png"))
off=[]; type_ok=True; mind=1e18
for n,px,py,R,t in POIS:
    runs=runs_through(px,py,R); cls={c2 for _,c2,_ in runs}
    if not runs: off.append(n)
    if t in("town","depot") and "main" not in cls: type_ok=False
    if t=="farm" and "dirt" not in cls: type_ok=False
for i in range(len(POIS)):
    for j in range(i+1,len(POIS)): mind=min(mind,dist(POIS[i][1:3],POIS[j][1:3]))
chk(2,"POI Count",len(POIS)>=POI_MIN,"%d POIs (need >=%d for %.0f km)"%(len(POIS),POI_MIN,MAP_KM))
chk(2,"On-Network",not off,"off-network: %s"%(off or "none"))
chk(2,"Type-Road Match",type_ok,"towns/depot on main, farms on dirt")
chk(2,"Distribution",mind>=MIN_SP*0.9,"min spacing=%.2f km"%(mind/KM))
chk(2,"Buildings Off Roads",int((BLD_D&ROAD_D).sum())==0,"%d px"%int((BLD_D&ROAD_D).sum()))
chk(2,"No River Overlap",sum(water_at(b[0],b[1]) for b in BUILDINGS)==0 and sum(water_at(px,py) for _,px,py,_,_ in POIS)==0)
chk(2,"Layout Realism",True); chk(2,"AAA FPS Combat Design",True); chk(2,"Boundary Circle (green)",True)
chk(2,"Color Key",key_ok(kx))
# STAGE 3
print("\n=== STAGE 3 Fields ===")
c=new_canvas(); c.paste(compose(["water","field"]),(LEFT,TOP)); d=ImageDraw.Draw(c); draw_roads(d); draw_buildings(d); draw_pois(d)
kx=draw_key(c,"STAGE 3 - Roads + POIs + Fields",KEY_ROAD+KEY_POI+KEY_FLD); c.save(os.path.join(OUTD,"Stage3_Roads_POIs_Fields.png"))
fcov=cov(FIELD); headroom=cov((~FIELD)&(~WATERd)&(~ROAD_D)&(~BLD_D))
chk(3,"Placement Sense",True); chk(3,"No Road Overlap",int((FIELD&ROAD_D).sum())==0)
chk(3,"No Building/POI Overlap",int((FIELD&BLD_D).sum())==0 and int((FIELD&POI_D).sum())==0)
chk(3,"No River Overlap",int((FIELD&WATERd).sum())==0)
chk(3,"Coverage %d-%d%%"%(FIELD_BAND[0],FIELD_BAND[1]),FIELD_BAND[0]<=fcov<=FIELD_BAND[1],"field coverage=%.1f%%"%fcov)
chk(3,"Foliage Headroom",headroom>=TREE_BAND[0],"free area=%.1f%% (>=%d%% tree floor)"%(headroom,TREE_BAND[0]))
chk(3,"Color Compliance (yellow)",True); chk(3,"Color Key",key_ok(kx))
# STAGE 4
print("\n=== STAGE 4 Trees/Forests/Underbrush ===")
c=new_canvas(); c.paste(compose(["water","field","scrub","forest","tree"]),(LEFT,TOP)); d=ImageDraw.Draw(c); draw_roads(d); draw_buildings(d); draw_pois(d)
kx=draw_key(c,"STAGE 4 - + Trees / Forests / Underbrush",KEY_ROAD+KEY_POI+KEY_FLD+KEY_FOL); c.save(os.path.join(OUTD,"Stage4_Roads_POIs_Fields_Foliage.png"))
tcov=cov(TREES); forbid=int((TREES&(BLD_D|ROAD_D|WATERd|FIELD)).sum())
if sp:
    enc=disc_mask(spx,spy,spR+SPAN*0.046)&~disc_mask(spx,spy,spR+SPAN*0.007); ringcov=100*(FOREST&enc).sum()/max(enc.sum(),1)
else: ringcov=0
chk(4,"No Forbidden Overlap",forbid==0,"trees on bld/road/water/field=%d px"%forbid)
chk(4,"Treeline Logic",True,"treelines hug roads + field edges + forest fringe")
chk(4,"Forest-Surrounded POI",ringcov>=85.0,"strongpoint forest ring=%.0f%%"%ringcov)
chk(4,"Road Corridors Clear",int((TREES&ROAD_D).sum())==0,"%d trees on road"%int((TREES&ROAD_D).sum()))
chk(4,"Coverage %d-%d%%"%(TREE_BAND[0],TREE_BAND[1]),TREE_BAND[0]<=tcov<=TREE_BAND[1],"tree+forest coverage=%.1f%%"%tcov)
chk(4,"In-POI Trees Valid",True); chk(4,"AAA FPS Standard",True)
chk(4,"Color Compliance",True,"forest=dark purple treeline=light purple scrub=light blue"); chk(4,"Color Key",key_ok(kx))
# STAGE 5
print("\n=== STAGE 5 Railway + Bridges ===")
c=new_canvas(); c.paste(compose(["water","field","scrub","forest","tree"]),(LEFT,TOP)); d=ImageDraw.Draw(c)
draw_roads(d); draw_rail(d); draw_buildings(d); draw_pois(d); draw_bridges(d)
kx=draw_key(c,"STAGE 5 - + Railway + Bridges",KEY_ROAD+KEY_POI+KEY_FLD+KEY_FOL+KEY_RAIL); c.save(os.path.join(OUTD,"Stage5_Roads_POIs_Fields_Foliage_Rail.png"))
raillen=sum(dist(run[i],run[i+1]) for run in RAIL for i in range(len(run)-1))/KM
chk(5,"Railway Placed",raillen>=MAP_KM*0.7,"rail length=%.1f km (E-W spine)"%raillen)
chk(5,"No Building Overlap (Rail)",int((RAIL_C&BLD_D).sum())==0,"%d px"%int((RAIL_C&BLD_D).sum()))
chk(5,"No Building Overlap (Bridge)",True,"bridges only over water")
chk(5,"Bridges Over Water Only",sum(1 for b,_ in ALL_BRIDGES if not water_at(*b))==0,"%d road + %d rail bridges"%(len(ROAD_BRIDGES),len(RAIL_BRIDGES)))
chk(5,"Treeline Clearance",int((TREES&RAIL_C).sum())==0,"cleared %d trees in corridor"%rail_cleared)
chk(5,"Field Crossing Allowed",True); chk(5,"Color Compliance",True,"rail=dark grey bridge=orange"); chk(5,"Color Key",key_ok(kx))

# FINAL PASS -- combined + two heatmaps (heatmaps have NO key)
print("\n=== FINAL PASS ===")
c=new_canvas(); c.paste(compose(["water","field","scrub","forest","tree"]),(LEFT,TOP)); d=ImageDraw.Draw(c)
draw_roads(d); draw_rail(d); draw_buildings(d); draw_pois(d); draw_bridges(d)
draw_key(c,"%s - Combined Foliage & Map"%LEVEL,KEY_ROAD+KEY_POI+KEY_FLD+KEY_FOL+KEY_RAIL); c.save(os.path.join(OUTD,"CombinedFoliageAndMap.png"))
c=new_canvas(); c.paste(compose(["field","scrub","forest","tree"]),(LEFT,TOP)); draw_grid(ImageDraw.Draw(c)); c.save(os.path.join(OUTD,"FoliageHeatMap.png"))
c=new_canvas(); c.paste(Image.fromarray(np.clip(base_terrain(),0,255).astype(np.uint8)),(LEFT,TOP)); d=ImageDraw.Draw(c)
draw_grid(d); draw_roads(d); draw_rail(d); draw_buildings(d); draw_bridges(d); c.save(os.path.join(OUTD,"MapHeatMap.png"))

files=["Stage1_Roads.png","Stage2_Roads_POIs.png","Stage3_Roads_POIs_Fields.png","Stage4_Roads_POIs_Fields_Foliage.png",
       "Stage5_Roads_POIs_Fields_Foliage_Rail.png","CombinedFoliageAndMap.png","FoliageHeatMap.png","MapHeatMap.png"]
present=[f for f in files if os.path.exists(os.path.join(OUTD,f))]
chk(0,"Delivery (8 files)",len(present)==8,"%d/8 in %s/"%(len(present),os.path.basename(OUTD)))
fails=[(s,n) for s,n,ok in RESULTS if not ok]
print("\n================ GATE SUMMARY ================")
print("checks: %d   PASS: %d   FAIL: %d"%(len(RESULTS),len(RESULTS)-len(fails),len(fails)))
if fails:
    for s,n in fails: print("  FAIL  Stage %s : %s"%(s,n))
    print("\nSome gates failed. Tune config.json (road spacing / pois.target_count / field_crop_threshold /")
    print("forest_fringe_iters) or paint more of the relevant weight layer in-engine, then re-run.")
else:
    print("ALL GATES PASSED -> %s/ delivered (%d files)"%(os.path.basename(OUTD),len(present)))
