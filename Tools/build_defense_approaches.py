"""Find additional original-geometry peeks for each fixed defender position."""
import bpy, json, math
from pathlib import Path
from mathutils import Vector

root = Path(__file__).resolve().parents[1]
helpers = (root/'Tools/build_combat_training_spots.py').read_text(encoding='utf8').split('# Hints are on source combat floors.')[0]
# The helper prefix only constructs BVHs; supply its required output argument.
import sys
sys.argv += ['--', str(root/'Saved/AscentCombatSimple/defense_approaches.json')]
exec(compile(helpers, 'combat_geometry_helpers', 'exec'))
data = json.loads((root/'Content/Data/trainer_combat_spots_v1.json').read_text(encoding='utf8'))
def blender(v): return Vector((v[0]/100, -v[1]/100, v[2]/100))
def ue(v): return [v.x*100, -v.y*100, v.z*100]
report = []
for row in data['points']:
    defender = blender(row['enemy']['pos'])
    base = blender(row['player']['pos'])
    base_peek = blender(row['peek']['pos'])
    selected = [(base, base_peek)]
    candidates = []
    # Search floors around this actual defense position at local elevation and source-route elevations.
    for ix in range(-25, 26):
        for iy in range(-25, 26):
            x, y = defender.x+ix*.75, defender.y+iy*.75
            for hint in sorted(set([round(defender.z, 1), round(base.z, 1), round(base_peek.z, 1)])):
                peek = floor_at(x,y,hint)
                if peek is None or not 2.5 < (peek-defender).length < 26: continue
                if not visible(peek,defender) or not standing_clear(peek): continue
                for ox,oy in [(1,0),(-1,0),(0,1),(0,-1),(1,1),(-1,1),(1,-1),(-1,-1)]:
                    hide = floor_at(x+ox*1.3,y+oy*1.3,peek.z)
                    if hide is None or visible(hide,defender) or not standing_clear(hide): continue
                    if not reachable(hide,peek): continue
                    direction=(peek-defender).normalized()
                    angle=math.degrees(math.atan2(direction.y,direction.x))
                    candidates.append((hide,peek,angle))
                    break
    # Prefer widely separated actual viewing directions, not repeated positions on one entry.
    while candidates and len(selected)<5:
        best=max(candidates,key=lambda c:min(math.degrees((c[1]-defender).normalized().angle((p-defender).normalized())) for _,p in selected))
        separation=min(math.degrees((best[1]-defender).normalized().angle((p-defender).normalized())) for _,p in selected)
        if separation<8: break
        selected.append((best[0],best[1]));candidates.remove(best)
    row['defense_approaches']=[{'hide':ue(h),'peek':ue(p)} for h,p in selected]
    evidence={'id':row['id'],'count':len(selected),'routes':[{'hide_m':list(h),'peek_m':list(p),'hidden':not visible(h,defender),'revealed':visible(p,defender),'ground_capsule_path':reachable(h,p)} for h,p in selected]}
    report.append(evidence)
    print('[defense-routes]',row['id'],len(selected),flush=True)
(root/'Saved/AscentCombatSimple/defense_approaches.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
(root/'Content/Data/trainer_combat_spots_v1.json').write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf8')
assert all(r['count']>=2 for r in report), 'Some defense points still lack distinct approaches'
