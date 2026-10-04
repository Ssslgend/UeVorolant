"""Build a simplified Ascent map blockout from primitives.

Reference layout is derived from Saved/AscentDuelExport/scene_inventory.json
and Tools/ascent_region_config.json (source: H:\\baiduyun\\亚海悬城reclear.blend).

Usage:
  blender -b -P Tools/build_simplified_ascent.py -- <output.blend> [render_dir]
"""

import math
import os
import sys

import bpy
from mathutils import Vector


def args_after_double_dash():
    argv = sys.argv
    if "--" not in argv:
        return []
    return argv[argv.index("--") + 1 :]


# ---- scene helpers ---------------------------------------------------------

def clean_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1.0
    # Drop default collection contents/names, then recreate root.
    for collection in list(bpy.data.collections):
        bpy.data.collections.remove(collection)
    root = bpy.data.collections.new("SimplifiedAscent")
    scene.collection.children.link(root)


def ensure_collection(name, parent_name="SimplifiedAscent"):
    parent = bpy.data.collections.get(parent_name)
    if parent is None:
        parent = bpy.data.collections.new(parent_name)
        bpy.context.scene.collection.children.link(parent)
    coll = bpy.data.collections.get(name)
    if coll is None:
        coll = bpy.data.collections.new(name)
        parent.children.link(coll)
    elif parent.children.get(name) is None:
        parent.children.link(coll)
    return coll


def link_to(obj, collection_name):
    coll = ensure_collection(collection_name)
    for c in list(obj.users_collection):
        c.objects.unlink(obj)
    coll.objects.link(obj)


def make_mat(name, color, rough=0.65, metallic=0.0):
    mat = bpy.data.materials.get(name)
    if mat is None:
        mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    bsdf = nodes.get("Principled BSDF")
    if bsdf is None:
        bsdf = nodes.new("ShaderNodeBsdfPrincipled")
        bsdf.name = "Principled BSDF"
        output = nodes.get("Material Output")
        if output is None:
            output = nodes.new("ShaderNodeOutputMaterial")
            output.name = "Material Output"
        links.new(bsdf.outputs["BSDF"], output.inputs["Surface"])
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Roughness"].default_value = rough
    bsdf.inputs["Metallic"].default_value = metallic
    return mat


def add_box(name, location, size, material, collection, bevel=0.08):
    bpy.ops.mesh.primitive_cube_add(size=1, location=location)
    obj = bpy.context.active_object
    obj.name = name
    obj.scale = (size[0] / 2, size[1] / 2, size[2] / 2)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel > 0:
        mod = obj.modifiers.new("Bevel", "BEVEL")
        mod.width = bevel
        mod.segments = 2
        mod.limit_method = "ANGLE"
    obj.data.materials.append(material)
    link_to(obj, collection)
    return obj


def add_cylinder(name, location, radius, depth, material, collection, vertices=24):
    bpy.ops.mesh.primitive_cylinder_add(
        vertices=vertices, radius=radius, depth=depth, location=location
    )
    obj = bpy.context.active_object
    obj.name = name
    obj.data.materials.append(material)
    link_to(obj, collection)
    return obj


def add_plane_pad(name, location, size, material, collection, z_offset=0.02):
    return add_box(
        name,
        (location[0], location[1], location[2] + z_offset),
        (size[0], size[1], 0.12),
        material,
        collection,
        bevel=0.04,
    )


# ---- materials -------------------------------------------------------------

def build_materials():
    return {
        "stone": make_mat("MAT_Stone_Cream", (0.78, 0.68, 0.52), 0.72),
        "stone_dark": make_mat("MAT_Stone_Dark", (0.32, 0.30, 0.28), 0.8),
        "plaster": make_mat("MAT_Plaster_Warm", (0.92, 0.82, 0.66), 0.68),
        "terracotta": make_mat("MAT_Terracotta", (0.72, 0.28, 0.14), 0.62),
        "wood": make_mat("MAT_Wood", (0.48, 0.30, 0.14), 0.65),
        "metal": make_mat("MAT_Metal", (0.32, 0.38, 0.48), 0.35, 0.65),
        "grass": make_mat("MAT_Grass", (0.18, 0.42, 0.16), 0.88),
        "water": make_mat("MAT_Water", (0.08, 0.38, 0.58), 0.12, 0.15),
        "site_a": make_mat("MAT_Site_A", (0.95, 0.42, 0.08), 0.48),
        "site_b": make_mat("MAT_Site_B", (0.10, 0.42, 0.85), 0.48),
        "mid": make_mat("MAT_Mid", (0.92, 0.78, 0.18), 0.52),
        "cover": make_mat("MAT_Cover", (0.58, 0.42, 0.28), 0.72),
        "spawn_atk": make_mat("MAT_Spawn_Atk", (0.85, 0.18, 0.16), 0.52),
        "spawn_def": make_mat("MAT_Spawn_Def", (0.12, 0.38, 0.78), 0.52),
        "path": make_mat("MAT_Path", (0.70, 0.62, 0.52), 0.76),
        "accent": make_mat("MAT_Accent_Teal", (0.12, 0.62, 0.58), 0.45),
    }


