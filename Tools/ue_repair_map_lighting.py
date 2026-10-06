"""Set up daylight in the open AscentPlayableMap, then save it.

Run with runpy.run_path(..., run_name='__main__') in the editor Python console.
Rotator uses named arguments deliberately: positional arguments are roll/pitch/yaw.
"""
import json
import math
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]

def main():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_editor_world()
    if world.get_path_name() != '/Game/AscentPlayableMap.AscentPlayableMap':
        raise RuntimeError('Open AscentPlayableMap before running this script')
    if editor.get_game_world() is not None:
        raise RuntimeError('Stop Play/Simulate before changing editor lighting')
    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = sub.get_all_level_actors()

    def actor(cls, label):
        existing = next((a for a in actors if isinstance(a, cls)), None)
        if existing:
            return existing
        created = sub.spawn_actor_from_class(cls, unreal.Vector(0, 0, 0))
        if created is None:
            raise RuntimeError('Could not create ' + label)
        created.set_actor_label(label)
        actors.append(created)
        return created

    sun = actor(unreal.DirectionalLight, 'Sun')
    sun.set_actor_rotation(unreal.Rotator(pitch=-45, yaw=40, roll=0), False)
    sun.set_actor_scale3d(unreal.Vector(1, 1, 1))
    light = sun.get_component_by_class(unreal.DirectionalLightComponent)
    light.set_mobility(unreal.ComponentMobility.MOVABLE)
    light.set_editor_property('intensity', 20000.0)
    light.set_editor_property('atmosphere_sun_light', True)
    light.set_editor_property('light_source_angle', 1.0)
    light.set_editor_property('cast_shadows', True)
    light.set_editor_property('light_color', unreal.Color(255, 247, 235, 255))

    atmosphere = actor(unreal.SkyAtmosphere, 'SkyAtmosphere')
    sky = actor(unreal.SkyLight, 'Sky')
    skylight = sky.get_component_by_class(unreal.SkyLightComponent)
    skylight.set_mobility(unreal.ComponentMobility.MOVABLE)
    skylight.set_editor_property('source_type', unreal.SkyLightSourceType.SLS_CAPTURED_SCENE)
    skylight.set_editor_property('real_time_capture', True)
    skylight.set_editor_property('intensity', 1.5)

    pp = actor(unreal.PostProcessVolume, 'DaylightExposure')
    pp.set_editor_property('unbound', True)
    settings = pp.get_editor_property('settings')
    settings.set_editor_property('override_auto_exposure_method', True)
    settings.set_editor_property('auto_exposure_method', unreal.AutoExposureMethod.AEM_HISTOGRAM)
    settings.set_editor_property('override_auto_exposure_min_brightness', True)
    settings.set_editor_property('override_auto_exposure_max_brightness', True)
    settings.set_editor_property('auto_exposure_min_brightness', 11.5)
    settings.set_editor_property('auto_exposure_max_brightness', 11.5)
    settings.set_editor_property('override_auto_exposure_bias', True)
    settings.set_editor_property('auto_exposure_bias', 0.0)
    pp.set_editor_property('settings', settings)

    camera = unreal.Vector(-19000, -11000, 21000)
    target = unreal.Vector(-5500, 3000, 7500)
    delta = target - camera
    rotation = unreal.Rotator(
        pitch=math.degrees(math.atan2(delta.z, math.hypot(delta.x, delta.y))),
        yaw=math.degrees(math.atan2(delta.y, delta.x)), roll=0)
    editor.set_level_viewport_camera_info(camera, rotation)
    sub.set_selected_level_actors([])
    saved = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    report = {'world': world.get_path_name(), 'saved': saved,
              'sun_rotation': str(sun.get_actor_rotation()), 'sun_intensity_lux': 20000,
              'sky_real_time_capture': True, 'sky_intensity': 1.5, 'exposure_ev100': 11.5,
              'actors': [a.get_actor_label() for a in sub.get_all_level_actors()]}
    (ROOT / 'Saved' / 'AscentPlayable' / 'lighting_repair_report.json').write_text(
        json.dumps(report, indent=2), encoding='utf-8')
    if not saved:
        raise RuntimeError('Lighting configured but level save failed')
    unreal.log('[lighting_repair] Daylight configured and AscentPlayableMap saved')

if __name__ == '__main__':
    main()
