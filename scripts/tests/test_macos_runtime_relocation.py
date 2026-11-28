import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from macos_runtime_relocation import ARCHIVE_SHA, IMPORT_SHA, EXPECTED, plan, verify_source


def evidence():
    imports = list(EXPECTED)
    edges = [{'import': name, 'status': 'unique-file-candidate', 'candidates': ['/opt/homebrew/Cellar/' + EXPECTED[name]], 'sha256': str(i) * 64} for i, name in enumerate(imports, 1)]
    return {'target': 'macos-arm64', 'tools': [
        {'path': f'build/game-clang/Runtime/KairoPlayer/tool{i}', 'sha256': '0' * 64,
         'imports': imports + ['/usr/lib/libSystem.B.dylib'] if i == 0 else ['/usr/lib/libSystem.B.dylib'],
         'host_resolution': {'images': [{'edges': edges}] if i == 0 else []}}
        for i in range(7)]}


class Tests(unittest.TestCase):
    def test_exact_receipt_and_tamper_rejection(self):
        self.assertRegex(ARCHIVE_SHA, r'^[0-9a-f]{64}$')
        self.assertRegex(IMPORT_SHA, r'^[0-9a-f]{64}$')
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory)
            archive = b'archive fixture'
            imports = json.dumps(evidence()).encode()
            a = hashlib.sha256(archive).hexdigest()
            i = hashlib.sha256(imports).hexdigest()
            (source / 'KairoStudio.zip').write_bytes(archive)
            (source / 'native-imports.json').write_bytes(imports)
            receipt = {'commit': '786e0c66a156fd3b37efdd8ba1bf5bcd7a3208ff',
                       'native_target': 'macos-arm64', 'native_tools_inspected': 7,
                       'hub_template_validation': True, 'installed_creator_export_validated': True,
                       'archive_sha256': a, 'native_import_inventory_sha256': i}
            (source / 'receipt.json').write_text(json.dumps(receipt))
            with patch('macos_runtime_relocation.ARCHIVE_SHA', a), patch('macos_runtime_relocation.IMPORT_SHA', i):
                self.assertEqual(verify_source(source), evidence())
                (source / 'native-imports.json').write_bytes(imports + b' ')
                with self.assertRaisesRegex(ValueError, 'Unexpected source import inventory'):
                    verify_source(source)
                (source / 'native-imports.json').write_bytes(imports)
                receipt['native_import_inventory_sha256'] = '0' * 64
                (source / 'receipt.json').write_text(json.dumps(receipt))
                with self.assertRaisesRegex(ValueError, 'Unexpected source import inventory'):
                    verify_source(source)

    def test_expected_plan(self):
        tools, libraries = plan(evidence())
        self.assertEqual(len(tools), 7)
        self.assertEqual(set(libraries), set(EXPECTED))
        self.assertEqual(sum(bool(row['changes']) for row in tools.values()), 1)

    def test_unsafe_or_unknown_rejected(self):
        for mutate in (lambda e: e['tools'][0].update(path='build/game-clang/Runtime/../tool'),
                       lambda e: e['tools'][0]['imports'].append('/tmp/unknown.dylib'),
                       lambda e: e['tools'][0]['host_resolution']['images'][0]['edges'][0].update(status='ambiguous'),
                       lambda e: e['tools'][1].update(path=e['tools'][0]['path'])):
            data = copy.deepcopy(evidence())
            mutate(data)
            with self.assertRaises(ValueError):
                plan(data)


if __name__ == '__main__':
    unittest.main()
