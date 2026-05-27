"""Extract a clean river centerline + local width from the Layer_3 (water) mask.
Approach: largest connected component -> chamfer distance transform ->
geodesic diameter endpoints (2x BFS) -> Dijkstra center-hugging path (cost favors
high distance-transform = medial axis) -> resample -> world coords + width.
Also extracts the main tributary as a second arm.

Calibrated mapping (export_weight_map PNG):
    world_x = -HALF + col*SX     (col 0 = west)
    world_y = -HALF + row*SX     (row 0 = south; bottom of PNG = north)
"""
import json, os, heapq
import numpy as np
from PIL import Image
from collections import deque

DIR = r"D:\Black Soil\UE5 Project\BlackSoil\Saved\Terrain"
HALF, SX = 599760.0, 595.0
M_PER_PX = SX / 100.0       # meters per pixel (5.95)
THRESH = 64

m = np.asarray(Image.open(os.path.join(DIR, "export_Layer_3.png")).convert("L"))
H, W = m.shape
b = m > THRESH
print("coverage %.2f%%" % (b.mean()*100))

# largest CC
def largest_cc(b):
    lab=np.zeros(b.shape,np.int32); cur=0; best=(0,0)
    for sy in range(b.shape[0]):
        for sx in range(b.shape[1]):
            if b[sy,sx] and lab[sy,sx]==0:
                cur+=1; q=deque([(sy,sx)]); lab[sy,sx]=cur; cnt=0
                while q:
                    y,x=q.popleft(); cnt+=1
                    for dy in(-1,0,1):
                        for dx in(-1,0,1):
                            ny,nx=y+dy,x+dx
                            if 0<=ny<b.shape[0] and 0<=nx<b.shape[1] and b[ny,nx] and lab[ny,nx]==0:
                                lab[ny,nx]=cur; q.append((ny,nx))
                if cnt>best[1]: best=(cur,cnt)
    return lab==best[0]
cc = largest_cc(b)

# chamfer DT (distance to background, px)
def chamfer_dt(mask):
    INF=1e9; d=np.where(mask,INF,0.0)
    Hh,Ww=mask.shape
    for y in range(Hh):
        row=d[y]; rowu=d[y-1] if y>0 else None
        for x in range(Ww):
            if row[x]>0:
                bst=row[x]
                if x>0: bst=min(bst,row[x-1]+1)
                if rowu is not None:
                    bst=min(bst,rowu[x]+1)
                    if x>0: bst=min(bst,rowu[x-1]+1.41421356)
                    if x<Ww-1: bst=min(bst,rowu[x+1]+1.41421356)
                row[x]=bst
    for y in range(Hh-1,-1,-1):
        row=d[y]; rowd=d[y+1] if y<Hh-1 else None
        for x in range(Ww-1,-1,-1):
            if row[x]>0:
                bst=row[x]
                if x<Ww-1: bst=min(bst,row[x+1]+1)
                if rowd is not None:
                    bst=min(bst,rowd[x]+1)
                    if x<Ww-1: bst=min(bst,rowd[x+1]+1.41421356)
                    if x>0: bst=min(bst,rowd[x-1]+1.41421356)
                row[x]=bst
    return d
dt = chamfer_dt(cc)
maxdt = dt.max()
print("max half-width px %.1f (~%.0f m full width)" % (maxdt, 2*maxdt*M_PER_PX))

idx = {p:i for i,p in enumerate(zip(*np.where(cc)))}
nodes = list(idx.keys())
print("component nodes:", len(nodes))

def neighbours(p):
    y,x=p
    for dy in(-1,0,1):
        for dx in(-1,0,1):
            if dy or dx:
                q=(y+dy,x+dx)
                if q in idx: yield q,(1.0 if (dy==0 or dx==0) else 1.41421356)

def bfs_far(src):
    dist={src:0.0}; q=deque([src]); far=src
    while q:
        u=q.popleft()
        for v,_ in neighbours(u):
            if v not in dist:
                dist[v]=dist[u]+1; q.append(v)
                if dist[v]>dist[far]: far=v
    return far, dist
a,_=bfs_far(nodes[0]); A,_=bfs_far(a); B,distA=bfs_far(A)
print("diameter endpoints px:", A, B, "geodesic len px %.0f"%distA[B])

# Dijkstra A->B, cost favors centerline (high dt). cost = step*(1 + k*(maxdt-dt))
k=0.06
def dijkstra(src,dst):
    INF=float('inf'); best={src:0.0}; pq=[(0.0,src)]; prev={}
    while pq:
        c,u=heapq.heappop(pq)
        if u==dst: break
        if c>best.get(u,INF): continue
        for v,w in neighbours(u):
            pen = 1.0 + k*(maxdt-dt[v[0],v[1]])
            nc=c+w*pen
            if nc<best.get(v,INF):
                best[v]=nc; prev[v]=u; heapq.heappush(pq,(nc,v))
    path=[dst];
    while path[-1]!=src: path.append(prev[path[-1]])
    path.reverse(); return path
main=dijkstra(A,B)
print("main spine px:", len(main))

def to_world(p):
    y,x=p; return (-HALF+x*SX, -HALF+y*SX)
def resample(path, step_px=26):
    out=[path[0]]; acc=0.0
    for i in range(1,len(path)):
        dy=path[i][0]-path[i-1][0]; dx=path[i][1]-path[i-1][1]
        acc+=(dy*dy+dx*dx)**0.5
        if acc>=step_px: out.append(path[i]); acc=0.0
    if out[-1]!=path[-1]: out.append(path[-1])
    return out
def arm_to_pts(path):
    pts=[]
    for p in resample(path):
        wx,wy=to_world(p)
        pts.append({"x":round(float(wx),1),"y":round(float(wy),1),
                    "width_m":round(float(2*dt[p[0],p[1]]*M_PER_PX),1)})
    return pts

# tributary: farthest node from the main spine, traced back toward spine
mainset=set(main)
_,dmain=None,None
dist_to_main={p:0 for p in main}; q=deque(main)
while q:
    u=q.popleft()
    for v,_ in neighbours(u):
        if v not in dist_to_main:
            dist_to_main[v]=dist_to_main[u]+1; q.append(v)
trib_tip=max(dist_to_main,key=lambda p:dist_to_main[p])
trib_len=dist_to_main[trib_tip]
print("tributary tip dist from main px:", trib_len)
rivers=[{"name":"Kalmius_Main","points":arm_to_pts(main)}]
if trib_len>40:   # only if a real arm
    trib=dijkstra(trib_tip, min(mainset,key=lambda p:(p[0]-trib_tip[0])**2+(p[1]-trib_tip[1])**2))
    rivers.append({"name":"Kalmius_Tributary","points":arm_to_pts(trib)})

for r in rivers:
    ws=[p["width_m"] for p in r["points"]]
    print(f"{r['name']}: {len(r['points'])} pts, width {min(ws):.0f}-{max(ws):.0f}m, "
          f"start({r['points'][0]['x']:.0f},{r['points'][0]['y']:.0f}) end({r['points'][-1]['x']:.0f},{r['points'][-1]['y']:.0f})")
json.dump({"rivers":rivers}, open(os.path.join(DIR,"river_world.json"),"w"))
print("saved river_world.json")
