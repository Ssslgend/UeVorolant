"""
Blender CLI FBX Export Script
用法: blender.exe -b "亚海悬城.blend" -P export_fbx.py

导出设置:
- Scale=100 (米→厘米)
- Apply Scalings=FBX All
- Apply Transform=True
- Forward=-Y, Up=Z
"""

import bpy
import sys
import os

# 解析命令行参数（-- 后面的参数）
argv = sys.argv
if "--" in argv:
    argv = argv[argv.index("--") + 1:]
else:
    argv = []

# 默认输出路径
blend_path = bpy.data.filepath
default_output = os.path.splitext(blend_path)[0] + "_UE.fbx"

output_path = argv[0] if argv else default_output

print(f"[Export] Source: {blend_path}")
print(f"[Export] Output: {output_path}")

# 选中所有物体
bpy.ops.object.select_all(action='SELECT')

# 导出 FBX
bpy.ops.export_scene.fbx(
    filepath=output_path,
    use_selection=False,          # 导出全部
    apply_unit_scale=True,
    apply_scale_options='FBX_SCALE_ALL',  # 关键：Apply Scalings = FBX All
    use_space_transform=True,
    axis_forward='-Y',
    axis_up='Z',
    bake_space_transform=True,    # 应用变换
    mesh_smooth_type='FACE',
    use_mesh_modifiers=True,
    use_armature_deform_only=True,
    add_leaf_bones=False,
    path_mode='COPY',
    embed_textures=False,
)

print(f"[Export] Done! FBX exported to: {output_path}")
