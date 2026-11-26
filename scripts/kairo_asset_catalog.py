#!/usr/bin/env python3
"""Search a local licensed GLB catalog and stage verified project recipe inputs."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import struct
import tempfile

LICENSES = {"CC0-1.0", "CC-BY-4.0", "MIT"}
MAX_FILE = 128 * 1024 * 1024
MAX_TOTAL = 512 * 1024 * 1024
MAX_MANIFEST = 2 * 1024 * 1024


def require(ok, message):
    if not ok:
        raise ValueError(message)


def read_bounded(path, limit):
    """Read at most the configured byte budget, never trust file metadata alone."""
    require(path.is_file() and not path.is_symlink(), f"Not a regular file: {path}")
    with path.open("rb") as stream:
        data = stream.read(limit + 1)
    require(len(data) <= limit, f"File exceeds budget: {path}")
    return data


def unique(pairs):
    """Reject duplicate keys so different JSON readers cannot disagree."""
    result = {}
    for key, value in pairs:
        require(key not in result, f"Duplicate JSON key: {key}")
        result[key] = value
    return result


def parse(data):
    return json.loads(data, object_pairs_hook=unique)


def safe_path(root, relative):
    """Catalog paths are portable relative paths; reject traversal and symlinks."""
    require(isinstance(relative, str) and relative and "\\" not in relative and ":" not in relative,
            "Invalid catalog path")
    parts = relative.split("/")
    require(all(p not in ("", ".", "..") for p in parts), "Unsafe catalog path")
    path = root
    for part in PurePosixPath(relative).parts:
        path = path / part
        require(not path.is_symlink(), "Catalog symlinks are not allowed")
    require(path.resolve().is_relative_to(root), "Catalog path escaped root")
    return path


def load_catalog(manifest):
    """Return a validated ID-indexed manifest. Metadata is author-supplied;
    allowlisted SPDX identifiers are a policy, not a legal authenticity check."""
    root = manifest.parent.resolve(strict=True)
    doc = parse(read_bounded(manifest, MAX_MANIFEST))
    require(isinstance(doc, dict) and set(doc) == {"version", "assets"} and type(doc["version"]) is int and doc["version"] == 1, "Unsupported catalog")
    require(isinstance(doc["assets"], list) and len(doc["assets"]) <= 4096, "Invalid asset list")
    entries = {}
    for item in doc["assets"]:
        require(isinstance(item, dict) and set(item) == {"id", "name", "file", "author", "license", "source", "sha256", "license_file", "tags"}, "Invalid asset fields")
        key = item["id"]
        require(isinstance(key, str) and re.fullmatch(r"[a-z][a-z0-9_-]{0,63}", key) and key not in entries, "Invalid or duplicate asset ID")
        for field in ("name", "author", "source"):
            value = item[field]
            require(isinstance(value, str) and 0 < len(value) <= 2048 and all(ord(c) >= 32 for c in value), f"Invalid {field}")
        require(isinstance(item["license"], str) and item["license"] in LICENSES, "License not on local catalog allowlist")
        require(isinstance(item["sha256"], str) and re.fullmatch(r"[0-9a-f]{64}", item["sha256"]), "Invalid SHA256")
        require(isinstance(item["tags"], list) and len(item["tags"]) <= 32 and all(isinstance(t, str) and 0 < len(t) <= 64 and all(ord(c) >= 32 for c in t) for t in item["tags"]), "Invalid tags")
        require(safe_path(root, item["file"]).suffix.lower() == ".glb", "Only GLB catalog assets are supported")
        safe_path(root, item["license_file"])
        entries[key] = item
    return root, entries


def search(entries, query):
    """Case-insensitive AND search across metadata, in stable ID order."""
    words = query.casefold().split()
    return [item for _, item in sorted(entries.items()) if all(word in " ".join([item["id"], item["name"], item["author"], item["license"], *item["tags"]]).casefold() for word in words)]


def stage(manifest, ids, output, *, expected_entries=None):
    """Validate selected bytes before reserving a new output directory.
    Preserve original license evidence and SHA256 in a provenance manifest;
    emit the exact file/author/license/source fields expected by recipe import.
    No network fetch, execution, archive extraction, or overwrite is allowed."""
    root, entries = load_catalog(manifest)
    require(0 < len(ids) <= 128 and len(ids) == len(set(ids)), "Select 1..128 distinct IDs")
    files, assets, provenance = {}, {}, []
    total = 0
    for key in sorted(ids):
        require(key in entries, f"Unknown asset ID: {key}")
        item = entries[key]
        # Authoring callers may bind the metadata the user actually reviewed.
        # Recheck against this same manifest load before staging any bytes.
        if expected_entries is not None:
            require(item == expected_entries.get(key), f"Catalog metadata changed: {key}; reopen catalog")
        data = read_bounded(safe_path(root, item["file"]), MAX_FILE)
        require(len(data) >= 20 and struct.unpack_from("<4sII", data) == (b"glTF", 2, len(data)), "Invalid GLB container")
        require(hashlib.sha256(data).hexdigest() == item["sha256"], f"Asset checksum mismatch: {key}")
        license_data = read_bounded(safe_path(root, item["license_file"]), 256 * 1024)
        require(bool(license_data.strip()), "Empty license evidence")
        total += len(data) + len(license_data)
        require(total <= MAX_TOTAL, "Selection exceeds byte budget")
        relative = f"assets/{key}.glb"
        license_relative = f"licenses/{key}.txt"
        files[relative], files[license_relative] = data, license_data
        # The recipe importer re-verifies the staged bytes and retains the
        # actual license file in the generated project, even after staging
        # is deleted. Metadata alone is not proof of legal entitlement.
        assets[key] = {"file": relative, "author": item["author"], "license": item["license"], "source": item["source"],
                       "sha256": item["sha256"], "license_file": license_relative,
                       "license_sha256": hashlib.sha256(license_data).hexdigest()}
        provenance.append({**item, "file": relative, "license_file": license_relative,
                           "license_sha256": hashlib.sha256(license_data).hexdigest()})
    recipe = {"name": "Catalog Gallery", "layout": "empty", "assets": assets, "prefabs": {}, "instances": []}
    for i, key in enumerate(sorted(ids)):
        recipe["prefabs"][key] = [{"name": key, "asset": key}]
        recipe["instances"].append({"name": key, "prefab": key, "position": [2 * (i % 8) - 7, 0, 2 * (i // 8) - 3]})
    for relative, obj in (("recipe.json", recipe), ("CATALOG_PROVENANCE.json", provenance)):
        files[relative] = (json.dumps(obj, indent=2, sort_keys=True) + "\n").encode()
    output.mkdir()
    try:
        for relative, data in sorted(files.items()):
            target = output / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            with target.open("xb") as stream:
                stream.write(data)
    except BaseException:
        shutil.rmtree(output)
        raise
    return output / "recipe.json"


def create_catalog_project(manifest, ids, output, *, expected_entries=None):
    """Create a regular Kairo project, not an intermediate staging directory.

    The existing recipe compiler owns the project format and provenance.
    Temporary staging is isolated and removed even on failure. Destination
    creation happens exclusively in create_project, which refuses overwrites.
    """
    from create_kairo_project import create_project
    output = Path(output).absolute()
    require(not output.exists() and not output.is_symlink(), "Project destination already exists")
    with tempfile.TemporaryDirectory(prefix="kairo-catalog-project-") as work:
        recipe = stage(Path(manifest), ids, Path(work) / "stage",
                       expected_entries=expected_entries)
        return create_project(recipe, output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("catalog", type=Path)
    sub = parser.add_subparsers(dest="command", required=True)
    find = sub.add_parser("search")
    find.add_argument("query", nargs="?", default="")
    export = sub.add_parser("stage")
    export.add_argument("output", type=Path)
    export.add_argument("ids", nargs="+")
    project = sub.add_parser("project", help="Create a new editable Kairo project")
    project.add_argument("output", type=Path)
    project.add_argument("ids", nargs="+")
    args = parser.parse_args()
    try:
        if args.command == "search":
            _, entries = load_catalog(args.catalog)
            print(json.dumps(search(entries, args.query), indent=2))
        elif args.command == "stage":
            print(stage(args.catalog, args.ids, args.output))
        else:
            print(create_catalog_project(args.catalog, args.ids, args.output))
    except (ValueError, OSError, RecursionError) as error:
        parser.exit(1, f"Catalog rejected: {error}\n")


if __name__ == "__main__":
    main()
