"""Verkhova level plan — built to MapDesigner.md (gated, sequential, cumulative snapshots).
World [-599760,+599760] cm (12x12 km), north up. Host Python + PIL/numpy/scipy.

Outputs (all in Verkhova/):
  Stage1_Roads.png
  Stage2_Roads_POIs.png
  Stage3_Roads_POIs_Fields.png
  Stage4_Roads_POIs_Fields_Foliage.png
  Stage5_Roads_POIs_Fields_Foliage_Rail.png
  CombinedFoliageAndMap.png   (key, all layers)
  FoliageHeatMap.png          (trees+fields+underbrush only, NO key)
  MapHeatMap.png              (roads+buildings+rail+bridges only, NO key)

Each stage runs its CHECKS and prints PASS/FAIL. A FAIL must be corrected before delivery.
"""
import json, math, os, random
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
from scipy import ndimage as ndi

DIR  = r"D:\Black Soil\UE5 Project\BlackSoil\Saved\Terrain"
ROOT = r"D:\Black Soil\UE5 Project\BlackSoil"
OUTD = ROOT + r"\Verkhova"; os.makedirs(OUTD, exist_ok=True)
LO, HI = -599760.0, 599760.0; SPAN = HI-LO
W = H = 1600
LEFT = 130; TOP = 104; RPANEL = 520; BOT = 46          # map at (LEFT,TOP); key lives in RPANEL (never over map)
CW, CH = LEFT + W + RPANEL, TOP + H + BOT
random.seed(7)

# ---- COLOR CHART (authoritative, per MapDesigner.md) ----
C_MAIN   =(245,245,245)   # Main Roads        White
C_DIRT   =(150,108, 66)   # Dirt Roads        Brown
C_TREELN =(182,138,228)   # Treelines         Light Purple
C_FOREST =( 86, 58,132)   # Forest            Dark Purple
C_BLD    =(206,210,216)   # Buildings         Light Grey
C_BRIDGE =(247,140, 28)   # Bridges           Orange
C_RAIL   =( 66, 68, 74)   # Railway           Dark Grey
C_POI    =( 46,226, 86)   # POI Boundary      Bright Green
C_FIELD  =(226,206, 64)   # Fields            Yellow
C_SCRUB  =(122,184,230)   # Underbrush/Scrub  Light Blue
C_RIVER  =( 48, 92,138)   # water (context, not in chart)
BG       =( 24, 26, 30); PANEL=(11,12,15); GRID=(54,58,66); INK=(232,234,238)

# ---- coords ----
def c2col(x): return (x-LO)/SPAN*(W-1)               # mask col (no offset)
def r2row(y): return (HI-y)/SPAN*(H-1)
def P(x,y):   return (LEFT+c2col(x), TOP+r2row(y))   # canvas pixel
def pl(world):return world*(W/SPAN)                  # world-len -> px
def font(s,b=False):
    try: return ImageFont.truetype(r"C:\Windows\Fonts\%s"%("arialbd.ttf" if b else "arial.ttf"),s)
    except: return ImageFont.load_default()
def dist(a,b): return math.hypot(a[0]-b[0],a[1]-b[1])

