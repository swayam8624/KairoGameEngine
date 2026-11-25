import importlib.util
import json
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
