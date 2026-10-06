"""Reimport AscentPlayable_UE.fbx into /Game/Ascent/AscentPlayable.

Run in UE Editor Python after Tools/export_project_map.bat:
  exec(open(r"F:/project/gitprojects/UE/valorant/ascent1/Tools/ue_reimport_map.py").read()); main()
"""
from __future__ import annotations

import os
import traceback

import unreal

FBX = r"F:\project\gitprojects\UE\valorant\ascent1\Saved\AscentPlayable\AscentPlayable_UE.fbx"
DEST = "/Game/Ascent"
NAME = "AscentPlayable"
TARGET = f"{DEST}/{NAME}.{NAME}"


def main() -> None:
    try:
        if not os.path.isfile(FBX):
            unreal.log_error(f"missing FBX: {FBX}")
            return

        # Prefer reimport on the existing asset so the same package is reused.
        if unreal.EditorAssetLibrary.does_asset_exist(TARGET):
            tools = unreal.AssetToolsHelpers.get_asset_tools()
            # FbxImportUI keeps scale/collision consistent with the original import.
            options = unreal.FbxImportUI()
            options.import_mesh = True
            options.import_as_skeletal = False
            options.import_materials = True
            options.import_textures = False
            options.static_mesh_import_data.combine_meshes = True
            options.static_mesh_import_data.generate_lightmap_u_vs = True
            options.static_mesh_import_data.import_uniform_scale = 1.0

            task = unreal.AssetImportTask()
            task.filename = FBX
            task.destination_path = DEST
            task.destination_name = NAME
            task.automated = True
            task.replace_existing = True
            task.save = True
            task.options = options

            tools.import_asset_tasks([task])
            unreal.log(f"[reimport_map] reimported {TARGET}")
        else:
            unreal.log_error(f"asset missing, run full import first: {TARGET}")
            return

        unreal.EditorAssetLibrary.save_asset(TARGET)
        unreal.log("[reimport_map] saved. Restart PIE to pick up the new map.")
    except Exception:
        unreal.log_error(traceback.format_exc())


if __name__ == "__main__":
    main()
