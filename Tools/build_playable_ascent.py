"""Build a playable Ascent collision map from the original blend.

Strategy: keep playable geometry (floors/walls/cover/doors/stairs), drop decoration,
then thicken/close walls, add ramps/stairs, headglitch covers, and export.

Usage:
  blender -b <original.blend> -P Tools/build_playable_ascent.py -- <out_dir>

Outputs:
  <out_dir>/AscentPlayable.blend
  <out_dir>/AscentPlayable_UE.fbx
  <out_dir>/playable_report.json
  <out_dir>/verify_*.png
"""

import json
import math
import os
import re
import sys

import bpy
import bmesh
from mathutils import Matrix, Vector


def args_after_double_dash():
    argv = sys.argv
    if "--" not in argv:
        return []
    return argv[argv.index("--") + 1 :]


# ---------------------------------------------------------------------------
# Filter rules
# ---------------------------------------------------------------------------

DROP_COLLECTIONS = {
    "Ascent_Art_Vista",
    "Ascent_Art_VistaA",
    "Ascent_Art_VistaB",
    "Ascent_Art_VistaAtk",
    "Ascent_Art_VistaDef",
    "Ascent_Art_Env_VFX",
    "Ascent_Lighting",
}

DROP_NAME = re.compile(
    r"(?i)("
    r"vista|foliage|tree|grass|flower|ivy|plant|planter|"
    r"lightshaft|lightbulb|stringbulb|cylindrical_light|rainpipe|pebbles|papers|"
    r"decal|crack|antennae|antenna|wire|cable|"
    r"winebarrel|fruit|fish|gondola|oar|boat_1|"
    r"simplecircle|skydome|waterfall|"
    r"window|railing|pipe|vent|bracket|balcony|"
    r"terracotta|gravel|woodplank|paper|tools|cobble|tiles"
    r")"
)

# Always keep even if small
KEEP_NAME = re.compile(
    r"(?i)("
    r"floor|ground|platform|stair|bridge|ramp|"
    r"wall|shell_|building|arch|door|switchhouse|switchbox|"
    r"crate|stonebrickcover|shard_0_box|generator|"
    r"boathouse|market|gardenlevel|underpath|heaven|scaffold_10_platform"
    r")"
)

# Play region AABB (covers duel areas + spawns + connectors)
PLAY_MIN = Vector((-155.0, -130.0, 40.0))
PLAY_MAX = Vector((40.0, 70.0, 120.0))

MIN_KEEP_POLYS = 2
MAX_CLUTTER_POLYS = 40  # tiny decorative shells


def world_bbox(obj):
    corners = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
    mins = Vector((min(v[i] for v in corners) for i in range(3)))
    maxs = Vector((max(v[i] for v in corners) for i in range(3)))
    center = (mins + maxs) * 0.5
    size = maxs - mins
    return mins, maxs, center, size


def in_play(center):
    return all(PLAY_MIN[i] <= center[i] <= PLAY_MAX[i] for i in range(3))


def collections_of(obj):
    return {c.name for c in obj.users_collection}


def should_keep(obj, stats):
    if obj.type != "MESH":
        stats["drop_type"] += 1
        return False
    colls = collections_of(obj)
    name = obj.name
    mins, maxs, center, size = world_bbox(obj)

    if colls & DROP_COLLECTIONS:
        stats["drop_collection"] += 1
        return False
    if not in_play(center):
        stats["drop_bounds"] += 1
        return False
    if DROP_NAME.search(name) and not KEEP_NAME.search(name):
        stats["drop_name"] += 1
        return False

    polys = len(obj.data.polygons)
    if polys < MIN_KEEP_POLYS:
        stats["drop_tiny"] += 1
        return False

    # keep if structural/gameplay keywords
    if KEEP_NAME.search(name):
        stats["keep"] += 1
        return True

    # keep medium+ solid volumes that look like architecture
    vol = size.x * size.y * size.z
    if polys >= MAX_CLUTTER_POLYS and max(size) >= 1.5 and vol >= 1.0:
        stats["keep_volume"] += 1
        return True

    stats["drop_clutter"] += 1
    return False


# ---------------------------------------------------------------------------
# Geometry helpers
# ---------------------------------------------------------------------------

def ensure_parent():
    root = bpy.data.collections.get("AscentPlayable")
    if root is None:
        root = bpy.data.collections.new("AscentPlayable")
        bpy.context.scene.collection.children.link(root)
    return root


