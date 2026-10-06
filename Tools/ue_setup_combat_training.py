"""Import delivered combat geometry and validate source-derived training points."""
import unreal, json, math, itertools, traceback
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Saved' / 'AscentCombatSimple'
FBX = OUT / 'AscentCombatSimple_UE.fbx'
DEST = '/Game/CombatTraining/Map'
MESH = DEST + '/AscentCombat.AscentCombat'
LEVEL = '/Game/CombatTraining/TrainerAscent'
REPORT = OUT / 'ue_training_import_report.json'

def xyz(v): return [float(v.x), float(v.y), float(v.z)]

def main():
    report = {'fbx': str(FBX), 'mesh': MESH, 'level': LEVEL, 'passed': False}
    try:
        editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        if editor.get_game_world() is not None: raise RuntimeError('Stop Play before setup')
        if not unreal.EditorAssetLibrary.does_asset_exist(MESH):
            task = unreal.AssetImportTask()
            task.filename, task.destination_path, task.destination_name = str(FBX), DEST, 'AscentCombat'
            task.automated, task.replace_existing, task.save = True, False, True
            options = unreal.FbxImportUI()
            options.import_mesh, options.import_as_skeletal = True, False
            options.import_materials, options.import_textures, options.import_animations = True, True, False
            data = options.static_mesh_import_data
            data.combine_meshes = True
            data.auto_generate_collision = False
            data.generate_lightmap_u_vs = False
            data.import_uniform_scale = 1.0
            data.transform_vertex_to_absolute = True
            data.bake_pivot_in_vertex = False
            options.static_mesh_import_data = data
            task.options = options
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
            report['imported_objects'] = list(task.imported_object_paths)
        mesh = unreal.EditorAssetLibrary.load_asset(MESH)
        if not isinstance(mesh, unreal.StaticMesh): raise RuntimeError('Map mesh not imported')
        body = mesh.get_editor_property('body_setup')
        if not body: raise RuntimeError('Map has no collision BodySetup')
        body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        body.set_editor_property('double_sided_geometry', True)
        # Retain diffuse/UV markings and make original single-sided shells visible.
        slots = list(mesh.get_editor_property('static_materials'))
        seen_materials = set()
        fallback_count = 0
        for slot in slots:
            material = slot.material_interface
            if not material or not material.get_path_name().startswith(DEST + '/'):
                # Never mutate an engine/shared fallback material.
                name = str(slot.material_slot_name)
                category = next((x for x in ['Floors_Stairs', 'Cover', 'Reference_Edges', 'Doors', 'Site_Markers'] if x in name), 'Walls_Structures')
                fallback_path = DEST + '/M_CombatFallback_' + category
                material = unreal.EditorAssetLibrary.load_asset(fallback_path) if unreal.EditorAssetLibrary.does_asset_exist(fallback_path) else None
                if not material:
                    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_CombatFallback_' + category, DEST, unreal.Material, unreal.MaterialFactoryNew())
                    color = {'Floors_Stairs': (.28, .32, .34), 'Cover': (.45, .32, .21), 'Reference_Edges': (.29, .37, .41), 'Doors': (.20, .30, .32), 'Site_Markers': (.24, .56, .42)}.get(category, (.61, .64, .65))
                    node = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -300, 0)
                    node.set_editor_property('constant', unreal.LinearColor(*color, 1.0))
                    unreal.MaterialEditingLibrary.connect_material_property(node, '', unreal.MaterialProperty.MP_BASE_COLOR)
                slot.material_interface = material
                fallback_count += 1
            if isinstance(material, unreal.Material) and material.get_path_name() not in seen_materials:
                seen_materials.add(material.get_path_name())
                material.set_editor_property('two_sided', True)
                unreal.MaterialEditingLibrary.recompile_material(material)
                unreal.EditorAssetLibrary.save_loaded_asset(material)
        mesh.set_editor_property('static_materials', slots)
        report['project_fallback_material_slots'] = fallback_count
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if unreal.EditorAssetLibrary.does_asset_exist(LEVEL):
            if not les.load_level(LEVEL): raise RuntimeError('Cannot load generated training level')
        elif not les.new_level(LEVEL): raise RuntimeError('Cannot create training level')
        actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        existing = actors.get_all_level_actors()

        def spawn(cls, label, location=(0, 0, 0)):
            obj = next((a for a in existing if a.get_actor_label() == label and isinstance(a, cls)), None)
            if not obj:
                obj = actors.spawn_actor_from_class(cls, unreal.Vector(*location))
                if not obj: raise RuntimeError('Cannot spawn ' + label)
                obj.set_actor_label(label)
                existing.append(obj)
            return obj

        map_actor = spawn(unreal.StaticMeshActor, 'AscentCombat')
        component = map_actor.static_mesh_component
        component.set_static_mesh(mesh)
        component.set_collision_profile_name('BlockAll')
        component.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
        map_actor.set_actor_location(unreal.Vector(0, 0, 0), False, False)
        map_actor.set_actor_scale3d(unreal.Vector(1, 1, 1))
        les.save_current_level()
        # Reload the saved level to rebuild the component physics state after import.
        les.load_level(LEVEL)
        existing = [a for a in actors.get_all_level_actors() if a]
        bounds = mesh.get_bounds()
        report['bounds_origin_cm'], report['bounds_extent_cm'] = xyz(bounds.origin), xyz(bounds.box_extent)
        report['collision'] = str(body.get_editor_property('collision_trace_flag'))
        report['double_sided_collision'] = bool(body.get_editor_property('double_sided_geometry'))
        report['material_slots'] = len(mesh.get_editor_property('static_materials'))

        source = json.loads((OUT / 'simplification_report.json').read_text(encoding='utf8'))
        pieces = [r for r in source['manifest'] if r['category']]
        source_lo = [min(r['bounds'][0][i] for r in pieces) * 100 for i in range(3)]
        source_hi = [max(r['bounds'][1][i] for r in pieces) * 100 for i in range(3)]
        ue_lo = [o - e for o, e in zip(xyz(bounds.origin), xyz(bounds.box_extent))]
        ue_hi = [o + e for o, e in zip(xyz(bounds.origin), xyz(bounds.box_extent))]
        # Detect the actual handedness conversion using asymmetric map bounds.
        matches = []
        for permutation in [(0, 1), (1, 0)]:
            for signs in itertools.product([-1, 1], repeat=2):
                lo = [min(source_lo[permutation[i]] * signs[i], source_hi[permutation[i]] * signs[i]) for i in range(2)] + [source_lo[2]]
                hi = [max(source_lo[permutation[i]] * signs[i], source_hi[permutation[i]] * signs[i]) for i in range(2)] + [source_hi[2]]
                error = max(abs(a - b) for a, b in zip(lo + hi, ue_lo + ue_hi))
                matches.append((error, permutation, signs))
        error, permutation, signs = min(matches)
        if error > 3: raise RuntimeError('Imported map bounds/units mismatch: ' + str(matches))
        report['source_to_ue_xy'] = {'permutation': permutation, 'signs': signs, 'bounds_error_cm': error}
        def to_ue(point): return [point[permutation[i]] * signs[i] * 100 for i in range(2)] + [point[2] * 100]
        world = editor.get_editor_world()
        report['trace_api'] = unreal.SystemLibrary.line_trace_single.__doc__
        report['hit_api'] = str(dir(unreal.HitResult))
        report['physics_api'] = [x for x in dir(body) if any(k in x for k in ['physics', 'collision', 'cook'])]

        def trace(start, end):
            result = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(*start), unreal.Vector(*end),
                unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
            if result is None: return None
            if isinstance(result, unreal.HitResult): hit = result
            else: hit = next((x for x in result if isinstance(x, unreal.HitResult)), None)
            if hit is None: raise RuntimeError('Unexpected trace return: ' + repr(result))
            # FHitResult fields are not reflected Python properties in UE 5.3.
            report['hit_value'] = str(hit)
            broken = hit.to_tuple()
            return broken if broken[0] else None

        source_spots = json.loads((OUT / 'source_training_spots.json').read_text(encoding='utf8'))
        valid, failures = [], []
        for point in source_spots['points']:
            p, e, peek = [to_ue(point[field]) for field in ['player_m', 'enemy_m', 'peek_m']]
            checked = []
            for location in [p, e, peek]:
                hit = trace([*location[:2], location[2] + 35], [*location[:2], location[2] - 60])
                if hit is None:
                    checked.append({'grounded': False, 'authored': location})
                else:
                    impact = xyz(hit[5])
                    checked.append({'grounded': abs(impact[2] - location[2]) < 3, 'authored': location, 'impact': impact})
            eye_p, eye_e, eye_peek = [[*loc[:2], loc[2] + 160] for loc in [p, e, peek]]
            hidden = trace(eye_p, eye_e) is not None
            exposed = trace(eye_peek, eye_e) is None
            record = {'id': point['id'], 'ground_checks': checked, 'hidden_at_start': hidden, 'visible_after_peek': exposed}
            if not all(c['grounded'] for c in checked) or not hidden or not exposed:
                failures.append(record)
                continue
            yaw = math.degrees(math.atan2(e[1] - p[1], e[0] - p[0]))
            valid.append({'id': point['id'], 'name': point['name'], 'region': point['region'], 'type': 'prefire',
                          'player': {'pos': p, 'yaw': yaw}, 'enemy': {'pos': e, 'yaw': yaw + 180, 'stance': 'stand'},
                          'peek': {'pos': peek}, 'bot': {'delayMs': 350, 'fireMode': 'instant_kill', 'losTrigger': True},
                          'difficulty': 1, 'notes': 'Source-derived corner; native cover and ground verified in UE.'})
            report.setdefault('validated_spots', []).append(record)
        report['failed_spots'] = failures
        if not all(any(p['region'] == r for p in valid) for r in ['A', 'B', 'Mid']):
            raise RuntimeError('Ground/LOS validation did not produce all three regions: ' + repr(failures))
        data = {'version': 1, 'generator': 'AscentCombatSimple/1.0',
                'map': {'id': 'ascent', 'displayName': '亚海悬城', 'mesh': MESH, 'sourceToUE': report['source_to_ue_xy']},
                'points': valid, 'settings': {'timeoutMs': 30000, 'resetDelayMs': 1500}}
        (ROOT / 'Content' / 'Data' / 'trainer_combat_spots_v1.json').write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding='utf8')
        sun = spawn(unreal.DirectionalLight, 'TrainerSun')
        sun.set_actor_rotation(unreal.Rotator(pitch=-45, yaw=40, roll=0), False)
        light = sun.get_component_by_class(unreal.DirectionalLightComponent)
        light.set_mobility(unreal.ComponentMobility.MOVABLE)
        light.set_editor_property('intensity', 20000.0)
        light.set_editor_property('atmosphere_sun_light', True)
        light.set_editor_property('light_source_angle', 1.0)
        spawn(unreal.SkyAtmosphere, 'TrainerAtmosphere')
        sky = spawn(unreal.SkyLight, 'TrainerSky').get_component_by_class(unreal.SkyLightComponent)
        sky.set_mobility(unreal.ComponentMobility.MOVABLE)
        sky.set_editor_property('real_time_capture', True)
        sky.set_editor_property('intensity', 1.5)
        pp = spawn(unreal.PostProcessVolume, 'TrainerExposure')
        pp.set_editor_property('unbound', True)
        settings = pp.get_editor_property('settings')
        for prop in ['min_brightness', 'max_brightness']:
            settings.set_editor_property('override_auto_exposure_' + prop, True)
            settings.set_editor_property('auto_exposure_' + prop, 11.5)
        settings.set_editor_property('override_motion_blur_amount', True)
        settings.set_editor_property('motion_blur_amount', 0.0)
        pp.set_editor_property('settings', settings)
        start = spawn(unreal.PlayerStart, 'TrainerPlayerStart')
        floor = valid[0]['player']['pos']
        start.set_actor_location(unreal.Vector(floor[0], floor[1], floor[2] + 95), False, False)
        start.set_actor_rotation(unreal.Rotator(pitch=0, yaw=valid[0]['player']['yaw'], roll=0), False)
        camera = unreal.Vector(18000, -15000, 20000)
        aim = bounds.origin
        delta = aim - camera
        editor.set_level_viewport_camera_info(camera, unreal.Rotator(pitch=math.degrees(math.atan2(delta.z, math.hypot(delta.x, delta.y))), yaw=math.degrees(math.atan2(delta.y, delta.x)), roll=0))
        report['saved_level'] = bool(les.save_current_level())
        report['actor_count'] = len(actors.get_all_level_actors())
        report['passed'] = report['saved_level']
        if not report['passed']: raise RuntimeError('Level save failed')
        unreal.log('[combat-training] imported and validated ' + str(len(valid)) + ' training points')
    except Exception:
        report['error'] = traceback.format_exc()
        unreal.log_error(report['error'])
        raise
    finally:
        REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')

if __name__ == '__main__': main()
