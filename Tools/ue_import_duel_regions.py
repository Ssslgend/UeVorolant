import json
import os
import re

import unreal


PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
FBX_DIR = os.path.join(PROJECT_ROOT, "Saved", "AscentDuelExport", "FBX")
EXPORT_REPORT_PATH = os.path.join(PROJECT_ROOT, "Saved", "AscentDuelExport", "export_report.json")
REPORT_PATH = os.path.join(PROJECT_ROOT, "Saved", "AscentDuelExport", "ue_import_report.json")
DESTINATION_ROOT = "/Game/AscentDuelRegions"

REGIONS = [
    "A_Site_Duel",
    "B_Site_Duel",
    "Mid_Duel",
    "A_Main_Entry",
    "B_Main_Entry",
    "Market_Door_Duel",
]


def make_import_options():
    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_materials", True)
    options.set_editor_property("import_textures", True)
    options.set_editor_property("import_animations", False)
    options.set_editor_property("import_as_skeletal", False)

    static_mesh_options = unreal.FbxStaticMeshImportData()
    static_mesh_options.set_editor_property("combine_meshes", True)
    static_mesh_options.set_editor_property("generate_lightmap_u_vs", True)
    static_mesh_options.set_editor_property("auto_generate_collision", True)
    static_mesh_options.set_editor_property("import_mesh_lo_ds", False)
    static_mesh_options.set_editor_property("import_translation", unreal.Vector(0, 0, 0))
    static_mesh_options.set_editor_property("import_rotation", unreal.Rotator(0, 0, 0))
    static_mesh_options.set_editor_property("import_uniform_scale", 1.0)

    options.set_editor_property("static_mesh_import_data", static_mesh_options)
    return options


def package_path(asset_path):
    return asset_path.split(".", 1)[0]


def safe_name(value):
    return re.sub(r"[^A-Za-z0-9_.-]+", "_", value).strip("._") or "unnamed"


def import_region(asset_tools, region_name):
    fbx_path = os.path.join(FBX_DIR, f"{region_name}.fbx")
    if not os.path.exists(fbx_path):
        raise RuntimeError(f"Missing FBX: {fbx_path}")

    destination_path = f"{DESTINATION_ROOT}/{region_name}"
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", fbx_path)
    task.set_editor_property("destination_path", destination_path)
    task.set_editor_property("destination_name", region_name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", make_import_options())

    asset_tools.import_asset_tasks([task])
    imported_paths = list(task.get_editor_property("imported_object_paths"))
    return {
        "region": region_name,
        "fbx_path": fbx_path,
        "fbx_size_bytes": os.path.getsize(fbx_path),
        "destination_path": destination_path,
        "imported_object_paths": imported_paths,
    }


def import_texture(asset_tools, source_path, destination_path):
    if not source_path or not os.path.exists(source_path):
        return ""

    unreal.EditorAssetLibrary.make_directory(destination_path)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source_path)
    task.set_editor_property("destination_path", destination_path)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", unreal.TextureFactory())

    asset_tools.import_asset_tasks([task])
    imported_paths = list(task.get_editor_property("imported_object_paths"))
    return imported_paths[0] if imported_paths else ""


def configure_texture(texture_asset_path, usage):
    asset = unreal.EditorAssetLibrary.load_asset(package_path(texture_asset_path))
    if not asset or asset.get_class().get_name() != "Texture2D":
        return False

    if usage == "normal":
        asset.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        asset.set_editor_property("srgb", False)
    elif usage == "packed":
        asset.set_editor_property("srgb", False)

    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    return True


def import_region_textures(asset_tools, region_name, region_report):
    imported = []
    for material in region_report.get("all_materials", []):
        destination_path = f"{DESTINATION_ROOT}/{region_name}/Textures/{safe_name(material.get('name', 'Material'))}"
        for texture in material.get("exported_textures", []):
            imported_path = import_texture(asset_tools, texture.get("exported_path", ""), destination_path)
            if imported_path:
                configure_texture(imported_path, texture.get("usage", "unknown"))
            imported.append(
                {
                    "material": material.get("name", ""),
                    "usage": texture.get("usage", "unknown"),
                    "source_path": texture.get("source_path", ""),
                    "exported_path": texture.get("exported_path", ""),
                    "imported_path": imported_path,
                }
            )
    return imported


def assets_under_region(region_name):
    return unreal.EditorAssetLibrary.list_assets(f"{DESTINATION_ROOT}/{region_name}", recursive=True, include_folder=False)


def material_assets_by_name(region_name):
    materials = {}
    for path in assets_under_region(region_name):
        asset = unreal.EditorAssetLibrary.load_asset(package_path(path))
        if asset and asset.get_class().get_name() == "Material":
            name = asset.get_name()
            materials[name.lower()] = path
            materials[safe_name(name).lower()] = path
    return materials


def imported_textures_by_material(imported_textures):
    result = {}
    for item in imported_textures:
        if item.get("imported_path"):
            result.setdefault(item.get("material", ""), []).append(item)
    return result


def first_texture_for_usage(textures, usage):
    for item in textures:
        if item.get("usage") == usage and item.get("imported_path"):
            return item["imported_path"]
    return ""