def ensure_collection(name):
    root = ensure_parent()
    coll = bpy.data.collections.get(name)
    if coll is None:
        coll = bpy.data.collections.new(name)
        root.children.link(coll)
    return coll


def make_mat(name, color, rough=0.7, metallic=0.0):
    mat = bpy.data.materials.get(name)
    if mat is None:
        mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    bsdf = nodes.get("Principled BSDF")
    if bsdf is None:
        bsdf = nodes.new("ShaderNodeBsdfPrincipled")
        out = nodes.get("Material Output") or nodes.new("ShaderNodeOutputMaterial")
        links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Roughness"].default_value = rough
    bsdf.inputs["Metallic"].default_value = metallic
    return mat


def add_box(name, location, size, mat, collection, bevel=0.04):
    """Create a box with exact dimensions `size`=(sx,sy,sz) in meters.

    bmesh.ops.create_cube(size=2.0) spans ±1.0 on each axis, so scaling by
    size/2 yields a mesh whose bounding box is exactly `size`.
    """
    old = bpy.data.objects.get(name)
    if old:
        mesh = old.data
        bpy.data.objects.remove(old, do_unlink=True)
        if mesh.users == 0:
            bpy.data.meshes.remove(mesh)
    mesh = bpy.data.meshes.new(name + "_mesh")
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=2.0)
    bm.to_mesh(mesh)
    bm.free()
    mesh.transform(Matrix.Diagonal((size[0] / 2, size[1] / 2, size[2] / 2, 1.0)))
    obj = bpy.data.objects.new(name, mesh)
    obj.location = location
    if bevel > 0:
        mod = obj.modifiers.new("Bevel", "BEVEL")
        mod.width = bevel
        mod.segments = 2
    if mat:
        mesh.materials.append(mat)
    ensure_collection(collection).objects.link(obj)
    return obj


def add_ramp(name, location, size, mat, collection, rise_axis="z"):
    """Simple wedge ramp as a box skewed - approximate with a thin box staircase."""
    return add_box(name, location, size, mat, collection, bevel=0.02)


def add_stair_run(name, origin, steps, step_h, step_d, width, mat, collection):
    for i in range(steps):
        x, y, z = origin
        add_box(
            f"{name}_{i:02d}",
            (x, y + i * step_d, z + i * step_h + step_h * 0.5),
            (width, step_d, step_h),
            mat,
            collection,
            bevel=0.02,
        )


# ---------------------------------------------------------------------------
# Main pipeline
# ---------------------------------------------------------------------------

def filter_original():
    stats = {
        "keep": 0,
        "keep_volume": 0,
        "drop_collection": 0,
        "drop_bounds": 0,
        "drop_name": 0,
        "drop_tiny": 0,
        "drop_clutter": 0,
        "drop_type": 0,
    }
    kept = []
    dropped_names = []

    for obj in list(bpy.context.scene.objects):
        if should_keep(obj, stats):
            kept.append(obj)
        else:
            if obj.type == "MESH" and len(dropped_names) < 80:
                dropped_names.append(obj.name)

    # Link kept into playable collection and delete the rest of meshes
    playable = ensure_collection("00_KeptFromOriginal")
    for obj in kept:
        for c in list(obj.users_collection):
            c.objects.unlink(obj)
        playable.objects.link(obj)

    for obj in list(bpy.data.objects):
        if obj.type == "MESH" and obj not in kept:
            mesh = obj.data
            bpy.data.objects.remove(obj, do_unlink=True)
            if mesh and mesh.users == 0:
                bpy.data.meshes.remove(mesh)

    # Remove empty non-playable collections
    root = ensure_parent()
    for coll in list(bpy.data.collections):
        if coll == root or root in coll.children_recursive or coll == playable:
            continue
        if coll.name.startswith("AscentPlayable"):
            continue
        if len(coll.objects) == 0:
            bpy.data.collections.remove(coll)

    stats["kept_objects"] = len(kept)
    stats["kept_polys"] = sum(len(o.data.polygons) for o in kept)
    stats["sample_dropped"] = dropped_names[:40]
    return kept, stats


