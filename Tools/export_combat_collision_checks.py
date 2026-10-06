"""Export native collision probes for restored walls/cover and audit old points."""
import bpy, json
from pathlib import Path
from mathutils import Vector
root=Path(__file__).resolve().parents[1]
audit_out=root/'Saved/AscentCombatSimple'
names=['Kingdom_0_MidWall','Shell_3_MidWallA','Kingdom_0_MidRampRailing_3.001',
       'Kingdom_0_MidRampRailing_3.002','FTBox_0_C_0','Pizza_0_RollUpDoor',
       'BSite_0_BoathouseBase','BoatShop_0_MainWalls','BSite_0_BoatHouseChunkA','BoatShop_0_MainBarricade',
       'Boat_1_BoxSternedRowboat_55','ABombsite_0_TarpCratesUp',
       'AScaffold_1_CoverAISMA_1','TarpCrate_0_CornerCloth_6']
objects={obj.get('source_object'):obj for obj in bpy.context.scene.objects if obj.type=='MESH'}
checks=[]
def ue(v):return [v.x*100,-v.y*100,v.z*100]
for name in names:
    obj=objects[name]
    obj.data.calc_loop_triangles()
    candidates=[]
    for tri in obj.data.loop_triangles:
        a,b,c=[obj.matrix_world@obj.data.vertices[i].co for i in tri.vertices]
        normal=(b-a).cross(c-a)
        if normal.length<1e-7:continue
        normal.normalize()
        if abs(normal.z)>.4:continue
        center=(a+b+c)/3
        candidates.append((tri.area,center,normal))
    assert candidates, name+' has no vertical surface'
    _,center,normal=max(candidates,key=lambda x:x[0])
    checks.append({'name':name,'start':ue(center+normal*.15),
                   'end':ue(center-normal*.15),'surface':ue(center)})
(root/'Content/Data/trainer_map_geometry_checks.json').write_text(json.dumps({'checks':checks},indent=2),encoding='utf8')
# Use the full restored world for ground, capsule clearance and cover checks.
exec(compile((root/'Tools/build_combat_training_spots.py').read_text(encoding='utf8').split('# Hints are on source combat floors.')[0],'restored_geometry_helpers','exec'))
data=json.loads((root/'Content/Data/trainer_combat_spots_v1.json').read_text(encoding='utf8'))
def source(p):return Vector((p[0]/100,-p[1]/100,p[2]/100))
audits=[]
for point in data['points']:
    p,e,peek=[source(point[key]['pos']) for key in ('player','enemy','peek')]
    audits.append({'id':point['id'],'standing_clear':[standing_clear(v) for v in (p,e,peek)],
                   'hidden':not visible(p,e),'exposed':visible(peek,e),'walking_peek':reachable(p,peek)})
(audit_out/'restored_geometry_point_audit.json').write_text(json.dumps({'points':audits,'collision_checks':checks},indent=2),encoding='utf8')
print('[restored-point-audit]',json.dumps(audits),flush=True)
