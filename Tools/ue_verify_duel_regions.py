import json
import os

import unreal


PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
REPORT_PATH = os.path.join(PROJECT_ROOT, "Saved", "AscentDuelExport", "ue_verify_report.json")
DESTINATION_ROOT = "/Game/AscentDuelRegions"

REGIONS = [
    "A_Site_Duel",
    "B_Site_Duel",
    "Mid_Duel",
    "A_Main_Entry",
    "B_Main_Entry",
    "Market_Door_Duel",
]


def package_path(asset_path):
    return asset_path.split(".", 1)[0]


def assets_under_region(region_name):
    region_path = f"{DESTINATION_ROOT}/{region_name}"
    return unreal.EditorAssetLibrary.list_assets(region_path, recursive=True, include_folder=False)


def class_counts(asset_paths):
    counts = {}
    for asset_path in asset_paths:
        asset = unreal.EditorAssetLibrary.load_asset(package_path(asset_path))
        if not asset:
            continue
        class_name = asset.get_class().get_name()
        counts[class_name] = counts.get(class_name, 0) + 1
    return counts


def material_texture_references(asset_paths):
    material_paths = []
    texture_references = set()
    for asset_path in asset_paths:
        asset = unreal.EditorAssetLibrary.load_asset(package_path(asset_path))
        if not asset:
            continue
        class_name = asset.get_class().get_name()
        if class_name not in ("Material", "MaterialInstanceConstant"):
            continue
        material_paths.append(asset_path)
        try:
            for texture in unreal.MaterialEditingLibrary.get_used_textures(asset):
                if texture:
                    texture_references.add(texture.get_path_name())
        except Exception as exc:
            unreal.log_warning(f"[AscentDuelVerify] Could not inspect texture refs for {asset_path}: {exc}")
    return material_paths, sorted(texture_references)


def verify_region(region_name):
    asset_path = f"{DESTINATION_ROOT}/{region_name}/{region_name}.{region_name}"
    asset = unreal.EditorAssetLibrary.load_asset(package_path(asset_path))
    result = {
        "region": region_name,
        "asset_path": asset_path,
        "exists": bool(asset),
    }
    if not asset:
        result["ok"] = False
        return result

    bounds = asset.get_bounds()
    extent = [bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z]
    static_materials = list(asset.get_editor_property("static_materials"))
    material_slot_count = len(static_materials)
    material_slots_with_assets = sum(1 for item in static_materials if item.material_interface)
    body_setup = asset.get_editor_property("body_setup")
    collision_trace_flag = str(body_setup.get_editor_property("collision_trace_flag")) if body_setup else ""
    region_assets = assets_under_region(region_name)
    counts = class_counts(region_assets)
    material_paths, texture_references = material_texture_references(region_assets)
    ok = (
        asset.get_class().get_name() == "StaticMesh"
        and all(value > 0 for value in extent)
        and material_slot_count > 0
        and material_slots_with_assets > 0
        and counts.get("Texture2D", 0) > 0
        and len(texture_references) > 0
        and "CTF_USE_COMPLEX_AS_SIMPLE" in collision_trace_flag
    )

    result.update(
        {
            "class": asset.get_class().get_name(),
            "bounds_box_extent": extent,
            "material_slot_count": material_slot_count,
            "material_slots_with_assets": material_slots_with_assets,
            "asset_class_counts": counts,
            "material_asset_count": len(material_paths),
            "texture_asset_count": counts.get("Texture2D", 0),
            "material_texture_reference_count": len(texture_references),
            "sample_texture_references": texture_references[:30],
            "collision_trace_flag": collision_trace_flag,
            "ok": ok,
        }
    )
    return result


def main():
    results = [verify_region(region_name) for region_name in REGIONS]
    with open(REPORT_PATH, "w", encoding="utf-8") as handle:
        json.dump({"regions": results, "all_ok": all(item["ok"] for item in results)}, handle, ensure_ascii=False, indent=2)

    for item in results:
        unreal.log(
            f"[AscentDuelVerify] {item['region']} ok={item['ok']} "
            f"class={item.get('class')} extent={item.get('bounds_box_extent')} "
            f"materials={item.get('material_slots_with_assets')}/{item.get('material_slot_count')} "
            f"textures={item.get('texture_asset_count')} refs={item.get('material_texture_reference_count')} "
            f"collision={item.get('collision_trace_flag')}"
        )
    if not all(item["ok"] for item in results):
        raise RuntimeError("One or more duel region assets failed verification")


main()
