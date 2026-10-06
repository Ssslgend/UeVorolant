"""Import the whole Ascent playable map FBX as ONE StaticMesh asset.

Run headless:
    UnrealEditor-Cmd.exe ascent1.uproject -run=pythonscript \
        -script="<this file>" -unattended -nop4 -nosplash -nullrhi -stdout

Produces:
    /Game/Ascent/AscentPlayable.AscentPlayable
    <project>/Saved/AscentPlayable/ue_import_report.json

Design notes
    * Uses unreal.AssetImportTask with combine_meshes=True so the 1636 source
      meshes collapse into a single StaticMesh.
    * Falls back to combine_meshes=False only if the combined import raises or
      yields no StaticMesh at all; every attempt is recorded in the report.
    * Mesh unit is guessed from the measured bounds:
          max magnitude <= 3000  -> meters       (suggested actor scale 100)
          max magnitude >  3000  -> centimeters  (suggested actor scale 1)
      3000 is the geometric midpoint of the two reference magnitudes the task
      specifies (+-300 for meters, +-30000 for centimeters).
    * Read-only w.r.t. Source/, Config/ and the .uproject: this script only
      creates assets under /Game/Ascent and writes JSON into Saved/.
"""

import json
import os
import time
import traceback

import unreal


PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
FBX_PATH = os.path.join(PROJECT_ROOT, "Saved", "AscentPlayable", "AscentPlayable_UE.fbx")
REPORT_PATH = os.path.join(PROJECT_ROOT, "Saved", "AscentPlayable", "ue_import_report.json")

DESTINATION_PATH = "/Game/Ascent"
DESTINATION_NAME = "AscentPlayable"
MAIN_ASSET_PATH = "{0}/{1}.{1}".format(DESTINATION_PATH, DESTINATION_NAME)

# Cross-check target: an already imported duel-region asset. The Blender source of
# A_Site_Duel is centred at [-6.7, -23.5, 69.6] (metres, saved/AscentDuelExport).
REGION_PROBE_ASSET = "/Game/AscentDuelRegions/A_Site_Duel/A_Site_Duel.A_Site_Duel"
REGION_PROBE_EXPECTED_CENTER = [-6.7, -23.5, 69.6]

UNIT_DECISION_THRESHOLD = 3000.0

TAG = "[AscentPlayableImport]"


def log(message):
    unreal.log("{0} {1}".format(TAG, message))


def log_warning(message):
    unreal.log_warning("{0} {1}".format(TAG, message))


def package_path(asset_path):
    return asset_path.split(".", 1)[0]


def build_import_options(combine_meshes):
    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    # Materials are imported so the merged mesh keeps its real material slots.
    # Textures are not: every texture path recorded by the Blender export is
    # missing on disk, so importing them would only add noise.
    options.set_editor_property("import_materials", True)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("import_animations", False)
    options.set_editor_property("import_as_skeletal", False)

    static_mesh_options = unreal.FbxStaticMeshImportData()
    static_mesh_options.set_editor_property("combine_meshes", bool(combine_meshes))
    static_mesh_options.set_editor_property("auto_generate_collision", True)
    static_mesh_options.set_editor_property("generate_lightmap_u_vs", True)
    static_mesh_options.set_editor_property("import_mesh_lo_ds", False)
    static_mesh_options.set_editor_property("import_translation", unreal.Vector(0.0, 0.0, 0.0))
    static_mesh_options.set_editor_property("import_rotation", unreal.Rotator(0.0, 0.0, 0.0))
    # FBX was exported from Blender with global_scale=100 + apply_unit_scale, so
    # no extra user scale is applied on the UE side.
    static_mesh_options.set_editor_property("import_uniform_scale", 1.0)

    options.set_editor_property("static_mesh_import_data", static_mesh_options)
    return options


