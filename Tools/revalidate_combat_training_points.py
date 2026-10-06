"""Adjust only points affected by restored source walls, never remove the walls."""
import bpy,json,math,sys
from pathlib import Path
from mathutils import Vector
root=Path(__file__).resolve().parents[1]
exec(compile((root/'Tools/build_combat_training_spots.py').read_text(encoding='utf8').split('# Hints are on source combat floors.')[0],'restored_point_helpers','exec'))
data_path=root/'Content/Data/trainer_combat_spots_v1.json'
data=json.loads(data_path.read_text(encoding='utf8'))
source_report=json.loads((root/'Saved/AscentCombatSimple/source_training_spots.json').read_text(encoding='utf8'))
source_rows={r['id']:r for r in source_report['points']}
def source(p):return Vector((p[0]/100,-p[1]/100,p[2]/100))
def ue(p):return [p.x*100,-p.y*100,p.z*100]
def valid(p,e,k):return all(standing_clear(v) for v in (p,e,k)) and not visible(p,e) and visible(k,e) and reachable(p,k)
changes=[]
for row in data['points']:
    p,e,k=[source(row[key]['pos']) for key in ('player','enemy','peek')]
    original=(p.copy(),e.copy(),k.copy())
    if not valid(p,e,k):
        best=None
        # Try retaining both player positions first; move the enemy off the
        # restored wall or around newly restored cover only as far as needed.
        candidates=[]
        for ix in range(-18,19):
            for iy in range(-18,19):
                enemy=floor_at(e.x+ix*.65,e.y+iy*.65,e.z)
                if enemy is not None and standing_clear(enemy):candidates.append(enemy)
        candidates.sort(key=lambda v:(v-e).length)
        for enemy in candidates:
            if valid(p,enemy,k):best=((enemy-e).length,p,enemy,k);break
        if best is None:
            for ix in range(-6,7):
                for iy in range(-6,7):
                    player=floor_at(p.x+ix*.65,p.y+iy*.65,p.z)
                    if player is None or not standing_clear(player):continue
                    for enemy in candidates[:200]:
                        if (player-enemy).length<3 or visible(player,enemy):continue
                        for dx,dy in [(1,0),(-1,0),(0,1),(0,-1),(1,1),(-1,1),(1,-1),(-1,-1)]:
                            peek=floor_at(player.x+dx*1.3,player.y+dy*1.3,player.z)
                            if peek is None or not valid(player,enemy,peek):continue
                            score=(player-p).length+(enemy-e).length+(peek-k).length*.25
                            if best is None or score<best[0]:best=(score,player,enemy,peek)
        assert best is not None,'No corner for restored map point '+row['id']
        _,p,e,k=best
        changes.append({'id':row['id'],'before':[list(v) for v in original],'after':[list(v) for v in (p,e,k)]})
    assert valid(p,e,k),row['id']
    for key,pos in zip(('player','enemy','peek'),(p,e,k)):row[key]['pos']=ue(pos)
    row['player']['yaw']=math.degrees(math.atan2(-(e.y-p.y),e.x-p.x))
    row['enemy']['yaw']=row['player']['yaw']+180
    row['defense_approaches']=[]
    src=source_rows[row['id']]
    for key,pos in zip(('player_m','enemy_m','peek_m'),(p,e,k)):src[key]=list(pos)
    print('[restored-point]',row['id'],'player',tuple(p),'enemy',tuple(e),'peek',tuple(k),flush=True)
data_path.write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf8')
(root/'Saved/AscentCombatSimple/source_training_spots.json').write_text(json.dumps(source_report,ensure_ascii=False,indent=2),encoding='utf8')
(root/'Saved/AscentCombatSimple/restored_point_changes.json').write_text(json.dumps({'changes':changes,'all_points_valid':True},indent=2),encoding='utf8')
