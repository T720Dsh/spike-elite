"""Read-only asset inventory; hashes do not prove authorship or licensing."""
import argparse
import hashlib
import json
from pathlib import Path

SUFFIXES = {".uasset", ".umap", ".png", ".jpg", ".jpeg", ".fbx", ".glb", ".wav", ".ttf", ".otf"}


def inventory(root):
    root = Path(root).resolve()
    content = root / "Content"
    if not content.is_dir():
        raise ValueError("Content directory does not exist")
    assets = []
    for path in sorted(content.rglob("*")):
        if path.is_symlink() or not path.is_file() or path.suffix.lower() not in SUFFIXES:
            continue
        if not path.resolve().is_relative_to(content.resolve()):
            continue
        with path.open("rb") as stream:
            digest = hashlib.file_digest(stream, "sha256").hexdigest()
        assets.append({"path": path.relative_to(root).as_posix(), "bytes": path.stat().st_size,
                       "sha256": digest, "license_status": "requires_documented_review"})
    return {"schema_version": 1, "asset_count": len(assets),
            "total_bytes": sum(a["bytes"] for a in assets), "assets": assets}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    print(json.dumps(inventory(args.root), ensure_ascii=False, indent=2))