def asset_class_name(asset_path):
    """Class name of an asset without loading it (asset registry first)."""
    try:
        data = unreal.EditorAssetLibrary.find_asset_data(asset_path)
    except Exception as exc:
        log_warning("find_asset_data failed for {0}: {1}".format(asset_path, exc))
        return ""
    if not data:
        return ""
    try:
        class_path = data.get_editor_property("asset_class_path")
        name = getattr(class_path, "asset_name", None)
        if name:
            return str(name)
    except Exception:
        pass
    try:
        return str(data.asset_class)
    except Exception:
        return ""


def static_meshes_under(path):
    found = []
    try:
        paths = unreal.EditorAssetLibrary.list_assets(path, recursive=True, include_folder=False)
    except Exception as exc:
        log_warning("list_assets failed for {0}: {1}".format(path, exc))
        return found
    for candidate in paths:
        if asset_class_name(candidate) == "StaticMesh":
            found.append(candidate)
    return found


def run_import_attempt(asset_tools, combine_meshes, attempt_index):
    record = {
        "attempt": attempt_index,
        "combine_meshes": bool(combine_meshes),
        "started_at": time.strftime("%Y-%m-%d %H:%M:%S"),
        "error": "",
        "imported_object_paths": [],
        "static_mesh_paths": [],
        "static_mesh_count": 0,
    }
    started = time.time()
    log("attempt {0}: combine_meshes={1}".format(attempt_index, combine_meshes))
    try:
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", FBX_PATH)
        task.set_editor_property("destination_path", DESTINATION_PATH)
        task.set_editor_property("destination_name", DESTINATION_NAME)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        task.set_editor_property("options", build_import_options(combine_meshes))
        asset_tools.import_asset_tasks([task])
        record["imported_object_paths"] = [str(p) for p in task.get_editor_property("imported_object_paths")]
    except Exception:
        record["error"] = traceback.format_exc()
        log_warning("attempt {0} raised".format(attempt_index))

    record["static_mesh_paths"] = [
        p for p in record["imported_object_paths"] if asset_class_name(p) == "StaticMesh"
    ]
    # The importer may also have written StaticMeshes that are not surfaced in
    # imported_object_paths; the destination folder is the ground truth.
    folder_meshes = static_meshes_under(DESTINATION_PATH)
    record["static_meshes_in_destination"] = folder_meshes
    for path in folder_meshes:
        if path not in record["static_mesh_paths"]:
            record["static_mesh_paths"].append(path)
    record["static_mesh_count"] = len(record["static_mesh_paths"])
    record["duration_seconds"] = round(time.time() - started, 2)
    record["expected_asset_present"] = package_path(MAIN_ASSET_PATH) in [
        package_path(p) for p in record["static_mesh_paths"]
    ]
    log(
        "attempt {0} done in {1}s: imported={2} static_meshes={3} expected_present={4} error={5}".format(
            attempt_index,
            record["duration_seconds"],
            len(record["imported_object_paths"]),
            record["static_mesh_count"],
            record["expected_asset_present"],
            bool(record["error"]),
        )
    )
    return record


def mesh_triangle_count(mesh):
    try:
        return int(mesh.get_num_triangles(0))
    except Exception:
        return -1


def mesh_lod_count(mesh):
    """LOD count. Only the (deprecated) EditorStaticMeshLibrary exposes it to Python in
    this 5.3 build; it returns -1 while the mesh has no built render data, which is
    always the case in a headless -nullrhi run."""
    try:
        return int(unreal.EditorStaticMeshLibrary.get_lod_count(mesh))
    except Exception:
        return -1


def pick_main_mesh(static_mesh_paths):
    """Prefer the exact expected path, else the heaviest candidate."""
    expected_pkg = package_path(MAIN_ASSET_PATH).lower()
    for path in static_mesh_paths:
        if package_path(path).lower() == expected_pkg:
            return path, "exact_expected_path"
    best_path = ""
    best_tris = -1
    for path in static_mesh_paths[:25]:
        mesh = unreal.EditorAssetLibrary.load_asset(package_path(path))
        if not mesh:
            continue
        tris = mesh_triangle_count(mesh)
        if tris > best_tris:
            best_tris = tris
            best_path = path
    if best_path:
        return best_path, "largest_of_{0}_candidates".format(min(len(static_mesh_paths), 25))
    return (static_mesh_paths[0] if static_mesh_paths else ""), "first_available"