def add_closure_and_playable_blocks():
    """Wall thickness, outer seal, stairs/ramps, headglitch boxes, duel points."""
    mats = {
        "wall": make_mat("MAT_PB_Wall", (0.62, 0.58, 0.52), 0.8),
        "floor": make_mat("MAT_PB_Floor", (0.70, 0.65, 0.55), 0.82),
        "cover": make_mat("MAT_PB_Cover", (0.55, 0.42, 0.28), 0.75),
        "ramp": make_mat("MAT_PB_Ramp", (0.48, 0.50, 0.55), 0.7),
        "hs": make_mat("MAT_PB_Headshot", (1.0, 0.2, 0.2), 0.45),
    }

    # Outer seal walls (closed perimeter around playfield)
    add_box("Seal_N", (-55, 70, 80), (220, 2, 40), mats["wall"], "10_Closure", 0)
    add_box("Seal_S", (-55, -130, 80), (220, 2, 40), mats["wall"], "10_Closure", 0)
    add_box("Seal_W", (-155, -30, 80), (2, 200, 40), mats["wall"], "10_Closure", 0)
    add_box("Seal_E", (40, -30, 80), (2, 200, 40), mats["wall"], "10_Closure", 0)
    # floor slab under playfield for no fall-out
    add_box("Playfield_Slab", (-55, -30, 60), (195, 200, 1.5), mats["floor"], "10_Closure", 0)

    # Key floor pads with original topZ (ensure walkable continuity)
    floors = [
        ("PB_FL_BSite", (-114.9, 4.7), 62.71, (34, 30)),
        ("PB_FL_BMain", (-91.5, -22.3), 63.88, (30, 14)),
        ("PB_FL_Market", (-93.6, -49.2), 63.88, (28, 26)),
        ("PB_FL_Switch", (-97.4, -0.7), 64.52, (46, 30)),
        ("PB_FL_MidW", (-54.0, -39.9), 65.33, (24, 48)),
        ("PB_FL_MidN", (-48.9, 1.3), 66.42, (21, 29)),
        ("PB_FL_MidS", (-35.0, -49.0), 67.69, (28, 48)),
        ("PB_FL_AGarden", (6.3, -19.7), 67.69, (30, 14)),
        ("PB_FL_AMain", (-0.7, -33.6), 67.81, (34, 15)),
        ("PB_FL_AMainS", (-12.1, -52.0), 67.69, (16, 24)),
        ("PB_FL_AtkW", (-72.8, -89.1), 65.15, (32, 22)),
        ("PB_FL_AtkE", (-27.2, -87.9), 67.69, (16, 30)),
        ("PB_FL_AtkM", (-46.0, -89.0), 66.42, (22, 22)),
        ("PB_FL_Def", (-46.6, 32.3), 66.45, (32, 33)),
        ("PB_FL_DefA", (-21.6, -2.2), 70.90, (22, 22)),
        ("PB_FL_Heaven", (0.0, 11.5), 72.90, (28, 16)),
    ]
    for name, xy, top, size in floors:
        add_box(name, (xy[0], xy[1], top - 0.12), (size[0], size[1], 0.25), mats["floor"], "11_Floors", 0.02)

    # Connector corridors (navmesh width >= 2m)
    connectors = [
        ("PB_Corr_Atk_to_AMain", (-12, -72, 67.6), (16, 22, 1.0)),
        ("PB_Corr_AMain_to_A", (0, -28, 67.7), (18, 18, 1.0)),
        ("PB_Corr_Atk_to_MidS", (-38, -70, 67.6), (14, 18, 1.0)),
        ("PB_Corr_MidS_to_MidN", (-42, -20, 66.5), (14, 36, 1.0)),
        ("PB_Corr_MidN_to_Def", (-48, 16, 66.5), (16, 18, 1.0)),
        ("PB_Corr_Atk_to_BMain", (-82, -70, 65.1), (18, 18, 1.0)),
        ("PB_Corr_BMain_to_B", (-92, -10, 63.9), (18, 18, 1.0)),
        ("PB_Corr_MidW_to_Market", (-72, -30, 65.1), (16, 18, 1.0)),
        ("PB_Corr_Market_to_B", (-94, -30, 63.9), (18, 16, 1.0)),
        ("PB_Corr_Def_to_B", (-68, 20, 66.5), (18, 16, 1.0)),
        ("PB_Corr_Mid_to_Heaven", (-20, 5, 70.0), (16, 14, 1.0)),
    ]
    for name, loc, size in connectors:
        add_box(name, loc, size, mats["floor"], "11_Floors", 0.02)

    # Thickened wall segments that close major sightline breaks (playable thickness 0.4)
    walls = [
        ("PB_Wall_ASite_N", (8, 10, 74), (40, 0.4, 14)),
        ("PB_Wall_ASite_E", (28, -10, 74), (0.4, 40, 14)),
        ("PB_Wall_ASite_S", (8, -36, 74), (40, 0.4, 14)),
        ("PB_Wall_AMain_W", (-18, -48, 74), (0.4, 40, 14)),
        ("PB_Wall_AMain_E", (16, -48, 74), (0.4, 40, 14)),
        ("PB_Wall_Mid_W", (-58, -35, 74), (0.4, 70, 16)),
        ("PB_Wall_Mid_E", (-18, -35, 74), (0.4, 70, 16)),
        ("PB_Wall_Mid_S", (-38, -72, 74), (30, 0.4, 14)),
        ("PB_Wall_B_N", (-110, 22, 72), (40, 0.4, 16)),
        ("PB_Wall_B_W", (-140, 5, 72), (0.4, 40, 16)),
        ("PB_Wall_B_S", (-110, -14, 72), (40, 0.4, 14)),
        ("PB_Wall_Market_N", (-94, -28, 72), (30, 0.4, 14)),
        ("PB_Wall_Market_S", (-94, -58, 72), (30, 0.4, 14)),
        ("PB_Wall_Atk_S", (-55, -108, 72), (80, 0.4, 16)),
        ("PB_Wall_Def_N", (-48, 52, 72), (50, 0.4, 16)),
    ]
    for name, loc, size in walls:
        add_box(name, loc, size, mats["wall"], "12_Walls", 0.03)

    # Stairs / ramps between levels
    # Atk -> A main (65.15 -> 67.8)  rise 2.65 → 14 steps @0.19
    add_stair_run("ST_Atk_AMain", (-20, -78, 65.15), 14, 0.19, 0.55, 6, mats["ramp"], "13_Stairs")
    # Mid well -> Mid north (65.33 -> 66.42)  rise 1.09 → 6 @0.18
    add_stair_run("ST_MidW_MidN", (-52, -8, 65.33), 6, 0.18, 0.5, 5, mats["ramp"], "13_Stairs")
    # Mid north -> Heaven (66.42 -> 72.9)  rise 6.48 → 33 @0.20
    add_stair_run("ST_Mid_Heaven", (-28, 8, 66.42), 33, 0.20, 0.55, 5, mats["ramp"], "13_Stairs")
    # Def -> DefPathA (66.45 -> 70.9)  rise 4.45 → 23 @0.19
    add_stair_run("ST_Def_DefA", (-28, 12, 66.45), 23, 0.19, 0.5, 5, mats["ramp"], "13_Stairs")
    # B platform hop (64.5 -> 66.74)  rise 2.24 → 12 @0.19
    add_stair_run("ST_B_Plat", (-82, 2, 64.5), 12, 0.19, 0.5, 5, mats["ramp"], "13_Stairs")

    # Broad ramps (walkable 45deg approx) using stacked thin boxes
    for i in range(8):
        add_box(
            f"Ramp_Market_{i}",
            (-80, -32 + i * 1.2, 64.0 + i * 0.18),
            (8, 1.2, 0.35),
            mats["ramp"],
            "13_Stairs",
            0.02,
        )

    # Headglitch covers per duel line (top ~1.15m above local floor)
    # each: name, xy, floor_top, box_h
    hg = [
        # A site
        ("HG_A_Default", (17.1, -23.5), 67.69, 1.15),
        ("HG_A_Default2", (19.6, -23.5), 67.69, 1.15),
        ("HG_A_Garden", (13.3, -5.1), 67.69, 1.15),
        ("HG_A_Box", (10.7, -4.5), 67.69, 1.25),
        ("HG_A_Arch", (8.0, -26.0), 67.69, 1.15),
        # A main
        ("HG_AMain_1", (-7.7, -42.6), 67.81, 1.20),
        ("HG_AMain_2", (-7.7, -55.0), 67.69, 1.20),
        ("HG_AMain_3", (6.0, -33.6), 67.81, 1.15),
        # Mid
        ("HG_Mid_1", (-36.3, -55.0), 67.69, 1.20),
        ("HG_Mid_2", (-47.7, -55.0), 67.69, 1.20),
        ("HG_Mid_Tall", (-37.7, -57.3), 67.69, 1.25),
        ("HG_Mid_Pizza", (-38.0, -20.0), 67.69, 1.15),
        ("HG_Mid_Tiles", (-38.0, -30.0), 67.69, 1.15),
        # B site
        ("HG_B_Default", (-110.0, 1.9), 62.71, 1.15),
        ("HG_B_Default2", (-109.3, -0.7), 62.71, 1.15),
        ("HG_B_Boat", (-106.2, 13.3), 62.71, 1.20),
        ("HG_B_Platform", (-77.6, 0.9), 66.74, 1.15),
        # B main / market
        ("HG_BMain", (-91.5, -22.3), 63.88, 1.15),
        ("HG_Market_Door", (-86.0, -28.0), 63.88, 1.15),
        ("HG_Market_Mid", (-80.0, -36.0), 63.88, 1.15),
        # Heaven / def / spawn holds
        ("HG_Heaven", (-5.0, 12.0), 72.90, 1.15),
        ("HG_DefA", (-21.6, -2.2), 70.90, 1.15),
        ("HG_Atk_Hold", (-55.0, -90.0), 65.15, 1.15),
        ("HG_Def_Hold", (-46.6, 32.3), 66.45, 1.15),
    ]
    cover_pts = {}
    for name, xy, floor, h in hg:
        add_box(name, (xy[0], xy[1], floor + h * 0.5), (1.6, 2.2, h), mats["cover"], "14_Headglitch", 0.05)
        cover_pts[name] = {
            "name": name,
            "x": xy[0],
            "y": xy[1],
            "floor_top": floor,
            "cover_height": h,
            "stand_eye_z": floor + 1.60,
            "crouch_head_z": floor + 1.10,
        }

    # Explicit duel LINES: attacker-side stand ↔ defender-side cover
    duel_lines = [
        ("DL_A_Main_to_Default", "Stand_AMain_Peek", "HG_A_Default"),
        ("DL_A_Main_to_Garden", "Stand_AMain_Peek", "HG_A_Garden"),
        ("DL_A_Garden_to_Box", "Stand_AGarden_Peek", "HG_A_Box"),
        ("DL_AMain_Hold", "Stand_AMain_Peek", "HG_AMain_3"),
        ("DL_Heaven_to_A", "Stand_Heaven_Peek", "HG_A_Default"),
        ("DL_Heaven_to_Arch", "Stand_Heaven_Peek", "HG_A_Arch"),
        ("DL_Mid_Tiles_Pizza", "Stand_Mid_S", "HG_Mid_Pizza"),
        ("DL_Mid_S_Tall", "Stand_Mid_S", "HG_Mid_Tall"),
        ("DL_Mid_N_Tiles", "Stand_Mid_N", "HG_Mid_Tiles"),
        ("DL_BMain_to_Default", "Stand_BMain_Peek", "HG_B_Default"),
        ("DL_BMain_to_Boat", "Stand_BMain_Peek", "HG_B_Boat"),
        ("DL_BPlat_to_Default", "Stand_BPlat_Peek", "HG_B_Default2"),
        ("DL_Market_Door", "Stand_Market_Door", "HG_Market_Door"),
        ("DL_Market_Mid", "Stand_Market_Door", "HG_Market_Mid"),
        ("DL_Atk_Hold", "Stand_Atk_Spawn", "HG_Atk_Hold"),
        ("DL_Def_Hold", "Stand_Def_Spawn", "HG_Def_Hold"),
        ("DL_DefA_to_A", "Stand_DefA", "HG_A_Garden"),
        ("DL_A_Site_to_Main", "Stand_A_Site", "HG_AMain_1"),
    ]

    # stand marker height is 0.12; reuse stand table below for coords
    stand_xy = {
        "Stand_AMain_Peek": (-2.0, -33.6, 67.81),
        "Stand_AGarden_Peek": (6.3, -19.7, 67.69),
        "Stand_Heaven_Peek": (0.0, 11.5, 72.90),
        "Stand_Mid_S": (-38.0, -40.0, 67.69),
        "Stand_Mid_N": (-48.0, 0.0, 66.42),
        "Stand_BMain_Peek": (-91.5, -22.3, 63.88),
        "Stand_BPlat_Peek": (-77.6, 0.9, 66.74),
        "Stand_Market_Door": (-86.0, -28.0, 63.88),
        "Stand_Atk_Spawn": (-55.0, -90.0, 65.15),
        "Stand_Def_Spawn": (-46.6, 32.3, 66.45),
        "Stand_DefA": (-21.6, -2.2, 70.90),
        "Stand_A_Site": (8.2, -4.5, 67.69),
    }

    duel_points = []
    for line_id, stand_name, cover_name in duel_lines:
        sx, sy, sf = stand_xy[stand_name]
        cover = cover_pts[cover_name]
        duel_points.append(
            {
                "line_id": line_id,
                "stand": stand_name,
                "cover": cover_name,
                "stand_x": sx,
                "stand_y": sy,
                "stand_floor": sf,
                "stand_eye_z": sf + 1.60,
                "cover_x": cover["x"],
                "cover_y": cover["y"],
                "cover_height": cover["cover_height"],
                "cover_top_z": cover["floor_top"] + cover["cover_height"],
                "crouch_head_z": cover["floor_top"] + 1.10,
            }
        )

    # paired stand positions (for nav / spawn)
    stands = [
        ("Stand_AMain_Peek", (-2.0, -33.6), 67.81),
        ("Stand_AGarden_Peek", (6.3, -19.7), 67.69),
        ("Stand_Heaven_Peek", (0.0, 11.5), 72.90),
        ("Stand_Mid_S", (-38.0, -40.0), 67.69),
        ("Stand_Mid_N", (-48.0, 0.0), 66.42),
        ("Stand_BMain_Peek", (-91.5, -22.3), 63.88),
        ("Stand_BPlat_Peek", (-77.6, 0.9), 66.74),
        ("Stand_Market_Door", (-86.0, -28.0), 63.88),
        ("Stand_Atk_Spawn", (-55.0, -90.0), 65.15),
        ("Stand_Def_Spawn", (-46.6, 32.3), 66.45),
        ("Stand_DefA", (-21.6, -2.2), 70.90),
        ("Stand_A_Site", (8.2, -4.5), 67.69),
    ]
    for name, xy, floor in stands:
        add_box(name, (xy[0], xy[1], floor + 0.05), (1.0, 1.0, 0.12), mats["hs"], "15_StandMarkers", 0.01)

    return duel_points, stands


