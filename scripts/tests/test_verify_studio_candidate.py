"""Synthetic candidate evidence tests, without running an untrusted archive."""
import hashlib
import json
from pathlib import Path
import stat
import tempfile
import unittest
import zipfile

from verify_studio_candidate import verify_candidate, digest


class Tests(unittest.TestCase):
    def fixture(self, root):
        self.commit = 'a' * 40
        self.files = {f'bin/tool{i}': b'native fixture' + bytes([i]) for i in range(7)}
        self.files.update({'workspace.lock': b'lock', 'KairoNotices/notice-index.json': b'{}',
                           'ThirdPartySourceNotices/source-notices.json': b'{}'})
        self.index = {n: {'bytes': len(d), 'sha256': hashlib.sha256(d).hexdigest(), 'executable': False}
                      for n, d in self.files.items()}
        with zipfile.ZipFile(root / 'KairoStudio.zip', 'w') as z:
            for name, data in {**self.files, 'studio-inventory.json': json.dumps(self.index).encode()}.items():
                info = zipfile.ZipInfo(name)
                info.create_system = 3
                info.external_attr = (stat.S_IFREG | 0o644) << 16
                z.writestr(info, data)
        imports = {'schema': 1, 'target': 'linux-x86_64',
                   'tools': [{'path': n, 'sha256': self.index[n]['sha256']} for n in self.files if n.startswith('bin/')]}
        (root / 'native-imports.json').write_text(json.dumps(imports))
        self.receipt = {'schema': 1, 'commit': self.commit, 'native_target': 'linux-x86_64',
                        'archive_sha256': digest(root / 'KairoStudio.zip'),
                        'native_import_inventory_sha256': digest(root / 'native-imports.json'),
                        'hub_template_validation': True, 'installed_creator_export_validated': True,
                        'launcher_help_validated': True, 'file_count': len(self.index) + 1, 'native_tools_inspected': 7,
                        'workspace_lock_sha256': self.index['workspace.lock']['sha256'],
                        'first_party_notice_index_sha256': self.index['KairoNotices/notice-index.json']['sha256'],
                        'third_party_source_notice_index_sha256': self.index['ThirdPartySourceNotices/source-notices.json']['sha256'],
                        'complete_creator_workflow': {'schema': 1, 'template': 'signalvault',
                            'native_authoring_validation': True, 'invalid_and_stale_edits_rejected': True,
                            'authored_ui_and_campaign_edits_exported': True, 'previous_release_and_private_state_preserved': True,
                            'both_relocated_players_validated': True, 'relocated_graphical_smoke': True,
                            'original_archive_sha256': 'b' * 64, 'edited_archive_sha256': 'c' * 64,
                            'requested_renderer': 'vulkan'}}
        self.save(root)

    def save(self, root):
        (root / 'receipt.json').write_text(json.dumps(self.receipt))

    def test_valid_and_wrong_commit(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.fixture(root)
            result = verify_candidate(root, 'linux-x86_64', self.commit)
            self.assertEqual(result['files_verified'], 10)
            self.assertFalse(result['hardware_gpu_qualified'])
            with self.assertRaisesRegex(ValueError, 'commit or target'):
                verify_candidate(root, 'linux-x86_64', 'd' * 40)

    def test_receipt_count_includes_inventory_itself(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.fixture(root)
            self.assertEqual(self.receipt['file_count'], 11)
            self.assertEqual(verify_candidate(root, 'linux-x86_64', self.commit)['files_verified'], 10)
            for count in (10, 12):
                self.receipt['file_count'] = count
                self.save(root)
                with self.assertRaisesRegex(ValueError, 'Studio inventory differs'):
                    verify_candidate(root, 'linux-x86_64', self.commit)

    def test_missing_render_evidence(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.fixture(root)
            self.receipt['complete_creator_workflow']['relocated_graphical_smoke'] = False
            self.save(root)
            with self.assertRaisesRegex(ValueError, 'relocated_graphical_smoke'):
                verify_candidate(root, 'linux-x86_64', self.commit)

    def test_archive_tamper(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.fixture(root)
            with (root / 'KairoStudio.zip').open('ab') as stream:
                stream.write(b'changed')
            with self.assertRaisesRegex(ValueError, 'digest differs'):
                verify_candidate(root, 'linux-x86_64', self.commit)


if __name__ == '__main__':
    unittest.main()
