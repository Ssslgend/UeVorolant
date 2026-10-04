import json
import os
import struct


PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
FBX_DIR = os.path.join(PROJECT_ROOT, "Saved", "AscentDuelExport", "FBX")
EXPORT_REPORT = os.path.join(PROJECT_ROOT, "Saved", "AscentDuelExport", "export_report.json")
VERIFY_REPORT = os.path.join(PROJECT_ROOT, "Saved", "AscentDuelExport", "fbx_verify_report.json")


def verify_file(path):
    with open(path, "rb") as handle:
        header = handle.read(23)
        version = None
        if header.startswith(b"Kaydara FBX Binary"):
            version = struct.unpack("<I", handle.read(4))[0]

    return {
        "path": path,
        "exists": os.path.exists(path),
        "size_bytes": os.path.getsize(path),
        "valid_binary_header": header.startswith(b"Kaydara FBX Binary"),
        "fbx_version": version,
    }


def main():
    with open(EXPORT_REPORT, "r", encoding="utf-8") as handle:
        export_report = json.load(handle)

    regions = []
    for region in export_report["regions"]:
        verification = verify_file(os.path.join(PROJECT_ROOT, region["fbx_path"]))
        verification.update(
            {
                "name": region["name"],
                "object_count": region["object_count"],
                "vertices": region["vertices"],
                "polygons": region["polygons"],
            }
        )
        regions.append(verification)

    report = {"regions": regions}
    with open(VERIFY_REPORT, "w", encoding="utf-8") as handle:
        json.dump(report, handle, ensure_ascii=False, indent=2)

    for region in regions:
        print(
            f"{region['name']}: header={region['valid_binary_header']} "
            f"version={region['fbx_version']} size={region['size_bytes']} "
            f"objects={region['object_count']} polys={region['polygons']}"
        )


if __name__ == "__main__":
    main()