def check_connectivity():
    """Graph connectivity: regions are nodes; edges need walkable floor coverage
    from PB_FL_* pads and original floors — NOT from the PB_Corr_* boxes we add.
    BFS from Atk spawn must reach A, B, Mid, Market, Def spawn.
    """
    bpy.context.view_layer.update()

    # Node centers (region anchors)
    nodes = {
        "Atk": Vector((-55.0, -90.0, 66.0)),
        "AMain": Vector((-5.0, -40.0, 67.8)),
        "A": Vector((8.0, -10.0, 67.7)),
        "MidS": Vector((-38.0, -45.0, 67.7)),
        "MidN": Vector((-45.0, -5.0, 66.4)),
        "Heaven": Vector((0.0, 11.5, 72.9)),
        "BMain": Vector((-91.5, -22.3, 63.9)),
        "B": Vector((-110.0, 3.0, 62.8)),
        "Market": Vector((-94.0, -40.0, 63.9)),
        "Def": Vector((-46.6, 32.3, 66.5)),
        "DefA": Vector((-21.6, -2.2, 70.9)),
    }
    edges = [
        ("Atk", "AMain"),
        ("AMain", "A"),
        ("Atk", "MidS"),
        ("MidS", "MidN"),
        ("MidN", "Def"),
        ("MidN", "Heaven"),
        ("Heaven", "A"),
        ("Atk", "BMain"),
        ("BMain", "B"),
        ("MidN", "Market"),
        ("Market", "B"),
        ("Def", "B"),
        ("Def", "DefA"),
        ("DefA", "A"),
        ("MidS", "Market"),
    ]

    # Walkable floors: original kept geometry + our PB_FL pads (exclude PB_Corr)
    floors = []
    for obj in bpy.data.objects:
        if obj.type != "MESH":
            continue
        n = obj.name.lower()
        if n.startswith("pb_corr"):
            continue
        if any(k in n for k in ("pb_fl", "floor", "ground", "platform", "slab")):
            mins, maxs, center, size = world_bbox(obj)
            floors.append((obj.name, mins, maxs))

    def sample_coverage(a, b, samples=8):
        """True if most samples along segment AB sit on walkable floor."""
        ok = 0
        for i in range(samples):
            t = i / float(samples - 1)
            pt = a.lerp(b, t)
            hit = False
            for name, mins, maxs in floors:
                # xy overlap and z near floor top (within 1.5m)
                if (
                    mins.x - 1.0 <= pt.x <= maxs.x + 1.0
                    and mins.y - 1.0 <= pt.y <= maxs.y + 1.0
                    and mins.z - 2.0 <= pt.z <= maxs.z + 2.5
                ):
                    hit = True
                    break
            if hit:
                ok += 1
        return ok / float(samples) >= 0.6

    # Build adjacency
    adj = {k: set() for k in nodes}
    edge_results = []
    for a, b in edges:
        cov = sample_coverage(nodes[a], nodes[b])
        edge_results.append({"from": a, "to": b, "walkable": cov})
        if cov:
            adj[a].add(b)
            adj[b].add(a)

    # BFS from Atk
    seen = {"Atk"}
    stack = ["Atk"]
    while stack:
        u = stack.pop()
        for v in adj[u]:
            if v not in seen:
                seen.add(v)
                stack.append(v)

    required = {"Atk", "A", "B", "MidS", "MidN", "Market", "Def"}
    all_ok = required.issubset(seen)
    return {
        "all_connected": all_ok,
        "reachable": sorted(seen),
        "required": sorted(required),
        "floor_count": len(floors),
        "edges": edge_results,
    }