def set_complex_collision(mesh):
    """collision_trace_flag = CTF_USE_COMPLEX_AS_SIMPLE on the mesh BodySetup."""
    result = {"body_setup_found": False, "body_setup_created": False, "flag_before": "", "flag_after": "", "error": ""}
    try:
        body_setup = mesh.get_editor_property("body_setup")
        if body_setup:
            result["body_setup_found"] = True
        else:
            try:
                body_setup = unreal.new_object(unreal.BodySetup, mesh)
                mesh.set_editor_property("body_setup", body_setup)
                result["body_setup_created"] = True
            except Exception as exc:
                result["error"] = "could not create BodySetup: {0}".format(exc)
                body_setup = None
        if not body_setup:
            return result
        try:
            result["flag_before"] = str(body_setup.get_editor_property("collision_trace_flag"))
        except Exception:
            pass
        body_setup.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        result["flag_after"] = str(body_setup.get_editor_property("collision_trace_flag"))
    except Exception:
        result["error"] = traceback.format_exc()
    return result


def collision_primitive_count(mesh):
    total = 0
    try:
        body_setup = mesh.get_editor_property("body_setup")
        agg = body_setup.get_editor_property("agg_geom") if body_setup else None
        if agg:
            for prop in ["box_elems", "sphere_elems", "sphyl_elems", "convex_elems", "tapered_capsule_elems"]:
                total += len(getattr(agg, prop, []))
    except Exception:
        pass
    return total


def mesh_summary(asset_path):
    summary = {
        "asset_path": asset_path,
        "exists": False,
        "class": "",
        "bounds_origin": None,
        "bounds_box_extent": None,
        "material_slot_count": 0,
        "material_slots_with_assets": 0,
        "collision_trace_flag": "",
        "collision_primitive_count": 0,
        "lod_count": 0,
        "triangle_count": -1,
        "unit_guess": "",
        "suggested_actor_scale": None,
    }
    mesh = unreal.EditorAssetLibrary.load_asset(package_path(asset_path))
    if not mesh:
        return summary

    summary["exists"] = True
    summary["class"] = mesh.get_class().get_name()
    bounds = mesh.get_bounds()
    origin = [round(float(bounds.origin.x), 3), round(float(bounds.origin.y), 3), round(float(bounds.origin.z), 3)]
    extent = [
        round(float(bounds.box_extent.x), 3),
        round(float(bounds.box_extent.y), 3),
        round(float(bounds.box_extent.z), 3),
    ]
    summary["bounds_origin"] = origin
    summary["bounds_box_extent"] = extent
    static_materials = list(mesh.get_editor_property("static_materials"))
    summary["material_slot_count"] = len(static_materials)
    summary["material_slots_with_assets"] = sum(1 for item in static_materials if item.material_interface)
    body_setup = mesh.get_editor_property("body_setup")
    if body_setup:
        summary["collision_trace_flag"] = str(body_setup.get_editor_property("collision_trace_flag"))
    summary["collision_primitive_count"] = collision_primitive_count(mesh)
    summary["lod_count"] = mesh_lod_count(mesh)
    summary["triangle_count"] = mesh_triangle_count(mesh)

    magnitudes = [abs(v) for v in origin + extent if v is not None]
    reference = max(magnitudes) if magnitudes else 0.0
    summary["max_abs_bound_magnitude"] = round(reference, 3)
    if reference <= UNIT_DECISION_THRESHOLD:
        summary["unit_guess"] = "meters"
        summary["suggested_actor_scale"] = 100
    else:
        summary["unit_guess"] = "centimeters"
        summary["suggested_actor_scale"] = 1
    return summary


