"""Restore exact Blender diffuse/tint links unsupported by FBX material conversion."""
import unreal,json,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Saved'/'AscentCombatSimple'
DEST='/Game/CombatTraining/Map'
def clean(name):return re.sub(r'[^A-Za-z0-9_]', '_', name)
mesh=unreal.EditorAssetLibrary.load_asset(DEST+'/AscentCombat')
manifest=json.loads((OUT/'combat_material_manifest.json').read_text(encoding='utf8'))
rows={clean(row['name']):row for row in manifest}
slots=list(mesh.get_editor_property('static_materials'))
report={'restored':[],'unmatched':[]}
for slot in slots:
    key=clean(str(slot.material_slot_name))
    row=rows.get(key)
    if not row:
        report['unmatched'].append(key);continue
    name='M_Native_'+key
    path=DEST+'/'+name
    mat=unreal.EditorAssetLibrary.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if not mat:mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,DEST,unreal.Material,unreal.MaterialFactoryNew())
    unreal.MaterialEditingLibrary.delete_all_material_expressions(mat)
    mat.set_editor_property('two_sided',True)
    texture=None
    if row['textures']:
        texname=clean(Path(row['textures'][0]).stem)
        texture=unreal.EditorAssetLibrary.load_asset(DEST+'/'+texname)
        if not texture:
            task=unreal.AssetImportTask();task.filename=str(OUT/'Textures'/row['textures'][0]);task.destination_path=DEST;task.destination_name=texname;task.automated=True;task.save=True
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task]);texture=unreal.EditorAssetLibrary.load_asset(DEST+'/'+texname)
    if texture:
        sample=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionTextureSample,-500,0)
        sample.set_editor_property('texture',texture)
        sample.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        tint=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector,-500,200)
        tint.set_editor_property('constant',unreal.LinearColor(*row['tint']))
        multiply=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionMultiply,-250,0)
        unreal.MaterialEditingLibrary.connect_material_expressions(sample,'RGB',multiply,'A')
        unreal.MaterialEditingLibrary.connect_material_expressions(tint,'',multiply,'B')
        unreal.MaterialEditingLibrary.connect_material_property(multiply,'',unreal.MaterialProperty.MP_BASE_COLOR)
    else:
        color=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector,-250,0)
        color.set_editor_property('constant',unreal.LinearColor(*row['color']))
        unreal.MaterialEditingLibrary.connect_material_property(color,'',unreal.MaterialProperty.MP_BASE_COLOR)
    rough=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionConstant,-250,250)
    rough.set_editor_property('r',.88)
    unreal.MaterialEditingLibrary.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.recompile_material(mat);unreal.EditorAssetLibrary.save_loaded_asset(mat)
    slot.material_interface=mat
    report['restored'].append({'slot':key,'texture':texture.get_path_name() if texture else None,'tint':row['tint']})
mesh.set_editor_property('static_materials',slots)
report['saved']=unreal.EditorAssetLibrary.save_loaded_asset(mesh)
(OUT/'ue_diffuse_restore_report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
unreal.log('[combat-diffuse] '+str(len(report['restored']))+' restored / '+str(len(report['unmatched']))+' unmatched')
