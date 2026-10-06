"""Focused reference renders; never save or alter the original source file."""
import bpy, sys, os
from mathutils import Vector
out = os.path.abspath(sys.argv[sys.argv.index('--') + 1])
os.makedirs(out, exist_ok=True)
scene = bpy.context.scene
for obj in scene.objects:
    if obj.type != 'MESH' or any(any(x in c.name for x in ('Vista','VFX','Lighting')) for c in obj.users_collection):
        obj.hide_render = True
scene.render.engine = 'BLENDER_WORKBENCH'
scene.render.resolution_x, scene.render.resolution_y = 1400, 1100
scene.render.resolution_percentage = 100
shading = scene.display.shading
shading.light = 'STUDIO'
shading.color_type = 'MATERIAL'
shading.show_shadows = shading.show_cavity = True
shading.background_type = 'WORLD'
scene.world.color = (.06,.06,.06)
cam = bpy.data.objects.new('ReferenceInspectionCamera', bpy.data.cameras.new('ReferenceInspectionCamera'))
scene.collection.objects.link(cam)
scene.camera = cam
cam.data.type = 'ORTHO'
for name, loc, aim, scale in [
    ('source_mid', (65,8,49), (19,49,3), 74),
    ('source_a_site', (89,44,90), (62,69,5), 70),
    ('source_b_site', (-76,40,50), (-30,78,1), 59),
]:
    cam.location = loc
    cam.rotation_euler = (Vector(aim)-cam.location).to_track_quat('-Z','Y').to_euler()
    cam.data.ortho_scale = scale
    scene.render.filepath = os.path.join(out, name+'.png')
    bpy.ops.render.render(write_still=True)
    print('[source-region]', name, flush=True)
