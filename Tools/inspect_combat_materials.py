import bpy, json, os, sys
out = sys.argv[sys.argv.index('--') + 1]
names = ['StoneBrickCover_0.001', 'Shard_0_Box_0', 'Shell_5_MidTrimA.001', 'CourtyardWall_1_TrimPieceA.001', 'StoneThin_0.001']
report = {'images_count': len(bpy.data.images), 'materials_count': len(bpy.data.materials), 'objects': {}}
sys.path.insert(0, os.path.dirname(__file__))
from combat_ascent_policy import classify
base = os.path.dirname(out)
rows = json.load(open(os.path.join(base, 'source_inventory.json'), encoding='utf8'))['objects']
vis = {r['name']: r for r in json.load(open(os.path.join(base, 'source_visibility.json'), encoding='utf8'))['objects']}
report['modifiers'] = {r['name']: [{'name': m.name, 'type': m.type} for m in bpy.data.objects[r['name']].modifiers] for r in rows if classify(r, vis[r['name']])[0] and bpy.data.objects[r['name']].modifiers}
for name in names:
    o = bpy.data.objects.get(name)
    mats = []
    for slot in o.material_slots:
        m = slot.material
        nodes = []
        if m and m.node_tree:
            for n in m.node_tree.nodes:
                d = {'name': n.name, 'type': n.type}
                if n.type == 'TEX_IMAGE' and n.image:
                    d.update(image=n.image.name, size=list(n.image.size), path=n.image.filepath, packed=bool(n.image.packed_file))
                if n.type == 'GROUP': d['group'] = n.node_tree.name if n.node_tree else None
                nodes.append(d)
        mats.append({'name': m.name if m else None, 'nodes': nodes})
    report['objects'][name] = mats
with open(out, 'w', encoding='utf8') as f: json.dump(report, f, ensure_ascii=False, indent=2)
print(json.dumps(report, ensure_ascii=True), flush=True)
