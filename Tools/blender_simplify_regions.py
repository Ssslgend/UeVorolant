"""按区域简化 Ascent 模型并导出 FBX。

用法:
  blender -b <file.blend> -P Tools/blender_simplify_regions.py -- \
    <config.json> <output_dir> <report.json>

配置中可选 simplify 段:
  "simplify": {
    "decimate_ratio": 0.5,
    "min_polys_for_decimate": 64,
    "skip_collections": ["Ascent_Art_Vista", "Ascent_Art_VistaA", ...],
    "skip_name_patterns": ["Tree_", "wireShape", "LightShaft"]
  }
"""

import json
import os
import re
import sys

import bpy
from mathutils import Vector


def args_after_double_dash():
    argv = sys.argv
    if "--" not in argv:
        return []
    return argv[argv.index("--") + 1 :]


def world_bbox(obj):
    corners = [obj.matrix_world @ Vector(corner) for corner in obj.bound_box]
    mins = [min(corner[i] for corner in corners) for i in range(3)]
    maxs = [max(corner[i] for corner in corners) for i in range(3)]
    center = [(mins[i] + maxs[i]) * 0.5 for i in range(3)]
    return mins, maxs, center


def intersects_region(obj_min, obj_max, center, extent):
    region_min = [center[i] - extent[i] for i in range(3)]
    region_max = [center[i] + extent[i] for i in range(3)]
    return all(obj_max[i] >= region_min[i] and obj_min[i] <= region_max[i] for i in range(3))


def collection_match(obj, allowed):
    collections = {collection.name for collection in obj.users_collection}
    return bool(collections.intersection(allowed))


def excluded_by_name(obj, patterns):
    return any(re.search(pattern, obj.name, re.IGNORECASE) for pattern in patterns)


def safe_name(value):
    return re.sub(r"[^A-Za-z0-9_.-]+", "_", value).strip("._") or "unnamed"


def region_objects(region, config):
    allowed = set(region["collections"])
    center = region["center"]
    extent = region["extent"]
    min_center_z = config.get("min_center_z", 40.0)
    exclude_patterns = config.get("exclude_name_patterns", [])
    simplify = config.get("simplify", {})
    skip_collections = set(simplify.get("skip_collections", []))
    skip_name_patterns = simplify.get("skip_name_patterns", [])

    selected = []
    skipped = {"collection": 0, "name": 0, "height": 0, "bounds": 0, "type": 0}
    for obj in bpy.context.scene.objects:
        if obj.type != "MESH":
            skipped["type"] += 1
            continue
        if not collection_match(obj, allowed):
            skipped["collection"] += 1
            continue
        if excluded_by_name(obj, exclude_patterns):
            skipped["name"] += 1
            continue
        if skip_collections and collection_match(obj, skip_collections):
            skipped["collection"] += 1
            continue
        if skip_name_patterns and excluded_by_name(obj, skip_name_patterns):
            skipped["name"] += 1
            continue

        obj_min, obj_max, obj_center = world_bbox(obj)
        if obj_center[2] < min_center_z:
            skipped["height"] += 1
            continue
        if not intersects_region(obj_min, obj_max, center, extent):
            skipped["bounds"] += 1
            continue
        selected.append(obj)
    return selected, skipped


def apply_decimate(obj, ratio, min_polys):
    mesh = obj.data
    before_polys = len(mesh.polygons)
    before_verts = len(mesh.vertices)
    if before_polys < min_polys or ratio >= 0.999:
        return {
            "decimated": False,
            "reason": "below_threshold" if before_polys < min_polys else "ratio_full",
            "polys_before": before_polys,
            "polys_after": before_polys,
            "verts_before": before_verts,
            "verts_after": before_verts,
        }

    modifier = obj.modifiers.new(name="RegionSimplify", type="DECIMATE")
    modifier.decimate_type = "COLLAPSE"
    modifier.ratio = max(0.01, min(1.0, ratio))
    modifier.use_collapse_triangulate = False

    depsgraph = bpy.context.evaluated_depsgraph_get()
    evaluated = obj.evaluated_get(depsgraph)
    simplified_mesh = bpy.data.meshes.new_from_object(evaluated)
    simplified_mesh.name = obj.data.name + "_simplified"

    old_mesh = obj.data
    obj.data = simplified_mesh
    obj.modifiers.remove(modifier)
    if old_mesh.users == 0:
        bpy.data.meshes.remove(old_mesh)

    return {
        "decimated": True,
        "reason": "collapse",
        "ratio": ratio,
        "polys_before": before_polys,
        "polys_after": len(simplified_mesh.polygons),
        "verts_before": before_verts,
        "verts_after": len(simplified_mesh.vertices),
    }


