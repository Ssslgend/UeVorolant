"""Create a source-derived combat-only scene; never overwrite the source blend."""
import bpy, bmesh, json, os, sys, math, hashlib, collections
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree
sys.path.insert(0, os.path.dirname(__file__))
from combat_ascent_policy import classify, region_of

OUT = os.path.abspath(sys.argv[sys.argv.index('--') + 1])
SOURCE = bpy.data.filepath
BLEND = os.path.join(OUT, 'AscentCombatSimple.blend')
FBX = os.path.join(OUT, 'AscentCombatSimple_UE.fbx')
assert os.path.normcase(SOURCE) != os.path.normcase(BLEND)
os.makedirs(OUT, exist_ok=True)

def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for block in iter(lambda: f.read(8 * 1024 * 1024), b''): h.update(block)
    return h.hexdigest()

source_hash = sha256(SOURCE)
inventory = json.load(open(os.path.join(OUT, 'source_inventory.json'), encoding='utf8'))
visibility = {r['name']: r for r in json.load(open(os.path.join(OUT, 'source_visibility.json'), encoding='utf8'))['objects']}
rows = inventory['objects']
scene = bpy.context.scene
root = bpy.data.collections.new('AscentCombatSimple')
scene.collection.children.link(root)
groups = {}
colors = {'Walls_Structures': (0.61, 0.64, 0.65, 1), 'Floors_Stairs': (0.28, 0.32, 0.34, 1),
          'Cover': (0.45, 0.32, 0.21, 1), 'Reference_Edges': (0.29, 0.37, 0.41, 1),
          'Doors': (0.20, 0.30, 0.32, 1), 'Site_Markers': (0.24, 0.56, 0.42, 1)}
materials = {}
images = {}
texture_dir = os.path.join(OUT, 'Textures')
os.makedirs(texture_dir, exist_ok=True)

def simple_material(old, category):
    # Retain original diffuse/UV markings on walls, boxes, and reference edges.
    # Remove costly layered shaders, normal maps, and roughness/metallic maps.
    key = (old.name if old else 'None', category)
    if key in materials: return materials[key]
    mat = bpy.data.materials.new('Combat_' + category + '_' + key[0])
    mat.use_nodes = True
    mat.diffuse_color = colors[category]
    bsdf = mat.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = colors[category]
    bsdf.inputs['Roughness'].default_value = 0.88
    if category not in {'Floors_Stairs', 'Site_Markers'} and old and old.node_tree:
        src = next((n for n in old.node_tree.nodes if n.type == 'TEX_IMAGE' and n.image and
                    ('_DF' in n.image.name.upper() or 'DIFFUSE' in n.image.name.upper())), None)
        if src and src.image.size[0] and src.image.size[1]:
            img = images.get(src.image.name)
            if not img:
                img = src.image.copy()
                img.name = 'Combat_' + src.image.name
                width, height = img.size
                if max(width, height) > 1024:
                    ratio = 1024 / max(width, height)
                    img.scale(max(1, round(width * ratio)), max(1, round(height * ratio)))
                filename = bpy.path.clean_name(src.image.name).removesuffix('.png') + '.png'
                img.filepath_raw = os.path.join(texture_dir, filename)
                img.file_format = 'PNG'
                img.save()
                with open(img.filepath_raw, 'rb') as texture_file:
                    encoded = texture_file.read()
                img.pack(data=encoded, data_len=len(encoded))
                img.filepath = '//Textures/' + filename
                images[src.image.name] = img
            tex = mat.node_tree.nodes.new('ShaderNodeTexImage')
            tex.image = img
            mat.node_tree.links.new(tex.outputs['Color'], bsdf.inputs['Base Color'])
            tint_node = old.node_tree.nodes.get('diffusecolor')
            if tint_node and tint_node.type == 'RGB':
                tint = tint_node.outputs[0].default_value
                if any(abs(tint[i] - 1) > 0.01 for i in range(3)):
                    multiply = mat.node_tree.nodes.new('ShaderNodeMixRGB')
                    multiply.blend_type = 'MULTIPLY'
                    multiply.inputs[0].default_value = 1
                    multiply.inputs[2].default_value = tint
                    mat.node_tree.links.new(tex.outputs['Color'], multiply.inputs[1])
                    mat.node_tree.links.new(multiply.outputs[0], bsdf.inputs['Base Color'])
    materials[key] = mat
    return mat