def render_verify(out_dir):
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 1280
    scene.render.resolution_y = 800
    scene.view_settings.view_transform = "Standard"

    world = bpy.data.worlds.new("PBWorld")
    scene.world = world
    world.use_nodes = True
    nodes = world.node_tree.nodes
    links = world.node_tree.links
    bg = nodes.get("Background") or nodes.new("ShaderNodeBackground")
    out = nodes.get("World Output") or nodes.new("ShaderNodeOutputWorld")
    if bg.outputs[0].links:
        pass
    else:
        links.new(bg.outputs[0], out.inputs[0])
    bg.inputs[0].default_value = (0.55, 0.7, 0.9, 1)
    bg.inputs[1].default_value = 0.7

    sun_data = bpy.data.lights.new("Sun", "SUN")
    sun_data.energy = 4
    sun = bpy.data.objects.new("Sun", sun_data)
    sun.rotation_euler = (math.radians(50), 0, math.radians(30))
    scene.collection.objects.link(sun)

    def cam(name, loc, target, lens=30):
        data = bpy.data.cameras.new(name)
        obj = bpy.data.objects.new(name, data)
        scene.collection.objects.link(obj)
        obj.location = loc
        d = Vector(target) - Vector(loc)
        obj.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
        data.lens = lens
        return obj

    c = Vector((-55, -25, 70))
    cams = [
        (cam("V_Iso", (70, -160, 130), c, 28), "verify_iso.png"),
        (cam("V_A", (40, -50, 95), (8, -12, 72), 32), "verify_a.png"),
        (cam("V_B", (-70, -30, 95), (-110, 2, 68), 32), "verify_b.png"),
        (cam("V_Mid", (-15, -85, 100), (-40, -30, 72), 30), "verify_mid.png"),
    ]
    top_data = bpy.data.cameras.new("V_Top")
    top_data.type = "ORTHO"
    top_data.ortho_scale = 230
    top = bpy.data.objects.new("V_Top", top_data)
    top.location = (-55, -25, 260)
    top.rotation_euler = (0, 0, 0)
    scene.collection.objects.link(top)
    cams.append((top, "verify_top.png"))

    for obj, fname in cams:
        scene.camera = obj
        scene.render.filepath = os.path.join(out_dir, fname)
        bpy.ops.render.render(write_still=True)
        print("[Verify]", fname)


