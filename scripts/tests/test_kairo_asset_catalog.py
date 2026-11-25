"""Exercise actual staged GLB bytes, recipe schema, provenance and failure paths."""
import hashlib
import json
from pathlib import Path
import struct
import tempfile
import unittest
import kairo_asset_catalog as catalog


class CatalogTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        doc = b'{"asset":{"version":"2.0"}}'
        doc += b' ' * (-len(doc) % 4)
        self.glb = struct.pack('<4sIIII', b'glTF', 2, 20 + len(doc), len(doc), 0x4E4F534A) + doc
        (self.root / 'model.glb').write_bytes(self.glb)
        (self.root / 'LICENSE.txt').write_text('Synthetic test fixture; test-only license metadata.')
        self.record = dict(id='cube', name='Blue Cube', file='model.glb', author='Fixture', license='CC0-1.0', source='generated-test', sha256=hashlib.sha256(self.glb).hexdigest(), license_file='LICENSE.txt', tags=['geometry', 'blue'])
        self.manifest = self.root / 'catalog.json'
        self.write()

    def write(self):
        self.manifest.write_text(json.dumps({'version': 1, 'assets': [self.record]}))

    def test_search_and_stage(self):
        _, entries = catalog.load_catalog(self.manifest)
        self.assertEqual(catalog.search(entries, 'BLUE geometry'), [self.record])
        self.assertEqual(catalog.search(entries, 'missing'), [])
        out = self.root / 'staged'
        recipe = json.loads(catalog.stage(self.manifest, ['cube'], out).read_text())
        self.assertEqual(recipe['assets']['cube']['file'], 'assets/cube.glb')
        self.assertEqual((out / 'assets/cube.glb').read_bytes(), self.glb)
        self.assertEqual((out / 'licenses/cube.txt').read_bytes(), (self.root / 'LICENSE.txt').read_bytes())
        self.assertEqual(json.loads((out / 'CATALOG_PROVENANCE.json').read_text())[0]['sha256'], self.record['sha256'])
        with self.assertRaises(FileExistsError):
            catalog.stage(self.manifest, ['cube'], out)

    def test_tamper_and_license(self):
        for field, bad in [('sha256', '0' * 64), ('license', 'unknown'), ('file', '../model.glb'), ('license_file', '/etc/passwd')]:
            original = self.record[field]
            self.record[field] = bad
            self.write()
            with self.assertRaises((ValueError, OSError)):
                catalog.stage(self.manifest, ['cube'], self.root / 'rejected')
            self.assertFalse((self.root / 'rejected').exists())
            self.record[field] = original
        self.write()
        (self.root / 'model.glb').write_bytes(self.glb + b'x')
        with self.assertRaises(ValueError):
            catalog.stage(self.manifest, ['cube'], self.root / 'rejected')

    def test_duplicates_and_symlink(self):
        with self.assertRaises(ValueError):
            catalog.stage(self.manifest, ['cube', 'cube'], self.root / 'rejected')
        try:
            (self.root / 'link.glb').symlink_to(self.root / 'model.glb')
        except OSError:
            self.skipTest('Symlink creation not supported')
        self.record['file'] = 'link.glb'
        self.write()
        with self.assertRaises(ValueError):
            catalog.load_catalog(self.manifest)


if __name__ == '__main__':
    unittest.main()
