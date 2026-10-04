import json
import os
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
    size = [maxs[i] - mins[i] for i in range(3)]
    return mins, maxs, center, size


def mesh_stats(obj):
    mesh = obj.data
    return {
        "vertices": len(mesh.vertices),
        "polygons": len(mesh.polygons),
        "materials": [slot.material.name if slot.material else "" for slot in obj.material_slots],
    }


def main():
    args = args_after_double_dash()
    output_path = args[0] if args else os.path.join(os.getcwd(), "scene_inventory.json")
    os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)

    objects = []
    collection_counts = {}

    for obj in bpy.context.scene.objects:
        if obj.type != "MESH":
            continue

        mins, maxs, center, size = world_bbox(obj)
        collections = [collection.name for collection in obj.users_collection]
        for collection in collections:
            collection_counts[collection] = collection_counts.get(collection, 0) + 1

        entry = {
            "name": obj.name,
            "type": obj.type,
            "collections": collections,
            "bbox_min": mins,
            "bbox_max": maxs,
            "bbox_center": center,
            "bbox_size": size,
        }
        entry.update(mesh_stats(obj))
        objects.append(entry)

    scene_mins = [min(obj["bbox_min"][i] for obj in objects) for i in range(3)] if objects else [0, 0, 0]
    scene_maxs = [max(obj["bbox_max"][i] for obj in objects) for i in range(3)] if objects else [0, 0, 0]

    report = {
        "blend_file": bpy.data.filepath,
        "mesh_object_count": len(objects),
        "collection_counts": dict(sorted(collection_counts.items())),
        "scene_bbox_min": scene_mins,
        "scene_bbox_max": scene_maxs,
        "objects": objects,
    }

    with open(output_path, "w", encoding="utf-8") as handle:
        json.dump(report, handle, ensure_ascii=False, indent=2)

    print(f"[Inventory] Wrote {len(objects)} mesh objects to {output_path}")


if __name__ == "__main__":
    main()
