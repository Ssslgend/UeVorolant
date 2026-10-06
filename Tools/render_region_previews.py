"""Render preview stills for a few AscentPlayableRegions FBX-like blends.

Actually opens each region by appending isn't needed - regions were built in
one blend. Re-open AscentPlayable and re-crop? Simpler: load regions_report
and render from the already-built scene saved? We did not save per-region blend.

This script loads AscentPlayable.blend and rebuilds ONE region lightly for
preview only - too heavy. Instead render from FBX? Blender can import FBX.

Simplest: import each FBX and render.
"""
import math
import os
import sys

import bpy
from mathutils import Vector


def args():
    argv = sys.argv
    return argv[argv.index("--") + 1 :] if "--" in argv else []


def clear_scene():
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)


def look_at(obj, target):
    d = Vector(target) - obj.location
    obj.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()


def render_fbx(fbx_path, out_png, label):
    clear_scene()
    bpy.ops.import_scene.fbx(filepath=fbx_path)
    objs = [o for o in bpy.data.objects if o.type == "MESH"]
    if not objs:
        print("NO_MESH", fbx_path)
        return
    mins = Vector((1e9, 1e9, 1e9))
    maxs = Vector((-1e9, -1e9, -1e9))
    for o in objs:
        for c in o.bound_box:
            w = o.matrix_world @ Vector(c)
            for i in range(3):
                mins[i] = min(mins[i], w[i])
                maxs[i] = max(maxs[i], w[i])
    center = (mins + maxs) * 0.5
    size = maxs - mins

    world = bpy.data.worlds.new("W")
    bpy.context.scene.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes.get("Background")
    if bg is None:
        bg = world.node_tree.nodes.new("ShaderNodeBackground")
        out = world.node_tree.nodes.get("World Output") or world.node_tree.nodes.new("ShaderNodeOutputWorld")
        world.node_tree.links.new(bg.outputs[0], out.inputs[0])
    bg.inputs[0].default_value = (0.55, 0.7, 0.9, 1)
    bg.inputs[1].default_value = 0.8

    sun_data = bpy.data.lights.new("S", "SUN")
    sun_data.energy = 4
    sun = bpy.data.objects.new("S", sun_data)
    sun.rotation_euler = (math.radians(55), 0, math.radians(25))
    bpy.context.scene.collection.objects.link(sun)

    dist = max(size.x, size.y, 12) * 1.4 + 15
    cam_data = bpy.data.cameras.new("C")
    cam = bpy.data.objects.new("C", cam_data)
    cam_data.lens = 28
    bpy.context.scene.collection.objects.link(cam)
    cam.location = (center.x + dist * 0.7, center.y - dist * 0.9, center.z + dist * 0.55)
    look_at(cam, center)

    top_data = bpy.data.cameras.new("T")
    top_data.type = "ORTHO"
    top_data.ortho_scale = max(size.x, size.y) * 1.15
    top = bpy.data.objects.new("T", top_data)
    top.location = (center.x, center.y, center.z + dist)
    top.rotation_euler = (0, 0, 0)
    bpy.context.scene.collection.objects.link(top)

    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 960
    scene.render.resolution_y = 600
    scene.view_settings.view_transform = "Standard"

    for cam_obj, suffix in [(cam, "iso"), (top, "top")]:
        scene.camera = cam_obj
        path = out_png.replace(".png", f"_{suffix}.png")
        scene.render.filepath = path
        bpy.ops.render.render(write_still=True)
        print("WROTE", path, "label", label)


def main():
    region_dir, out_dir = args()[0], args()[1]
    os.makedirs(out_dir, exist_ok=True)
    wanted = [
        "A_Main_Default.fbx",
        "A_Heaven_Default.fbx",
        "Mid_Tiles_Pizza.fbx",
        "BMain_Default.fbx",
    ]
    for fn in wanted:
        fbx = os.path.join(region_dir, fn)
        if not os.path.exists(fbx):
            print("MISSING", fbx)
            continue
        name = os.path.splitext(fn)[0]
        render_fbx(fbx, os.path.join(out_dir, name + ".png"), name)
        # purge imported data lightly between files
        for block in list(bpy.data.meshes):
            if block.users == 0:
                bpy.data.meshes.remove(block)
    print("PREVIEW_DONE")


if __name__ == "__main__":
    main()