def bounds(mesh):
    return [[min(v.co[i] for v in mesh.vertices) for i in range(3)],
            [max(v.co[i] for v in mesh.vertices) for i in range(3)]]

def bvh(mesh):
    return BVHTree.FromPolygons([v.co[:] for v in mesh.vertices], [list(p.vertices) for p in mesh.polygons])

manifest, retained, errors, planar_fallbacks = [], [], [], []
samples_checked, max_surface_error, max_bound_error = 0, 0.0, 0.0
for index, row in enumerate(rows):
    category, reason = classify(row, visibility[row['name']])
    entry = {'source_name': row['name'], 'category': category, 'reason': reason,
             'source_polygons': row['polygons'], 'region': region_of(row)}
    manifest.append(entry)
    if not category: continue
    obj = bpy.data.objects[row['name']]
    assert all(m.type == 'NODES' and m.name == 'Smooth by Angle' for m in obj.modifiers), 'Source has geometric modifiers: ' + obj.name
    mesh = obj.data.copy()
    mesh.transform(obj.matrix_world)
    if obj.matrix_world.determinant() < 0: mesh.flip_normals()
    mesh.update()
    before_bounds = bounds(mesh)
    before_tree = bvh(mesh)
    # Probe original triangle centers on every retained object, including walls.
    mesh.calc_loop_triangles()
    tris = list(mesh.loop_triangles)
    step = max(1, len(tris) // 32)
    probes = []
    for tri in tris[::step]:
        a, b, c = [mesh.vertices[i].co.copy() for i in tri.vertices]
        normal = (b - a).cross(c - a)
        if normal.length < 1e-8: continue
        normal.normalize()
        probes.append(((a + b + c) / 3, normal))
    bm = bmesh.new()
    bm.from_mesh(mesh)
    # Only dissolve coplanar internal edges. No ratio decimation or vertex moves.
    bmesh.ops.dissolve_limit(bm, angle_limit=0.00001, use_dissolve_boundaries=False,
                            verts=list(bm.verts), edges=list(bm.edges), delimit={'MATERIAL', 'NORMAL'})
    bm.to_mesh(mesh)
    bm.free()
    mesh.update()
    after_bounds = bounds(mesh)
    bound_error = max(abs(before_bounds[a][i] - after_bounds[a][i]) for a in range(2) for i in range(3))
    after_tree = bvh(mesh)
    local_error = 0.0
    local_checked = 0
    error_start = len(errors)
    for point, normal in probes:
        origin = point + normal * 0.01
        hit0 = before_tree.ray_cast(origin, -normal, 0.025)
        hit1 = after_tree.ray_cast(origin, -normal, 0.025)
        if hit0[0] is None: continue
        if hit1[0] is None:
            errors.append({'name': obj.name, 'error': 'surface disappeared', 'point': list(point)})
            continue
        error = (hit0[0] - hit1[0]).length
        local_error = max(local_error, error)
        local_checked += 1
    if bound_error > 0.0001 or local_error > 0.0001:
        errors.append({'name': obj.name, 'bbox_error': bound_error, 'surface_error': local_error})
    if len(errors) > error_start:
        # Some source pieces contain overlapping faces. Keep their exact mesh
        # instead of accepting a changed surface after planar dissolution.
        planar_fallbacks.append({'name': obj.name, 'reason': errors[error_start:]})
        errors[error_start:] = []
        bpy.data.meshes.remove(mesh)
        mesh = obj.data.copy()
        mesh.transform(obj.matrix_world)
        if obj.matrix_world.determinant() < 0: mesh.flip_normals()
        mesh.update()
        after_bounds = bounds(mesh)
        bound_error = local_error = 0.0
        local_checked = 0
    max_surface_error = max(max_surface_error, local_error)
    max_bound_error = max(max_bound_error, bound_error)
    samples_checked += local_checked
    # Preserve all source material slots, so coplanar UV/material boundaries survive.
    old_materials = [slot.material for slot in obj.material_slots]
    mesh.materials.clear()
    for old in old_materials or [None]: mesh.materials.append(simple_material(old, category))
    new = bpy.data.objects.new('Combat__' + obj.name, mesh)
    key = (entry['region'], category)
    if key not in groups:
        region_col = bpy.data.collections.get('Combat_' + entry['region'])
        if not region_col:
            region_col = bpy.data.collections.new('Combat_' + entry['region'])
            root.children.link(region_col)
        col = bpy.data.collections.new(entry['region'] + '__' + category)
        region_col.children.link(col)
        groups[key] = col
    groups[key].objects.link(new)
    new['source_object'] = row['name']
    new['combat_category'] = category
    new['source_geometry_unchanged'] = True
    new.color = colors[category]
    new.hide_render = False
    new.hide_viewport = False
    for p in mesh.polygons: p.use_smooth = False
    entry.update(final_polygons=len(mesh.polygons), bounds=after_bounds,
                 surface_samples=local_checked, max_surface_error=local_error,
                 max_bbox_error=bound_error)
    retained.append(new)
    if len(retained) % 100 == 0: print('[combat-build] kept', len(retained), flush=True)

assert not errors, json.dumps(errors[:10])
print('[combat-build] geometry checks passed; removing unused source data', flush=True)
keep_names = {o.name for o in retained}
bpy.data.batch_remove(ids=[obj for obj in bpy.data.objects if obj.name not in keep_names])
keep_cols = {root.name} | {c.name for c in root.children} | {c.name for c in groups.values()}
bpy.data.batch_remove(ids=[col for col in bpy.data.collections if col.name not in keep_cols])
for data in list(bpy.data.materials): data.use_fake_user = False
for data in list(bpy.data.images): data.use_fake_user = False
for data in list(bpy.data.node_groups): data.use_fake_user = False
bpy.data.orphans_purge(do_local_ids=True, do_linked_ids=True, do_recursive=True)
print('[combat-build] unused data removed; saving files', flush=True)
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1.0
scene['source_file'] = SOURCE
scene['source_sha256'] = source_hash
scene['simplification'] = 'Original combat geometry; decorations/vistas removed; coplanar dissolve only.'
scene['head_height_reference'] = 'Original wall bands, box edges and diffuse markings. No invented headshot lines.'
scene.render.engine = 'BLENDER_WORKBENCH'
scene.render.resolution_x = 1600
scene.render.resolution_y = 1200
scene.render.resolution_percentage = 100
shade = scene.display.shading
shade.light = 'STUDIO'
shade.color_type = 'MATERIAL'
shade.show_shadows = True
shade.show_cavity = True
shade.cavity_type = 'BOTH'
shade.show_specular_highlight = False
shade.background_type = 'WORLD'
scene.world.color = (0.045, 0.052, 0.058)
target = Vector((23, 49, 4))
for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type == 'VIEW_3D':
            space = area.spaces.active
            space.shading.type = 'SOLID'
            space.shading.color_type = 'MATERIAL'
            space.clip_end = 2000
            space.region_3d.view_location = target
            space.region_3d.view_distance = 170
            space.region_3d.view_rotation = (Vector((175, -110, 180)) - target).to_track_quat('Z', 'Y')
            space.overlay.show_overlays = False
bpy.ops.object.select_all(action='DESELECT')
bpy.ops.wm.save_as_mainfile(filepath=BLEND, compress=True)
for obj in retained: obj.select_set(True)
bpy.context.view_layer.objects.active = retained[0]
bpy.ops.export_scene.fbx(filepath=FBX, use_selection=True, object_types={'MESH'},
                         global_scale=1.0, apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE',
                         axis_forward='-Y', axis_up='Z', bake_space_transform=True,
                         use_mesh_modifiers=True, use_triangles=True, add_leaf_bones=False,
                         bake_anim=False, path_mode='COPY', embed_textures=True)
for obj in retained: obj.select_set(False)
polys_before = sum(r['polygons'] for r in rows)
polys_after = sum(len(o.data.polygons) for o in retained)
triangles = 0
for obj in retained:
    obj.data.calc_loop_triangles()
    triangles += len(obj.data.loop_triangles)
report = {'source': SOURCE, 'source_sha256': source_hash, 'source_unchanged': sha256(SOURCE) == source_hash,
          'blend': BLEND, 'fbx': FBX, 'source_mesh_objects': len(rows), 'final_mesh_objects': len(retained),
          'source_polygons': polys_before, 'retained_source_polygons': sum(x['source_polygons'] for x in manifest if x['category']),
          'final_polygons': polys_after, 'final_triangles': triangles,
          'polygon_reduction_percent': (1 - polys_after / polys_before) * 100,
          'kept_categories': dict(collections.Counter(x['category'] for x in manifest if x['category'])),
          'removed_reasons': dict(collections.Counter(x['reason'] for x in manifest if not x['category'])),
          'geometry_validation': {'samples_checked': samples_checked, 'max_surface_error_m': max_surface_error,
                                  'max_bbox_error_m': max_bound_error, 'errors': errors, 'planar_fallbacks': planar_fallbacks},
          'diffuse_textures': len(images), 'texture_max_size': 1024,
          'units': '1 Blender coordinate unit = 1 meter; FBX vertices converted to centimeters',
          'source_units_metadata': 'Original scene scale_length 0.01 corrected without moving geometry',
          'manifest': manifest}
with open(os.path.join(OUT, 'simplification_report.json'), 'w', encoding='utf8') as f:
    json.dump(report, f, ensure_ascii=False, indent=2)

# Presentation helpers exist only after saving/exporting; they are absent from the asset.
camera_data = bpy.data.cameras.new('TemporaryPreviewCamera')
camera = bpy.data.objects.new('TemporaryPreviewCamera', camera_data)
scene.collection.objects.link(camera)
scene.camera = camera
camera_data.type = 'ORTHO'
views = {'combat_top': ((14.5, 45.7, 220), (14.5, 45.7, 3), 194),
         'combat_overview': ((175, -110, 180), (23, 49, 3), 190),
         'combat_a_site': ((89, 44, 90), (62, 69, 5), 70),
         'combat_b_site': ((-76, 40, 50), (-30, 78, 1), 59),
         'combat_mid': ((65, 8, 49), (19, 49, 3), 74),
         'combat_front': ((23, -250, 9), (23, 49, 9), 165),
         'combat_back': ((23, 320, 9), (23, 49, 9), 165),
         'combat_left': ((-250, 49, 9), (23, 49, 9), 165),
         'combat_right': ((320, 49, 9), (23, 49, 9), 165),
         'combat_bottom': ((23, 49, -220), (23, 49, 3), 165)}
for name, (location, aim, scale) in views.items():
    camera.location = location
    camera.rotation_euler = (Vector(aim) - camera.location).to_track_quat('-Z', 'Y').to_euler()
    camera_data.ortho_scale = scale
    scene.render.filepath = os.path.join(OUT, name + '.png')
    bpy.ops.render.render(write_still=True)
    print('[combat-preview]', name, flush=True)
print('[combat-build] complete:', len(retained), 'objects;', polys_after, 'polygons;', triangles, 'triangles', flush=True)
