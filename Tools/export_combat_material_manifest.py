import bpy,json
from pathlib import Path
root=Path(bpy.data.filepath).parent
materials=[]
for m in bpy.data.materials:
    row={'name':m.name,'color':list(m.diffuse_color),'textures':[],'tint':[1,1,1,1]}
    if m.node_tree:
        for n in m.node_tree.nodes:
            if n.type=='TEX_IMAGE' and n.image: row['textures'].append(Path(bpy.path.abspath(n.image.filepath)).name)
            if n.type=='MIX_RGB' and n.blend_type=='MULTIPLY':row['tint']=list(n.inputs[2].default_value)
    materials.append(row)
(root/'combat_material_manifest.json').write_text(json.dumps(materials,ensure_ascii=False,indent=2),encoding='utf8')
print('Exported',len(materials),'native diffuse materials')
