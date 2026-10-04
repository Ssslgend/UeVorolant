import json
import os
import re
import shutil
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


def selected_objects_for_region(region, config):
    allowed = set(region["collections"])
    center = region["center"]
    extent = region["extent"]
    min_center_z = config.get("min_center_z", 40.0)
    exclude_patterns = config.get("exclude_name_patterns", [])

    selected = []
    for obj in bpy.context.scene.objects:
        if obj.type != "MESH":
            continue
        if not collection_match(obj, allowed):
            continue
        if excluded_by_name(obj, exclude_patterns):
            continue

        obj_min, obj_max, obj_center = world_bbox(obj)
        if obj_center[2] < min_center_z:
            continue
        if not intersects_region(obj_min, obj_max, center, extent):
            continue

        selected.append(obj)

    return selected


def region_bounds(objects):
    if not objects:
        return {"min": [0, 0, 0], "max": [0, 0, 0]}
    mins = []
    maxs = []
    for obj in objects:
        obj_min, obj_max, _ = world_bbox(obj)
        mins.append(obj_min)
        maxs.append(obj_max)
    return {
        "min": [min(values[i] for values in mins) for i in range(3)],
        "max": [max(values[i] for values in maxs) for i in range(3)],
    }


def export_region(region, objects, output_dir, scale):
    for obj in bpy.context.scene.objects:
        obj.select_set(False)

    export_objects = []
    temp_collection = bpy.data.collections.new(f"__codex_export_{region['name']}")
    bpy.context.scene.collection.children.link(temp_collection)
    try:
        for obj in objects:
            export_obj = obj.copy()
            export_obj.data = obj.data.copy()
            export_obj.animation_data_clear()
            export_obj.name = f"codex_export_{obj.name}"
            export_obj.data.name = f"codex_export_{obj.data.name}"
            temp_collection.objects.link(export_obj)
            export_obj.select_set(True)
            export_objects.append(export_obj)

        if export_objects:
            bpy.context.view_layer.objects.active = export_objects[0]
            bpy.context.view_layer.update()

        output_path = os.path.join(output_dir, f"{region['name']}.fbx")
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

    return output_path


def material_texture_paths(material):
    texture_paths = set()
    if not material or not material.use_nodes:
        return []

    for node in material.node_tree.nodes:
        image = getattr(node, "image", None)
        if not image:
            continue
        if image.filepath:
            texture_paths.add(bpy.path.abspath(image.filepath))

    return sorted(texture_paths)


def texture_usage(path):
    name = os.path.splitext(os.path.basename(path))[0].lower()
    if re.search(r"(^|[_-])(nm|normal|nrm)([_-]|$)", name):
        return "normal"
    if re.search(r"(^|[_-])(mra|orm|rao|rough|roughness|metal|metallic|ao)([_-]|$)", name):
        return "packed"
    if re.search(r"(^|[_-])(df|diff|diffuse|basecolor|albedo|color)([_-]|$)", name):
        return "base_color"
    return "unknown"


def material_entries(objects):
    materials = {}
    missing_texture_slots = 0
    for obj in objects:
        for slot in obj.material_slots:
            material = slot.material
            if not material:
                missing_texture_slots += 1
                continue

            entry = materials.setdefault(
                material.name,
                {
                    "texture_paths": set(),
                    "object_count": 0,
                },
            )
            entry["object_count"] += 1
            entry["texture_paths"].update(material_texture_paths(material))

    entries = []
    for name, item in materials.items():
        entries.append(
            {
                "name": name,
                "safe_name": safe_name(name),
                "object_count": item["object_count"],
                "texture_paths": sorted(item["texture_paths"]),
            }
        )
    return entries, missing_texture_slots


