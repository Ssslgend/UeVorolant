import unreal
from pathlib import Path
root=Path(__file__).resolve().parents[1]
task=unreal.AssetImportTask()
task.filename=str(root/'Saved'/'AscentCombatSimple'/'combat_top.png')
task.destination_path='/Game/CombatTraining/UI'
task.destination_name='T_AscentMap'
task.automated=True;task.save=True;task.replace_existing=False
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
if not unreal.EditorAssetLibrary.does_asset_exist('/Game/CombatTraining/UI/T_AscentMap'):
    source=next((p for p in task.imported_object_paths if unreal.EditorAssetLibrary.does_asset_exist(p)), '/Game/CombatTraining/UI/combat_top')
    if not unreal.EditorAssetLibrary.rename_asset(source,'/Game/CombatTraining/UI/T_AscentMap'):raise RuntimeError('Cannot name minimap')
texture=unreal.EditorAssetLibrary.load_asset('/Game/CombatTraining/UI/T_AscentMap')
texture.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
unreal.EditorAssetLibrary.save_loaded_asset(texture)
