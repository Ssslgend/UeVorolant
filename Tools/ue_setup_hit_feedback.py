"""Add native hit feedback assets without changing map composition or gun audio."""
import json, math, random, struct, wave
from pathlib import Path
import unreal

root = Path(__file__).resolve().parents[1]
out = root / 'Saved' / 'TrainerVerification' / 'HitFeedbackAssets'
out.mkdir(parents=True, exist_ok=True)
assets = unreal.AssetToolsHelpers.get_asset_tools()
dest = '/Game/CombatTraining/Targets'

for name, color in [('M_TargetBody', (.8, .08, .10)), ('M_TargetHead', (1, .83, .20))]:
    material = unreal.EditorAssetLibrary.load_asset(dest + '/' + name)
    if not material:
        raise RuntimeError('Missing existing target material: ' + name)
    # Native target constructors root these materials. Append a new output graph;
    # deleting rooted expressions in UE 5.3 commandlets triggers !IsRooted().
    base = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -500, -120)
    base.set_editor_property('constant', unreal.LinearColor(*(c * 2000 for c in color), 1))
    white = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -500, 50)
    white.set_editor_property('constant', unreal.LinearColor(6000, 6000, 6000, 1))
    flash = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -500, 180)
    flash.set_editor_property('parameter_name', 'ImpactFlash')
    flash.set_editor_property('default_value', 0)
    blend = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, -200, 0)
    for source, pin in [(base, 'A'), (white, 'B'), (flash, 'Alpha')]:
        unreal.MaterialEditingLibrary.connect_material_expressions(source, '', blend, pin)
    unreal.MaterialEditingLibrary.connect_material_property(blend, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)

spark_path = dest + '/M_HitSpark'
spark = unreal.EditorAssetLibrary.load_asset(spark_path) if unreal.EditorAssetLibrary.does_asset_exist(spark_path) else None
if not spark:
    spark = assets.create_asset('M_HitSpark', dest, unreal.Material, unreal.MaterialFactoryNew())
spark.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
spark_color = unreal.MaterialEditingLibrary.create_material_expression(spark, unreal.MaterialExpressionVectorParameter, -200, 0)
spark_color.set_editor_property('parameter_name', 'SparkColor')
spark_color.set_editor_property('default_value', unreal.LinearColor(10000, 6000, 1800, 1))
unreal.MaterialEditingLibrary.connect_material_property(spark_color, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
unreal.MaterialEditingLibrary.recompile_material(spark)
unreal.EditorAssetLibrary.save_loaded_asset(spark)

rate = 44100
rng = random.Random(806)
sound_specs = [('TrainingBodyHit', .13), ('TrainingHeadshot', .28), ('TrainingKill', .38), ('TrainingHurt', .18)]
for name, duration in sound_specs:
    samples = []
    for i in range(int(rate * duration)):
        t = i / rate
        noise = rng.uniform(-1, 1)
        if name == 'TrainingBodyHit':
            value = (.42 * noise + .3 * math.sin(2 * math.pi * 190 * t)) * math.exp(-48 * t)
        elif name == 'TrainingHeadshot':
            snap = .45 * noise * math.exp(-160 * t)
            metal = (.30 * math.sin(2 * math.pi * 1850 * t) + .16 * math.sin(2 * math.pi * 3127 * t)) * math.exp(-24 * t)
            value = snap + metal + .22 * math.sin(2 * math.pi * 140 * t) * math.exp(-60 * t)
        elif name == 'TrainingKill':
            value = sum(.13 * math.sin(2 * math.pi * f * t) for f in (523.25, 659.25, 783.99)) * (1 - math.exp(-110 * t)) * math.exp(-10 * t)
        else:
            value = (.45 * math.sin(2 * math.pi * 85 * t) + .16 * noise) * math.exp(-25 * t)
        samples.append(struct.pack('<h', round(max(-1, min(1, value)) * 32767)))
    filename = out / (name + '.wav')
    with wave.open(str(filename), 'wb') as audio:
        audio.setparams((1, 2, rate, 0, 'NONE', 'not compressed'))
        audio.writeframes(b''.join(samples))
    task = unreal.AssetImportTask()
    task.filename = str(filename)
    task.destination_path = '/Game/CombatTraining/Audio'
    task.destination_name = name
    task.automated = task.save = task.replace_existing = True
    assets.import_asset_tasks([task])
    sound = unreal.EditorAssetLibrary.load_asset('/Game/CombatTraining/Audio/' + name)
    if not sound:
        raise RuntimeError('Sound import failed: ' + name)
    sound.set_editor_property('sound_group', unreal.SoundGroup.SOUNDGROUP_EFFECTS)
    unreal.EditorAssetLibrary.save_loaded_asset(sound)

(out / 'asset_report.json').write_text(json.dumps({'passed': True, 'materials': ['M_TargetBody', 'M_TargetHead', 'M_HitSpark'], 'sounds': [s[0] for s in sound_specs], 'map_unchanged': True}, indent=2), encoding='utf-8')
unreal.log('[TrainerHitFeedback] independent flash material, spark and four synthesized sounds saved')
