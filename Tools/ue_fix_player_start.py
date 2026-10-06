"""Snap PlayerStart onto the AscentPlayable floor.

Uses verified trainer-spot world positions (from trainer_spots_v1.json), then
line-traces downward to land exactly on the mesh surface.

Run in UE Editor Python:

  exec(open(r"F:/project/gitprojects/UE/valorant/ascent1/Tools/ue_fix_player_start.py").read()); main()
"""
from __future__ import annotations

import traceback

import unreal

TAG = "[ue_fix_player_start]"

# Verified floor positions from Content/Data/trainer_spots_v1.json (world cm).
# Ascent playable map, actor at origin scale 100.
SPOTS = [
    {"name": "A_Main_to_Site", "pos": (-200.0, -3360.0, 6781.0), "yaw": 27.9},
    {"name": "Mid_to_Market", "pos": (-3800.0, -4000.0, 6769.0), "yaw": 90.0},
    {"name": "B_Main_to_Boat", "pos": (-9150.0, -2230.0, 6388.0), "yaw": 127.4},
]

# Which spot to use as the default PlayerStart (0 = A Main).
DEFAULT_SPOT = 0

# Capsule half-height + small clearance so we don't start intersecting geometry.
CAPSULE_CLEARANCE = 92.0


def log(msg: str) -> None:
    unreal.log(f"{TAG} {msg}")


def log_error(msg: str) -> None:
    unreal.log_error(f"{TAG} {msg}")


def all_actors():
    return list(unreal.EditorLevelLibrary.get_all_level_actors())


def find_map_actor():
    for a in all_actors():
        name = a.get_name()
        label = a.get_actor_label() if hasattr(a, "get_actor_label") else name
        if "AscentPlayable" in name or "AscentPlayable" in label:
            return a
    # fallback: any StaticMeshActor with a mesh
    for a in all_actors():
        if isinstance(a, unreal.StaticMeshActor):
            comp = a.get_component_by_class(unreal.StaticMeshComponent)
            if comp and comp.get_editor_property("static_mesh"):
                return a
    return None


def find_or_create_player_start():
    for a in all_actors():
        if isinstance(a, unreal.PlayerStart):
            return a
    log("no PlayerStart found, creating one")
    return unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.PlayerStart, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0)
    )


def trace_floor(world, xy_z_hint, drop_from, drop_to):
    """Line trace downward; returns hit location or None."""
    params = unreal.LineTraceParams()
    hit = world.line_trace_single_by_channel(
        unreal.Vector(xy_z_hint[0], xy_z_hint[1], drop_from),
        unreal.Vector(xy_z_hint[0], xy_z_hint[1], drop_to),
        unreal.TraceTypeQuery.TraceTypeQuery1,  # often Visibility; fall back below
        params,
    )
    if hit and hit.get_editor_property("blocking_hit"):
        return hit.get_editor_property("location")
    # fallback: Visibility channel via TraceTypeQuery2
    hit = world.line_trace_single_by_channel(
        unreal.Vector(xy_z_hint[0], xy_z_hint[1], drop_from),
        unreal.Vector(xy_z_hint[0], xy_z_hint[1], drop_to),
        unreal.TraceTypeQuery.TraceTypeQuery2,
        params,
    )
    if hit and hit.get_editor_property("blocking_hit"):
        return hit.get_editor_property("location")
    return None


def main() -> None:
    try:
        map_actor = find_map_actor()
        if map_actor is None:
            log_error("AscentPlayable mesh actor not found in level")
            return

        # Map bounds -> drop start/end for trace
        origin = map_actor.get_actor_location()
        scale = map_actor.get_actor_scale3d()
        log(f"map actor={map_actor.get_name()} loc={origin} scale={scale}")

        # Use mesh component bounds if available (world space)
        drop_from = 20000.0
        drop_to = -20000.0

        spot = SPOTS[DEFAULT_SPOT]
        log(f"using spot '{spot['name']}' pos={spot['pos']} yaw={spot['yaw']}")

        ps = find_or_create_player_start()
        if ps is None:
            log_error("failed to get PlayerStart")
            return

        world = unreal.EditorLevelLibrary.get_editor_world()
        grounded = None
        if world is not None:
            # Trace above the spot XY from high up
            grounded = trace_floor(world, spot["pos"], drop_from, drop_to)
            if grounded is not None:
                log(f"floor hit at {grounded}")
            else:
                log("line trace missed, falling back to spot Z")

        if grounded is not None:
            loc = unreal.Vector(grounded.x, grounded.y, grounded.z + CAPSULE_CLEARANCE)
        else:
            loc = unreal.Vector(spot["pos"][0], spot["pos"][1], spot["pos"][2] + CAPSULE_CLEARANCE)

        ps.set_actor_location(loc, False, False)
        ps.set_actor_rotation(unreal.Rotator(0.0, spot["yaw"], 0.0), False)
        ps.set_actor_label("PS_Start")
        log(f"PlayerStart -> {loc} yaw={spot['yaw']}")

        # Also drop a note: teleport-friendly mid air rescue if still floating
        final = ps.get_actor_location()
        log(f"final PlayerStart location = {final}")

        if hasattr(unreal.EditorLevelLibrary, "save_current_level"):
            unreal.EditorLevelLibrary.save_current_level()
            log("level saved")

        log("done. Press Play to test. Run with DEFAULT_SPOT=1 or 2 to try Mid / B Main.")
    except Exception:
        log_error(traceback.format_exc())


if __name__ == "__main__":
    main()
