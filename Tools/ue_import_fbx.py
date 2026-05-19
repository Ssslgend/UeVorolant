"""
UE5 Python Auto Import Script
在 UE 编辑器的 Python 控制台里直接粘贴运行即可

功能：自动导入 FBX 地图到 Content/Ascent 目录
设置：Scale=1.0, 合并Mesh, 生成光照UV
"""

import unreal
import os

# ============ 配置区 ============
FBX_PATH = r"H:\baiduyun\亚海悬城_UE.fbx"
IMPORT_DIR = "/Game/Ascent"  # UE 内容路径
# ================================

def import_fbx():
    if not os.path.exists(FBX_PATH):
        unreal.log_error(f"FBX file not found: {FBX_PATH}")
        return

    # 创建目标文件夹
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

    # 配置导入参数
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', FBX_PATH)
    task.set_editor_property('destination_path', IMPORT_DIR)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('automated', True)  # 不弹窗
    task.set_editor_property('save', True)

    # FBX 导入选项
    fbx_options = unreal.FbxImportUI()

    # === 通用设置 ===
    fbx_options.set_editor_property('import_mesh', True)
    fbx_options.set_editor_property('import_materials', True)
    fbx_options.set_editor_property('import_textures', True)
    fbx_options.set_editor_property('import_animations', False)
    fbx_options.set_editor_property('import_rigid_mesh', True)

    # === 缩放设置（最关键！）===
    fbx_options.set_editor_property('import_uniform_scale', 1.0)

    # === Mesh 设置 ===
    mesh_options = unreal.FbxMeshImportData()
    mesh_options.set_editor_property('combine_meshes', True)  # 合并所有Mesh
    mesh_options.set_editor_property('generate_lightmap_u_vs', True)
    mesh_options.set_editor_property('auto_generate_collision', True)

    # === Static Mesh 导入设置 ===
    static_mesh_options = unreal.FbxStaticMeshImportData()
    static_mesh_options.set_editor_property('combine_meshes', True)
    static_mesh_options.set_editor_property('generate_lightmap_u_vs', True)
    static_mesh_options.set_editor_property('auto_generate_collision', True)
    static_mesh_options.set_editor_property('import_normals', True)
    static_mesh_options.set_editor_property('import_tangents', True)

    fbx_options.set_editor_property('static_mesh_import_data', static_mesh_options)
    fbx_options.set_editor_property('mesh_import_data', static_mesh_options)

    task.set_editor_property('options', fbx_options)

    # 执行导入
    unreal.log(f"Starting FBX import: {FBX_PATH}")
    unreal.log(f"Destination: {IMPORT_DIR}")

    result = asset_tools.import_asset_tasks([task])

    if result:
        unreal.log("=== FBX IMPORT SUCCESSFUL! ===")
        unreal.log(f"Assets imported to: {IMPORT_DIR}")

        # 列出导入的资产
        asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()
        assets = asset_registry.get_assets_by_path(IMPORT_DIR, recursive=True)
        unreal.log(f"Total assets imported: {len(assets)}")
        for i, asset in enumerate(assets[:20]):  # 只显示前20个
            unreal.log(f"  [{i+1}] {asset.asset_name} ({asset.asset_class_path.asset_name})")
        if len(assets) > 20:
            unreal.log(f"  ... and {len(assets) - 20} more")
    else:
        unreal.log_error("FBX import failed!")

# 运行！
import_fbx()
