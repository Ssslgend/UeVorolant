"""Build verification-only walk waypoints on unchanged source floors and cover."""
import bpy, json, math, heapq, sys
from pathlib import Path
from mathutils import Vector
root=Path(__file__).resolve().parents[1]
sys.argv += ['--',str(root/'Saved/TrainerVerification/combat_walk_routes.json')]
exec(compile((root/'Tools/build_combat_training_spots.py').read_text(encoding='utf8').split('# Hints are on source combat floors.')[0],'source_geometry_helpers','exec'))
# Composite building meshes also contain stairs and interior floors. Trace all native geometry.
def floor_at(x,y,hint):
    hit=world.ray_cast(Vector((x,y,hint+.4)),Vector((0,0,-1)),4.2)
    # Small stair nosings have steep normals; step-height/path checks handle these edges.
    return hit[0] if hit[0] is not None and abs(hit[1].z)>.2 else None
data=json.loads((root/'Content/Data/trainer_combat_spots_v1.json').read_text(encoding='utf8'))
def source(v):return Vector((v[0]/100,-v[1]/100,v[2]/100))
def ue(v):return [v.x*100,-v.y*100,v.z*100]
def walk_edge(a,b):
    if b.z-a.z>=-.4:return reachable(a,b)
    # Original catwalks include drops. Require clear departure and a clear landing column.
    if a.z-b.z>3.5:return False
    upper=Vector((b.x,b.y,a.z))
    d=(upper-a).normalized();side=Vector((-d.y,d.x,0))
    for z in [.4,.9,1.6]:
        for offset in [-.24,0,.24]:
            if not clear_segment(a+side*offset+Vector((0,0,z)),upper+side*offset+Vector((0,0,z))):return False
    return clear_segment(b+Vector((0,0,.25)),upper+Vector((0,0,1.82)))
step=.65
def key(p):return (round(p.x/step),round(p.y/step),round(p.z*10))
def aligned(p):
    choices=[]
    for dx in range(-1,2):
        for dy in range(-1,2):
            q=floor_at((round(p.x/step)+dx)*step,(round(p.y/step)+dy)*step,p.z)
            if q is not None and standing_clear(q) and reachable(p,q):choices.append(q)
    return min(choices,key=lambda q:(q-p).length) if choices else None
def path_between(start,end):
    initial=aligned(start);target=aligned(end)
    if initial is None or target is None:return None,0
    initial_key=key(initial);target_key=key(target)
    heap=[((initial-target).length,0,initial_key)]
    points={initial_key:initial};cost={initial_key:0};previous={};closed=set();serial=0
    while heap:
        _,_,k=heapq.heappop(heap)
        if k in closed:continue
        closed.add(k);p=points[k]
        if (p-target).length<.7 and reachable(p,target):
            result=[end,target,p]
            while k in previous:k=previous[k];result.append(points[k])
            result.append(start);return list(reversed(result)),len(closed)
        if len(closed)>140000:break
        for dx,dy in [(1,0),(-1,0),(0,1),(0,-1),(1,1),(-1,1),(1,-1),(-1,-1)]:
            q=floor_at(p.x+dx*step,p.y+dy*step,p.z)
            if q is None or q.z-p.z>.4 or p.z-q.z>3.5:continue
            qk=key(q)
            if qk in closed:continue
            score=cost[k]+(q-p).length
            if score>=cost.get(qk,math.inf):continue
            if not standing_clear(q) or not walk_edge(p,q):continue
            points[qk]=q;cost[qk]=score;previous[qk]=k;serial+=1
            heapq.heappush(heap,(score+(q-target).length,serial,qk))
    nearest=min((points[k] for k in closed),key=lambda p:(p-target).length)
    print('[walk-frontier]',tuple(nearest),'target',tuple(target),'distance',(nearest-target).length,flush=True)
    return None,len(closed)
tour=[(data['points'][0]['id']+'_player',source(data['points'][0]['player']['pos']))]
# Traverse every actual target floor, including both sites and mid, without teleporting between them.
for i in [0,1,2,4,3,6,5]:tour.append((data['points'][i]['id']+'_enemy',source(data['points'][i]['enemy']['pos'])))
routes=[]
if '--only-first' in sys.argv:tour=tour[:2]
for (a,start),(b,end) in zip(tour,tour[1:]):
    points,visited=path_between(start,end)
    print('[combat-walk]',a,'->',b,'found',bool(points),'nodes',visited,flush=True)
    routes.append({'from':a,'to':b,'found':bool(points),'visited':visited,'waypoints':[ue(p) for p in points] if points else []})
out=root/'Saved/TrainerVerification/combat_walk_routes.json'
out.write_text(json.dumps({'source_blend':bpy.data.filepath,'routes':routes},ensure_ascii=False,indent=2),encoding='utf8')
assert all(r['found'] for r in routes),'A native-floor route could not be verified'
