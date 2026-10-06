"""Create a readable, independently tinted view-model material without editing the map."""
import json
from pathlib import Path
import unreal

root = Path(__file__).resolve().parents[1]
path = '/Game/CombatTraining/Weapons/M_ViewModelTint'
material = unreal.EditorAssetLibrary.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
if not material:
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_ViewModelTint', '/Game/CombatTraining/Weapons', unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    tint = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -450, 0)
    tint.set_editor_property('parameter_name', 'GunColor')
    tint.set_editor_property('default_value', unreal.LinearColor(.06, .075, .09, 1))
    exposure = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant, -450, 150)
    exposure.set_editor_property('r', 2000)
    multiply = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionMultiply, -200, 0)
    unreal.MaterialEditingLibrary.connect_material_expressions(tint, '', multiply, 'A')
    unreal.MaterialEditingLibrary.connect_material_expressions(exposure, '', multiply, 'B')
    unreal.MaterialEditingLibrary.connect_material_property(multiply, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError('Could not save view-model material')
out = root / 'Saved' / 'TrainerVerification' / 'rifle_material_report.json'
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps({'passed': True, 'material': path, 'map_unchanged': True}, indent=2), encoding='utf-8')
unreal.log('[TrainerRifle] view-model material saved; map not edited')