def attach_textures_to_materials(region_name, region_report, imported_textures):
    materials = material_assets_by_name(region_name)
    textures_by_material = imported_textures_by_material(imported_textures)
    attached = []

    for material in region_report.get("all_materials", []):
        material_name = material.get("name", "")
        material_path = materials.get(material_name.lower()) or materials.get(safe_name(material_name).lower())
        if not material_path:
            continue

        texture_items = textures_by_material.get(material_name, [])
        base_color_path = first_texture_for_usage(texture_items, "base_color")
        normal_path = first_texture_for_usage(texture_items, "normal")
        if not base_color_path and not normal_path:
            continue

        material_asset = unreal.EditorAssetLibrary.load_asset(package_path(material_path))
        if not material_asset:
            continue

        try:
            unreal.MaterialEditingLibrary.delete_all_material_expressions(material_asset)
        except Exception:
            pass

        x = -600
        y = -120
        if base_color_path:
            texture_asset = unreal.EditorAssetLibrary.load_asset(package_path(base_color_path))
            if texture_asset:
                node = unreal.MaterialEditingLibrary.create_material_expression(
                    material_asset, unreal.MaterialExpressionTextureSample, x, y
                )
                node.set_editor_property("texture", texture_asset)
                unreal.MaterialEditingLibrary.connect_material_property(
                    node, "RGB", unreal.MaterialProperty.MP_BASE_COLOR
                )
                attached.append({"material": material_name, "usage": "base_color", "texture": base_color_path})
                y += 240

        if normal_path:
            texture_asset = unreal.EditorAssetLibrary.load_asset(package_path(normal_path))
            if texture_asset:
                node = unreal.MaterialEditingLibrary.create_material_expression(
                    material_asset, unreal.MaterialExpressionTextureSample, x, y
                )
                node.set_editor_property("texture", texture_asset)
                node.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
                unreal.MaterialEditingLibrary.connect_material_property(node, "RGB", unreal.MaterialProperty.MP_NORMAL)
                attached.append({"material": material_name, "usage": "normal", "texture": normal_path})

        unreal.MaterialEditingLibrary.recompile_material(material_asset)
        unreal.EditorAssetLibrary.save_loaded_asset(material_asset)

    return attached


def static_mesh_summary(asset_path):
    asset = unreal.EditorAssetLibrary.load_asset(package_path(asset_path))
    if not asset:
        return {"asset_path": asset_path, "exists": False}

    bounds = asset.get_bounds()
    static_materials = list(asset.get_editor_property("static_materials"))
    body_setup = asset.get_editor_property("body_setup")
    aggregate_geom = body_setup.get_editor_property("agg_geom") if body_setup else None
    collision_count = 0
    if aggregate_geom:
        for prop in ["box_elems", "sphere_elems", "sphyl_elems", "convex_elems", "tapered_capsule_elems"]:
            collision_count += len(getattr(aggregate_geom, prop, []))

    return {
        "asset_path": asset_path,
        "exists": True,
        "class": asset.get_class().get_name(),
        "bounds_origin": [bounds.origin.x, bounds.origin.y, bounds.origin.z],
        "bounds_box_extent": [bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z],
        "collision_primitive_count": collision_count,
        "material_slot_count": len(static_materials),
        "material_slots_with_assets": sum(1 for item in static_materials if item.material_interface),
    }


def imported_class_counts(imported_paths):
    counts = {}
    for path in imported_paths:
        asset = unreal.EditorAssetLibrary.load_asset(package_path(path))
        if not asset:
            continue
        class_name = asset.get_class().get_name()
        counts[class_name] = counts.get(class_name, 0) + 1
    return counts


def main():
    os.makedirs(os.path.dirname(REPORT_PATH), exist_ok=True)
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    imported = []
    with open(EXPORT_REPORT_PATH, "r", encoding="utf-8") as handle:
        export_report = json.load(handle)
    export_regions = {item["name"]: item for item in export_report.get("regions", [])}

    for region_name in REGIONS:
        unreal.log(f"[AscentDuelImport] Importing {region_name}")
        result = import_region(asset_tools, region_name)
        region_report = export_regions.get(region_name, {})
        result["imported_textures"] = import_region_textures(asset_tools, region_name, region_report)
        result["attached_texture_references"] = attach_textures_to_materials(
            region_name, region_report, result["imported_textures"]
        )
        result["asset_summaries"] = [static_mesh_summary(path) for path in result["imported_object_paths"]]
        result["imported_class_counts"] = imported_class_counts(assets_under_region(region_name))
        imported.append(result)

    unreal.EditorAssetLibrary.save_directory(DESTINATION_ROOT, only_if_is_dirty=False, recursive=True)

    report = {
        "destination_root": DESTINATION_ROOT,
        "fbx_dir": FBX_DIR,
        "regions": imported,
    }
    with open(REPORT_PATH, "w", encoding="utf-8") as handle:
        json.dump(report, handle, ensure_ascii=False, indent=2)

    unreal.log(f"[AscentDuelImport] Wrote report: {REPORT_PATH}")


main()

