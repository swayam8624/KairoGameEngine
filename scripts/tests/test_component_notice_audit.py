import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

import component_notice_audit as audit


def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args], stderr=subprocess.DEVNULL).decode().strip()


def init(repo, files):
    repo.mkdir()
    git(repo, 'init', '-q')
    git(repo, 'config', 'user.name', 'Fixture')
    git(repo, 'config', 'user.email', 'fixture@example.invalid')
    for name, content in files.items():
        path = repo / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
    git(repo, 'add', '.')
    git(repo, 'commit', '-qm', 'Fixture')
    return git(repo, 'rev-parse', 'HEAD')


class NoticeTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.component = self.root / 'KairoFixture'
        self.pin = init(self.component, {'LICENSE.txt': b'original\r\n', 'vendor/COPYING.md': b'vendor',
                                         'src/main.cpp': b'code', 'docs/other.md': b'other'})
        self.engine = self.root / 'KairoGameEngine'
        init(self.engine, {'workspace.lock': ('KairoFixture ' + self.pin + '\n').encode(), 'LICENSE': b'engine'})
        self.output = self.root / 'evidence'

    def collect(self):
        return audit.collect(self.engine, self.root, self.output, ['KairoFixture'])

    def test_exact_committed_notices_even_if_worktree_modified(self):
        (self.component / 'LICENSE.txt').write_bytes(b'changed')
        result = self.collect()
        component = result['components'][1]
        self.assertEqual(component['commit'], self.pin)
        self.assertTrue(component['root_notice_found'])
        self.assertFalse(result['license_coverage_verified'])
        self.assertEqual([r['source_path'] for r in component['notice_candidates']], ['LICENSE.txt', 'vendor/COPYING.md'])
        for record in component['notice_candidates']:
            content = (self.output / record['evidence_path']).read_bytes()
            self.assertEqual(hashlib.sha256(content).hexdigest(), record['sha256'])
        self.assertEqual((self.output / component['notice_candidates'][0]['evidence_path']).read_bytes(), b'original\r\n')
        self.assertEqual(json.loads((self.output / 'component-notices.json').read_text()), result)

    def test_wrong_pin_and_mutated_lock_fail_without_evidence(self):
        git(self.component, 'commit', '--allow-empty', '-qm', 'Another revision')
        with self.assertRaises(ValueError):
            self.collect()
        self.assertFalse(self.output.exists())
        git(self.component, 'checkout', '--detach', self.pin)
        (self.engine / 'workspace.lock').write_text('KairoFixture ' + '0' * 40 + '\n')
        with self.assertRaises(ValueError):
            self.collect()
        self.assertFalse(self.output.exists())

    def test_existing_output_and_malformed_lock(self):
        self.output.mkdir()
        (self.output / 'keep').write_text('keep')
        with self.assertRaises(ValueError):
            self.collect()
        self.assertEqual((self.output / 'keep').read_text(), 'keep')
        for raw in [b'', b'../repo ' + b'a' * 40, b'KairoFixture bad',
                    (('KairoFixture ' + self.pin + '\n') * 2).encode()]:
            with self.assertRaises(ValueError):
                audit.parse_lock(raw)

    def test_missing_root_notice_is_explicit(self):
        git(self.component, 'rm', 'LICENSE.txt')
        git(self.component, 'commit', '-qm', 'Remove root notice')
        (self.engine / 'workspace.lock').write_text('KairoFixture ' + git(self.component, 'rev-parse', 'HEAD') + '\n')
        git(self.engine, 'commit', '-qam', 'Change pin')
        result = self.collect()['components'][1]
        self.assertFalse(result['root_notice_found'])
        self.assertEqual(len(result['notice_candidates']), 1)


if __name__ == '__main__':
    unittest.main()
