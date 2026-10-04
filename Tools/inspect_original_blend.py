"""Render diagnostic screenshots from the original Ascent blend."""
import bpy
import os
import math
from mathutils import Vector

OUT = os.environ.get("RENDER_OUT", r"F:/project/gitprojects/UE/valorant/ascent1/Saved/AscentOriginalInspect")
os.makedirs(OUT, exist_ok=True)

scene = bpy.context.scene
print("[Inspect] blend:", bpy.data.filepath)
meshes = [o for o in scene.objects if o.type == "MESH"]
print("[Inspect] mesh_count:", len(meshes))

total_polys = 0
total_verts = 0
for o in meshes:
    total_polys += len(o.data.polygons)
    total_verts += len(o.data.vertices)
print("[Inspect] total_polys:", total_polys, "total_verts:", total_verts)

# Scene bounds
mins = [1e9, 1e9, 1e9]
maxs = [-1e9, -1e9, -1e9]
for o in meshes:
    for c in o.bound_box:
        w = o.matrix_world @ Vector(c)
        for i in range(3):
            mins[i] = min(mins[i], w[i])
            maxs[i] = max(maxs[i], w[i])
print("[Inspect] bbox_min", [round(v, 1) for v in mins])
print("[Inspect] bbox_max", [round(v, 1) for v in maxs])
center = [(mins[i] + maxs[i]) * 0.5 for i in range(3)]
print("[Inspect] center", [round(v, 1) for v in center])

# World + light
world = bpy.data.worlds.new("InspectWorld")
scene.world = world
world.use_nodes = True
nodes = world.node_tree.nodes
links = world.node_tree.links
bg = nodes.get("Background")
if bg is None:
    bg = nodes.new("ShaderNodeBackground")
    out = nodes.get("World Output") or nodes.new("ShaderNodeOutputWorld")
    links.new(bg.outputs["Background"], out.inputs["Surface"])
bg.inputs["Color"].default_value = (0.55, 0.7, 0.9, 1)
bg.inputs["Strength"].default_value = 0.7

sun_data = bpy.data.lights.new("Sun", type="SUN")
sun_data.energy = 4
sun = bpy.data.objects.new("Sun", sun_data)
sun.rotation_euler = (math.radians(50), 0, math.radians(30))
scene.collection.objects.link(sun)

scene.render.engine = "BLENDER_EEVEE"
scene.render.resolution_x = 1280
scene.render.resolution_y = 800
scene.view_settings.view_transform = "Standard"


def add_cam(name, loc, target, lens=30, ortho=None):
    data = bpy.data.cameras.new(name)
    cam = bpy.data.objects.new(name, data)
    scene.collection.objects.link(cam)
    cam.location = loc
    d = Vector(target) - Vector(loc)
    cam.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
    if ortho:
        data.type = "ORTHO"
        data.ortho_scale = ortho
    else:
        data.lens = lens
    return cam


# Playfield focus around region centers
c = Vector((-55, -25, 68))
cam_top = add_cam("Cam_Top", (c.x, c.y, 400), (c.x, c.y, 60), ortho=220)
cam_iso = add_cam("Cam_Iso", (c.x + 120, c.y - 160, c.z + 120), c, lens=28)
cam_a = add_cam("Cam_A", (45, -55, 100), (8, -10, 72), lens=32)
cam_b = add_cam("Cam_B", (-70, -35, 100), (-115, 2, 68), lens=32)
cam_mid = add_cam("Cam_Mid", (-15, -85, 105), (-40, -25, 72), lens=30)
cam_street = add_cam("Cam_Street", (-20, -70, 85), (-20, -10, 72), lens=24)

for cam, name in [
    (cam_top, "orig_top.png"),
    (cam_iso, "orig_iso.png"),
    (cam_a, "orig_a.png"),
    (cam_b, "orig_b.png"),
    (cam_mid, "orig_mid.png"),
    (cam_street, "orig_street.png"),
]:
    scene.camera = cam
    path = os.path.join(OUT, name)
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    print("[Inspect] wrote", path)

print("[Inspect] DONE")