# ---- map layout ------------------------------------------------------------
# Coordinates follow the source blend inventory (play height ~Z=65-70).
# Axis: +X east, +Y north, +Z up. Attacker spawn south, defender north.

PLAY_Z = 65.0
A_Z = 67.5
B_Z = 63.5
MID_Z = 66.0
ATK_Z = 65.5
DEF_Z = 67.0


def build_ground(m):
    add_box(
        "Ground_Base",
        (-55, -35, PLAY_Z - 1.2),
        (220, 200, 2.4),
        m["stone_dark"],
        "00_Ground",
        bevel=0,
    )
    add_box(
        "Ground_Main_Playfield",
        (-55, -35, PLAY_Z - 0.15),
        (170, 150, 0.3),
        m["stone"],
        "00_Ground",
        bevel=0.05,
    )
    # outer grass apron
    add_box(
        "Grass_North",
        (-55, 70, PLAY_Z - 0.4),
        (180, 40, 0.5),
        m["grass"],
        "00_Ground",
        bevel=0.1,
    )
    add_box(
        "Grass_South",
        (-55, -125, PLAY_Z - 0.4),
        (180, 35, 0.5),
        m["grass"],
        "00_Ground",
        bevel=0.1,
    )


def build_a_site(m):
    c = "10_A_Site"
    # main platform / garden
    add_box("A_Site_Platform", (5, -18, A_Z), (48, 42, 5.0), m["plaster"], c)
    add_plane_pad("A_Plant_Zone", (8, -5, A_Z + 2.5), (25, 15), m["site_a"], c)

    # castle walls (east of A)
    add_box("A_CastleWall_N", (-2, 8, A_Z + 6), (22, 3, 16), m["stone"], c)
    add_box("A_CastleWall_E", (14, -6, A_Z + 6), (3, 28, 16), m["stone"], c)
    add_box("A_CastleWall_S", (2, -38, A_Z + 5), (26, 3, 12), m["stone"], c)

    # A main entrance arch corridor (from south)
    add_box("A_Main_Floor", (-2, -55, A_Z - 0.5), (28, 30, 1.2), m["path"], c)
    add_box("A_Main_Wall_W", (-18, -55, A_Z + 4), (2.5, 30, 12), m["stone"], c)
    add_box("A_Main_Wall_E", (16, -55, A_Z + 4), (2.5, 30, 12), m["stone"], c)
    add_box("A_Main_Roof", (-1, -55, A_Z + 11), (32, 32, 1.2), m["terracotta"], c)

    # garden / tree room west of plant
    add_box("A_TreeRoom_Floor", (-18, -12, A_Z - 1.8), (18, 22, 1.0), m["grass"], c)
    add_box("A_TreeRoom_Wall", (-28, -12, A_Z + 3), (2, 22, 10), m["stone"], c)

    # ramps / stairs from mid
    add_box("A_Heaven_Platform", (-10, 12, A_Z + 4), (16, 10, 1.2), m["wood"], c)
    add_box("A_Heaven_Pillar", (-18, 12, A_Z), (2, 2, 10), m["stone_dark"], c)
    add_box("A_Heaven_Pillar2", (-2, 12, A_Z), (2, 2, 10), m["stone_dark"], c)

    # cover
    add_box("A_Cover_Crate1", (0, -12, A_Z + 3.2), (4, 4, 2.2), m["cover"], c)
    add_box("A_Cover_Crate2", (10, -22, A_Z + 3.2), (5, 3, 2.2), m["cover"], c)
    add_box("A_Cover_Barrel", (-6, -2, A_Z + 3.5), (2.2, 2.2, 2.5), m["metal"], c)


