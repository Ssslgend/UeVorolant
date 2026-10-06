"""Reimport restored source geometry while preserving the training level/lighting."""
import unreal, json, traceback
from pathlib import Path
root=Path(__file__).resolve().parents[1]
out=root/'Saved/AscentCombatSimple'
report={'passed':False}
try:
    les=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert les.load_level('/Game/CombatTraining/TrainerAscent')
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    def actor_state():
        def values(a):
            p,r,s=a.get_actor_location(),a.get_actor_rotation(),a.get_actor_scale3d()
            return (a.get_actor_label(),a.get_class().get_name(),p.x,p.y,p.z,r.pitch,r.yaw,r.roll,s.x,s.y,s.z)
        return sorted(values(a) for a in actors.get_all_level_actors())
    before=actor_state()
    task=unreal.AssetImportTask()
    task.filename=str(out/'AscentCombatSimple_UE.fbx')
    task.destination_path='/Game/CombatTraining/Map'
    task.destination_name='AscentCombat'
    task.automated=task.replace_existing=task.replace_existing_settings=task.save=True
    options=unreal.FbxImportUI()
    options.import_mesh=True
    options.import_as_skeletal=options.import_materials=options.import_textures=options.import_animations=False
    options.static_mesh_import_data.combine_meshes=True
    options.static_mesh_import_data.auto_generate_collision=False
    options.static_mesh_import_data.generate_lightmap_u_vs=False
    options.static_mesh_import_data.transform_vertex_to_absolute=True
    options.static_mesh_import_data.import_uniform_scale=1.0
    task.options=options
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh=unreal.EditorAssetLibrary.load_asset('/Game/CombatTraining/Map/AscentCombat')
    body=mesh.get_editor_property('body_setup')
    body.set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    body.set_editor_property('double_sided_geometry',True)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    # Restore source diffuse slots, including newly retained source objects.
    script=root/'Tools/ue_restore_combat_diffuse.py'
    exec(compile(script.read_text(encoding='utf8'),str(script),'exec'),{'__file__':str(script)})
    task=unreal.AssetImportTask()
    task.filename=str(out/'combat_top.png')
    task.destination_path='/Game/CombatTraining/UI'
    task.destination_name='T_AscentMap'
    task.automated=task.replace_existing=task.save=True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture=unreal.EditorAssetLibrary.load_asset('/Game/CombatTraining/UI/T_AscentMap')
    texture.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
    assert unreal.EditorAssetLibrary.save_loaded_asset(texture)
    after=actor_state()
    assert before==after, 'Level composition changed during map reimport'
    report.update(passed=True,actors_preserved=True,actors=len(after),
                  imported_mesh=mesh.get_path_name(),material_slots=len(mesh.get_editor_property('static_materials')),
                  complex_collision=True,double_sided_collision=True)
except Exception:
    report['error']=traceback.format_exc()
    raise
finally:
    (out/'ue_cover_update_report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
