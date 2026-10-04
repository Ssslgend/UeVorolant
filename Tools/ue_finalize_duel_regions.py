import json
import os

import unreal


PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
REPORT_PATH = os.path.join(PROJECT_ROOT, "Saved", "AscentDuelExport", "ue_finalize_report.json")
DESTINATION_ROOT = "/Game/AscentDuelRegions"

REGIONS = [
    "A_Site_Duel",
    "B_Site_Duel",
    "Mid_Duel",
    "A_Main_Entry",
    "B_Main_Entry",
    "Market_Door_Duel",
]


def asset_path_for_region(region_name):
    return f"{DESTINATION_ROOT}/{region_name}/{region_name}.{region_name}"


def finalize_static_mesh(region_name):
    asset_path = asset_path_for_region(region_name)
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not asset:
        raise RuntimeError(f"Missing imported mesh asset: {asset_path}")

    body_setup = asset.get_editor_property("body_setup")
    if not body_setup:
        raise RuntimeError(f"Static mesh has no BodySetup: {asset_path}")

    body_setup.set_editor_property(
        "collision_trace_flag",
        unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE,
    )

    unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)

    bounds = asset.get_bounds()
    return {
        "region": region_name,
        "asset_path": asset_path,
        "class": asset.get_class().get_name(),
        "bounds_box_extent": [bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z],
        "collision_trace_flag": str(body_setup.get_editor_property("collision_trace_flag")),
    }


def main():
    results = [finalize_static_mesh(region_name) for region_name in REGIONS]
    unreal.EditorAssetLibrary.save_directory(DESTINATION_ROOT, only_if_is_dirty=False, recursive=True)

    with open(REPORT_PATH, "w", encoding="utf-8") as handle:
        json.dump({"regions": results}, handle, ensure_ascii=False, indent=2)

    unreal.log(f"[AscentDuelFinalize] Wrote report: {REPORT_PATH}")


main()
