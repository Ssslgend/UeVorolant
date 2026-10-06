"""Recalculate normals + lightmap UVs on AscentPlayable (Shell FBX often flips normals).

Run in UE Editor Python:
  exec(open(r"F:/project/gitprojects/UE/valorant/ascent1/Tools/ue_fix_map_normals.py").read()); main()
"""
from __future__ import annotations

import traceback

import unreal

ASSET = "/Game/Ascent/AscentPlayable.AscentPlayable"


def main() -> None:
    try:
        mesh = unreal.load_asset(ASSET)
        if mesh is None:
            unreal.log_error(f"missing {ASSET}")
            return

        # StaticMeshEditorSubsystem (UE5) can rebuild normals / lightmap UVs.
        sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        if sub is None:
            unreal.log_error("no StaticMeshEditorSubsystem")
            return

        # 1) Recompute normals outward (fixes flipped shells from mirrored Blender scales).
        try:
            sub.recompute_normals(mesh)
            unreal.log("[fix_normals] recompute_normals ok")
        except Exception as exc:
            unreal.log_warning(f"[fix_normals] recompute_normals failed: {exc}")

        # 2) Recompute tangents.
        try:
            sub.recompute_tangents(mesh)
            unreal.log("[fix_normals] recompute_tangents ok")
        except Exception as exc:
            unreal.log_warning(f"[fix_normals] recompute_tangents failed: {exc}")

        # 3) Build / replace lightmap UVs (channel 1) so lighting is not black.
        try:
            ok = sub.generate_lightmap_u_vs(mesh)
            unreal.log(f"[fix_normals] generate_lightmap_u_vs -> {ok}")
        except Exception as exc:
            unreal.log_warning(f"[fix_normals] generate_lightmap_u_vs failed: {exc}")

        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        unreal.log("[fix_normals] saved")
    except Exception:
        unreal.log_error(traceback.format_exc())


if __name__ == "__main__":
    main()
