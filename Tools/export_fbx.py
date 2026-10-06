"""
Blender CLI FBX Export Script
Usage: blender.exe -b "map.blend" -P export_fbx.py -- "out.fbx"

Export settings:
- Scale=100 (m -> cm)
- Apply Scalings=FBX All
- Apply Transform=True
- Forward=-Y, Up=Z

Must run under blender -b (background). Do NOT call UI-context operators
(e.g. bpy.ops.object.select_all) here - their poll() fails without a window.
"""

import bpy
import sys
import os

# Parse args after "--"
argv = sys.argv
if "--" in argv:
    argv = argv[argv.index("--") + 1:]
else:
    argv = []

blend_path = bpy.data.filepath
default_output = os.path.splitext(blend_path)[0] + "_UE.fbx"
output_path = argv[0] if argv else default_output

print(f"[Export] Source: {blend_path}")
print(f"[Export] Output: {output_path}")

# Resolve output dir; create it if missing
out_dir = os.path.dirname(output_path)
if out_dir and not os.path.isdir(out_dir):
    os.makedirs(out_dir, exist_ok=True)

try:
    bpy.ops.export_scene.fbx(
        filepath=output_path,
        use_selection=False,          # export all objects
        apply_unit_scale=True,
        apply_scale_options='FBX_SCALE_ALL',  # Apply Scalings = FBX All
        use_space_transform=True,
        axis_forward='-Y',
        axis_up='Z',
        bake_space_transform=True,    # apply transforms
        mesh_smooth_type='FACE',
        use_mesh_modifiers=True,
        use_armature_deform_only=True,
        add_leaf_bones=False,
        path_mode='COPY',
        embed_textures=False,
    )
except Exception as exc:
    print(f"[Export] FAILED: {exc}", file=sys.stderr)
    sys.exit(1)

if not os.path.isfile(output_path) or os.path.getsize(output_path) == 0:
    print(f"[Export] FAILED: output missing or empty: {output_path}", file=sys.stderr)
    sys.exit(1)

print(f"[Export] Done! FBX exported to: {output_path} ({os.path.getsize(output_path)} bytes)")
sys.exit(0)
