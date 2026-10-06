"""Independent verification of the imported Ascent playable StaticMesh.

Run in a *fresh* headless editor process so the asset is genuinely re-loaded
from disk instead of being read back from the importing process' memory:

    UnrealEditor-Cmd.exe ascent1.uproject -run=pythonscript \
        -script="<this file>" -unattended -nop4 -nosplash -nullrhi -stdout

Writes <project>/Saved/AscentPlayable/ue_verify_report.json and logs one
machine readable RESULT line.
"""

import json
import os

import unreal


PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
REPORT_PATH = os.path.join(PROJECT_ROOT, "Saved", "AscentPlayable", "ue_verify_report.json")
ASSET_PATH = "/Game/Ascent/AscentPlayable.AscentPlayable"
PACKAGE_PATH = "/Game/Ascent/AscentPlayable"
FOLDER = "/Game/Ascent"
DISK_PATH = os.path.join(PROJECT_ROOT, "Content", "Ascent", "AscentPlayable.uasset")

TAG = "[AscentPlayableVerify]"


def log(message):
    unreal.log("{0} {1}".format(TAG, message))


def log_warning(message):
    unreal.log_warning("{0} {1}".format(TAG, message))


def package_path(asset_path):
    return asset_path.split(".", 1)[0]


def asset_class_name(asset_path):
    try:
        data = unreal.EditorAssetLibrary.find_asset_data(asset_path)
    except Exception:
        return ""
    if not data:
        return ""
    try:
        name = getattr(data.get_editor_property("asset_class_path"), "asset_name", None)
        if name:
            return str(name)
    except Exception:
        pass
    try:
        return str(data.asset_class)
    except Exception:
        return ""


def folder_inventory():
    inventory = {}
    try:
        for path in unreal.EditorAssetLibrary.list_assets(FOLDER, recursive=True, include_folder=False):
            name = asset_class_name(path)
            inventory[name] = inventory.get(name, 0) + 1
    except Exception as exc:
        log_warning("folder inventory failed: {0}".format(exc))
    return inventory


def disk_inventory():
    """Filesystem truth: what actually exists under Content/Ascent."""
    folder = os.path.join(PROJECT_ROOT, "Content", "Ascent")
    info = {"folder": folder, "exists": os.path.isdir(folder), "uasset_count": 0, "sidecar_count": 0, "other_uassets": []}
    if not info["exists"]:
        return info
    for name in sorted(os.listdir(folder)):
        full = os.path.join(folder, name)
        if not os.path.isfile(full):
            continue
        if name.endswith(".uasset"):
            info["uasset_count"] += 1
            if name != "AscentPlayable.uasset":
                info["other_uassets"].append(name)
        else:
            info["sidecar_count"] += 1
    info["material_asset_file_count"] = len(info["other_uassets"])
    info["other_uassets"] = info["other_uassets"][:5]
    return info


def material_slot_report(mesh):
    """Which material assets the mesh slots actually point at, and can they load?"""
    info = {"sample_paths": [], "unresolved_count": 0, "checked": 0}
    try:
        static_materials = list(mesh.get_editor_property("static_materials"))
    except Exception as exc:
        info["error"] = str(exc)
        return info
    for index, item in enumerate(static_materials):
        interface = item.material_interface
        if not interface:
            info["unresolved_count"] += 1
            continue
        if index < 5:
            path = interface.get_path_name()
            info["sample_paths"].append(path)
            info["checked"] += 1
            pkg = path.split(".", 1)[0]
            try:
                loaded = unreal.load_asset(pkg)
                info.setdefault("samples_reload_ok", []).append(bool(loaded))
            except Exception:
                info.setdefault("samples_reload_ok", []).append(False)
    return info


def mesh_lod_count(mesh):
    """LOD count. In UE 5.3 only the (deprecated) EditorStaticMeshLibrary exposes it to
    Python; the StaticMeshEditorSubsystem is not exposed to Python in this build. It
    returns -1 when the mesh has no built render data, which is the case in a headless
    -nullrhi run, so -1 here means 'not built in this session', not 'no LODs'."""
    try:
        return int(unreal.EditorStaticMeshLibrary.get_lod_count(mesh))
    except Exception:
        return -1


def collision_primitive_count(mesh):
    total = 0
    try:
        body_setup = mesh.get_editor_property("body_setup")
        agg = body_setup.get_editor_property("agg_geom") if body_setup else None
        if agg:
            for prop in ["box_elems", "sphere_elems", "sphyl_elems", "convex_elems", "tapered_capsule_elems"]:
                total += len(getattr(agg, prop, []))
    except Exception:
        pass
    return total


