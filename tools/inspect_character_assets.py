"""Read-only UE editor audit. Does not save, import, delete, or modify assets.
Run: UnrealEditor-Cmd Project.uproject -run=pythonscript -script=<this file> -NullRHI
"""
import json
from pathlib import Path
import unreal


def inspect_assets():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(True)
    records = []
    candidates = registry.get_assets_by_path("/Game/Mannequins", recursive=True)
    for data in candidates:
        kind = str(data.asset_class_path.asset_name)
        if kind not in {"SkeletalMesh", "Skeleton", "AnimSequence", "AnimBlueprint"}:
            continue
        path = str(data.package_name)
        record = {"path": path, "class": kind, "loaded": False}
        try:
            asset = data.get_asset()
            if asset is None:
                raise ValueError("Asset load returned None")
            record["loaded"] = True
            if kind in {"SkeletalMesh", "AnimSequence"}:
                skeleton = asset.get_editor_property("skeleton")
                record["skeleton"] = skeleton.get_path_name() if skeleton else None
            if kind == "SkeletalMesh":
                materials = asset.get_editor_property("materials")
                record["materials"] = [
                    m.material_interface.get_path_name() if m.material_interface else None
                    for m in materials
                ]
        except Exception as error:
            record["error"] = str(error)
        records.append(record)
    report = {"scope": "/Game/Mannequins", "visual_review": "not_performed",
              "license_review": "not_approved", "assets": records,
              "loaded": sum(r["loaded"] for r in records), "count": len(records)}
    output = Path(unreal.Paths.project_saved_dir()) / "Logs" / "M12b_character_assets.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("M12B ASSET AUDIT: count={} loaded={} report={}".format(len(records), report["loaded"], output))


inspect_assets()
