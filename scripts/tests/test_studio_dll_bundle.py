import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch

import studio_dll_bundle as bundle


class BundleTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.stage = self.root / 'stage'
        self.tools = ['editor/Editor.exe', 'player/Player.exe']
        for path in self.tools:
            p = self.stage / path
            p.parent.mkdir(parents=True)
            p.write_bytes(b'tool')
        self.dlls = self.root / 'dlls'; self.dlls.mkdir()
        self.notices = self.root / 'notices'; self.notices.mkdir()
        self.dll = self.dlls / 'GLFW3.dll'
        dos = bytearray(64); dos[:2] = b'MZ'; struct.pack_into('<I', dos, 60, 64)
        pe = bytearray(26); pe[:4] = b'PE\0\0'; struct.pack_into('<H', pe, 4, 0x8664)
        struct.pack_into('<H', pe, 22, 0x2000); pe[24:] = b'\x0b\x02'
        self.dll.write_bytes(dos + pe)
        self.notice = self.notices / 'glfw.txt'; self.notice.write_text('fixture notice')
        self.row = {'name': 'glfw3.dll', 'sha256': bundle.digest(self.dll), 'notice': 'glfw.txt',
                    'notice_sha256': bundle.digest(self.notice), 'origin': 'fixture only'}
        self.graph = [{'name': 'glfw3.dll', 'status': 'explicit-root-candidate',
                       'sha256': self.row['sha256'], 'candidates': [str(self.dll)]},
                      {'name': 'kernel32.dll', 'status': 'host-system-directory-present'}]
        self.policy = self.root / 'policy.json'
        self.write_policy([self.row])

    def write_policy(self, rows):
        self.policy.write_text(json.dumps({'schema': 1, 'target': 'windows-x86_64', 'libraries': rows}))

    def execute(self):
        return bundle.stage(self.stage, self.tools, self.graph, self.policy, self.notices, [self.dlls], 'windows-x86_64')

    def assert_clean(self):
        self.assertFalse((self.stage / 'editor/glfw3.dll').exists())
        self.assertFalse((self.stage / 'player/glfw3.dll').exists())
        self.assertFalse((self.stage / 'ThirdPartyNotices').exists())

    def test_exact_approved_bytes_and_no_system_copy(self):
        result = self.execute()
        self.assertEqual(len(result['libraries']), 1)
        self.assertFalse(result['third_party_licenses_verified'])
        for path in result['libraries'][0]['copies']:
            self.assertEqual(bundle.digest(self.stage / path), self.row['sha256'])
        self.assertEqual(bundle.digest(self.stage / result['libraries'][0]['notice']), self.row['notice_sha256'])
        self.assertFalse((self.stage / 'editor/kernel32.dll').exists())

    def test_missing_approval_and_unresolved_graph(self):
        self.write_policy([])
        with self.assertRaises(ValueError): self.execute()
        self.write_policy([self.row])
        self.graph.append({'name': 'missing.dll', 'status': 'unresolved'})
        with self.assertRaises(ValueError): self.execute()
        self.assert_clean()

    def test_notice_and_dll_changes_rejected(self):
        self.notice.write_text('changed')
        with self.assertRaises(ValueError): self.execute()
        self.notice.write_text('fixture notice')
        self.dll.write_bytes(self.dll.read_bytes() + b'changed')
        with self.assertRaises(ValueError): self.execute()
        self.assert_clean()

    def test_wrong_machine_rejected_even_with_approved_hash(self):
        data = bytearray(self.dll.read_bytes()); struct.pack_into('<H', data, 68, 0xaa64)
        self.dll.write_bytes(data)
        self.row['sha256'] = self.graph[0]['sha256'] = bundle.digest(self.dll)
        self.write_policy([self.row])
        with self.assertRaises(ValueError): self.execute()
        self.assert_clean()

    def test_existing_and_rollback(self):
        old = self.stage / 'player/GLFW3.DLL'; old.write_bytes(b'keep')
        with self.assertRaises(ValueError): self.execute()
        self.assertEqual(old.read_bytes(), b'keep'); old.unlink()
        with patch.object(bundle.shutil, 'copyfileobj', side_effect=OSError('fixture failure')):
            with self.assertRaises(OSError): self.execute()
        self.assert_clean()

    def test_duplicate_json_rejected(self):
        self.policy.write_text('{"schema":1,"schema":1,"target":"windows-x86_64","libraries":[]}')
        with self.assertRaises(ValueError): self.execute()
        self.assert_clean()


if __name__ == '__main__':
    unittest.main()
