"""Create /Game/AscentPlayableMap as THE playable level for the new AscentPlayable mesh.

Run once in UE Editor Python (Window -> Developer Tools -> Output Log -> Python):

  exec(open(r"F:/project/gitprojects/UE/valorant/ascent1/Tools/ue_make_playable_map.py").read()); main()

Result:
  - New level /Game/AscentPlayableMap
  - AscentPlayable static mesh placed at origin, actor scale 100 (mesh is in meters)
  - PlayerStart + DirectionalLight + SkyLight
  - Level saved

Then set DefaultEngine.ini maps to /Game/AscentPlayableMap (done separately)
and restart the editor.
"""
from __future__ import annotations

import traceback

import unreal

LEVEL_PATH = "/Game/AscentPlayableMap"
MESH_PATH = "/Game/Ascent/AscentPlayable.AscentPlayable"
TAG = "[ue_make_playable_map]"


def log(msg: str) -> None:
    unreal.log(f"{TAG} {msg}")


def log_error(msg: str) -> None:
    unreal.log_error(f"{TAG} {msg}")


def _level_subsystem():
    if hasattr(unreal, "LevelEditorSubsystem"):
        return unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    return None


def _spawn(cls, loc, label, rot=None):
    les = _level_subsystem()
    rot = rot or unreal.Rotator(0.0, 0.0, 0.0)
    actor = None
    if les is not None and hasattr(les, "spawn_actor_from_class"):
        actor = les.spawn_actor_from_class(cls, loc, rot)
    elif hasattr(unreal.EditorLevelLibrary, "spawn_actor_from_class"):
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(cls, loc, rot)
    if actor:
        actor.set_actor_label(label)
    return actor


def main() -> None:
    try:
        mesh = unreal.EditorAssetLibrary.load_asset(MESH_PATH)
        if mesh is None:
            log_error(f"mesh not found: {MESH_PATH}")
            return

        les = _level_subsystem()
        created = False

        if unreal.EditorAssetLibrary.does_asset_exist(f"{LEVEL_PATH}.{LEVEL_PATH.split('/')[-1]}"):
            log(f"level exists, opening: {LEVEL_PATH}")
            if les is not None:
                les.load_level(LEVEL_PATH)
            else:
                unreal.EditorLevelLibrary.load_level(LEVEL_PATH)
        else:
            log(f"creating new level: {LEVEL_PATH}")
            if les is not None:
                created = les.new_level(LEVEL_PATH)
            else:
                created = unreal.EditorLevelLibrary.new_level(LEVEL_PATH)
            if created is False:
                log_error("new_level returned False")
                return

        # Place map mesh. Mesh bounds are meters -> actor scale 100 (UE cm).
        map_actor = None
        if hasattr(unreal.EditorLevelLibrary, "spawn_actor_from_object"):
            map_actor = unreal.EditorLevelLibrary.spawn_actor_from_object(
                mesh, unreal.Vector(0.0, 0.0, 0.0)
            )
        if map_actor is None:
            map_actor = _spawn(unreal.StaticMeshActor, unreal.Vector(0.0, 0.0, 0.0), "AscentPlayable")
            if map_actor is not None:
                map_actor.static_mesh_component.set_static_mesh(mesh)

        if map_actor is None:
            log_error("failed to spawn map actor")
            return

        map_actor.set_actor_label("AscentPlayable")
        map_actor.set_actor_scale3d(unreal.Vector(100.0, 100.0, 100.0))
        log(f"map actor={map_actor.get_name()} scale=100")

        # PlayerStart: sit above ground near map center-ish; PrefireArena can teleport later.
        ps = _spawn(
            unreal.PlayerStart,
            unreal.Vector(-5500.0, 3000.0, 9000.0),
            "PS_Start",
            unreal.Rotator(0.0, 0.0, 0.0),
        )
        log(f"player start={ps.get_name() if ps else None}")

        sun = _spawn(
            unreal.DirectionalLight,
            unreal.Vector(0.0, 0.0, 5000.0),
            "Sun",
            unreal.Rotator(-35.0, 40.0, 0.0),
        )
        sky = _spawn(unreal.SkyLight, unreal.Vector(0.0, 0.0, 3000.0), "Sky")
        log(f"lights sun={sun.get_name() if sun else None} sky={sky.get_name() if sky else None}")

        if les is not None and hasattr(les, "save_current_level"):
            les.save_current_level()
        else:
            unreal.EditorLevelLibrary.save_current_level()

        unreal.EditorAssetLibrary.save_asset(LEVEL_PATH)
        log(f"saved {LEVEL_PATH}. Now set DefaultEngine.ini maps to this level and restart.")
    except Exception:
        log_error(traceback.format_exc())


if __name__ == "__main__":
    main()
