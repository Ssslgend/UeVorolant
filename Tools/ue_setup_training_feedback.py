import unreal, math, random, struct, wave
from pathlib import Path
root=Path(__file__).resolve().parents[1]
out=root/'Saved'/'TrainerVerification'/'Assets';out.mkdir(parents=True,exist_ok=True)
dest='/Game/CombatTraining/Targets'
for name,color in [('M_TargetBody',(.8,.08,.10)),('M_TargetHead',(1,.83,.20))]:
    path=dest+'/'+name
    material=unreal.EditorAssetLibrary.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if not material:material=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,dest,unreal.Material,unreal.MaterialFactoryNew())
    material.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    node=unreal.MaterialEditingLibrary.get_material_property_input_node(material,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if not isinstance(node,unreal.MaterialExpressionConstant3Vector):
        node=unreal.MaterialEditingLibrary.create_material_expression(material,unreal.MaterialExpressionConstant3Vector,-250,0)
    # Unlit color still passes through exposure. Keep training targets readable at daylight EV.
    node.set_editor_property('constant',unreal.LinearColor(*(channel*2000 for channel in color),1))
    unreal.MaterialEditingLibrary.connect_material_property(node,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(material);unreal.EditorAssetLibrary.save_loaded_asset(material)
random.seed(41)
for name,duration in [('TrainingShot',.12),('TrainingHit',.10)]:
    filename=out/(name+'.wav');rate=44100;samples=[]
    for i in range(int(rate*duration)):
        t=i/rate
        if name=='TrainingShot':value=(random.uniform(-1,1)*.55+math.sin(2*math.pi*110*t)*.25)*math.exp(-45*t)
        else:value=math.sin(2*math.pi*1300*t)*.35*math.exp(-40*t)
        samples.append(struct.pack('<h',int(max(-1,min(1,value))*32767)))
    with wave.open(str(filename),'wb') as audio:audio.setparams((1,2,rate,0,'NONE','not compressed'));audio.writeframes(b''.join(samples))
    task=unreal.AssetImportTask();task.filename=str(filename);task.destination_path='/Game/CombatTraining/Audio';task.destination_name=name;task.automated=True;task.save=True;task.replace_existing=True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
les=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
les.load_level('/Game/CombatTraining/TrainerAscent')
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if isinstance(actor,unreal.SkyLight):
        c=actor.get_component_by_class(unreal.SkyLightComponent)
        c.set_editor_property('real_time_capture',False)
        c.set_editor_property('source_type',unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
        cubemap=unreal.load_object(None,'/Engine/MapTemplates/Sky/DaylightAmbientCubemap.DaylightAmbientCubemap')
        if not cubemap:raise RuntimeError('Missing ambient cubemap')
        c.set_editor_property('cubemap',cubemap)
        # Specified HDR cubemap is normalized radiance. Match the fixed daylight EV 11.5.
        c.set_editor_property('intensity',2000.0)
if not les.save_current_level():raise RuntimeError('Could not save training sky')
unreal.log('[TrainerFeedback] targets, sounds and quality-independent sky fill saved')