def probe_region_asset():
    """Calibrate against an already-imported region whose Blender centre is known."""
    probe = {
        "asset_path": REGION_PROBE_ASSET,
        "blender_center_meters": REGION_PROBE_EXPECTED_CENTER,
        "exists": False,
        "bounds_origin": None,
        "bounds_box_extent": None,
        "implied_unit": "",
    }
    mesh = unreal.EditorAssetLibrary.load_asset(package_path(REGION_PROBE_ASSET))
    if not mesh:
        return probe
    probe["exists"] = True
    bounds = mesh.get_bounds()
    probe["bounds_origin"] = [
        round(float(bounds.origin.x), 3),
        round(float(bounds.origin.y), 3),
        round(float(bounds.origin.z), 3),
    ]
    probe["bounds_box_extent"] = [
        round(float(bounds.box_extent.x), 3),
        round(float(bounds.box_extent.y), 3),
        round(float(bounds.box_extent.z), 3),
    ]
    ratio = float(bounds.origin.z) / REGION_PROBE_EXPECTED_CENTER[2] if REGION_PROBE_EXPECTED_CENTER[2] else 0.0
    probe["origin_z_ratio_to_blender_meters"] = round(ratio, 4)
    if 0.5 <= ratio <= 2.0:
        probe["implied_unit"] = "meters"
    elif 50.0 <= ratio <= 200.0:
        probe["implied_unit"] = "centimeters"
    else:
        probe["implied_unit"] = "unexpected"
    return probe


