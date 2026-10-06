import bpy,sys,json,math
from pathlib import Path
from mathutils import Vector
root=Path(__file__).resolve().parents[1]
sys.argv += ['--',str(root/'Saved/TrainerVerification/walk_diagnostics.json')]
exec(compile((root/'Tools/build_combat_training_spots.py').read_text(encoding='utf8').split('# Hints are on source combat floors.')[0],'helpers','exec'))
dg=bpy.context.evaluated_depsgraph_get()
for y in [63.7,64.025,64.35,64.675,65,65.325,65.65,66,67,69]:
    p=world.ray_cast(Vector((63.375,y,5.5)),Vector((0,0,-1)),3)[0]
    if p is None:print('[walk-probe]',y,'no floor',flush=True);continue
    blockers=[]
    for z in [.25,.4,.9,1.55,1.82]:
        for i in range(16):
            d=Vector((math.cos(i*math.tau/16),math.sin(i*math.tau/16),0))
            h=bpy.context.scene.ray_cast(dg,p+Vector((0,0,z)),d,distance=.28)
            if h[0]:blockers.append((z,h[4].name,list(h[1])))
    print('[walk-probe]',y,'floor',tuple(p),'normal',tuple(world.ray_cast(Vector((63.375,y,5.5)),Vector((0,0,-1)),3)[1]),'standing',standing_clear(p),'blockers',blockers[:5],flush=True)