def build_b_site(m):
    c = "20_B_Site"
    add_box("B_Site_Platform", (-112, 5, B_Z), (42, 36, 3.5), m["plaster"], c)
    add_plane_pad("B_Plant_Zone", (-110, 3, B_Z + 1.8), (22, 23), m["site_b"], c)

    # boathouse block
    add_box("B_Boathouse_Base", (-138, 5, B_Z + 3), (18, 28, 8), m["wood"], c)
    add_box("B_Boathouse_Roof", (-138, 5, B_Z + 9), (20, 30, 1.5), m["terracotta"], c)
    add_box("B_Boathouse_Back", (-148, 8, B_Z + 6), (2, 22, 14), m["stone"], c)

    # switch room / defender box
    add_box("B_Switchhouse", (-97, 0, B_Z + 4), (20, 16, 10), m["stone"], c)
    add_box("B_Switchhouse_Roof", (-97, 0, B_Z + 10), (22, 18, 1.2), m["terracotta"], c)

    # B main interior
    add_box("B_Main_Floor", (-92, -28, B_Z + 0.4), (30, 14, 1.0), m["path"], c)
    add_box("B_Main_Wall_N", (-92, -20, B_Z + 6), (30, 2, 12), m["stone"], c)
    add_box("B_Main_Wall_S", (-92, -36, B_Z + 6), (30, 2, 12), m["stone"], c)
    add_box("B_Main_Wall_W", (-108, -28, B_Z + 6), (2, 16, 12), m["stone"], c)

    # lane wall between B and mid
    add_box("B_Lane_Wall", (-78, -8, B_Z + 5), (3, 36, 12), m["stone"], c)

    # cover
    add_box("B_Cover_Platform", (-78, 2, B_Z + 3), (6, 12, 4), m["cover"], c)
    add_box("B_Cover_Crate1", (-100, 12, B_Z + 2.8), (5, 4, 2.2), m["cover"], c)
    add_box("B_Cover_Crate2", (-122, -6, B_Z + 2.8), (4, 5, 2.2), m["cover"], c)
    add_box("B_Crate_Stack", (-118, 12, B_Z + 3.2), (3.5, 3.5, 3.0), m["wood"], c)


def build_mid(m):
    c = "30_Mid"
    add_box("Mid_Main_Floor", (-38, -35, MID_Z), (34, 58, 1.2), m["path"], c)
    add_plane_pad("Mid_Tile", (-38, -30, MID_Z + 0.7), (24, 20), m["mid"], c)

    # mid walls / pizza / tiles
    add_box("Mid_Wall_W", (-58, -35, MID_Z + 5), (2.5, 55, 12), m["stone"], c)
    add_box("Mid_Wall_E", (-18, -35, MID_Z + 5), (2.5, 55, 12), m["stone"], c)
    add_box("Mid_Kingdom_Wall", (-45, -48, MID_Z + 4), (7, 36, 8), m["stone_dark"], c)

    # mid top / tiles platform
    add_box("Mid_Top_Platform", (-38, -8, MID_Z + 1.5), (28, 18, 2.0), m["plaster"], c)
    add_box("Mid_Top_Railing", (-38, 1, MID_Z + 3.2), (28, 1.0, 2.0), m["metal"], c)

    # connector to defender
    add_box("Mid_Def_Floor", (-48, 12, DEF_Z), (22, 22, 1.2), m["path"], c)
    add_box("Mid_Def_Wall", (-48, 24, DEF_Z + 5), (22, 2, 12), m["stone"], c)

    # courtyard / market connector (west of mid)
    add_box("Mid_Courtyard", (-68, -6, MID_Z + 1), (22, 24, 2.0), m["plaster"], c)
    add_box("Mid_Courtyard_WallN", (-68, 8, MID_Z + 6), (22, 2, 14), m["stone"], c)
    add_box("Mid_Courtyard_WallW", (-80, -6, MID_Z + 6), (2, 24, 14), m["stone"], c)

    # cover in mid
    add_box("Mid_Cover_Box1", (-32, -28, MID_Z + 2.2), (4, 4, 2.5), m["cover"], c)
    add_box("Mid_Cover_Box2", (-44, -20, MID_Z + 2.2), (5, 3, 2.5), m["cover"], c)
    add_box("Mid_Pillar", (-38, -42, MID_Z + 4), (3, 3, 10), m["stone_dark"], c)


