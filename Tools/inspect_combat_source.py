"""Read source visibility and render the playable area without changing its file."""
import bpy, json, sys, os
from mathutils import Vector

out = os.path.abspath(sys.argv[sys.argv.index('--') + 1])
os.makedirs(out, exist_ok=True)
scene = bpy.context.scene
rows = []
for obj in scene.objects:
    if obj.type != 'MESH':
        continue
    rows.append({'name': obj.name, 'hide_render': obj.hide_render,
                 'hide_viewport': obj.hide_viewport, 'hidden': obj.hide_get(),
                 'location': list(obj.location), 'users': obj.data.users,
                 'collections': [c.name for c in obj.users_collection]})
with open(os.path.join(out, 'source_visibility.json'), 'w', encoding='utf-8') as f:
    json.dump({'units': scene.unit_settings.system, 'scale_length': scene.unit_settings.scale_length,
               'objects': rows}, f, indent=2)
for obj in scene.objects:
    if obj.type != 'MESH' or any(('Vista' in c.name or 'VFX' in c.name or 'Lighting' in c.name) for c in obj.users_collection):
        obj.hide_render = True
    elif all(abs(v) < 0.001 for v in obj.location):
        obj.hide_render = True
scene.render.engine = 'BLENDER_WORKBENCH'
scene.render.resolution_x = 1400
scene.render.resolution_y = 1100
scene.render.resolution_percentage = 100
scene.display.shading.light = 'STUDIO'
scene.display.shading.color_type = 'MATERIAL'
scene.display.shading.show_shadows = True
scene.display.shading.show_cavity = True
scene.display.shading.cavity_type = 'BOTH'
scene.display.shading.show_specular_highlight = False
scene.display.shading.background_type = 'WORLD'
scene.world.color = (0.06, 0.06, 0.06)
camera_data = bpy.data.cameras.new('InspectionCamera')
camera = bpy.data.objects.new('InspectionCamera', camera_data)
scene.collection.objects.link(camera)
camera_data.type = 'ORTHO'
camera_data.ortho_scale = 175
camera.location = (25, 50, 220)
camera.rotation_euler = (Vector((25, 50, 3)) - camera.location).to_track_quat('-Z', 'Y').to_euler()
scene.camera = camera
scene.render.filepath = os.path.join(out, 'source_combat_top.png')
bpy.ops.render.render(write_still=True)
print('[source-inspection] finished', flush=True)
