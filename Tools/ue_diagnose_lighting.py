"""Read the open editor world's lighting state; does not change assets."""
import json
from pathlib import Path
import unreal

OUT = Path(__file__).resolve().parents[1] / 'Saved' / 'AscentPlayable' / 'lighting_diagnostic.json'

def props(obj, names):
    result = {}
    for name in names:
        try:
            result[name] = str(obj.get_editor_property(name))
        except Exception as exc:
            result[name] = 'unavailable: ' + str(exc)
    return result

def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    report = {'world': world.get_path_name(), 'actors': []}
    for a in actors:
        row = {'label': a.get_actor_label(), 'class': a.get_class().get_name(),
               'location': str(a.get_actor_location()), 'rotation': str(a.get_actor_rotation()),
               'scale': str(a.get_actor_scale3d())}
        for cls in (unreal.DirectionalLightComponent, unreal.SkyLightComponent):
            c = a.get_component_by_class(cls)
            if c:
                row['light'] = props(c, ['intensity', 'mobility', 'visible', 'affects_world',
                    'source_type', 'cubemap', 'real_time_capture', 'atmosphere_sun_light',
                    'lower_hemisphere_is_black', 'light_color'])
        c = a.get_component_by_class(unreal.StaticMeshComponent)
        if c:
            row['mesh'] = str(c.get_editor_property('static_mesh'))
            row['bounds'] = str(a.get_actor_bounds(False))
            row['material_samples'] = [str(c.get_material(i)) for i in range(min(5, c.get_num_materials()))]
        report['actors'].append(row)
    try:
        report['camera'] = str(unreal.EditorLevelLibrary.get_level_viewport_camera_info())
    except Exception as exc:
        report['camera'] = str(exc)
    OUT.write_text(json.dumps(report, indent=2), encoding='utf-8')
    unreal.log('[lighting_diagnostic] ' + str(OUT))

if __name__ == '__main__':
    main()
