"""Crop AscentPlayable into independent per-duel-region training scenes.

Policy (S2b):
- Crop circle: midpoint of stand/cover, radius = distance + peek margin
- Delete small props (max edge < 0.35m) that are not headglitch covers
- Solidify thin walls (10-20cm)
- Unify materials to 4 grays
- Merge by material into few meshes
- Add simplified collision boxes + ramp collision over stairs
- Export per-region FBX + audit

Usage:
  blender -b <AscentPlayable.blend> -P Tools/build_playable_regions.py -- <out_dir>
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
    return argv[argv.index("--") + 1 :] if "--" in argv else []


def world_bbox(obj):
    """True world AABB from mesh verts (bound_box can be stale after edits)."""
    mesh = obj.data
    if mesh is None or len(mesh.vertices) == 0:
        corners = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
        mins = Vector((min(v[i] for v in corners) for i in range(3)))
        maxs = Vector((max(v[i] for v in corners) for i in range(3)))
        return mins, maxs, (mins + maxs) * 0.5, maxs - mins
    mw = obj.matrix_world
    xs, ys, zs = [], [], []
    for v in mesh.vertices:
        w = mw @ v.co
        xs.append(w.x)
        ys.append(w.y)
        zs.append(w.z)
    mins = Vector((min(xs), min(ys), min(zs)))
    maxs = Vector((max(xs), max(ys), max(zs)))
    return mins, maxs, (mins + maxs) * 0.5, maxs - mins


def make_mat(name, color, rough=0.8):
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
    bsdf.inputs["Metallic"].default_value = 0.0
    return mat


def add_box(name, location, size, mat, collection):
    mesh = bpy.data.meshes.new(name + "_mesh")
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=2.0)
    bm.to_mesh(mesh)
    bm.free()
    mesh.transform(Matrix.Diagonal((size[0] / 2, size[1] / 2, size[2] / 2, 1.0)))
    obj = bpy.data.objects.new(name, mesh)
    obj.location = location
    obj.matrix_world = Matrix.Translation(Vector(location))
    if mat:
        mesh.materials.append(mat)
    collection.objects.link(obj)
    bpy.context.view_layer.update()
    return obj


# Independent training regions (name, stand_xy, cover_xy, floor_z_est)
REGIONS = [
    ("A_Main_Default", (-2.0, -33.6), (17.1, -23.5), 67.8),
    ("A_Garden_Box", (6.3, -19.7), (10.7, -4.5), 67.7),
    ("A_Heaven_Default", (0.0, 11.5), (17.1, -23.5), 70.0),
    ("Mid_Tiles_Pizza", (-38.0, -40.0), (-38.0, -20.0), 67.0),
    ("Mid_S_Tall", (-38.0, -40.0), (-37.7, -57.3), 67.7),
    ("BMain_Default", (-91.5, -22.3), (-110.0, 1.9), 63.5),
    ("BPlat_Default", (-77.6, 0.9), (-109.3, -0.7), 65.0),
    ("Market_Door", (-86.0, -28.0), (-86.0, -28.0), 65.0),
    ("Atk_Hold", (-55.0, -90.0), (-55.0, -90.0), 66.0),
    ("Def_Hold", (-46.6, 32.3), (-46.6, 32.3), 66.5),
]

PEEK_MARGIN = 8.0
MIN_KEEP_EDGE = 0.35
HG_NAMES = re.compile(r"(?i)^(hg_|stand_|pb_fl|pb_wall|pb_corr|seal_|st_|ramp_|safety_)")


def classify_material_slot(obj, mats):
    name = obj.name.lower()
    if name.startswith(("hg_", "stand_")):
        return mats["cover"]
    if "wall" in name or name.startswith("seal"):
        return mats["wall"]
    if any(k in name for k in ("floor", "ground", "platform", "corr", "slab", "st_", "ramp")):
        return mats["floor"]
    # original: larger tops -> cover else wall/floor heuristic
    _, _, _, size = world_bbox(obj)
    if size.z >= 0.8 and max(size.x, size.y) <= 4.5:
        return mats["cover"]
    if size.z >= 4.0:
        return mats["wall"]
    return mats["floor"]


def solidify_thin(obj, thickness=0.15):
    _, _, _, size = world_bbox(obj)
    dims = sorted([size.x, size.y, size.z])
    if dims[0] >= 0.08:
        return False
    if dims[1] < 0.2:
        return False
    # already thick enough in another axis as a wall plate
    mod = obj.modifiers.get("PB_Solidify")
    if mod is None:
        mod = obj.modifiers.new("PB_Solidify", "SOLIDIFY")
        mod.thickness = thickness
        mod.offset = 0.0
    return True


def apply_modifiers(obj):
    """Bake object modifiers (Solidify etc.) into mesh data."""
    if not obj.modifiers:
        return
    deps = bpy.context.evaluated_depsgraph_get()
    ev = obj.evaluated_get(deps)
    new_mesh = bpy.data.meshes.new_from_object(ev)
    old = obj.data
    obj.data = new_mesh
    for mod in list(obj.modifiers):
        obj.modifiers.remove(mod)
    if old.users == 0:
        bpy.data.meshes.remove(old)


def merge_by_material(objs, collection, tag):
    """Merge objects that share a material. Modifiers must already be applied."""
    buckets = {}
    mat_by_key = {}
    for o in objs:
        mat = o.data.materials[0] if o.data.materials else None
        key = mat.name if mat else "None"
        mat_by_key[key] = mat
        buckets.setdefault(key, []).append(o)

    merged = []
    for mat_name, group in buckets.items():
        if len(group) == 1:
            merged.append(group[0])
            continue
        bm = bmesh.new()
        for o in group:
            # evaluated mesh already baked; transform world into merge space
            tmp = o.data.copy()
            tmp.transform(o.matrix_world)
            bm.from_mesh(tmp)
            bpy.data.meshes.remove(tmp)
        mesh = bpy.data.meshes.new(f"{tag}_{mat_name}")
        bm.to_mesh(mesh)
        bm.free()
        for o in group:
            old = o.data
            bpy.data.objects.remove(o, do_unlink=True)
            if old.users == 0:
                bpy.data.meshes.remove(old)
        obj = bpy.data.objects.new(f"{tag}_{mat_name}", mesh)
        mat = mat_by_key.get(mat_name) or bpy.data.materials.get(mat_name)
        if mat:
            mesh.materials.append(mat)
        collection.objects.link(obj)
        merged.append(obj)
    return merged


def build_collision_boxes(objs, collection, mat):
    """Per-piece simplified AABB collision from PRE-MERGE objects.

    Merged mega-meshes would produce useless region-sized boxes; collision
    must be built from individual walls/floors/covers. Stairs get inflated
    ramp colliders so CharacterMovement does not catch on steps.
    """
    boxes = []
    ramps = 0
    for o in objs:
        mins, maxs, center, size = world_bbox(o)
        if min(size) < 0.05 or max(size) < 0.25:
            continue
        n = o.name.lower()
        name = "UCX_" + o.name[:40]
        box = add_box(name, tuple(center), tuple(size), mat, collection)
        boxes.append(box)
        # stair/ramp: extra smooth ramp collider
        if n.startswith(("st_", "ramp_")) or "stair" in n:
            add_box(
                "UCXR_" + o.name[:36],
                (center.x, center.y, center.z + 0.2),
                (size.x + 0.15, size.y + 0.15, size.z + 0.4),
                mat,
                collection,
            )
            ramps += 1
    return boxes, ramps


GLOBAL_EXCLUDE = re.compile(r"(?i)^(safety_|seal_)")


def replace_with_aabb_box(name, bmin, bmax, mat, collection):
    """Create a box exactly covering [bmin,bmax] in world space."""
    size = (bmax.x - bmin.x, bmax.y - bmin.y, bmax.z - bmin.z)
    center = ((bmin.x + bmax.x) / 2, (bmin.y + bmax.y) / 2, (bmin.z + bmax.z) / 2)
    return add_box(name, center, size, mat, collection)


def process_region(src_objects, region_name, stand_xy, cover_xy, floor_z, out_dir, mats_all):
    cx = (stand_xy[0] + cover_xy[0]) * 0.5
    cy = (stand_xy[1] + cover_xy[1]) * 0.5
    dist = math.hypot(stand_xy[0] - cover_xy[0], stand_xy[1] - cover_xy[1])
    radius = max(dist + PEEK_MARGIN, 18.0)

    pad = 0.5
    bmin = Vector((cx - radius - pad, cy - radius - pad, 45.0))
    bmax = Vector((cx + radius + pad, cy + radius + pad, 95.0))

    root = bpy.data.collections.new(region_name)
    bpy.context.scene.collection.children.link(root)
    coll_vis = bpy.data.collections.new("Visual")
    coll_col = bpy.data.collections.new("Collision")
    root.children.link(coll_vis)
    root.children.link(coll_col)

    kept = []
    dropped = {"far": 0, "small": 0, "global": 0, "clipped": 0, "ok": 0}
    for obj in src_objects:
        if obj.type != "MESH":
            continue
        if GLOBAL_EXCLUDE.match(obj.name):
            dropped["global"] += 1
            continue
        mins, maxs, center, size = world_bbox(obj)

        # intersection with crop box
        imin = Vector((max(mins.x, bmin.x), max(mins.y, bmin.y), max(mins.z, bmin.z)))
        imax = Vector((min(maxs.x, bmax.x), min(maxs.y, bmax.y), min(maxs.z, bmax.z)))
        if imin.x >= imax.x or imin.y >= imax.y or imin.z >= imax.z:
            dropped["far"] += 1
            continue

        edge = max(size.x, size.y, size.z)
        if edge < MIN_KEEP_EDGE and not HG_NAMES.match(obj.name):
            dropped["small"] += 1
            continue

        # Any mesh that extends outside the crop box is replaced by its
        # AABB ∩ crop_box so FBX bounds cannot leak past the region.
        extends = (
            mins.x < bmin.x
            or maxs.x > bmax.x
            or mins.y < bmin.y
            or maxs.y > bmax.y
        )
        if extends:
            box = replace_with_aabb_box(
                f"CLIP_{obj.name[:40]}", imin, imax, mats_all["floor"], coll_vis
            )
            mat = classify_material_slot(box, mats_all)
            if box.data.materials:
                box.data.materials[0] = mat
            else:
                box.data.materials.append(mat)
            kept.append(box)
            dropped["clipped"] += 1
            continue

        # modest-sized mesh fully used (copy as-is)
        new = obj.copy()
        new.data = obj.data.copy()
        new.animation_data_clear()
        coll_vis.objects.link(new)
        kept.append(new)
        dropped["ok"] += 1

    # Unify materials
    for o in kept:
        mat = classify_material_slot(o, mats_all)
        if o.data.materials:
            o.data.materials[0] = mat
        else:
            o.data.materials.append(mat)

    # Solidify thin walls, then BAKE modifiers before merge (B1)
    solidified = 0
    for o in kept:
        if solidify_thin(o, 0.15):
            solidified += 1
    bpy.context.view_layer.update()
    for o in kept:
        apply_modifiers(o)

    # Collision from PRE-MERGE pieces (B2)
    boxes, ramp_cols = build_collision_boxes(kept, coll_col, mats_all["collision"])

    # Merge visual by material AFTER modifiers are baked
    merged = merge_by_material(kept, coll_vis, region_name)

    # Enforce poly budget after solidify bake
    total_polys = sum(len(o.data.polygons) for o in coll_vis.objects)
    if total_polys > 180000:
        ratio = max(0.25, 180000.0 / float(total_polys))
        for o in coll_vis.objects:
            if len(o.data.polygons) < 50:
                continue
            mod = o.modifiers.new("BudgetDecimate", "DECIMATE")
            mod.decimate_type = "COLLAPSE"
            mod.ratio = ratio
            apply_modifiers(o)

    # Export visual + collision FBX
    fbx_path = os.path.join(out_dir, f"{region_name}.fbx")
    export_objs = list(coll_vis.objects) + list(coll_col.objects)
    # Bake world transforms so FBX cannot mix local/world spaces.
    bpy.context.view_layer.update()
    cleaned = []
    for o in export_objs:
        if o.type != "MESH":
            continue
        o.data.transform(o.matrix_world)
        o.matrix_world = Matrix.Identity(4)
        # drop origin junk (unit cubes left at 0,0,0)
        mn, mx, ctr, sz = world_bbox(o)
        if abs(ctr.x) < 2 and abs(ctr.y) < 2 and max(sz.x, sz.y) < 3:
            mesh = o.data
            bpy.data.objects.remove(o, do_unlink=True)
            if mesh.users == 0:
                bpy.data.meshes.remove(mesh)
            continue
        cleaned.append(o)
    export_objs = cleaned
    for obj in bpy.context.scene.objects:
        obj.select_set(False)
    for o in export_objs:
        o.select_set(True)
    # debug: max world span of export set
    if export_objs:
        emin = Vector((1e9, 1e9, 1e9))
        emax = Vector((-1e9, -1e9, -1e9))
        for o in export_objs:
            mn, mx, _, _ = world_bbox(o)
            for i in range(3):
                emin[i] = min(emin[i], mn[i])
                emax[i] = max(emax[i], mx[i])
        print(f"[Debug] {region_name} export_span=({emax.x-emin.x:.1f},{emax.y-emin.y:.1f}) crop=({bmax.x-bmin.x:.1f},{bmax.y-bmin.y:.1f})")
        # list offenders that exceed crop
        for o in export_objs:
            mn, mx, ctr, sz = world_bbox(o)
            if mx.x - mn.x > (bmax.x - bmin.x) * 1.02 or mx.y - mn.y > (bmax.y - bmin.y) * 1.02:
                print(f"  OFFENDER {o.name} span=({mx.x-mn.x:.1f},{mx.y-mn.y:.1f}) ctr=({ctr.x:.1f},{ctr.y:.1f})")
            elif ctr.x < bmin.x - 1 or ctr.x > bmax.x + 1 or ctr.y < bmin.y - 1 or ctr.y > bmax.y + 1:
                print(f"  OUTSIDE {o.name} ctr=({ctr.x:.1f},{ctr.y:.1f}) span=({mx.x-mn.x:.1f},{mx.y-mn.y:.1f})")
    for o in export_objs:
        o.select_set(True)
    if export_objs:
        bpy.context.view_layer.objects.active = export_objs[0]
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

    # Stats BEFORE cleanup
    vis_polys = sum(len(o.data.polygons) for o in coll_vis.objects)
    vis_objs = len(list(coll_vis.objects))
    col_count = len(list(coll_col.objects))
    materials_used = sorted(
        {s.material.name for o in coll_vis.objects for s in o.material_slots if s.material}
    )

    # Remove this region's objects so later regions cannot leak them into FBX.
    for o in list(coll_vis.objects) + list(coll_col.objects):
        mesh = o.data if o.type == "MESH" else None
        bpy.data.objects.remove(o, do_unlink=True)
        if mesh and mesh.users == 0:
            bpy.data.meshes.remove(mesh)
    try:
        bpy.data.collections.remove(root)
    except Exception:
        pass

    audit = {
        "region": region_name,
        "center": [cx, cy],
        "radius": radius,
        "dropped": dropped,
        "solidified": solidified,
        "ramp_colliders": ramp_cols,
        "visual_objects": vis_objs,
        "visual_polys": vis_polys,
        "collision_boxes": col_count,
        "materials_used": materials_used,
        "fbx_path": fbx_path,
        "fbx_size_bytes": os.path.getsize(fbx_path),
        "pass_objects_lt_300": vis_objs < 300,
        "pass_polys_lt_200k": vis_polys < 200000,
        "pass_materials": 3 <= len(materials_used) <= 4,
        "pass_collision": col_count >= 3,
    }
    return audit


def main():
    out_dir = args_after_double_dash()[0] if args_after_double_dash() else r"F:/project/gitprojects/UE/valorant/ascent1/Saved/AscentPlayableRegions"
    os.makedirs(out_dir, exist_ok=True)

    mats = {
        "wall": make_mat("MAT_Gray_Wall", (0.55, 0.55, 0.56), 0.85),
        "floor": make_mat("MAT_Gray_Floor", (0.42, 0.42, 0.43), 0.9),
        "cover": make_mat("MAT_Gray_Cover", (0.62, 0.58, 0.52), 0.8),
        "collision": make_mat("MAT_Gray_Collision", (0.2, 0.2, 0.22), 0.9),
    }

    # Snapshot source objects once
    src = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    print("[Regions] source meshes", len(src))

    results = []
    for name, stand, cover, fz in REGIONS:
        audit = process_region(src, name, stand, cover, fz, out_dir, mats)
        results.append(audit)
        print(
            f"[Region] {name}: objs={audit['visual_objects']} polys={audit['visual_polys']} "
            f"col={audit['collision_boxes']} pass_obj={audit['pass_objects_lt_300']} "
            f"pass_poly={audit['pass_polys_lt_200k']}"
        )

    report = {
        "source": bpy.data.filepath,
        "regions": results,
        "policy": {
            "min_keep_edge": MIN_KEEP_EDGE,
            "peek_margin": PEEK_MARGIN,
            "solidify_m": 0.15,
            "materials": ["wall", "floor", "cover", "collision"],
            "eye_height_m": 1.60,
            "crouch_head_m": 1.10,
        },
        "all_pass_objects": all(r["pass_objects_lt_300"] for r in results),
        "all_pass_polys": all(r["pass_polys_lt_200k"] for r in results),
        "all_pass_materials": all(r.get("pass_materials") for r in results),
        "all_pass_collision": all(r.get("pass_collision") for r in results),
    }
    report_path = os.path.join(out_dir, "regions_report.json")
    with open(report_path, "w", encoding="utf-8") as f:
        json.dump(report, f, ensure_ascii=False, indent=2)
    print("[Regions] report", report_path)
    print("[Regions] all_pass_objects", report["all_pass_objects"], "all_pass_polys", report["all_pass_polys"])
    print("[Regions] DONE")


if __name__ == "__main__":
    main()
