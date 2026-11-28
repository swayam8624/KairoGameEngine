import pathlib
import shutil
import subprocess
import tempfile
import unittest
import build_notice_inventory as audit


class Tests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = pathlib.Path(temp.name)
        self.repo = self.root / '_deps/fixture-src'
        self.repo.mkdir(parents=True)
        self.git('init', '-q')
        self.git('config', 'user.name', 'Fixture')
        self.git('config', 'user.email', 'fixture@example.invalid')
        self.git('remote', 'add', 'origin', 'https://github.com/example/fixture.git')
        (self.repo / 'LICENSE').write_bytes(b'original notice\n')
        self.git('add', '.')
        self.git('commit', '-qm', 'fixture')
        self.pin = self.git('rev-parse', 'HEAD')
        self.deps = {'fixture': ('example/fixture', self.pin, 'runtime')}
        self.dest = self.root / 'result'

    def git(self, *args):
        return subprocess.check_output(['git', '-C', str(self.repo), *args]).decode().strip()

    def collect(self):
        return audit.collect(self.root, self.dest, dependencies=self.deps)

    def test_committed_notice_and_dirty_worktree(self):
        (self.repo / 'LICENSE').write_text('edited')
        (self.root / '_deps/unknown-src').mkdir()
        result = self.collect()
        self.assertEqual((self.dest / 'fixture/0000.txt').read_bytes(), b'original notice\n')
        self.assertEqual(result['dependencies'][0]['tracked_worktree_changes'], ['LICENSE'])
        self.assertEqual(result['unclassified_fetched_sources'], ['unknown'])
        self.assertFalse(result['third_party_licenses_verified'])

    def test_wrong_pin_and_origin(self):
        self.deps['fixture'] = ('example/fixture', '0' * 40, 'runtime')
        with self.assertRaises(ValueError): self.collect()
        self.deps['fixture'] = ('wrong/fixture', self.pin, 'runtime')
        with self.assertRaises(ValueError): self.collect()
        self.assertFalse(self.dest.exists())

    def test_distribution_scopes_and_exact_bytes(self):
        for name, scope in (('editor', 'editor'), ('testing', 'test')):
            shutil.copytree(self.repo, self.root / ('_deps/' + name + '-src'))
            self.deps[name] = ('example/fixture', self.pin, scope)
        game = audit.collect(self.root, self.dest, dependencies=self.deps, distribution=True)
        self.assertEqual(game['distribution_scope'], 'game')
        self.assertEqual([r['name'] for r in game['dependencies']], ['fixture'])
        self.assertEqual(game['excluded_fetched_sources'], ['editor', 'testing'])
        self.assertEqual((self.dest / 'fixture/0000.txt').read_bytes(), b'original notice\n')
        self.assertFalse((self.dest / 'testing').exists())
        studio = audit.collect(self.root, self.root / 'studio', editor=True,
                               dependencies=self.deps, distribution=True)
        self.assertEqual([r['name'] for r in studio['dependencies']], ['editor', 'fixture'])
        self.assertEqual(studio['excluded_fetched_sources'], ['testing'])
        self.assertFalse(studio['third_party_licenses_verified'])
        shutil.rmtree(self.root / '_deps/editor-src')
        with self.assertRaises(ValueError):
            audit.collect(self.root, self.root / 'missing', editor=True,
                          dependencies=self.deps, distribution=True)
        self.assertFalse((self.root / 'missing').exists())

    def test_missing_and_no_overwrite(self):
        self.dest.mkdir()
        (self.dest / 'keep').touch()
        with self.assertRaises(ValueError): self.collect()
        self.assertTrue((self.dest / 'keep').exists())
        self.dest.rename(self.root / 'saved')
        self.git('rm', 'LICENSE')
        self.git('commit', '-qm', 'remove notice')
        self.deps['fixture'] = ('example/fixture', self.git('rev-parse', 'HEAD'), 'runtime')
        with self.assertRaises(ValueError): self.collect()
        self.assertFalse(self.dest.exists())


if __name__ == '__main__':
    unittest.main()