def main():
    result = {
        "asset_path": ASSET_PATH,
        "package_path": PACKAGE_PATH,
        "uasset_on_disk": os.path.exists(DISK_PATH),
        "uasset_size_bytes": os.path.getsize(DISK_PATH) if os.path.exists(DISK_PATH) else 0,
        "does_asset_exist": False,
        "load_object_ok": False,
        "class": "",
        "is_static_mesh": False,
        "bounds_origin": None,
        "bounds_box_extent": None,
        "bounds_nonzero": False,
        "material_slot_count": 0,
        "material_slots_with_assets": 0,
        "collision_trace_flag": "",
        "collision_flag_ok": False,
        "collision_primitive_count": 0,
        "lod_count": 0,
        "triangle_count": -1,
        "unit_guess": "",
        "suggested_actor_scale": None,
        "folder_inventory": {},
        "ok": False,
        "error": "",
    }

    try:
        result["does_asset_exist"] = bool(unreal.EditorAssetLibrary.does_asset_exist(PACKAGE_PATH))
    except Exception as exc:
        log_warning("does_asset_exist failed: {0}".format(exc))

    mesh = None
    try:
        mesh = unreal.load_asset(PACKAGE_PATH)
    except Exception as exc:
        result["error"] = "unreal.load_asset raised: {0}".format(exc)
        log_warning(result["error"])

    if mesh is None:
        try:
            mesh = unreal.EditorAssetLibrary.load_asset(PACKAGE_PATH)
        except Exception as exc:
            result["error"] = (result["error"] + " | EditorAssetLibrary.load_asset raised: {0}".format(exc)).strip(" |")

    if mesh is None:
        log_warning("asset could not be loaded: {0}".format(PACKAGE_PATH))
        result["folder_inventory"] = folder_inventory()
        result["disk_inventory"] = disk_inventory()
        with open(REPORT_PATH, "w", encoding="utf-8") as handle:
            json.dump(result, handle, ensure_ascii=False, indent=2)
        log("RESULT ok=False load_object_ok=False asset={0}".format(ASSET_PATH))
        return

    result["load_object_ok"] = True
    result["class"] = mesh.get_class().get_name()
    result["is_static_mesh"] = result["class"] == "StaticMesh"

    if result["is_static_mesh"]:
        bounds = mesh.get_bounds()
        origin = [round(float(bounds.origin.x), 3), round(float(bounds.origin.y), 3), round(float(bounds.origin.z), 3)]
        extent = [
            round(float(bounds.box_extent.x), 3),
            round(float(bounds.box_extent.y), 3),
            round(float(bounds.box_extent.z), 3),
        ]
        result["bounds_origin"] = origin
        result["bounds_box_extent"] = extent
        result["bounds_nonzero"] = all(value > 0.0 for value in extent)

        static_materials = list(mesh.get_editor_property("static_materials"))
        result["material_slot_count"] = len(static_materials)
        result["material_slots_with_assets"] = sum(1 for item in static_materials if item.material_interface)
        result["material_slots_detail"] = material_slot_report(mesh)

        body_setup = mesh.get_editor_property("body_setup")
        if body_setup:
            result["collision_trace_flag"] = str(body_setup.get_editor_property("collision_trace_flag"))
        result["collision_flag_ok"] = "CTF_USE_COMPLEX_AS_SIMPLE" in result["collision_trace_flag"]
        result["collision_primitive_count"] = collision_primitive_count(mesh)

        result["lod_count"] = mesh_lod_count(mesh)
        if result["lod_count"] < 0:
            result["lod_count_note"] = (
                "EditorStaticMeshLibrary.get_lod_count returned -1: StaticMesh render data is not "
                "built in a headless -nullrhi session (probe_lod_api.py confirmed the same value). "
                "Not a mesh defect and unrelated to the required checks."
            )
        try:
            result["triangle_count"] = int(mesh.get_num_triangles(0))
        except Exception:
            pass

        reference = max([abs(v) for v in origin + extent])
        result["max_abs_bound_magnitude"] = round(reference, 3)
        if reference <= 3000.0:
            result["unit_guess"] = "meters"
            result["suggested_actor_scale"] = 100
        else:
            result["unit_guess"] = "centimeters"
            result["suggested_actor_scale"] = 1

    result["folder_inventory"] = folder_inventory()
    result["disk_inventory"] = disk_inventory()
    result["ok"] = bool(
        result["load_object_ok"]
        and result["is_static_mesh"]
        and result["bounds_nonzero"]
        and result["material_slot_count"] > 0
        and result["collision_flag_ok"]
        and result["disk_inventory"]["uasset_count"] > 1
    )

    with open(REPORT_PATH, "w", encoding="utf-8") as handle:
        json.dump(result, handle, ensure_ascii=False, indent=2)

    log(
        "load_ok={0} class={1} origin={2} extent={3} materials={4}/{5} collision={6} lods={7} tris={8} ok={9}".format(
            result["load_object_ok"],
            result["class"],
            result["bounds_origin"],
            result["bounds_box_extent"],
            result["material_slots_with_assets"],
            result["material_slot_count"],
            result["collision_trace_flag"],
            result["lod_count"],
            result["triangle_count"],
            result["ok"],
        )
    )
    log("RESULT {0}".format(json.dumps(result, ensure_ascii=False)))
    log("report written: {0}".format(REPORT_PATH))


main()