def build_market(m):
    c = "40_Market"
    add_box("Market_Floor", (-94, -42, B_Z + 0.5), (32, 28, 1.2), m["path"], c)
    add_box("Market_Building", (-94, -42, B_Z + 6), (28, 24, 12), m["plaster"], c)
    add_box("Market_Roof", (-94, -42, B_Z + 13), (32, 28, 1.4), m["terracotta"], c)
    # openings (doorways as darker recesses)
    add_box("Market_Door_Mid", (-78, -42, B_Z + 4), (1.5, 8, 8), m["wood"], c)
    add_box("Market_Door_B", (-94, -28, B_Z + 4), (8, 1.5, 8), m["wood"], c)
    add_box("Market_Window", (-94, -55, B_Z + 6), (10, 1.2, 5), m["water"], c)

    # market exterior north wall
    add_box("Market_Ext_Wall", (-99, -14, B_Z + 6), (42, 1.5, 12), m["stone"], c)


def build_spawns(m):
    c = "50_Spawns"
    # attacker spawn (south)
    add_box("Atk_Spawn_Floor", (-55, -100, ATK_Z), (70, 28, 1.2), m["path"], c)
    add_plane_pad("Atk_Spawn_Pad", (-55, -100, ATK_Z + 0.7), (50, 16), m["spawn_atk"], c)
    add_box("Atk_Spawn_Wall_S", (-55, -116, ATK_Z + 5), (70, 2, 12), m["stone"], c)
    add_box("Atk_Spawn_Wall_W", (-92, -100, ATK_Z + 5), (2, 28, 12), m["stone"], c)
    add_box("Atk_Spawn_Wall_E", (-18, -100, ATK_Z + 5), (2, 28, 12), m["stone"], c)
    add_box("Atk_Bridge", (-72, -88, ATK_Z + 1), (18, 16, 2.5), m["wood"], c)

    # defender spawn (north)
    add_box("Def_Spawn_Floor", (-48, 36, DEF_Z), (48, 24, 1.2), m["path"], c)
    add_plane_pad("Def_Spawn_Pad", (-48, 36, DEF_Z + 0.7), (36, 14), m["spawn_def"], c)
    add_box("Def_Spawn_Wall_N", (-48, 50, DEF_Z + 5), (48, 2, 12), m["stone"], c)
    add_box("Def_Spawn_Wall_W", (-72, 36, DEF_Z + 5), (2, 24, 12), m["stone"], c)
    add_box("Def_Spawn_Wall_E", (-24, 36, DEF_Z + 5), (2, 24, 12), m["stone"], c)

    # defender path to A
    add_box("Def_PathA_Floor", (-22, -2, DEF_Z + 0.2), (24, 24, 1.2), m["path"], c)
    add_box("Def_PathA_Wall", (-8, -2, DEF_Z + 5), (2, 24, 12), m["stone"], c)

    # defender path to B
    add_box("Def_PathB_Floor", (-72, 18, DEF_Z - 0.5), (28, 18, 1.2), m["path"], c)
    add_box("Def_PathB_Wall", (-72, 28, DEF_Z + 5), (28, 2, 10), m["stone"], c)


def build_outer_walls(m):
    c = "60_Outer"
    # simplified perimeter so the map reads as enclosed
    add_box("Perimeter_West", (-155, -35, PLAY_Z + 8), (3, 160, 20), m["stone_dark"], c)
    add_box("Perimeter_East", (35, -35, PLAY_Z + 8), (3, 160, 20), m["stone_dark"], c)
    add_box("Perimeter_South", (-55, -130, PLAY_Z + 8), (190, 3, 20), m["stone_dark"], c)
    add_box("Perimeter_North", (-55, 65, PLAY_Z + 8), (190, 3, 20), m["stone_dark"], c)

    # canal water strip near B (Ascent signature)
    add_box("Canal_Water", (-140, -20, PLAY_Z + 0.2), (18, 70, 0.4), m["water"], c)
    add_box("Canal_Wall_E", (-130, -20, PLAY_Z + 3), (2, 70, 8), m["stone"], c)


