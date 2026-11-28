import hashlib
import json
from pathlib import Path
import tempfile
import unittest
import zipfile
import clean_studio_linux as clean


class Tests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.archive = self.root / 'fixture.zip'
        self.target = self.root / 'Installed Studio'
        self.data = b'fixture executable'

    def archive_with(self, name='bin/tool', wrong=False):
        with zipfile.ZipFile(self.archive, 'w') as z:
            for path, data in [(name, self.data), (clean.INDEX, json.dumps({name: {'bytes': len(self.data), 'sha256': '0' * 64 if wrong else hashlib.sha256(self.data).hexdigest(), 'executable': True}}).encode())]:
                entry = zipfile.ZipInfo(path)
                entry.create_system = 3
                entry.external_attr = 0o100644 << 16
                z.writestr(entry, data)
        return clean.digest(self.archive)

    def test_exact_readonly_extract(self):
        result = clean.extract(self.archive, self.target, self.archive_with())
        self.assertEqual(list(result), ['bin/tool'])
        self.assertEqual((self.target / 'bin/tool').read_bytes(), self.data)
        self.assertEqual((self.target / 'bin/tool').stat().st_mode & 0o777, 0o555)
        self.assertEqual(self.target.stat().st_mode & 0o777, 0o555)
        with self.assertRaises(ValueError):
            clean.extract(self.archive, self.target, clean.digest(self.archive))
        self.target.chmod(0o755)
        (self.target / 'bin').chmod(0o755)

    def test_hash_and_traversal_rejected(self):
        expected = self.archive_with('../escape')
        with self.assertRaises(ValueError): clean.extract(self.archive, self.target, expected)
        with self.assertRaises(ValueError): clean.extract(self.archive, self.target, '0' * 64)
        self.assertFalse(self.target.exists())

    def test_content_mismatch_rolls_back(self):
        with self.assertRaises(ValueError): clean.extract(self.archive, self.target, self.archive_with(wrong=True))
        self.assertFalse(self.target.exists())


if __name__ == '__main__':
    unittest.main()
