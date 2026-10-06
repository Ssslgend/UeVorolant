"""Import AscentPlayable_UE.fbx into /Game/Ascent and wire a training level.

Run from UE Editor Python:
  import importlib.util, pathlib
  p = pathlib.Path(r"F:/project/gitprojects/UE/valorant/ascent1/Tools/ue_setup_trainer_mvp.py")
  spec = importlib.util.spec_from_file_location("ue_setup_trainer_mvp", p)
  m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m); m.main()

Expects Saved/AscentPlayable/AscentPlayable_UE.fbx (Scale=100 already applied on export).
"""
from __future__ import annotations

import os
import traceback

import unreal

ROOT = r"F:/project/gitprojects/UE/valorant/ascent1"
FBX = os.path.join(ROOT, "Saved", "AscentPlayable", "AscentPlayable_UE.fbx")
DEST = "/Game/Ascent"
MESH_NAME = "AscentPlayable"
LEVEL_PATH = "/Game/Training/TrainerAscent"


def log(msg: str) -> None:
    unreal.log(f"[ue_setup_trainer_mvp] {msg}")


def import_map_mesh() -> bool:
    # Already imported in this project — skip FBX reimport.
    if unreal.EditorAssetLibrary.does_asset_exist(f"{DEST}/{MESH_NAME}.{MESH_NAME}"):
        log(f"mesh already present: {DEST}/{MESH_NAME}")
        return True

    if not os.path.isfile(FBX):
        unreal.log_error(f"FBX not found: {FBX}")
        return False

    task = unreal.AssetImportTask()
    task.filename = FBX
    task.destination_path = DEST
    task.destination_name = MESH_NAME
    task.automated = True
    task.replace_existing = True
    task.save = True

    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = True
    options.import_textures = False
    options.static_mesh_import_data.combine_meshes = True
    options.static_mesh_import_data.generate_lightmap_u_vs = True
    # FBX was exported at Scale=100 (Blender meters -> UE cm)
    options.static_mesh_import_data.import_translation = unreal.Vector(0.0, 0.0, 0.0)
    options.static_mesh_import_data.import_rotation = unreal.Rotator(0.0, 0.0, 0.0)
    options.static_mesh_import_data.import_uniform_scale = 1.0
    task.options = options

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported = list(task.imported_object_paths or [])
    log(f"imported={imported}")
    return any(MESH_NAME in p for p in imported)


def build_level() -> bool:
    # Prefer the 5.3 LevelEditorSubsystem; fall back to EditorLevelLibrary.
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem) if hasattr(unreal, "LevelEditorSubsystem") else None

    level_asset = f"{LEVEL_PATH}.{LEVEL_PATH.split('/')[-1]}"
    exists = unreal.EditorAssetLibrary.does_asset_exist(level_asset) or unreal.EditorAssetLibrary.does_asset_exist(LEVEL_PATH)

    if les is not None:
        if exists:
            les.load_level(LEVEL_PATH)
        else:
            les.new_level(LEVEL_PATH)
    else:
        if hasattr(unreal.EditorLevelLibrary, "load_level") and exists:
            unreal.EditorLevelLibrary.load_level(LEVEL_PATH)
        elif hasattr(unreal.EditorLevelLibrary, "new_level"):
            unreal.EditorLevelLibrary.new_level(LEVEL_PATH)
        else:
            unreal.log_error("no level create/load API available")
            return False

    mesh_path = f"{DEST}/{MESH_NAME}.{MESH_NAME}"
    mesh = unreal.EditorAssetLibrary.load_asset(mesh_path)
    if mesh is None:
        unreal.log_error(f"missing mesh {mesh_path}")
        return False

    def spawn(cls, loc, label, rot=None):
        if les is not None and hasattr(les, "spawn_actor_from_class"):
            actor = les.spawn_actor_from_class(cls, loc, rot or unreal.Rotator(0.0, 0.0, 0.0))
        elif hasattr(unreal.EditorLevelLibrary, "spawn_actor_from_class"):
            actor = unreal.EditorLevelLibrary.spawn_actor_from_class(cls, loc, rot or unreal.Rotator(0.0, 0.0, 0.0))
        else:
            return None
        if actor:
            actor.set_actor_label(label)
        return actor

    # Drop the playable geometry at the origin (export used world space).
    actor = None
    if hasattr(unreal.EditorLevelLibrary, "spawn_actor_from_object"):
        actor = unreal.EditorLevelLibrary.spawn_actor_from_object(mesh, unreal.Vector(0.0, 0.0, 0.0))
    if actor is None:
        actor = spawn(unreal.StaticMeshActor, unreal.Vector(0.0, 0.0, 0.0), "AscentPlayable")
        if actor:
            actor.static_mesh_component.set_static_mesh(mesh)
    else:
        actor.set_actor_label("AscentPlayable")
    log(f"spawned map actor={actor.get_name() if actor else None}")

    sun = spawn(unreal.DirectionalLight, unreal.Vector(0.0, 0.0, 1000.0), "TrainerSun", unreal.Rotator(-35.0, 40.0, 0.0))
    sky = spawn(unreal.SkyLight, unreal.Vector(0.0, 0.0, 800.0), "TrainerSky")
    ps = spawn(unreal.PlayerStart, unreal.Vector(-200.0, -3360.0, 6900.0), "PS_AMain", unreal.Rotator(0.0, 27.5, 0.0))

    if les is not None and hasattr(les, "save_current_level"):
        les.save_current_level()
    elif hasattr(unreal.EditorLevelLibrary, "save_current_level"):
        unreal.EditorLevelLibrary.save_current_level()
    log(f"saved level {LEVEL_PATH}")
    return True


def main() -> None:
    try:
        ok_mesh = import_map_mesh()
        ok_level = build_level() if ok_mesh else False
        log(f"done mesh={ok_mesh} level={ok_level}")
    except Exception:
        unreal.log_error(traceback.format_exc())


if __name__ == "__main__":
    main()