def main():
    started = time.time()
    os.makedirs(os.path.dirname(REPORT_PATH), exist_ok=True)

    log("FBX={0} exists={1} size={2}".format(FBX_PATH, os.path.exists(FBX_PATH), os.path.getsize(FBX_PATH) if os.path.exists(FBX_PATH) else 0))
    log("target asset={0}".format(MAIN_ASSET_PATH))

    report = {
        "asset_path": MAIN_ASSET_PATH,
        "class": "",
        "bounds_origin": None,
        "bounds_box_extent": None,
        "material_slot_count": 0,
        "collision_trace_flag": "",
        "import_succeeded": False,
        "unit_guess": "",
        "suggested_actor_scale": None,
        "notes": "",
        "fbx_path": FBX_PATH,
        "fbx_size_bytes": os.path.getsize(FBX_PATH) if os.path.exists(FBX_PATH) else 0,
        "destination_path": DESTINATION_PATH,
        "destination_name": DESTINATION_NAME,
        "import_uniform_scale": 1.0,
        "combine_meshes_requested": True,
        "combine_meshes_used": None,
        "attempts": [],
        "region_calibration": {},
        "main_mesh_selection": "",
        "generated_at": time.strftime("%Y-%m-%d %H:%M:%S"),
    }

    if not os.path.exists(FBX_PATH):
        report["notes"] = "FBX not found: {0}".format(FBX_PATH)
        with open(REPORT_PATH, "w", encoding="utf-8") as handle:
            json.dump(report, handle, ensure_ascii=False, indent=2)
        log_warning("FBX missing, aborting")
        return

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

    attempt = run_import_attempt(asset_tools, True, 1)
    report["attempts"].append(attempt)
    used_attempt = attempt

    if (not attempt["static_mesh_paths"]) or attempt["error"]:
        log_warning("combine_meshes=True did not produce a StaticMesh, retrying with combine_meshes=False")
        fallback = run_import_attempt(asset_tools, False, 2)
        report["attempts"].append(fallback)
        if fallback["static_mesh_paths"]:
            used_attempt = fallback

    report["combine_meshes_used"] = used_attempt["combine_meshes"]
    static_mesh_paths = used_attempt["static_mesh_paths"]

    if not static_mesh_paths:
        report["notes"] = "No StaticMesh asset was produced by any import attempt. See attempts[].error."
        with open(REPORT_PATH, "w", encoding="utf-8") as handle:
            json.dump(report, handle, ensure_ascii=False, indent=2)
        log_warning("import failed: no StaticMesh produced")
        return

    main_path, selection_reason = pick_main_mesh(static_mesh_paths)
    report["main_mesh_selection"] = selection_reason
    report["static_mesh_paths"] = static_mesh_paths

    mesh = unreal.EditorAssetLibrary.load_asset(package_path(main_path))
    if not mesh:
        report["notes"] = "Main mesh could not be loaded: {0}".format(main_path)
        with open(REPORT_PATH, "w", encoding="utf-8") as handle:
            json.dump(report, handle, ensure_ascii=False, indent=2)
        log_warning("main mesh load failed")
        return

    collision = set_complex_collision(mesh)
    report["collision_setup"] = collision
    try:
        unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)
    except Exception:
        log_warning("save_loaded_asset failed: {0}".format(traceback.format_exc()))

    # Persist the containing folder as well (material assets created by the import).
    try:
        unreal.EditorAssetLibrary.save_directory(DESTINATION_PATH, only_if_is_dirty=False, recursive=True)
    except Exception as exc:
        log_warning("save_directory failed: {0}".format(exc))

    summary = mesh_summary(main_path)
    report.update(
        {
            "asset_path": main_path,
            "class": summary["class"],
            "bounds_origin": summary["bounds_origin"],
            "bounds_box_extent": summary["bounds_box_extent"],
            "material_slot_count": summary["material_slot_count"],
            "material_slots_with_assets": summary["material_slots_with_assets"],
            "collision_trace_flag": summary["collision_trace_flag"],
            "collision_primitive_count": summary["collision_primitive_count"],
            "lod_count": summary["lod_count"],
            "triangle_count": summary["triangle_count"],
            "import_succeeded": bool(summary["exists"] and summary["class"] == "StaticMesh"),
            "unit_guess": summary["unit_guess"],
            "suggested_actor_scale": summary["suggested_actor_scale"],
            "max_abs_bound_magnitude": summary.get("max_abs_bound_magnitude"),
            "unit_decision_threshold": UNIT_DECISION_THRESHOLD,
            "expected_asset_path": MAIN_ASSET_PATH,
            "expected_asset_matches_main": package_path(main_path).lower() == package_path(MAIN_ASSET_PATH).lower(),
            "unreal_units_per_meter": 100.0,
            "region_calibration": probe_region_asset(),
            "folder_static_mesh_count": len(static_mesh_paths),
            "duration_seconds": round(time.time() - started, 2),
        }
    )
    report["asset_exists_after_import"] = bool(unreal.EditorAssetLibrary.does_asset_exist(package_path(main_path)))

    notes = []
    notes.append("combine_meshes={0} requested, used={1}".format(True, report["combine_meshes_used"]))
    if report["combine_meshes_used"] and len(static_mesh_paths) == 1:
        notes.append("single StaticMesh produced as required")
    else:
        notes.append(
            "{0} StaticMesh assets exist under {1}; main mesh selected by '{2}'".format(
                len(static_mesh_paths), DESTINATION_PATH, selection_reason
            )
        )
    if not report["expected_asset_matches_main"]:
        notes.append("main mesh path differs from the requested target {0}".format(MAIN_ASSET_PATH))
    if attempt["error"]:
        notes.append("first attempt raised; see attempts[0].error")
    if collision.get("error"):
        notes.append("collision flag issue: {0}".format(collision["error"]))
    if "CTF_USE_COMPLEX_AS_SIMPLE" not in (summary["collision_trace_flag"] or ""):
        notes.append("collision_trace_flag is NOT CTF_USE_COMPLEX_AS_SIMPLE")
    notes.append("unit inferred from max |bounds| = {0} (threshold {1})".format(
        summary.get("max_abs_bound_magnitude"), UNIT_DECISION_THRESHOLD))
    notes.append("Source/, Config/ and the .uproject were not modified by this script")
    report["notes"] = "; ".join(notes)

    with open(REPORT_PATH, "w", encoding="utf-8") as handle:
        json.dump(report, handle, ensure_ascii=False, indent=2)

    log("report written: {0}".format(REPORT_PATH))
    log(
        "RESULT import_succeeded={0} asset={1} class={2} origin={3} extent={4} materials={5} collision={6} unit={7} scale={8}".format(
            report["import_succeeded"],
            report["asset_path"],
            report["class"],
            report["bounds_origin"],
            report["bounds_box_extent"],
            report["material_slot_count"],
            report["collision_trace_flag"],
            report["unit_guess"],
            report["suggested_actor_scale"],
        )
    )


main()