def copy_region_textures(objects, texture_root, region_name):
    entries, missing_texture_slots = material_entries(objects)
    region_texture_dir = os.path.join(texture_root, safe_name(region_name))
    copied_count = 0

    for entry in entries:
        exported_textures = []
        material_dir = os.path.join(region_texture_dir, entry["safe_name"])
        for source_path in entry["texture_paths"]:
            source_path = os.path.abspath(source_path)
            exists = os.path.exists(source_path)
            exported_path = ""
            if exists:
                os.makedirs(material_dir, exist_ok=True)
                exported_path = os.path.join(material_dir, safe_name(os.path.basename(source_path)))
                if not os.path.exists(exported_path) or os.path.getmtime(source_path) > os.path.getmtime(exported_path):
                    shutil.copy2(source_path, exported_path)
                copied_count += 1
            exported_textures.append(
                {
                    "source_path": source_path,
                    "exported_path": exported_path,
                    "usage": texture_usage(source_path),
                    "exists": exists,
                }
            )
        entry["exported_textures"] = exported_textures

    return {
        "material_count": len(entries),
        "materials_with_textures": sum(1 for item in entries if item["texture_paths"]),
        "missing_texture_slots": missing_texture_slots,
        "copied_texture_count": copied_count,
        "texture_root": region_texture_dir,
        "all_materials": entries,
        "sample_materials": entries[:30],
    }


def normal_report(objects):
    negative_scale_objects = []
    zero_area_faces = 0
    total_faces = 0
    for obj in objects:
        if obj.matrix_world.to_3x3().determinant() < 0:
            negative_scale_objects.append(obj.name)
        mesh = obj.data
        for polygon in mesh.polygons:
            total_faces += 1
            if polygon.area <= 1.0e-8:
                zero_area_faces += 1

    return {
        "negative_scale_object_count": len(negative_scale_objects),
        "sample_negative_scale_objects": negative_scale_objects[:30],
        "zero_area_faces": zero_area_faces,
        "total_faces_checked": total_faces,
    }


def object_report(objects):
    report = {
        "object_count": len(objects),
        "vertices": sum(len(obj.data.vertices) for obj in objects),
        "polygons": sum(len(obj.data.polygons) for obj in objects),
        "sample_objects": [obj.name for obj in objects[:40]],
        "bounds": region_bounds(objects),
    }
    report["normal_audit"] = normal_report(objects)
    return report


def main():
    args = args_after_double_dash()
    if len(args) != 3:
        raise SystemExit(
            "Usage: blender -b <file.blend> -P Tools/blender_export_regions.py "
            "-- <config.json> <output_dir> <report.json>"
        )

    config_path, output_dir, report_path = args
    os.makedirs(output_dir, exist_ok=True)
    os.makedirs(os.path.dirname(os.path.abspath(report_path)), exist_ok=True)
    texture_root = os.path.join(os.path.dirname(os.path.abspath(output_dir)), "Textures")

    with open(config_path, "r", encoding="utf-8") as handle:
        config = json.load(handle)

    report = {
        "blend_file": bpy.data.filepath,
        "config_path": config_path,
        "output_dir": output_dir,
        "regions": [],
    }

    scale = float(config.get("export_scale", 100.0))
    for region in config["regions"]:
        objects = selected_objects_for_region(region, config)
        if not objects:
            raise RuntimeError(f"No objects selected for region {region['name']}")

        output_path = export_region(region, objects, output_dir, scale)
        size_bytes = os.path.getsize(output_path)
        entry = object_report(objects)
        entry.update(copy_region_textures(objects, texture_root, region["name"]))
        entry.update(
            {
                "name": region["name"],
                "label": region.get("label", region["name"]),
                "fbx_path": output_path,
                "fbx_size_bytes": size_bytes,
            }
        )
        report["regions"].append(entry)
        print(
            f"[Export] {region['name']}: {entry['object_count']} objects, "
            f"{entry['polygons']} polys, {size_bytes / 1024 / 1024:.1f} MB"
        )

    with open(report_path, "w", encoding="utf-8") as handle:
        json.dump(report, handle, ensure_ascii=False, indent=2)

    print(f"[Export] Report written to {report_path}")


if __name__ == "__main__":
    main()