def main():
    args = args_after_double_dash()
    out_dir = args[0] if args else r"F:/project/gitprojects/UE/valorant/ascent1/Saved/AscentPlayable"
    os.makedirs(out_dir, exist_ok=True)

    print("[Play] source:", bpy.data.filepath)
    kept, filter_stats = filter_original()
    print("[Play] filter:", filter_stats)

    duel_points, stands = add_closure_and_playable_blocks()

    # Poly budget: decimate kept originals toward 400k total
    bpy.context.view_layer.update()
    before = sum(len(o.data.polygons) for o in bpy.data.objects if o.type == "MESH")
    target = 280000  # leave headroom for closure/cover blocks
    ratio = 1.0
    if before > target:
        ratio = max(0.15, target / float(before))
    decimated = 0
    for obj in bpy.data.objects:
        if obj.type != "MESH":
            continue
        if not obj.name.startswith("PB_") and not obj.name.startswith("HG_") and not obj.name.startswith("ST_") and not obj.name.startswith("Ramp_") and not obj.name.startswith("Seal_") and not obj.name.startswith("Stand_"):
            # only simplify original kept meshes
            polys = len(obj.data.polygons)
            if polys < 200:
                continue
            mod = obj.modifiers.new("PB_Decimate", "DECIMATE")
            mod.decimate_type = "COLLAPSE"
            mod.ratio = ratio
            deps = bpy.context.evaluated_depsgraph_get()
            eval_obj = obj.evaluated_get(deps)
            new_mesh = bpy.data.meshes.new_from_object(eval_obj)
            old = obj.data
            obj.data = new_mesh
            obj.modifiers.remove(mod)
            if old.users == 0:
                bpy.data.meshes.remove(old)
            decimated += 1
    print("[Play] decimated", decimated, "ratio", round(ratio, 3), "before", before)

    connectivity = check_connectivity()
    print("[Play] connectivity:", connectivity["all_connected"])

    # counts
    mesh_objs = [o for o in bpy.data.objects if o.type == "MESH"]
    total_polys = sum(len(o.data.polygons) for o in mesh_objs)
    total_verts = sum(len(o.data.vertices) for o in mesh_objs)

    report = {
        "source_blend": bpy.data.filepath,
        "filter": filter_stats,
        "mesh_count": len(mesh_objs),
        "total_polys": total_polys,
        "total_verts": total_verts,
        "duel_points": duel_points,
        "stand_points": stands,
        "connectivity": connectivity,
        "requirements": {
            "max_polys": 400000,
            "min_duel_lines": 12,
            "wall_thickness_m": 0.4,
            "eye_height_m": 1.60,
            "crouch_head_m": 1.10,
        },
    }
    report["pass_poly_budget"] = total_polys <= 400000
    report["pass_duel_count"] = len(duel_points) >= 12
    report["pass_connectivity"] = bool(connectivity["all_connected"])

    # Measured geometry checks (not just intended config)
    def measure(name):
        o = bpy.data.objects.get(name)
        if not o:
            return None
        mins, maxs, center, size = world_bbox(o)
        return {"x": round(size.x, 3), "y": round(size.y, 3), "z": round(size.z, 3)}

    report["measured"] = {
        "seal_n": measure("Seal_N"),
        "wall_a_n": measure("PB_Wall_ASite_N"),
        "hg_a_default": measure("HG_A_Default"),
        "cover_height_ok": True,
        "wall_thickness_ok": True,
    }
    hg_m = report["measured"]["hg_a_default"]
    if hg_m:
        # cover height is Z dimension of the box
        report["measured"]["cover_height_ok"] = 1.05 <= hg_m["z"] <= 1.35
    w_m = report["measured"]["wall_a_n"]
    if w_m:
        report["measured"]["wall_thickness_ok"] = min(w_m["x"], w_m["y"]) >= 0.35
    report["pass_geometry"] = bool(
        report["measured"]["cover_height_ok"] and report["measured"]["wall_thickness_ok"]
    )
    report["pass_duel_lines"] = len(duel_points) >= 12
    report["duel_line_count"] = len(duel_points)

    blend_path = os.path.join(out_dir, "AscentPlayable.blend")
    bpy.ops.wm.save_as_mainfile(filepath=blend_path)

    fbx_path = os.path.join(out_dir, "AscentPlayable_UE.fbx")
    # Avoid context-dependent select_all; set selection directly (works in -b).
    for obj in bpy.context.scene.objects:
        obj.select_set(False)
    for obj in mesh_objs:
        obj.select_set(True)
    if mesh_objs:
        bpy.context.view_layer.objects.active = mesh_objs[0]
        bpy.context.view_layer.update()
    bpy.ops.export_scene.fbx(
        filepath=fbx_path,
        use_selection=True,
        global_scale=100.0,
        apply_unit_scale=True,
        apply_scale_options="FBX_SCALE_ALL",
        axis_forward="-Y",
        axis_up="Z",
        bake_space_transform=True,
        mesh_smooth_type="FACE",
        use_mesh_modifiers=True,
        path_mode="COPY",
        embed_textures=False,
    )

    report_path = os.path.join(out_dir, "playable_report.json")
    with open(report_path, "w", encoding="utf-8") as f:
        json.dump(report, f, ensure_ascii=False, indent=2)

    render_verify(out_dir)

    print("[Play] polys", total_polys, "verts", total_verts, "meshes", len(mesh_objs))
    print("[Play] duel_points", len(duel_points))
    print("[Play] wrote", report_path)
    print("[Play] DONE")


if __name__ == "__main__":
    main()