def export_region_fbx(region, objects, output_dir, scale, decimate_ratio, min_polys):
    for obj in bpy.context.scene.objects:
        obj.select_set(False)

    export_objects = []
    temp_collection = bpy.data.collections.new(f"__simplify_export_{region['name']}")
    bpy.context.scene.collection.children.link(temp_collection)
    stats = {
        "object_count": 0,
        "decimated_object_count": 0,
        "polys_before": 0,
        "polys_after": 0,
        "verts_before": 0,
        "verts_after": 0,
        "decimate_details": [],
    }

    try:
        for obj in objects:
            export_obj = obj.copy()
            export_obj.data = obj.data.copy()
            export_obj.animation_data_clear()
            export_obj.name = f"simplify_{obj.name}"
            export_obj.data.name = f"simplify_{obj.data.name}"
            temp_collection.objects.link(export_obj)

            detail = apply_decimate(export_obj, decimate_ratio, min_polys)
            detail["object"] = obj.name
            stats["decimate_details"].append(detail)
            stats["polys_before"] += detail["polys_before"]
            stats["polys_after"] += detail["polys_after"]
            stats["verts_before"] += detail["verts_before"]
            stats["verts_after"] += detail["verts_after"]
            if detail["decimated"]:
                stats["decimated_object_count"] += 1

            export_obj.select_set(True)
            export_objects.append(export_obj)

        stats["object_count"] = len(export_objects)
        if export_objects:
            bpy.context.view_layer.objects.active = export_objects[0]
            bpy.context.view_layer.update()

        output_path = os.path.join(output_dir, f"{region['name']}_simplified.fbx")
        bpy.ops.export_scene.fbx(
            filepath=output_path,
            use_selection=True,
            global_scale=scale,
            apply_unit_scale=True,
            apply_scale_options="FBX_SCALE_ALL",
            use_space_transform=True,
            axis_forward="-Y",
            axis_up="Z",
            bake_space_transform=True,
            mesh_smooth_type="FACE",
            use_mesh_modifiers=True,
            use_armature_deform_only=True,
            add_leaf_bones=False,
            path_mode="COPY",
            embed_textures=True,
        )
    finally:
        for export_obj in export_objects:
            mesh = export_obj.data
            bpy.data.objects.remove(export_obj, do_unlink=True)
            if mesh and mesh.users == 0:
                bpy.data.meshes.remove(mesh)
        bpy.data.collections.remove(temp_collection)
        bpy.context.view_layer.update()

    return output_path, stats


def main():
    args = args_after_double_dash()
    if len(args) != 3:
        raise SystemExit(
            "Usage: blender -b <file.blend> -P Tools/blender_simplify_regions.py "
            "-- <config.json> <output_dir> <report.json>"
        )

    config_path, output_dir, report_path = args
    os.makedirs(output_dir, exist_ok=True)
    os.makedirs(os.path.dirname(os.path.abspath(report_path)), exist_ok=True)

    with open(config_path, "r", encoding="utf-8") as handle:
        config = json.load(handle)

    simplify = config.get("simplify", {})
    decimate_ratio = float(simplify.get("decimate_ratio", 0.5))
    min_polys = int(simplify.get("min_polys_for_decimate", 64))
    scale = float(config.get("export_scale", 100.0))

    report = {
        "blend_file": bpy.data.filepath,
        "config_path": config_path,
        "output_dir": output_dir,
        "decimate_ratio": decimate_ratio,
        "min_polys_for_decimate": min_polys,
        "regions": [],
    }

    for region in config["regions"]:
        objects, skipped = region_objects(region, config)
        if not objects:
            raise RuntimeError(f"No objects selected for region {region['name']}")

        output_path, stats = export_region_fbx(
            region, objects, output_dir, scale, decimate_ratio, min_polys
        )
        size_bytes = os.path.getsize(output_path)
        reduction = 0.0
        if stats["polys_before"] > 0:
            reduction = 1.0 - (stats["polys_after"] / stats["polys_before"])

        entry = {
            "name": region["name"],
            "label": region.get("label", region["name"]),
            "fbx_path": output_path,
            "fbx_size_bytes": size_bytes,
            "skipped": skipped,
            "object_count": stats["object_count"],
            "decimated_object_count": stats["decimated_object_count"],
            "polys_before": stats["polys_before"],
            "polys_after": stats["polys_after"],
            "verts_before": stats["verts_before"],
            "verts_after": stats["verts_after"],
            "poly_reduction": reduction,
            "sample_decimate": stats["decimate_details"][:20],
        }
        report["regions"].append(entry)
        print(
            f"[Simplify] {region['name']}: {entry['object_count']} objects, "
            f"{entry['polys_before']} -> {entry['polys_after']} polys "
            f"({reduction * 100:.1f}% reduced), {size_bytes / 1024 / 1024:.1f} MB"
        )

    with open(report_path, "w", encoding="utf-8") as handle:
        json.dump(report, handle, ensure_ascii=False, indent=2)

    print(f"[Simplify] Report written to {report_path}")


if __name__ == "__main__":
    main()