def build_wayfinding(m):
    c = "70_Wayfinding"
    # colored floor arrows / site letters as simple pads
    add_box("Sign_A", (8, -5, A_Z + 2.7), (8, 8, 0.2), m["site_a"], c, bevel=0.02)
    add_box("Sign_B", (-110, 3, B_Z + 2.0), (8, 8, 0.2), m["site_b"], c, bevel=0.02)
    add_box("Sign_Mid", (-38, -30, MID_Z + 0.9), (8, 8, 0.2), m["mid"], c, bevel=0.02)
    add_box("Sign_Atk", (-55, -100, ATK_Z + 0.9), (10, 6, 0.2), m["spawn_atk"], c, bevel=0.02)
    add_box("Sign_Def", (-48, 36, DEF_Z + 0.9), (10, 6, 0.2), m["spawn_def"], c, bevel=0.02)


def add_lighting_and_camera():
    bpy.ops.object.light_add(type="SUN", location=(40, -40, 180))
    sun = bpy.context.active_object
    sun.name = "Sun_Key"
    sun.data.energy = 3.5
    sun.data.angle = math.radians(8)
    sun.rotation_euler = (math.radians(48), math.radians(12), math.radians(28))

    bpy.ops.object.light_add(type="AREA", location=(-40, -20, 120))
    fill = bpy.context.active_object
    fill.name = "Sky_Fill"
    fill.data.energy = 800
    fill.data.size = 80

    world = bpy.data.worlds.new("AscentSky")
    bpy.context.scene.world = world
    world.use_nodes = True
    nodes = world.node_tree.nodes
    links = world.node_tree.links
    bg = nodes.get("Background")
    if bg is None:
        bg = nodes.new("ShaderNodeBackground")
        bg.name = "Background"
        output = nodes.get("World Output")
        if output is None:
            output = nodes.new("ShaderNodeOutputWorld")
            output.name = "World Output"
        links.new(bg.outputs["Background"], output.inputs["Surface"])
    bg.inputs["Color"].default_value = (0.55, 0.72, 0.92, 1.0)
    bg.inputs["Strength"].default_value = 0.55

    bpy.ops.object.camera_add(location=(55, -130, 130))
    cam = bpy.context.active_object
    cam.name = "Cam_Overview"
    cam.data.lens = 28
    # look at map center
    direction = Vector((-55, -25, 68)) - cam.location
    cam.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    bpy.context.scene.camera = cam

    bpy.ops.object.camera_add(location=(-55, -25, 220))
    top = bpy.context.active_object
    top.name = "Cam_Top"
    top.data.type = "ORTHO"
    top.data.ortho_scale = 220
    top.rotation_euler = (0, 0, 0)


def build_map():
    clean_scene()
    m = build_materials()
    build_ground(m)
    build_a_site(m)
    build_b_site(m)
    build_mid(m)
    build_market(m)
    build_spawns(m)
    build_outer_walls(m)
    build_wayfinding(m)
    add_lighting_and_camera()

    # collection for cameras/lights
    for obj in bpy.context.scene.objects:
        if obj.type in {"LIGHT", "CAMERA"}:
            link_to(obj, "90_CamerasLights")

    mesh_count = sum(1 for o in bpy.context.scene.objects if o.type == "MESH")
    print(f"[Build] Mesh objects: {mesh_count}")


def render_views(render_dir):
    os.makedirs(render_dir, exist_ok=True)
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 1280
    scene.render.resolution_y = 800
    scene.render.film_transparent = False

    jobs = [
        ("Cam_Overview", "overview.png"),
        ("Cam_Top", "top.png"),
    ]
    for cam_name, filename in jobs:
        cam = bpy.data.objects.get(cam_name)
        if cam is None:
            continue
        scene.camera = cam
        scene.render.filepath = os.path.join(render_dir, filename)
        bpy.ops.render.render(write_still=True)
        print(f"[Render] {scene.render.filepath}")


def main():
    args = args_after_double_dash()
    output_path = args[0] if args else "SimplifiedAscent.blend"
    render_dir = args[1] if len(args) > 1 else os.path.join(os.path.dirname(output_path) or ".", "renders")

    build_map()
    os.makedirs(os.path.dirname(os.path.abspath(output_path)) or ".", exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(output_path))
    print(f"[Build] Saved {output_path}")
    render_views(render_dir)
    print("[Build] Done")


if __name__ == "__main__":
    main()