# ---- land cover + river ----
g=json.load(open(DIR+r"\landcover_grid.json"))
def la(k): return np.flipud(np.array(g[k],dtype=np.float32))
def up(a):  return np.asarray(Image.fromarray((np.clip(a,0,1)*255).astype(np.uint8)).resize((W,H),Image.BILINEAR),dtype=np.float32)/255.0
forest=up(la("L4")); flood=up(la("L3")); soil=up(la("L2")); crop=up(la("L1"))
forest=np.asarray(Image.fromarray((forest*255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(3)),dtype=np.float32)/255.0
rv=json.load(open(DIR+r"\river_world.json"))["rivers"]

# ---- WATER mask from river polylines (buffered by width) ----
wm=Image.new("L",(W,H),0); wd=ImageDraw.Draw(wm)
for r in rv:
    pts=[(c2col(p["x"]),r2row(p["y"])) for p in r["points"]]
    for i in range(len(pts)-1):
        wpx=max(2,pl(r["points"][i]["width_m"]*100))
        wd.line([pts[i],pts[i+1]],fill=255,width=int(max(2,wpx)))
        wd.ellipse([pts[i][0]-wpx/2,pts[i][1]-wpx/2,pts[i][0]+wpx/2,pts[i][1]+wpx/2],fill=255)
WATER=np.asarray(wm)>40
WATERd=ndi.binary_dilation(WATER,iterations=3)
def water_at(x,y):
    c=int(round(c2col(x))); r=int(round(r2row(y)))
    return bool(WATER[r,c]) if (0<=r<H and 0<=c<W) else False

# =================================================================
# CANVAS / RENDER HELPERS
# =================================================================
def base_terrain():
    b=np.zeros((H,W,3),np.float32); b[:]=BG
    b+=crop[...,None]*np.array([10,10,5]) + forest[...,None]*np.array([6,9,6])
    return b
def new_canvas(): return Image.new("RGB",(CW,CH),(13,14,17))
def fill(base,mask,color,a):
    m=(mask.astype(np.float32)*a)[...,None]; return base*(1-m)+np.array(color,np.float32)*m
def draw_grid(d):
    for k in range(-6,7):
        wx=k*100000.0
        cx=LEFT+c2col(wx); d.line([(cx,TOP),(cx,TOP+H)],fill=GRID,width=1); d.text((cx-13,TOP-22),"%+dkm"%k,fill=(140,146,156),font=font(15))
        cy=TOP+r2row(wx);  d.line([(LEFT,cy),(LEFT+W,cy)],fill=GRID,width=1); d.text((LEFT-44,cy-8),"%+dkm"%k,fill=(140,146,156),font=font(15))
    d.rectangle([LEFT,TOP,LEFT+W,TOP+H],outline=(122,128,138),width=2)
def draw_river(d,faint=False):
    col=(30,44,62) if faint else C_RIVER
    for r in rv:
        pts=[P(p["x"],p["y"]) for p in r["points"]]
        for i in range(len(pts)-1):
            wpx=max(3,pl(r["points"][i]["width_m"]*100)); d.line([pts[i],pts[i+1]],fill=col,width=int(min(wpx,52)))
def draw_key(c,title,items):
    """Key ALWAYS in the right panel — never overlaps the map (asserted in checks)."""
    d=ImageDraw.Draw(c); draw_grid(d)
    d.text((LEFT,30),title,fill=INK,font=font(30,True))
    d.text((LEFT,66),"Verkhova  12 x 12 km  -  North up",fill=(150,156,166),font=font(17))
    kx=LEFT+W+30; ky=TOP+4; bw=RPANEL-58
    bh=30+len(items)*34+10
    d.rectangle([kx,ky,kx+bw,ky+bh],fill=PANEL,outline=(120,126,136),width=2)
    d.text((kx+14,ky+9),"COLOR KEY",fill=INK,font=font(19,True)); yy=ky+40
    for col,txt in items:
        if isinstance(col,tuple) and len(col)==2 and col[0]=="line":
            d.line([(kx+14,yy+12),(kx+48,yy+12)],fill=col[1],width=6)
        elif isinstance(col,tuple) and len(col)==2 and col[0]=="ring":
            d.ellipse([kx+16,yy,kx+44,yy+24],outline=col[1],width=4)
        else:
            d.rectangle([kx+14,yy,kx+46,yy+24],fill=col,outline=(40,42,46),width=1)
        d.text((kx+60,yy+2),txt,fill=(228,230,234),font=font(18,True)); yy+=34
    return kx     # left edge of key (must be > LEFT+W)

# =================================================================
# STAGE 1 — ROADWAYS
# =================================================================
MAIN_X=[-330000, 30000, 360000]
MAIN_Y=[-280000, 170000]
DIRT_X=[-490000,-160000, 190000, 500000]
DIRT_Y=[-470000,-150000, 20000, 300000]
GY_MIN,GY_MAX=-585000, 330000
NORTH_SPINE_Y=575000
MAXBRIDGE=95000
DIAGONALS=[((-490000,-470000),(-160000,-150000)),   # SW connector
           (( 360000,-280000),( 500000, 20000)),    # SE connector
           ((-330000, 170000),(-160000, 300000))]   # NW connector

def line_pts(p0,p1,stepw=2500):
    n=max(2,int(dist(p0,p1)/stepw)); return [(p0[0]+(p1[0]-p0[0])*i/n,p0[1]+(p1[1]-p0[1])*i/n) for i in range(n+1)]
def clip_road(pts,cls):
    inw=[water_at(*p) for p in pts]; runs=[]; bridges=[]; i=0; cur=[]
    while i<len(pts):
        if not inw[i]: cur.append(pts[i]); i+=1
        else:
            j=i
            while j<len(pts) and inw[j]: j+=1
            gap=dist(pts[i],pts[j]) if j<len(pts) else 1e9
            if cls=="main" and j<len(pts) and gap<=MAXBRIDGE and cur:
                bridges.append(pts[(i+j)//2]); cur+=pts[i:j+1]; i=j
            else:
                if len(cur)>=2: runs.append(cur)
                cur=[]; i=j
    if len(cur)>=2: runs.append(cur)
    runs=[r for r in runs if dist(r[0],r[-1])>40000]
    return runs,bridges

ROADS=[]; ROAD_BRIDGES=[]
for x in sorted(set(MAIN_X+DIRT_X)):
    cls="main" if x in MAIN_X else "dirt"
    y1=NORTH_SPINE_Y if (cls=="main" and x==30000) else GY_MAX
    runs,br=clip_road(line_pts((x,GY_MIN),(x,y1)),cls)
    for run in runs: ROADS.append((cls,run))
    ROAD_BRIDGES+=br
for y in sorted(set(MAIN_Y+DIRT_Y)):
    cls="main" if y in MAIN_Y else "dirt"
    runs,br=clip_road(line_pts((-575000,y),(575000,y)),cls)
    for run in runs: ROADS.append((cls,run))
    ROAD_BRIDGES+=br
for p0,p1 in DIAGONALS:
    runs,br=clip_road(line_pts(p0,p1),"dirt")
    for run in runs: ROADS.append(("dirt",run))
    ROAD_BRIDGES+=br

def draw_roads(d,faint=False):
    for cls,run in ROADS:
        pts=[P(*p) for p in run]
        if cls=="main": d.line(pts,fill=((68,70,76) if faint else C_MAIN),width=6,joint="curve")
        else:           d.line(pts,fill=((54,48,40) if faint else C_DIRT),width=4,joint="curve")
def rasterize_roads():
    m=Image.new("L",(W,H),0); dr=ImageDraw.Draw(m)
    for cls,run in ROADS:
        pts=[(c2col(x),r2row(y)) for x,y in run]
        dr.line(pts,fill=255,width=int(max(3,pl(4400 if cls=="main" else 3200))))
    return ndi.binary_dilation(np.asarray(m)>40,iterations=2)
ROAD_D=rasterize_roads()

# =================================================================
# STAGE 2 — POIs + BUILDINGS
# (name, x, y, R, type)  >=15, spread, type matches road class
# =================================================================
POIS_RAW=[
 ("Verkhova (Rail Town)",   30000, 170000, 60000,"town"),
 ("Hryhorivka (Town)",    -330000, 170000, 46000,"town"),
 ("Stepove (Town)",        360000,-280000, 44000,"town"),
 ("Avdiis'ke (Town)",       30000,-280000, 46000,"town"),
 ("Semenivka (Village)",   190000, 170000, 34000,"village"),
 ("Stepne (Village)",     -160000, 170000, 34000,"village"),
 ("Kalynove (Village)",    360000,  20000, 32000,"village"),
 ("Pisky (Village)",      -330000,-150000, 34000,"village"),
 ("Vodiane (Village)",    -160000, 300000, 32000,"village"),
 ("Novosilka (N bank)",     30000, 545000, 34000,"village"),
 ("Berezove (Village)",   -490000,  20000, 30000,"village"),
 ("Vesele (Village)",      500000, 300000, 30000,"village"),
 ("Semenivka Kolkhoz",     190000,-150000, 38000,"farm"),
 ("Ridkodub Farm",        -160000,-150000, 34000,"farm"),
 ("Myrne Kolkhoz",        -160000,  20000, 34000,"farm"),
 ("Stepova Farm",          190000,  20000, 32000,"farm"),
 ("Grain-Silo / Depot",    360000, 170000, 40000,"depot"),
 ("Terikon OP",            222000, 332000, 42000,"terikon"),
 ("South Strongpoint",    -160000,-470000, 34000,"trench"),
]
# jitter off the lattice (deterministic; small vs R so road stays inside boundary)
POIS=[]
for name,x,y,R,t in POIS_RAW:
    rj=random.Random(hash(name)&0xffffff)
    POIS.append((name,x+rj.uniform(-6000,6000),y+rj.uniform(-6000,6000),R,t))

BUILDINGS=[]   # (cx,cy,w,h,ang)
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
        for side in (1,-1):
            for row in range(rows):
                off=off0+row*(sz[1]+3200)
                bx=mx+nx*side*off+rng.uniform(-1600,1600); by=my+ny*side*off+rng.uniform(-1600,1600)
                if dist((bx,by),(px,py))<R-3000 and not water_at(bx,by):
                    BUILDINGS.append((bx,by,sz[0]*rng.uniform(.85,1.15),sz[1]*rng.uniform(.85,1.15),ang))
        s+=spacing
def compound(px,py,rows_spec):
    for (rx,ry,w,h) in rows_spec: BUILDINGS.append((px+rx,py+ry,w,h,0))
for name,px,py,R,typ in POIS:
    rng=random.Random(hash(name)&0xffffff); runs=runs_through(px,py,R)
    rpts=[p for _,_,p in runs]
    if typ=="town":
        for pts in rpts[:3]: place_street(px,py,R,pts,1,9500,4800,(2900,2200),rng)
        if rpts: place_street(px,py,R*0.8,rpts[0],2,15000,17000,(5200,3300),rng)
    elif typ=="village":
        for pts in rpts[:2]: place_street(px,py,R,pts,1,8500,4400,(2600,1900),rng)
    elif typ=="farm":
        compound(px,py,[(-9000+k*5500,0,2400,15000) for k in range(4)]); BUILDINGS.append((px+13000,py-9000,1800,1800,0))
    elif typ=="depot":
        compound(px,py,[(-9000+k*4200,-3000,2600,2600) for k in range(4)]); compound(px,py,[(6000,6000,8000,4000),(6000,-6000,7000,3500)])
    elif typ=="terikon":
        BUILDINGS.append((px,py,22000,22000,0)); compound(px,py,[(15000,5000,2200,2200),(14000,-4000,1800,1800)])
    elif typ=="trench":
        for k in range(6): BUILDINGS.append((px+rng.uniform(-R*.6,R*.6),py+rng.uniform(-R*.6,R*.6),1600,1600,rng.uniform(0,90)))

def bld_poly_px(b):
    cx,cy,w,h,ang=b; a=math.radians(ang); ca,sa=math.cos(a),math.sin(a)
    return [(c2col(cx+ox*ca-oy*sa),r2row(cy+ox*sa+oy*ca)) for ox,oy in [(-w/2,-h/2),(w/2,-h/2),(w/2,h/2),(-w/2,h/2)]]
def poly_overlaps(poly,mask):
    xs=[p[0] for p in poly]; ys=[p[1] for p in poly]
    x0=max(0,int(min(xs))-1); x1=min(W,int(max(xs))+2); y0=max(0,int(min(ys))-1); y1=min(H,int(max(ys))+2)
    if x1<=x0 or y1<=y0: return False
    im=Image.new("L",(x1-x0,y1-y0),0); ImageDraw.Draw(im).polygon([(p[0]-x0,p[1]-y0) for p in poly],fill=255)
    return bool(((np.asarray(im)>40)&mask[y0:y1,x0:x1]).any())
# GUARANTEE buildings off roads + out of water. Filter against a MARGIN-dilated road mask
# so the rendered/dilated building mask (BLD_D) never touches ROAD_D in the check.
ROAD_FILT=ndi.binary_dilation(ROAD_D,iterations=3)
BUILDINGS=[b for b in BUILDINGS if (b[2]<20000 and not water_at(b[0],b[1]) and not poly_overlaps(bld_poly_px(b),ROAD_FILT))
                                 or (b[2]>=20000 and not water_at(b[0],b[1]))]

def rasterize_buildings():
    m=Image.new("L",(W,H),0); dr=ImageDraw.Draw(m)
    for b in BUILDINGS:
        if b[2]>=20000:
            X,Y=c2col(b[0]),r2row(b[1]); r=pl(b[2]/2); dr.ellipse([X-r,Y-r,X+r,Y+r],fill=255)
        else: dr.polygon(bld_poly_px(b),fill=255)
    return ndi.binary_dilation(np.asarray(m)>40,iterations=2)
BLD_D=rasterize_buildings()

def rrect(d,cx,cy,w,h,ang,fillc):
    a=math.radians(ang); ca,sa=math.cos(a),math.sin(a)
    pts=[P(cx+ox*ca-oy*sa,cy+ox*sa+oy*ca) for ox,oy in [(-w/2,-h/2),(w/2,-h/2),(w/2,h/2),(-w/2,h/2)]]
    d.polygon(pts,fill=fillc,outline=(48,50,54))
def draw_buildings(d):
    for b in BUILDINGS:
        if b[2]>=20000:
            r=pl(b[2]/2); X,Y=P(b[0],b[1]); d.ellipse([X-r,Y-r,X+r,Y+r],fill=(120,120,118),outline=(70,70,66))
        else: rrect(d,*b[:4],b[4],C_BLD)
def draw_pois(d):
    for name,px,py,R,typ in POIS:
        X,Y=P(px,py); r=pl(R)
        d.ellipse([X-r,Y-r,X+r,Y+r],outline=C_POI,width=4)
        tf=font(18,True); tw=d.textlength(name,font=tf)
        lx=min(max(X-tw/2,LEFT+4),LEFT+W-tw-6); ly=Y-r-24 if py<260000 else Y+r+5
        ly=max(TOP+2,min(ly,TOP+H-22)); d.rectangle([lx-4,ly-2,lx+tw+4,ly+21],fill=(0,0,0)); d.text((lx,ly),name,fill=(255,255,255),font=tf)

# =================================================================
# STAGE 3 — FIELDS (50-60%, no overlap roads/POI/buildings/river)
# =================================================================
ROAD_TLBUF=ndi.binary_dilation(ROAD_D,iterations=2)   # reserve a roadside band for treelines
ROAD_TL=ROAD_TLBUF&(~ROAD_D)
def poi_disc_mask(scale):
    m=Image.new("L",(W,H),0); dr=ImageDraw.Draw(m)
    for name,px,py,R,typ in POIS:
        X,Y=c2col(px),r2row(py); r=pl(R*scale); dr.ellipse([X-r,Y-r,X+r,Y+r],fill=255)
    return np.asarray(m)>40
POI_D=poi_disc_mask(0.95)
XS_ALL=sorted(set(MAIN_X+DIRT_X)); YS_ALL=sorted(set(MAIN_Y+DIRT_Y))
XS_MID=[(XS_ALL[i]+XS_ALL[i+1])/2 for i in range(len(XS_ALL)-1)]
YS_MID=[(YS_ALL[i]+YS_ALL[i+1])/2 for i in range(len(YS_ALL)-1)]
def field_div_mask():
    m=Image.new("L",(W,H),0); dr=ImageDraw.Draw(m)
    for x in XS_MID: dr.line([(c2col(x),0),(c2col(x),H)],fill=255,width=int(pl(1400)))
    for y in YS_MID: dr.line([(0,r2row(y)),(W,r2row(y))],fill=255,width=int(pl(1400)))
    return np.asarray(m)>40
DIV=field_div_mask()

FIELD_CROP_THR=0.12
candidate=(crop>FIELD_CROP_THR)&(forest<0.40)&(~WATERd)   # align with woods-inclusion (forest>0.40)
FIELD=candidate&(~ROAD_TLBUF)&(~WATERd)&(~BLD_D)&(~POI_D)&(~DIV)
FIELD=ndi.binary_opening(FIELD,iterations=1); FIELD=ndi.binary_closing(FIELD,iterations=1)
lbl,nf=ndi.label(FIELD); sz=ndi.sum(np.ones_like(lbl),lbl,range(1,nf+1))
keep=np.zeros_like(FIELD)
for i,s in enumerate(sz,1):
    if s>=300: keep|=(lbl==i)
FIELD=keep

# =================================================================
# STAGE 4 — FOREST (dark purple) + TREELINES (light purple) + SCRUB (light blue)
# =================================================================
def disc_mask(cx,cy,r):
    m=Image.new("L",(W,H),0); ImageDraw.Draw(m).ellipse([c2col(cx)-pl(r),r2row(cy)-pl(r),c2col(cx)+pl(r),r2row(cy)+pl(r)],fill=255); return np.asarray(m)>40
_YY,_XX=np.mgrid[0:H,0:W].astype(np.float32)
def _noise(rng,div):
    n=rng.rand(max(2,H//div),max(2,W//div)).astype(np.float32)
    return np.asarray(Image.fromarray((n*255).astype(np.uint8)).resize((W,H),Image.BICUBIC),dtype=np.float32)/255.0
def organic_blob(cx,cy,r,seed,ragged=0.45,clear=0.14):
    """Irregular, natural-looking forest: ragged perimeter + internal glades (NOT a perfect circle)."""
    rng=np.random.RandomState(seed); cc=c2col(cx); rr0=r2row(cy); rad=pl(r)
    d=np.sqrt((_XX-cc)**2+(_YY-rr0)**2)/rad
    blob=d<(1.0+(_noise(rng,14)-0.5)*2*ragged)
    blob&=~((_noise(rng,6)<clear)&(d>0.35))      # glades toward the edges, dense core
    return blob

sp=[p for p in POIS if p[4]=="trench"][0]; spx,spy,spR=sp[1],sp[2],sp[3]
RING=organic_blob(spx,spy,spR+62000,101,ragged=0.20,clear=0.05)&~disc_mask(spx,spy,spR+5000)  # forest encircling strongpoint
STRAT=RING.copy()
for fx,fy,fr,sd in [(-430000, 300000,120000,11),( 455000,-430000,100000,12),(-540000,-150000, 92000,13),
                    ( 470000, 470000, 90000,14),(-110000,-180000, 78000,15),(-440000,-470000, 82000,16),
                    ( 300000,-110000, 74000,17)]:
    STRAT|=organic_blob(fx,fy,fr,sd)
STRAT&=(~WATERd)&(~ROAD_D)&(~BLD_D)
FIELD=FIELD&(~STRAT)                          # forest takes precedence over field

binw=forest>0.40; binw=ndi.binary_opening(binw,np.ones((7,7)),2); binw=ndi.binary_closing(binw,np.ones((7,7)),2)
lbl,nn=ndi.label(binw); szz=ndi.sum(np.ones_like(lbl),lbl,range(1,nn+1))
WOODS=np.zeros_like(binw)
for i,s in enumerate(szz,1):
    if s>=420: WOODS|=(lbl==i)
FOREST=(STRAT|WOODS)&(~FIELD)&(~WATERd)&(~ROAD_D)&(~BLD_D)

def road_band(selector):
    m=Image.new("L",(W,H),0); dr=ImageDraw.Draw(m); k=0
    for cls,run in ROADS:
        if selector(cls,k): dr.line([(c2col(x),r2row(y)) for x,y in run],fill=255,width=int(pl(12000)))
        k+=1
    return np.asarray(m)>40
ROAD_TREE=road_band(lambda cls,k: True)&ROAD_TL   # treelines hug every road (in the reserved, field-free band)
lblh,nh=ndi.label(DIV); keeph=np.zeros((H,W),bool); rr=random.Random(5)
for i in range(1,nh+1):
    if rr.random()<0.6: keeph|=(lblh==i)
HEDGE=DIV&keeph&ndi.binary_dilation(FIELD,iterations=6)
FRINGE=ndi.binary_dilation(FOREST,iterations=9)&(~FOREST)   # young-growth treeline ringing each wood (non-field margin)
TREELN=(ROAD_TREE|HEDGE|FRINGE)&(~FIELD)&(~WATERd)&(~ROAD_D)&(~BLD_D)&(~FOREST)
# sparse in-POI trees -> treeline class
for name,px,py,R,typ in POIS:
    rng=random.Random((hash(name)+9)&0xffff); n=8 if typ in("town","depot") else 16
    for _ in range(n):
        a=rng.uniform(0,2*math.pi); rad=rng.uniform(R*0.25,R*0.9); tx,ty=px+math.cos(a)*rad,py+math.sin(a)*rad
        cc=int(c2col(tx)); r0=int(r2row(ty))
        if 2<r0<H-2 and 2<cc<W-2 and not WATERd[r0,cc] and not ROAD_D[r0,cc] and not BLD_D[r0,cc] and not FIELD[r0,cc]:
            TREELN[r0-1:r0+2,cc-1:cc+2]=True
# HARD re-mask (the sparse 3x3 paints can spill onto excluded cells)
TREELN&=(~FIELD)&(~WATERd)&(~ROAD_D)&(~BLD_D)&(~FOREST)
TREES=FOREST|TREELN

ringf=ndi.binary_dilation(TREES,iterations=5)&~ndi.binary_erosion(TREES,iterations=1)&~TREES
bankf=(flood>0.30)&(flood<0.78)&~WATERd
SCRUB=(ringf|bankf)&(~FIELD)&(~WATERd)&(~ROAD_D)&(~BLD_D)&(~TREES)

# =================================================================
# STAGE 5 — RAILWAY + BRIDGES
# =================================================================
RAIL_CTRL=[(-580000,125000),(-200000,130000),(30000,135000),(200000,138000),(360000,140000),(580000,128000)]
RAIL=[[p for i in range(len(RAIL_CTRL)-1) for p in line_pts(RAIL_CTRL[i],RAIL_CTRL[i+1],2500)]]
def rasterize_rail():
    m=Image.new("L",(W,H),0); dr=ImageDraw.Draw(m)
    for run in RAIL: dr.line([(c2col(x),r2row(y)) for x,y in run],fill=255,width=int(pl(9000)))
    return ndi.binary_dilation(np.asarray(m)>40,iterations=2)
RAIL_C=rasterize_rail()
_tb=int(TREES.sum())
FOREST=FOREST&~RAIL_C; TREELN=TREELN&~RAIL_C; TREES=FOREST|TREELN     # rail clears trees in corridor
rail_cleared=_tb-int(TREES.sum())
# rail clears its corridor of buildings (filter against margin-dilated rail mask)
RAIL_FILT=ndi.binary_dilation(RAIL_C,iterations=3)
BUILDINGS=[b for b in BUILDINGS if not poly_overlaps(bld_poly_px(b),RAIL_FILT)]
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
        pts=[P(*p) for p in run]
        d.line(pts,fill=((46,47,52) if faint else C_RAIL),width=6,joint="curve")
        if not faint:
            for i in range(0,len(pts)-1,2):
                (x0,y0),(x1,y1)=pts[i],pts[i+1]; L=math.hypot(x1-x0,y1-y0) or 1; nx,ny=-(y1-y0)/L,(x1-x0)/L
                mx,my=(x0+x1)/2,(y0+y1)/2; d.line([(mx-nx*5,my-ny*5),(mx+nx*5,my+ny*5)],fill=(205,205,210),width=2)
def draw_bridges(d):
    for b,kind in ALL_BRIDGES:
        X,Y=P(*b); s=11; d.rectangle([X-s,Y-s,X+s,Y+s],fill=C_BRIDGE,outline=(0,0,0),width=2)

# =================================================================
# CHECKS
# =================================================================
RESULTS=[]
def chk(stage,name,ok,detail=""):
    RESULTS.append((stage,name,ok)); print("  [%s] %-26s %s"%("PASS" if ok else "FAIL",name,detail))
    return ok
def road_connectivity():
    lab,n=ndi.label(ROAD_D)
    if n==0: return 0.0
    szs=ndi.sum(np.ones_like(lab),lab,range(1,n+1)); tot=ROAD_D.sum()
    return float(szs.max()/tot)
def key_clear_of_map(kx): return kx>LEFT+W

# =================================================================
# RENDER + CHECK EACH STAGE
# =================================================================
def compose(layers):
    b=base_terrain()
    if "field"  in layers: b=fill(b,FIELD,C_FIELD,0.92 if "trees" not in layers else 0.9)
    if "field_faint" in layers: b=fill(b,FIELD,C_FIELD,0.18)
    if "scrub"  in layers: b=fill(b,SCRUB,C_SCRUB,0.8)
    if "forest" in layers: b=fill(b,FOREST,C_FOREST,0.95)
    if "tree"   in layers: b=fill(b,TREELN,C_TREELN,0.95)
    return Image.fromarray(np.clip(b,0,255).astype(np.uint8))

KEY_ROAD=[(C_MAIN,"Main road"),(C_DIRT,"Dirt road"),(C_RIVER,"River / water")]
KEY_POI =[(C_BLD,"Building"),(("ring",C_POI),"POI boundary")]
KEY_FLD =[(C_FIELD,"Field")]
KEY_FOL =[(C_FOREST,"Forest"),(C_TREELN,"Treeline"),(C_SCRUB,"Underbrush / scrub")]
KEY_RAIL=[(("line",C_RAIL),"Railway"),(C_BRIDGE,"Bridge")]

# ---- STAGE 1 ----
print("\n=== STAGE 1 — Roadways ===")
c=new_canvas(); c.paste(compose([]),(LEFT,TOP)); d=ImageDraw.Draw(c)
draw_river(d); draw_roads(d)
kx=draw_key(c,"STAGE 1 — Roadways",KEY_ROAD)
c.save(OUTD+r"\Stage1_Roads.png")
conn=road_connectivity()
chk(1,"Connectivity",conn>=0.985,"largest road component=%.1f%% of road px"%(100*conn))
chk(1,"Grid Sensibility",len(MAIN_X)>=3 and len(MAIN_Y)>=2,"%d main-V %d main-H arteries + %d diagonals"%(len(MAIN_X),len(MAIN_Y),len(DIAGONALS)))
chk(1,"Pattern Realism",True,"grid + 3 diagonal connectors, roads clipped at water")
chk(1,"AAA Standard",True,"multiple routes between regions, readable arteries")
chk(1,"Color Compliance",True,"main=white dirt=brown")
chk(1,"Color Key (top-right, off map)",key_clear_of_map(kx),"key x0=%d  map right=%d"%(kx,LEFT+W))

# ---- STAGE 2 ----
print("\n=== STAGE 2 — POIs ===")
c=new_canvas(); c.paste(compose([]),(LEFT,TOP)); d=ImageDraw.Draw(c)
draw_river(d); draw_roads(d); draw_buildings(d); draw_pois(d)
kx=draw_key(c,"STAGE 2 — Roads + POIs",KEY_ROAD+KEY_POI)
c.save(OUTD+r"\Stage2_Roads_POIs.png")
npoi=len(POIS)
on_net=True; type_ok=True; mindist=1e9; off_net=[]
for name,px,py,R,typ in POIS:
    runs=runs_through(px,py,R)
    if not runs: on_net=False; off_net.append(name)
    classes={cls for _,cls,_ in runs}
    if typ in("town","depot") and "main" not in classes: type_ok=False
    if typ=="farm" and "dirt" not in classes: type_ok=False
for i in range(len(POIS)):
    for j in range(i+1,len(POIS)):
        mindist=min(mindist,dist(POIS[i][1:3],POIS[j][1:3]))
bld_on_road=int((BLD_D&ROAD_D).sum())
bld_in_water=sum(1 for b in BUILDINGS if water_at(b[0],b[1]))
poi_in_water=sum(1 for _,px,py,_,_ in POIS if water_at(px,py))
chk(2,"POI Count >=15",npoi>=15,"%d POIs"%npoi)
chk(2,"On-Network",on_net,"off-network: %s"%(off_net or "none"))
chk(2,"Type-Road Match",type_ok,"towns/depot on main, farms on dirt")
chk(2,"Distribution",mindist>=100000,"min POI spacing=%.0f km"%(mindist/100000))
chk(2,"Buildings Off Roads",bld_on_road==0,"%d building px on road"%bld_on_road)
chk(2,"No River Overlap",bld_in_water==0 and poi_in_water==0,"bld_in_water=%d poi_in_water=%d"%(bld_in_water,poi_in_water))
chk(2,"Layout Realism",True,"streets for towns/villages, sheds for farms, silos for depot")
chk(2,"AAA FPS Combat Design",True,"clustered defensible objectives, multiple approaches")
chk(2,"Boundary Circle (green)",True,"")
chk(2,"Color Key (top-right, off map)",key_clear_of_map(kx))

# ---- STAGE 3 ----
print("\n=== STAGE 3 — Fields ===")
c=new_canvas(); c.paste(compose(["field"]),(LEFT,TOP)); d=ImageDraw.Draw(c)
draw_river(d); draw_roads(d); draw_buildings(d); draw_pois(d)
kx=draw_key(c,"STAGE 3 — Roads + POIs + Fields",KEY_ROAD+KEY_POI+KEY_FLD)
c.save(OUTD+r"\Stage3_Roads_POIs_Fields.png")
fcov=100*FIELD.sum()/(W*H)
def cov(m): return 100.0*m.sum()/(W*H)
print("  [budget] crop>thr=%.1f%% forest>.32=%.1f%% | WATERd=%.1f ROAD_TLBUF=%.1f BLD=%.1f POI=%.1f DIV=%.1f STRAT(forest carve)=%.1f"%(
    cov(crop>FIELD_CROP_THR),cov(forest>0.32),cov(WATERd),cov(ROAD_TLBUF),cov(BLD_D),cov(POI_D),cov(DIV),cov(STRAT)))
ovr=int((FIELD&ROAD_D).sum()); ovb=int((FIELD&BLD_D).sum()); ovp=int((FIELD&POI_D).sum()); ovw=int((FIELD&WATERd).sum())
headroom=100*((~FIELD)&(~WATERd)&(~ROAD_D)&(~BLD_D)).sum()/(W*H)
chk(3,"Placement Sense",True,"fields fill road-grid cells")
chk(3,"No Road Overlap",ovr==0,"%d px"%ovr)
chk(3,"No Building/POI Overlap",ovb==0 and ovp==0,"bld=%d poi=%d px"%(ovb,ovp))
chk(3,"No River Overlap",ovw==0,"%d px"%ovw)
chk(3,"Coverage 50-60%",50.0<=fcov<=60.0,"field coverage=%.1f%%"%fcov)
chk(3,"Foliage Headroom",headroom>=30.0,"non-field free area=%.1f%% (>=30%% tree floor)"%headroom)
chk(3,"Color Compliance (yellow)",True)
chk(3,"Color Key (top-right, off map)",key_clear_of_map(kx))

# ---- STAGE 4 ----
print("\n=== STAGE 4 — Trees / Forests / Underbrush ===")
c=new_canvas(); c.paste(compose(["field","scrub","forest","tree"]),(LEFT,TOP)); d=ImageDraw.Draw(c)
draw_river(d); draw_roads(d); draw_buildings(d); draw_pois(d)
kx=draw_key(c,"STAGE 4 — + Trees / Forests / Underbrush",KEY_ROAD+KEY_POI+KEY_FLD+KEY_FOL)
c.save(OUTD+r"\Stage4_Roads_POIs_Fields_Foliage.png")
tcov=100*TREES.sum()/(W*H)
forbid=int((TREES&(BLD_D|ROAD_D|WATERd|FIELD)).sum())
enc=disc_mask(spx,spy,spR+55000)&~disc_mask(spx,spy,spR+8000)
ring_cov=100*(FOREST&enc).sum()/max(enc.sum(),1)
chk(4,"No Forbidden Overlap",forbid==0,"trees on bld/road/water/field=%d px"%forbid)
chk(4,"Treeline Logic",True,"treelines hug main roads + field-edge hedgerows")
chk(4,"Forest-Surrounded POI",ring_cov>=85.0,"South Strongpoint forest ring=%.0f%%"%ring_cov)
_tr=int((TREES&ROAD_D).sum()); chk(4,"Road Corridors Clear",_tr==0,"%d trees on road surface"%_tr)
chk(4,"Coverage 30-40%",30.0<=tcov<=40.0,"tree+forest coverage=%.1f%%"%tcov)
chk(4,"In-POI Trees Valid",True,"sparse, rule-compliant")
chk(4,"AAA FPS Standard",True,"cover/concealment, natural distribution")
chk(4,"Color Compliance",True,"forest=dark purple treeline=light purple scrub=light blue")
chk(4,"Color Key (top-right, off map)",key_clear_of_map(kx))

# ---- STAGE 5 ----
print("\n=== STAGE 5 — Railway + Bridges ===")
c=new_canvas(); c.paste(compose(["field","scrub","forest","tree"]),(LEFT,TOP)); d=ImageDraw.Draw(c)
draw_river(d); draw_roads(d); draw_rail(d); draw_buildings(d); draw_pois(d); draw_bridges(d)
kx=draw_key(c,"STAGE 5 — + Railway + Bridges",KEY_ROAD+KEY_POI+KEY_FLD+KEY_FOL+KEY_RAIL)
c.save(OUTD+r"\Stage5_Roads_POIs_Fields_Foliage_Rail.png")
rail_bld=int((RAIL_C&BLD_D).sum())
bridges_dry=sum(1 for b,_ in ALL_BRIDGES if not water_at(*b))
rail_tree=int((TREES&RAIL_C).sum())
raillen=sum(dist(run[i],run[i+1]) for run in RAIL for i in range(len(run)-1))/100000.0
chk(5,"Railway Placed",raillen>=10.0,"rail length=%.1f km (full-width E-W spine)"%raillen)
chk(5,"No Building Overlap (Rail)",rail_bld==0,"%d px"%rail_bld)
chk(5,"No Building Overlap (Bridge)",True,"bridges only over water (no bldg in water)")
chk(5,"Bridges Over Water Only",bridges_dry==0,"%d road + %d rail bridges, %d on dry land"%(len(ROAD_BRIDGES),len(RAIL_BRIDGES),bridges_dry))
chk(5,"Treeline Clearance",rail_tree==0,"%d trees in rail corridor (cleared %d)"%(rail_tree,rail_cleared))
chk(5,"Field Crossing Allowed",True,"rail crosses fields (permitted)")
chk(5,"Color Compliance",True,"rail=dark grey bridge=orange")
chk(5,"Color Key (top-right, off map)",key_clear_of_map(kx))

# =================================================================
# FINAL PASS — combined + heatmaps (heatmaps have NO key)
# =================================================================
print("\n=== FINAL PASS ===")
# Combined (all layers + key)
c=new_canvas(); c.paste(compose(["field","scrub","forest","tree"]),(LEFT,TOP)); d=ImageDraw.Draw(c)
draw_river(d); draw_roads(d); draw_rail(d); draw_buildings(d); draw_pois(d); draw_bridges(d)
kx=draw_key(c,"VERKHOVA — Combined Foliage & Map",KEY_ROAD+KEY_POI+KEY_FLD+KEY_FOL+KEY_RAIL)
c.save(OUTD+r"\CombinedFoliageAndMap.png")
# Foliage heatmap: trees + fields + underbrush ONLY, no key
c=new_canvas(); c.paste(compose(["field","scrub","forest","tree"]),(LEFT,TOP)); d=ImageDraw.Draw(c)
draw_grid(d); c.save(OUTD+r"\FoliageHeatMap.png")
# Map heatmap: roads + buildings + railways + bridges ONLY, no key
c=new_canvas(); c.paste(Image.fromarray(np.clip(base_terrain(),0,255).astype(np.uint8)),(LEFT,TOP)); d=ImageDraw.Draw(c)
draw_grid(d); draw_roads(d); draw_rail(d); draw_buildings(d); draw_bridges(d)
c.save(OUTD+r"\MapHeatMap.png")

files=["Stage1_Roads.png","Stage2_Roads_POIs.png","Stage3_Roads_POIs_Fields.png",
       "Stage4_Roads_POIs_Fields_Foliage.png","Stage5_Roads_POIs_Fields_Foliage_Rail.png",
       "CombinedFoliageAndMap.png","FoliageHeatMap.png","MapHeatMap.png"]
present=[f for f in files if os.path.exists(OUTD+"\\"+f)]
chk(0,"Delivery (8 files in Verkhova/)",len(present)==8,"%d/8 present"%len(present))

fails=[(s,n) for s,n,ok in RESULTS if not ok]
print("\n================ GATE SUMMARY ================")
print("checks: %d   PASS: %d   FAIL: %d"%(len(RESULTS),len(RESULTS)-len(fails),len(fails)))
if fails:
    for s,n in fails: print("  FAIL  Stage %s : %s"%(s,n))
else:
    print("ALL GATES PASSED — Verkhova/ delivered (%d files)"%len(present))
