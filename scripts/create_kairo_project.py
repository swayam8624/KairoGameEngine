#!/usr/bin/env python3
"""Generate a fresh Kairo project from reusable scene recipes.

Uses the engine's existing versioned project, scene, asset and input formats.
No custom runtime loader, external Python dependency or source-code changes
are needed to play a generated project.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import uuid

ROOT = Path(__file__).resolve().parents[1]
TEMPLATE = ROOT / "Samples/PlayableVerticalSlice"
MAX_RECIPE = 2 * 1024 * 1024
MAX_ASSET = 128 * 1024 * 1024
MAX_TOTAL = 512 * 1024 * 1024
MAX_ENTITIES = 10000
NAMESPACE = uuid.UUID("b4df8bf5-703d-4a73-bbe9-7133ea11363e")
BUILTINS = {name: f"20000000-0000-4000-8000-00000000000{i}"
            for i, name in enumerate(("cube", "plane", "sphere", "cylinder"), 1)}
MATERIAL = "20000000-0000-4000-8000-000000000010"
LAYERS = "18446744073709551615"


def require(condition, message):
    if not condition:
        raise ValueError(message)


def fields(obj, allowed, required=()):
    require(isinstance(obj, dict), "Expected an object")
    require(set(obj) <= set(allowed) and set(required) <= set(obj),
            f"Unexpected or missing fields: {sorted(obj)}")


def name(value):
    # Conservative portable names compatible with the existing quoted grammar.
    require(isinstance(value, str) and 1 <= len(value) <= 96
            and all(32 <= ord(c) <= 126 for c in value), "Invalid printable name")
    return value


def key(value):
    require(isinstance(value, str) and re.fullmatch(r"[a-z][a-z0-9_-]{0,63}", value),
            "Invalid asset or prefab key")
    return value


def quoted(value):
    return '"' + name(value).replace("\\", "\\\\").replace('"', '\\"') + '"'


def number(value, low, high):
    require(type(value) in (int, float) and low <= value <= high,
            f"Expected finite number between {low} and {high}")
    return float(value)


def vec(value, low, high):
    require(isinstance(value, list) and len(value) == 3, "Expected a 3-vector")
    return [number(x, low, high) for x in value]


def transform(obj):
    pos = vec(obj.get("position", [0, 0, 0]), -1000000, 1000000)
    scale = vec(obj.get("scale", [1, 1, 1]), .001, 1000)
    yaw = math.radians(number(obj.get("yaw", 0), -360, 360)) / 2
    return "transform " + " ".join(format(x, ".12g") for x in
                                  (*pos, 0, math.sin(yaw), 0, math.cos(yaw), *scale))


def read_bounded(path, limit):
    require(path.is_file(), f"Missing regular file: {path}")
    with path.open("rb") as stream:
        data = stream.read(limit + 1)
    require(len(data) <= limit, f"File exceeds size limit: {path}")
    return data


def unique_json(pairs):
    result = {}
    for k, v in pairs:
        require(k not in result, f"Duplicate JSON key: {k}")
        result[k] = v
    return result


def parse_json(data):
    def reject_constant(value):
        raise ValueError(f"Invalid JSON constant: {value}")
    return json.loads(data, object_pairs_hook=unique_json, parse_constant=reject_constant)


def recipe_file(root, path):
    """Resolve a strict portable relative path with no symbolic-link hops.
    The recipe directory is trusted local storage, not a concurrent sandbox.
    """
    require(isinstance(path, str) and path and "\\" not in path and ":" not in path,
            "Invalid recipe file path")
    parts = path.split("/")
    require(all(p not in ("", ".", "..") for p in parts), "Unsafe recipe file path")
    source = root
    for part in parts:
        source = source / part
        require(not source.is_symlink(), "Recipe symlinks are not allowed")
    source = source.resolve(strict=True)
    require(source.is_relative_to(root), "Recipe file escapes directory")
    return source


def glb_bytes(root, path):
    require(isinstance(path, str) and path.endswith(".glb"), "Only local .glb assets supported")
    source = recipe_file(root, path)
    data = read_bounded(source, MAX_ASSET)
    require(len(data) >= 20, "Truncated GLB")
    magic, version, length, chunk_size, chunk_type = struct.unpack_from("<4sIIII", data)
    require(magic == b"glTF" and version == 2 and length == len(data)
            and chunk_type == 0x4E4F534A and chunk_size % 4 == 0
            and 20 + chunk_size <= len(data), "Invalid GLB header")
    doc = parse_json(data[20:20 + chunk_size])
    require(isinstance(doc, dict) and isinstance(doc.get("asset"), dict)
            and doc["asset"].get("version") == "2.0", "Invalid glTF metadata")
    # Imported assets must be self-contained. No hidden dependencies or
    # remote fetches from embedded references, including extensions.
    pending = [doc]
    while pending:
        item = pending.pop()
        if isinstance(item, dict):
            for k, v in item.items():
                require(k != "uri", "GLB contains URI; use fully embedded buffers and images")
                pending.append(v)
        elif isinstance(item, list):
            pending.extend(item)
    offset = 20 + chunk_size
    if offset < len(data):
        require(offset + 8 <= len(data), "Truncated binary chunk")
        size, kind = struct.unpack_from("<II", data, offset)
        require(kind == 0x004E4942 and size % 4 == 0
                and offset + 8 + size == len(data), "Invalid GLB binary chunk")
    return data


def entity(entity_id, label, obj, parent=None, component=None, collider=False):
    lines = [f"entity {entity_id} {quoted(label)}"]
    if parent is not None:
        lines.append(f"parent {parent}")
    lines += ["enabled true", "layer 0", transform(obj)]
    if component:
        lines.append(component)
    if collider:
        lines.append("collider box 0.5 0.5 0.5 0.7 0 2 4294967295 false")
    return "\n".join(lines + ["end"]) + "\n"


def author_gameplay(gameplay, scene, input_text, menu_validator):
    """Compile bounded gameplay authoring into the existing native formats.

    This is a template-specific spawn/control/menu layer, not a new runtime
    scripting language. Keep the original camera framing relative to spawn.
    The native C++ menu validator is the sole authority for menu acceptance.
    """
    fields(gameplay, ("spawn", "keyboard", "menu"))
    if "spawn" in gameplay:
        x, y, z = vec(gameplay["spawn"], -7, 7)
        require(-6 <= x <= 6 and -6 <= z <= 6 and .92 <= y <= 6,
                "Spawn outside the supported arena")
        def relocate(match):
            block = match.group(0)
            entity_id = int(match.group(1))
            if entity_id not in (1, 20):
                return block
            offset = (x, y, z) if entity_id == 1 else (x, y - .92 + 7, z - 3.4 + 12.5)
            def change(line):
                values = line.group(1).split()
                require(len(values) == 10, "Unexpected template transform")
                return "transform " + " ".join([*(format(v, ".12g") for v in offset), *values[3:]])
            block, count = re.subn(r"(?m)^transform (.+)$", change, block)
            require(count == 1, "Missing player/camera transform")
            return block
        require(len(re.findall(r"(?m)^entity (?:1|20) ", scene)) == 2,
                "Unexpected gameplay template")
        scene = re.sub(r"(?ms)^entity (\d+) .*?^end$", relocate, scene)
    if "keyboard" in gameplay:
        keyboard = gameplay["keyboard"]
        actions = {"forward": "W", "back": "S", "left": "A", "right": "D",
                   "jump": "Space", "quit": "Escape"}
        fields(keyboard, actions.keys())
        actions.update(keyboard)
        # Only known key names are accepted, with no duplicate bindings.
        allowed = set("ABCDEFGHIJKLMNOPQRSTUVWXYZ") | {"Space", "Escape"}
        require(all(isinstance(v, str) and v in allowed for v in actions.values()),
                "Unsupported keyboard key")
        require(len(set(actions.values())) == len(actions), "Duplicate keyboard key")
        replacements = dict(zip(("W", "S", "A", "D", "Space", "Escape"), actions.values()))
        input_text, count = re.subn(r'(?m)^(bind "(?:Move|Jump|Quit)" key )(\w+)( .+)
    recipe_path = recipe_path.resolve(strict=True)
    recipe = parse_json(read_bounded(recipe_path, MAX_RECIPE))
    fields(recipe, ("name", "layout", "assets", "prefabs", "instances", "gameplay"), ("name", "prefabs", "instances"))
    project_name = name(recipe["name"])
    layout = recipe.get("layout", "arena")
    require(layout in ("arena", "empty"), "Layout must be arena or empty")
    assets = recipe.get("assets", {})
    prefabs = recipe["prefabs"]
    instances = recipe["instances"]
    require(isinstance(assets, dict) and len(assets) <= 128, "Invalid asset map")
    require(isinstance(prefabs, dict) and len(prefabs) <= 256, "Invalid prefab map")
    require(isinstance(instances, list) and len(instances) <= MAX_ENTITIES, "Invalid instances")
    descriptor = (template / "Project.kproject").read_text(encoding="utf-8")
    descriptor, count = re.subn(r"(?m)^name .+$", lambda _: "name " + quoted(project_name), descriptor)
    require(count == 1, "Unexpected project template")
    manifest = (template / "Assets.kassets").read_text(encoding="utf-8")
    scene = (template / "Scenes/PlayableArena.kscene").read_text(encoding="utf-8")
    next_id = max(map(int, re.findall(r"(?m)^entity (\d+) ", scene))) + 1
    if layout == "empty":
        # Preserve the known-good player, floor, boundary walls, player
        # visual, camera and lighting. Remove the arena's decorative props.
        # Use the template's stable IDs, never rename or renumber references.
        omit = {3, 4, 5, 6, 7, 12, 13}
        blocks = re.split(r"(?m)(?=^entity \d+ )", scene)
        scene = blocks[0] + "".join(block for block in blocks[1:]
            if int(re.match(r"entity (\d+) ", block).group(1)) not in omit)
    files = {"Project.kproject": descriptor.encode(), "Config/Input.kinput":
             (template / "Config/Input.kinput").read_bytes()}
    if "gameplay" in recipe:
        scene, input_text, menu = author_gameplay(recipe["gameplay"], scene, files["Config/Input.kinput"].decode("utf-8"), menu_validator)
        files["Config/Input.kinput"] = input_text.encode("utf-8")
        if menu is not None:
            files["Config/Menu.kmenu"] = menu
    imports = {}
    provenance = []
    total = 0
    for asset_key, record in sorted(assets.items()):
        key(asset_key)
        fields(record, ("file", "author", "license", "source", "sha256", "license_file", "license_sha256"),
               ("file", "author", "license", "source"))
        for field in ("author", "license", "source"):
            require(isinstance(record[field], str) and 1 <= len(record[field]) <= 2048
                    and all(ord(c) >= 32 for c in record[field]), f"Missing asset {field}")
        data = glb_bytes(recipe_path.parent, record["file"])
        total += len(data)
        require(total <= MAX_TOTAL, "Total asset budget exceeded")
        digest = hashlib.sha256(data).hexdigest()
        if "sha256" in record:
            require(record["sha256"] == digest, "Imported asset checksum mismatch")
        require(("license_file" in record) == ("license_sha256" in record), "License evidence requires both file and checksum")
        asset_id = str(uuid.uuid5(NAMESPACE, asset_key + ":" + digest))
        dest = f"Content/Imported/{asset_id}.glb"
        files[dest] = data
        imports[asset_key] = asset_id
        manifest += f'asset {asset_id} scene source 1 "{dest}" "kairo.gltf.scene"\nend\n'
        attribution = {"asset": asset_key, "path": dest, "sha256": digest,
                       "author": record["author"], "license": record["license"],
                       "source": record["source"]}
        # Preserve verified original license bytes in the final project, not
        # just a path back into a temporary catalog staging directory.
        if "license_file" in record:
            evidence = read_bounded(recipe_file(recipe_path.parent, record["license_file"]), 256 * 1024)
            require(bool(evidence.strip()), "Empty license evidence")
            evidence_hash = hashlib.sha256(evidence).hexdigest()
            require(record["license_sha256"] == evidence_hash, "License evidence checksum mismatch")
            total += len(evidence)
            require(total <= MAX_TOTAL, "Total asset and license budget exceeded")
            evidence_dest = f"Licenses/{asset_id}.txt"
            files[evidence_dest] = evidence
            attribution.update(license_file=evidence_dest, license_sha256=evidence_hash)
        provenance.append(attribution)
    # Compile every definition, even unused ones. A malformed prefab cannot
    # lie dormant until a designer happens to place its first instance.
    compiled = {}
    for prefab_key, parts in sorted(prefabs.items()):
        key(prefab_key)
        require(isinstance(parts, list) and 1 <= len(parts) <= 128, "Invalid prefab parts")
        compiled[prefab_key] = []
        for part in parts:
            fields(part, ("name", "mesh", "asset", "position", "yaw", "scale", "collider"), ("name",))
            name(part["name"])
            transform(part)
            require(("mesh" in part) != ("asset" in part), "Part needs exactly one mesh or imported asset")
            collider = part.get("collider", False)
            require(type(collider) is bool, "Collider must be boolean")
            if "mesh" in part:
                mesh = key(part["mesh"])
                require(mesh in BUILTINS, "Unknown builtin mesh")
                component = f"mesh-renderer {BUILTINS[mesh]} true true true {LAYERS} 1 {MATERIAL}"
                require(not collider or mesh == "cube", "Automatic box collider only supported for cubes")
            else:
                asset = key(part["asset"])
                require(asset in imports and not collider, "Unknown asset or unsupported imported collider")
                component = f"scene-instance {imports[asset]} true true true {LAYERS}"
            compiled[prefab_key].append((part, component, collider))
    scene_parts = [scene.rstrip() + "\n"]
    used_labels = set()
    expanded = 0
    for instance in instances:
        fields(instance, ("name", "prefab", "position", "yaw", "scale"), ("name", "prefab"))
        label = name(instance["name"])
        require(label not in used_labels, "Duplicate instance name")
        used_labels.add(label)
        prefab = key(instance["prefab"])
        require(prefab in compiled, "Unknown prefab")
        # Static collider scale is inherited from parent. Restrict root scale
        # to uniform so rotated child colliders cannot acquire shear.
        scale = vec(instance.get("scale", [1, 1, 1]), .001, 1000)
        require(scale[0] == scale[1] == scale[2], "Instance scale must be uniform")
        expanded += 1 + len(compiled[prefab])
        require(expanded <= MAX_ENTITIES, "Expanded entity budget exceeded")
        parent = next_id
        scene_parts.append(entity(parent, label, instance))
        next_id += 1
        for part, component, collider in compiled[prefab]:
            scene_parts.append(entity(next_id, part["name"], part, parent, component, collider))
            next_id += 1
    files["Assets.kassets"] = manifest.encode()
    files["Scenes/PlayableArena.kscene"] = "".join(scene_parts).encode()
    files["ASSET_LICENSES.json"] = (json.dumps(provenance, indent=2, sort_keys=True) + "\n").encode()
    return files


def create_project(recipe, output, template=TEMPLATE, *, menu_validator=None):
    files = compile_recipe(recipe, template, menu_validator=menu_validator)
    # All recipe validation precedes destination creation. mkdir exclusively
    # reserves a fresh directory; an existing project is never overwritten.
    output = output.absolute()
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
    return output / "Project.kproject"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("recipe", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--menu-validator", type=Path, help="Built KairoMenuDocumentTool for authored menus")
    args = parser.parse_args()
    try:
        print(create_project(args.recipe, args.output, menu_validator=args.menu_validator))
    except (ValueError, OSError, RecursionError, subprocess.SubprocessError) as error:
        print(f"Kairo project creation failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
,
                                  lambda m: m[1] + replacements[m[2]] + m[3], input_text)
        require(count == 6, "Unexpected gameplay input template")
    menu = None
    if "menu" in gameplay:
        buttons = gameplay["menu"]
        require(isinstance(buttons, list) and 1 <= len(buttons) <= 64, "Invalid menu buttons")
        rows = ["kairo-menu 1"]
        for button in buttons:
            fields(button, ("id", "label", "action", "rect"), ("id", "label", "action", "rect"))
            ident, label, action, rect = (button[k] for k in ("id", "label", "action", "rect"))
            require(isinstance(ident, str) and re.fullmatch(r"[a-z0-9-]{1,64}", ident),
                    "Invalid menu ID")
            require(isinstance(label, str) and 1 <= len(label) <= 64 and all(32 <= ord(c) <= 126 for c in label),
                    "Invalid menu label")
            require(action in ("resume", "quit"), "Invalid menu action")
            require(isinstance(rect, list) and len(rect) == 4 and all(type(v) is int and 0 <= v <= 1000 for v in rect),
                    "Invalid menu coordinates")
            rows.append("button " + quoted(ident) + " " + quoted(label) + " " + action + " " + " ".join(map(str, rect)))
        require(menu_validator is not None, "Authored menu requires --menu-validator")
        validator = Path(menu_validator).resolve(strict=True)
        result = subprocess.run([str(validator)], input=("\n".join(rows) + "\n").encode("ascii"),
                                capture_output=True, timeout=5)
        require(result.returncode == 0, "Native menu validator rejected authored menu: " +
                result.stderr.decode("utf-8", errors="replace")[:512])
        require(result.stdout.startswith(b"kairo-menu 1\n") and len(result.stdout) <= 32768,
                "Invalid native menu validator response")
        menu = result.stdout
    return scene, input_text, menu


def compile_recipe(recipe_path, template=TEMPLATE, *, menu_validator=None):
    recipe_path = recipe_path.resolve(strict=True)
    recipe = parse_json(read_bounded(recipe_path, MAX_RECIPE))
    fields(recipe, ("name", "layout", "assets", "prefabs", "instances"), ("name", "prefabs", "instances"))
    project_name = name(recipe["name"])
    layout = recipe.get("layout", "arena")
    require(layout in ("arena", "empty"), "Layout must be arena or empty")
    assets = recipe.get("assets", {})
    prefabs = recipe["prefabs"]
    instances = recipe["instances"]
    require(isinstance(assets, dict) and len(assets) <= 128, "Invalid asset map")
    require(isinstance(prefabs, dict) and len(prefabs) <= 256, "Invalid prefab map")
    require(isinstance(instances, list) and len(instances) <= MAX_ENTITIES, "Invalid instances")
    descriptor = (template / "Project.kproject").read_text(encoding="utf-8")
    descriptor, count = re.subn(r"(?m)^name .+$", lambda _: "name " + quoted(project_name), descriptor)
    require(count == 1, "Unexpected project template")
    manifest = (template / "Assets.kassets").read_text(encoding="utf-8")
    scene = (template / "Scenes/PlayableArena.kscene").read_text(encoding="utf-8")
    next_id = max(map(int, re.findall(r"(?m)^entity (\d+) ", scene))) + 1
    if layout == "empty":
        # Preserve the known-good player, floor, boundary walls, player
        # visual, camera and lighting. Remove the arena's decorative props.
        # Use the template's stable IDs, never rename or renumber references.
        omit = {3, 4, 5, 6, 7, 12, 13}
        blocks = re.split(r"(?m)(?=^entity \d+ )", scene)
        scene = blocks[0] + "".join(block for block in blocks[1:]
            if int(re.match(r"entity (\d+) ", block).group(1)) not in omit)
    files = {"Project.kproject": descriptor.encode(), "Config/Input.kinput":
             (template / "Config/Input.kinput").read_bytes()}
    imports = {}
    provenance = []
    total = 0
    for asset_key, record in sorted(assets.items()):
        key(asset_key)
        fields(record, ("file", "author", "license", "source", "sha256", "license_file", "license_sha256"),
               ("file", "author", "license", "source"))
        for field in ("author", "license", "source"):
            require(isinstance(record[field], str) and 1 <= len(record[field]) <= 2048
                    and all(ord(c) >= 32 for c in record[field]), f"Missing asset {field}")
        data = glb_bytes(recipe_path.parent, record["file"])
        total += len(data)
        require(total <= MAX_TOTAL, "Total asset budget exceeded")
        digest = hashlib.sha256(data).hexdigest()
        if "sha256" in record:
            require(record["sha256"] == digest, "Imported asset checksum mismatch")
        require(("license_file" in record) == ("license_sha256" in record), "License evidence requires both file and checksum")
        asset_id = str(uuid.uuid5(NAMESPACE, asset_key + ":" + digest))
        dest = f"Content/Imported/{asset_id}.glb"
        files[dest] = data
        imports[asset_key] = asset_id
        manifest += f'asset {asset_id} scene source 1 "{dest}" "kairo.gltf.scene"\nend\n'
        attribution = {"asset": asset_key, "path": dest, "sha256": digest,
                       "author": record["author"], "license": record["license"],
                       "source": record["source"]}
        # Preserve verified original license bytes in the final project, not
        # just a path back into a temporary catalog staging directory.
        if "license_file" in record:
            evidence = read_bounded(recipe_file(recipe_path.parent, record["license_file"]), 256 * 1024)
            require(bool(evidence.strip()), "Empty license evidence")
            evidence_hash = hashlib.sha256(evidence).hexdigest()
            require(record["license_sha256"] == evidence_hash, "License evidence checksum mismatch")
            total += len(evidence)
            require(total <= MAX_TOTAL, "Total asset and license budget exceeded")
            evidence_dest = f"Licenses/{asset_id}.txt"
            files[evidence_dest] = evidence
            attribution.update(license_file=evidence_dest, license_sha256=evidence_hash)
        provenance.append(attribution)
    # Compile every definition, even unused ones. A malformed prefab cannot
    # lie dormant until a designer happens to place its first instance.
    compiled = {}
    for prefab_key, parts in sorted(prefabs.items()):
        key(prefab_key)
        require(isinstance(parts, list) and 1 <= len(parts) <= 128, "Invalid prefab parts")
        compiled[prefab_key] = []
        for part in parts:
            fields(part, ("name", "mesh", "asset", "position", "yaw", "scale", "collider"), ("name",))
            name(part["name"])
            transform(part)
            require(("mesh" in part) != ("asset" in part), "Part needs exactly one mesh or imported asset")
            collider = part.get("collider", False)
            require(type(collider) is bool, "Collider must be boolean")
            if "mesh" in part:
                mesh = key(part["mesh"])
                require(mesh in BUILTINS, "Unknown builtin mesh")
                component = f"mesh-renderer {BUILTINS[mesh]} true true true {LAYERS} 1 {MATERIAL}"
                require(not collider or mesh == "cube", "Automatic box collider only supported for cubes")
            else:
                asset = key(part["asset"])
                require(asset in imports and not collider, "Unknown asset or unsupported imported collider")
                component = f"scene-instance {imports[asset]} true true true {LAYERS}"
            compiled[prefab_key].append((part, component, collider))
    scene_parts = [scene.rstrip() + "\n"]
    used_labels = set()
    expanded = 0
    for instance in instances:
        fields(instance, ("name", "prefab", "position", "yaw", "scale"), ("name", "prefab"))
        label = name(instance["name"])
        require(label not in used_labels, "Duplicate instance name")
        used_labels.add(label)
        prefab = key(instance["prefab"])
        require(prefab in compiled, "Unknown prefab")
        # Static collider scale is inherited from parent. Restrict root scale
        # to uniform so rotated child colliders cannot acquire shear.
        scale = vec(instance.get("scale", [1, 1, 1]), .001, 1000)
        require(scale[0] == scale[1] == scale[2], "Instance scale must be uniform")
        expanded += 1 + len(compiled[prefab])
        require(expanded <= MAX_ENTITIES, "Expanded entity budget exceeded")
        parent = next_id
        scene_parts.append(entity(parent, label, instance))
        next_id += 1
        for part, component, collider in compiled[prefab]:
            scene_parts.append(entity(next_id, part["name"], part, parent, component, collider))
            next_id += 1
    files["Assets.kassets"] = manifest.encode()
    files["Scenes/PlayableArena.kscene"] = "".join(scene_parts).encode()
    files["ASSET_LICENSES.json"] = (json.dumps(provenance, indent=2, sort_keys=True) + "\n").encode()
    return files


def create_project(recipe, output, template=TEMPLATE):
    files = compile_recipe(recipe, template)
    # All recipe validation precedes destination creation. mkdir exclusively
    # reserves a fresh directory; an existing project is never overwritten.
    output = output.absolute()
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
    return output / "Project.kproject"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("recipe", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        print(create_project(args.recipe, args.output))
    except (ValueError, OSError, RecursionError) as error:
        print(f"Kairo project creation failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
