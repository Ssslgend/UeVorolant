"""Apply verified outdoor lighting to the open level (AscentPlayableMap).

Proven fix 2026-10-05 (viewport was full black):
  - Sun pitch was +40 (pointing at sky) due to positional Rotator args
    (unreal.Rotator(roll, pitch, yaw) — always use named args)
  - SkyLight had no SkyAtmosphere to capture

Run in UE Editor Python (Output Log -> Python):

  exec(open(r"F:/project/gitprojects/UE/valorant/ascent1/Tools/ue_setup_lighting.py").read()); main()
"""
from __future__ import annotations

import traceback

import unreal

TAG = "[ue_setup_lighting]"


def log(msg: str) -> None:
    unreal.log(f"{TAG} {msg}")


def log_error(msg: str) -> None:
    unreal.log_error(f"{TAG} {msg}")


def all_actors():
    if hasattr(unreal.EditorLevelLibrary, "get_all_level_actors"):
        return list(unreal.EditorLevelLibrary.get_all_level_actors())
    return []


def find_by_class(cls):
    for a in all_actors():
        if isinstance(a, cls):
            return a
    return None


def spawn(cls, loc, label, rot=None):
    rot = rot or unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0)
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(cls, loc, rot)
    if actor:
        actor.set_actor_label(label)
    return actor


def get_comp(actor, comp_cls):
    return actor.get_component_by_class(comp_cls)


def set_mobility(actor, mobility) -> None:
    root = actor.root_component
    if root is not None:
        try:
            root.set_editor_property("mobility", mobility)
        except Exception as exc:
            log(f"mobility: {exc}")


def setup_sun() -> None:
    # Named args only — positional order is (roll, pitch, yaw) and pointed the sun up.
    sun_rot = unreal.Rotator(pitch=-45.0, yaw=40.0, roll=0.0)

    sun = find_by_class(unreal.DirectionalLight)
    if sun is None:
        sun = spawn(
            unreal.DirectionalLight,
            unreal.Vector(0.0, 0.0, 5000.0),
            "Sun",
            sun_rot,
        )
        log("created DirectionalLight 'Sun'")
    else:
        sun.set_actor_label("Sun")
        sun.set_actor_rotation(sun_rot, False)
        log(f"reusing {sun.get_name()}")

    set_mobility(sun, unreal.ComponentMobility.MOVABLE)

    c = get_comp(sun, unreal.DirectionalLightComponent)
    if c is None:
        log_error("Sun has no DirectionalLightComponent")
        return

    # 20000 lux daylight
    c.set_editor_property("intensity", 20000.0)
    c.set_editor_property("light_color", unreal.Color(255, 242, 224, 255))
    c.set_editor_property("use_temperature", True)
    c.set_editor_property("temperature", 6500.0)
    c.set_editor_property("atmosphere_sun_light", True)
    c.set_editor_property("cast_shadows", True)
    log("Sun: Movable, pitch=-45 yaw=40, 20000 lux, atmosphere_sun_light=True")


def setup_atmosphere() -> None:
    atm = find_by_class(unreal.SkyAtmosphere)
    if atm is None:
        atm = spawn(unreal.SkyAtmosphere, unreal.Vector(0.0, 0.0, 0.0), "SkyAtmosphere")
        log("created SkyAtmosphere")
    else:
        atm.set_actor_label("SkyAtmosphere")
        log(f"reusing {atm.get_name()}")


def setup_skylight() -> None:
    sky = find_by_class(unreal.SkyLight)
    if sky is None:
        sky = spawn(unreal.SkyLight, unreal.Vector(0.0, 0.0, 3000.0), "Sky")
        log("created SkyLight 'Sky'")
    else:
        sky.set_actor_label("Sky")
        log(f"reusing {sky.get_name()}")

    set_mobility(sky, unreal.ComponentMobility.MOVABLE)

    c = get_comp(sky, unreal.SkyLightComponent)
    if c is None:
        log_error("Sky has no SkyLightComponent")
        return

    c.set_editor_property("intensity", 1.5)
    c.set_editor_property("real_time_capture", True)
    try:
        c.set_editor_property("source_type", unreal.SkyLightSourceType.SLS_CAPTURED_SCENE)
    except Exception:
        pass
    log("SkyLight: Movable, real_time_capture=True, intensity=1.5")


def setup_exposure() -> None:
    pp = find_by_class(unreal.PostProcessVolume)
    if pp is None:
        pp = spawn(unreal.PostProcessVolume, unreal.Vector(0.0, 0.0, 0.0), "DaylightExposure")
        log("created PostProcessVolume 'DaylightExposure'")
    else:
        pp.set_actor_label("DaylightExposure")
        log(f"reusing {pp.get_name()}")

    pp.set_editor_property("b_unbound", True)
    settings = pp.settings

    # Lock EV100 so brightness does not swing with view direction
    ev = 11.5
    try:
        settings.override_auto_exposure_min_brightness = True
        settings.override_auto_exposure_max_brightness = True
        settings.auto_exposure_min_brightness = ev
        settings.auto_exposure_max_brightness = ev
    except Exception as exc:
        log(f"exposure min/max: {exc}")
    try:
        settings.override_auto_exposure_bias = True
        settings.auto_exposure_bias = 0.0
    except Exception:
        pass

    try:
        settings.override_bloom_intensity = True
        settings.bloom_intensity = 0.3
    except Exception:
        pass

    log(f"DaylightExposure: Unbound=True, EV100 min=max={ev}")


def main() -> None:
    try:
        log("applying verified outdoor lighting ...")
        setup_sun()
        setup_atmosphere()
        setup_skylight()
        setup_exposure()

        if hasattr(unreal.EditorLevelLibrary, "save_current_level"):
            unreal.EditorLevelLibrary.save_current_level()
            log("level saved")
        log("done. Enter Play to verify.")
    except Exception:
        log_error(traceback.format_exc())


if __name__ == "__main__":
    main()
