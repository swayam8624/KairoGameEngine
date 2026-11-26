import importlib.util
import json
import random
import os
import subprocess
import shutil
from pathlib import Path
import struct
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("generator", ROOT / "scripts/create_kairo_project.py")
g = importlib.util.module_from_spec(spec)
spec.loader.exec_module(g)


class ProjectRecipeTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.recipe = self.root / "recipe.json"
        self.data = {"name": "My Arena", "prefabs": {"box": [
            {"name": "Crate", "mesh": "cube", "collider": True}]},
            "instances": [{"name": "A", "prefab": "box"},
                          {"name": "B", "prefab": "box", "position": [2, 0, 3], "yaw": 90}]}

    def write(self):
        self.recipe.write_text(json.dumps(self.data), encoding="utf-8")

    def glb(self, doc):
        chunk = json.dumps(doc).encode()
        chunk += b" " * ((-len(chunk)) % 4)
        return struct.pack("<4sIIII", b"glTF", 2, 20 + len(chunk), len(chunk), 0x4E4F534A) + chunk

    def test_fresh_and_deterministic(self):
        self.write()
        a = g.create_project(self.recipe, self.root / "one")
        b = g.create_project(self.recipe, self.root / "two")
        self.assertEqual(a.read_bytes(), b.read_bytes())
        self.assertEqual(g.compile_recipe(self.recipe), g.compile_recipe(self.recipe))
        scene = (a.parent / "Scenes/PlayableArena.kscene").read_text()
        self.assertIn('entity 25 "A"\n', scene)
        self.assertIn('entity 26 "Crate"\nparent 25\n', scene)
        self.assertIn('entity 28 "Crate"\nparent 27\n', scene)
        with self.assertRaises(FileExistsError):
            g.create_project(self.recipe, a.parent)
        self.assertIn('name "My Arena"', a.read_text())

    def test_invalid_definitions_leave_no_output(self):
        for part in [{"name": "X", "mesh": "missing"}, {"name": "X", "mesh": "cube", "scale": [1, 0, 1]},
                     {"name": "X", "mesh": "sphere", "collider": True}, {"name": "X\nend", "mesh": "cube"}]:
            self.data["prefabs"]["unused"] = [part]
            self.write()
            output = self.root / "bad"
            with self.assertRaises(ValueError):
                g.create_project(self.recipe, output)
            self.assertFalse(output.exists())

    def test_asset_import_and_provenance(self):
        # Structurally valid fixture isolates the import/registration boundary;
        # the native CI gate uses the repository's real licensed ToyCar model.
        asset = self.root / "model.glb"
        asset.write_bytes(self.glb({"asset": {"version": "2.0"}}))
        self.data["assets"] = {"prop": {"file": "model.glb", "author": "Example",
                              "license": "CC0-1.0", "source": "test fixture"}}
        self.data["prefabs"]["box"] = [{"name": "Prop", "asset": "prop"}]
        self.write()
        files = g.compile_recipe(self.recipe)
        record = json.loads(files["ASSET_LICENSES.json"])[0]
        self.assertEqual(files[record["path"]], asset.read_bytes())
        self.assertIn(record["path"].encode(), files["Assets.kassets"])
        self.assertIn(b"scene-instance ", files["Scenes/PlayableArena.kscene"])
        self.assertEqual(len(record["sha256"]), 64)
        asset.write_bytes(self.glb({"asset": {"version": "2.0"}, "buffers": [{"uri": "../outside.bin"}]}))
        with self.assertRaises(ValueError):
            g.compile_recipe(self.recipe)

    def test_traversal_and_duplicate_fields(self):
        self.data["assets"] = {"prop": {"file": "../model.glb", "author": "A", "license": "CC0", "source": "local"}}
        self.write()
        with self.assertRaises(ValueError):
            g.compile_recipe(self.recipe)
        self.recipe.write_text('{"name":"A","name":"B","prefabs":{},"instances":[]}')
        with self.assertRaises(ValueError):
            g.compile_recipe(self.recipe)

    def test_expanded_budget_and_demo(self):
        self.data["prefabs"]["box"] *= 128
        self.data["instances"] = [{"name": str(i), "prefab": "box"} for i in range(100)]
        self.write()
        with self.assertRaises(ValueError):
            g.compile_recipe(self.recipe)
        files = g.compile_recipe(ROOT / "Samples/ProjectRecipes/TempleArena.json")
        self.assertIn(b"North gateway", files["Scenes/PlayableArena.kscene"])

    def test_empty_layout_preserves_runtime_dependencies(self):
        self.data["layout"] = "empty"
        self.write()
        scene = g.compile_recipe(self.recipe)["Scenes/PlayableArena.kscene"].decode()
        self.assertIn('entity 1 "Player"', scene)
        self.assertIn('entity 2 "Arena Floor"', scene)
        self.assertIn('entity 14 "Player Visual"\nparent 1', scene)
        self.assertIn('entity 20 "Gameplay Camera"', scene)
        self.assertIn('entity 25 "A"', scene)
        self.assertNotIn('entity 3 "Center Plinth"', scene)
        self.assertNotIn('entity 12 "Beacon A"', scene)
        self.data["layout"] = "unknown"
        self.write()
        with self.assertRaises(ValueError):
            g.compile_recipe(self.recipe)

    def test_randomized_deterministic_expansion(self):
        rng = random.Random(72)
        for trial in range(500):
            self.data["instances"] = [
                {"name": f"instance-{i}", "prefab": "box",
                 "position": [rng.uniform(-100, 100), 0, rng.uniform(-100, 100)],
                 "yaw": rng.uniform(-360, 360)}
                for i in range(rng.randrange(1, 30))]
            self.write()
            a = g.compile_recipe(self.recipe)
            b = g.compile_recipe(self.recipe)
            self.assertEqual(a, b, f"Recipe {trial} was not deterministic")
            lines = a["Scenes/PlayableArena.kscene"].decode().splitlines()
            ids = [int(line.split()[1]) for line in lines if line.startswith("entity ")]
            self.assertEqual(len(ids), len(set(ids)))
            # The built-in player visual contributes one additional parent.
            self.assertEqual(sum(line.startswith("parent ") for line in lines),
                             len(self.data["instances"]) + 1)

    def test_authored_spawn_and_controls(self):
        self.data["layout"] = "empty"
        self.data["gameplay"] = {"spawn": [-2, 1.5, 1], "keyboard": {"forward": "I", "back": "K",
                                 "left": "J", "right": "L", "jump": "U", "quit": "Q"}}
        self.write()
        files = g.compile_recipe(self.recipe)
        scene = files["Scenes/PlayableArena.kscene"].decode()
        self.assertIn('transform -2 1.5 1 0 0 0 1 1 1 1', scene)
        self.assertIn('transform -2 7.58 10.1 -0.21643961 0 0 0.97629601 1 1 1', scene)
        self.assertIn('bind "Move" key I 0 1 0\n', files["Config/Input.kinput"].decode())
        self.assertIn('bind "Jump" key U 1 0 0\n', files["Config/Input.kinput"].decode())
        self.assertIn('bind "Move" gamepad-axis LeftX 1 0 0.15\n', files["Config/Input.kinput"].decode())
        self.assertEqual(files, g.compile_recipe(self.recipe))
        for bad in ({"spawn": [7, 1, 0]}, {"spawn": [0, .1, 0]}, {"spawn": [True, 1, 0]},
                    {"keyboard": {"forward": "S"}}, {"keyboard": {"jump": "unknown"}},
                    {"keyboard": {"fire": "F"}}, {"unsupported": True}):
            self.data["gameplay"] = bad
            self.write()
            with self.assertRaises(ValueError):
                g.create_project(self.recipe, self.root / "invalid")
            self.assertFalse((self.root / "invalid").exists())

    def test_native_menu_recipe(self):
        # Compile the actual Player parser, not a Python approximation.
        compiler = os.environ.get("CXX") or shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("Requires C++23 compiler")
        validator = self.root / "menu-validator"
        subprocess.run([compiler, "-std=c++23", "-O2", "-I", str(ROOT / "Runtime/KairoPlayer"),
                        str(ROOT / "Samples/AuthoredMenu/MenuDocumentTool.cpp"), "-o", str(validator)],
                       check=True, timeout=60)
        menu = [{"id": "02-quit", "label": "EXIT", "action": "quit", "rect": [300, 520, 400, 140]},
                {"id": "01-play", "label": 'START "GAME"', "action": "resume", "rect": [300, 320, 400, 140]}]
        self.data["gameplay"] = {"menu": menu}
        self.write()
        with self.assertRaises(ValueError):
            g.compile_recipe(self.recipe)
        project = g.create_project(self.recipe, self.root / "valid", menu_validator=validator)
        document = (project.parent / "Config/Menu.kmenu").read_bytes()
        self.assertEqual(document, subprocess.run([str(validator)], input=document,
                         check=True, capture_output=True, timeout=5).stdout)
        self.assertLess(document.index(b"01-play"), document.index(b"02-quit"))
        for rect in ([300, 320, 400, 140], [900, 500, 400, 140], [0, 0, 0, 100]):
            menu[0]["rect"] = rect
            self.write()
            with self.assertRaises(ValueError):
                g.create_project(self.recipe, self.root / "invalid", menu_validator=validator)
            self.assertFalse((self.root / "invalid").exists())

    def test_instance_validation(self):
        for instance in [{"name": "A", "prefab": "box"},
                         {"name": "B", "prefab": "missing"},
                         {"name": "B", "prefab": "box", "scale": [1, 2, 1]},
                         {"name": "B", "prefab": "box", "position": [True, 0, 0]}]:
            self.data["instances"][1] = instance
            self.write()
            with self.assertRaises(ValueError):
                g.compile_recipe(self.recipe)


if __name__ == "__main__":
    unittest.main()
