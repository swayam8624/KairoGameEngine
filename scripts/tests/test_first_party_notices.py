import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

import first_party_notices as notices


def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args], stderr=subprocess.DEVNULL).decode().strip()


def init(repo, files):
    repo.mkdir()
    git(repo, 'init', '-q')
    git(repo, 'config', 'user.name', 'Fixture')
    git(repo, 'config', 'user.email', 'fixture@example.invalid')
    for name, data in files.items():
        (repo / name).write_bytes(data)
    git(repo, 'add', '.')
    git(repo, 'commit', '-qm', 'Fixture')
    return git(repo, 'rev-parse', 'HEAD')


class Tests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.component = self.root / 'KairoFixture'
        self.blob = b'MIT License\n\nOriginal fixture\n'
        self.pin = init(self.component, {'LICENSE': self.blob})
        self.engine = self.root / 'KairoGameEngine'
        self.lock = self.engine / 'workspace.lock'
        init(self.engine, {'LICENSE': self.blob, 'workspace.lock': ('KairoFixture ' + self.pin + '\n').encode()})
        self.dest = self.root / 'notices'

    def stage(self):
        return notices.stage(self.engine, self.lock, self.dest, ('KairoFixture',))

    def test_committed_bytes_and_receipt(self):
        (self.component / 'LICENSE').write_text('modified worktree')
        result = self.stage()
        self.assertEqual(len(result['notices']), 2)
        self.assertFalse(result['third_party_licenses_verified'])
        for row in result['notices']:
            self.assertEqual((self.dest / row['path']).read_bytes(), self.blob)
            self.assertEqual(row['sha256'], hashlib.sha256(self.blob).hexdigest())
        self.assertEqual(json.loads((self.dest / notices.INDEX).read_bytes()), result)

    def test_wrong_pin_and_dirty_lock(self):
        git(self.component, 'commit', '--allow-empty', '-qm', 'Wrong pin')
        with self.assertRaises(ValueError): self.stage()
        git(self.component, 'checkout', '--detach', self.pin)
        self.lock.write_text('KairoFixture ' + '0' * 40 + '\n')
        with self.assertRaises(ValueError): self.stage()
        self.assertFalse(self.dest.exists())

    def test_missing_notice_and_existing_output(self):
        self.dest.mkdir()
        (self.dest / 'keep').write_text('keep')
        with self.assertRaises(FileExistsError): self.stage()
        self.assertEqual((self.dest / 'keep').read_text(), 'keep')
        git(self.component, 'rm', 'LICENSE')
        git(self.component, 'commit', '-qm', 'Remove notice')
        self.lock.write_text('KairoFixture ' + git(self.component, 'rev-parse', 'HEAD') + '\n')
        git(self.engine, 'commit', '-qam', 'Update pin')
        with self.assertRaises(ValueError): self.stage()


if __name__ == '__main__':
    unittest.main()
