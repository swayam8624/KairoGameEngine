"""Real generated GLB -> catalog -> recipe -> project provenance boundary."""
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import tempfile
import unittest
from create_kairo_project import create_project
from kairo_asset_catalog import stage

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("demo_catalog", ROOT / "Samples/AssetCatalog/create_demo_catalog.py")
demo = importlib.util.module_from_spec(spec)
spec.loader.exec_module(demo)


class CatalogProjectTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.catalog = demo.generate(self.root / "catalog")
        self.recipe = stage(self.catalog, ["triangle"], self.root / "staging")

    def test_full_import_and_evidence_survives_staging(self):
        original = (self.root / "catalog/triangle.glb").read_bytes()
        license_bytes = (self.root / "catalog/LICENSE.txt").read_bytes()
        project = create_project(self.recipe, self.root / "project").parent
        shutil.rmtree(self.root / "staging")
        shutil.rmtree(self.root / "catalog")
        record, = json.loads((project / "ASSET_LICENSES.json").read_text())
        self.assertEqual((project / record["path"]).read_bytes(), original)
        self.assertEqual((project / record["license_file"]).read_bytes(), license_bytes)
        self.assertEqual(record["sha256"], hashlib.sha256(original).hexdigest())
        self.assertEqual(record["license_sha256"], hashlib.sha256(license_bytes).hexdigest())
        self.assertIn(record["path"], (project / "Assets.kassets").read_text())
        self.assertIn("scene-instance ", (project / "Scenes/PlayableArena.kscene").read_text())

    def test_tamper_and_partial_evidence_rejected_before_output(self):
        source = self.recipe.parent / "licenses/triangle.txt"
        original = source.read_bytes()
        source.write_bytes(original + b"tampered")
        output = self.root / "rejected"
        with self.assertRaises(ValueError):
            create_project(self.recipe, output)
        self.assertFalse(output.exists())
        source.write_bytes(original)
        doc = json.loads(self.recipe.read_text())
        doc["assets"]["triangle"]["sha256"] = "0" * 64
        self.recipe.write_text(json.dumps(doc))
        with self.assertRaises(ValueError):
            create_project(self.recipe, output)
        self.assertFalse(output.exists())
        doc["assets"]["triangle"]["sha256"] = hashlib.sha256((self.recipe.parent / "assets/triangle.glb").read_bytes()).hexdigest()
        del doc["assets"]["triangle"]["license_sha256"]
        self.recipe.write_text(json.dumps(doc))
        with self.assertRaises(ValueError):
            create_project(self.recipe, output)
        self.assertFalse(output.exists())

    def test_symlink_evidence_rejected(self):
        source = self.recipe.parent / "licenses/triangle.txt"
        original = self.root / "license-copy"
        original.write_bytes(source.read_bytes())
        source.unlink()
        try:
            source.symlink_to(original)
        except OSError:
            self.skipTest("Symlinks unavailable")
        with self.assertRaises(ValueError):
            create_project(self.recipe, self.root / "rejected")


if __name__ == "__main__":
    unittest.main()
