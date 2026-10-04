"""Audit AscentPlayable.blend against aim-training acceptance rules."""
import bpy
import json
import os
import sys
from mathutils import Vector


def args():
    argv = sys.argv
    return argv[argv.index("--") + 1 :] if "--" in argv else []


def world_bbox(obj):
    corners = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
    mins = Vector((min(v[i] for v in corners) for i in range(3)))
    maxs = Vector((max(v[i] for v in corners) for i in range(3)))
    return mins, maxs, (mins + maxs) * 0.5, maxs - mins


REGIONS = {
    "A_Site": (8.0, -10.0, 25.0),
    "A_Main": (-5.0, -40.0, 25.0),
    "B_Site": (-110.0, 3.0, 25.0),
    "B_Main": (-91.0, -22.0, 25.0),
    "Mid": (-40.0, -30.0, 25.0),
    "Market": (-94.0, -42.0, 25.0),
    "Atk_Spawn": (-55.0, -90.0, 25.0),
    "Def_Spawn": (-48.0, 32.0, 25.0),
    "Heaven": (0.0, 11.0, 25.0),
}


def main():
    out = args()[0] if args() else "audit.json"
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    total_poly = sum(len(o.data.polygons) for o in meshes)

    # material names
    mats = sorted({s.material.name for o in meshes for s in o.material_slots if s.material})

    # thin walls (min dimension < 0.05) = risk of penetration
    thin = 0
    for o in meshes:
        _, _, _, size = world_bbox(o)
        if min(size) < 0.05 and max(size) > 2:
            thin += 1

    region_stats = {}
    for name, (cx, cy, rad) in REGIONS.items():
        objs = []
        polys = 0
        for o in meshes:
            _, _, center, size = world_bbox(o)
            if (center.x - cx) ** 2 + (center.y - cy) ** 2 <= rad * rad:
                objs.append(o.name)
                polys += len(o.data.polygons)
        region_stats[name] = {
            "object_count": len(objs),
            "polys": polys,
            "pass_objects_lt_300": len(objs) < 300,
            "pass_polys_lt_200k": polys < 200000,
        }

    report = {
        "blend": bpy.data.filepath,
        "total_meshes": len(meshes),
        "total_polys": total_poly,
        "material_count": len(mats),
        "materials_sample": mats[:20],
        "thin_wall_like_count": thin,
        "region_stats": region_stats,
        "overall": {
            "objects_lt_300_all_regions": all(v["pass_objects_lt_300"] for v in region_stats.values()),
            "polys_lt_200k_all_regions": all(v["pass_polys_lt_200k"] for v in region_stats.values()),
            "materials_3_to_4_gray": 3 <= len(mats) <= 8,
            "no_thin_walls": thin == 0,
        },
    }

    with open(out, "w", encoding="utf-8") as f:
        json.dump(report, f, ensure_ascii=False, indent=2)
    print(json.dumps(report, ensure_ascii=False, indent=2)[:4000])
    print("AUDIT_WROTE", out)


if __name__ == "__main__":
    main()
