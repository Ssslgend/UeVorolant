"""Reopen delivery files and verify topology, provenance, bounds and FBX units."""
import bpy, json, os, sys, hashlib
from mathutils import Vector

out = os.path.abspath(sys.argv[sys.argv.index('--') + 1])
report = json.load(open(os.path.join(out, 'simplification_report.json'), encoding='utf8'))
expected = {r['source_name']: r for r in report['manifest'] if r['category']}
objects = [o for o in bpy.context.scene.objects if o.type == 'MESH']
assert len(objects) == len(expected)
assert len(bpy.context.scene.objects) == len(objects), 'Unexpected preview helpers in saved scene'
assert bpy.context.scene.unit_settings.scale_length == 1
assert all(o.data.users == 1 for o in objects), 'Shared geometry remains'
assert all(not o.modifiers for o in objects)
assert all(all(abs(x - 1) < 1e-6 for x in o.scale) for o in objects)
assert all(o.get('source_object') in expected for o in objects)
assert sum(len(o.data.polygons) for o in objects) == report['final_polygons']
assert all(not o.hide_render and not o.hide_viewport for o in objects)
assert all(im.packed_file for im in bpy.data.images if im.type == 'IMAGE'), 'Unpacked image dependency'
assert all(max(im.size) <= 1024 for im in bpy.data.images if im.type == 'IMAGE')
assert report['geometry_validation']['errors'] == []

def bounds(obj):
    points = [obj.matrix_world @ v.co for v in obj.data.vertices]
    return [[min(v[i] for v in points) for i in range(3)], [max(v[i] for v in points) for i in range(3)]]

expected_bounds = {o.name: bounds(o) for o in objects}
key_covers = ['StoneBrickCover_0.001', 'Shard_0_Box_49.001', 'Shard_0_Box.005', 'ABombsite_0_TarpCratesUp', 'AScaffold_1_CoverAISMA_1']
key_covers += ['FTBox_0_C_0', 'BoatShop_0_MainBarricade', 'Boat_1_BoxSternedRowboat_55', 'TarpCrate_0_CornerCloth_6']
key_walls = ['Kingdom_0_MidWall', 'Shell_3_MidWallA', 'Kingdom_0_MidRampRailing_3.001',
             'Kingdom_0_MidRampRailing_3.002', 'Pizza_0_RollUpDoor', 'BSite_0_BoathouseBase', 'BoatShop_0_MainWalls', 'BSite_0_BoatHouseChunkA']
key_floors = ['AGRound_5_BombsiteA', 'BombsiteB_0_Ground', 'MidFloor_0', 'MidFloor_2', 'BMainInreriorFloor', 'TreeRoom_0_Ground_125', 'AtkStairs_1_VinoISMA_0', 'AtkStairs_1_VinoISMA_1', 'DefPathAGround_0_Stairs']
assert all(n in expected for n in key_covers + key_floors + key_walls), 'Missing critical combat geometry'
cover_check = {n: expected[n]['bounds'] for n in key_covers}
original_hash = hashlib.sha256()
with open(report['source'], 'rb') as f:
    for block in iter(lambda: f.read(8 * 1024 * 1024), b''): original_hash.update(block)
assert original_hash.hexdigest() == report['source_sha256'], 'Source file changed'

blend_audit = {'mesh_objects': len(objects), 'single_user_meshes': True, 'original_source_unchanged': True,
               'packed_images': len(bpy.data.images), 'polygons': report['final_polygons'],
               'key_cover_bounds_m': cover_check, 'critical_floors_present': key_floors,
               'critical_walls_present': key_walls,
               'geometry_surface_samples': report['geometry_validation']['samples_checked']}

# Test exported units by importing FBX into a fresh scene, not by trusting options.
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=report['fbx'], use_image_search=False)
imported = [o for o in bpy.context.scene.objects if o.type == 'MESH']
assert len(imported) == len(expected_bounds), (len(imported), len(expected_bounds))
max_error = 0.0
for obj in imported:
    assert obj.name in expected_bounds, obj.name
    actual = bounds(obj)
    want = expected_bounds[obj.name]
    max_error = max(max_error, max(abs(actual[a][i] - want[a][i]) for a in range(2) for i in range(3)))
assert max_error < 0.001, 'FBX units or axes changed: max error ' + str(max_error)
assert sum(len(o.data.polygons) for o in imported) == report['final_triangles']
audit = {'blend': blend_audit, 'fbx': {'mesh_objects': len(imported), 'triangles': report['final_triangles'],
          'max_roundtrip_bbox_error_m': max_error, 'units_and_axes_roundtrip_passed': True}, 'passed': True}
with open(os.path.join(out, 'verification_report.json'), 'w', encoding='utf8') as f:
    json.dump(audit, f, ensure_ascii=False, indent=2)
print('[combat-verify]', json.dumps(audit, ensure_ascii=True), flush=True)
