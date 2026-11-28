import json
from pathlib import Path
import shutil
import tempfile
import unittest
import studio_game_export as export


class Tests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.studio = self.root / 'studio'; self.studio.mkdir()
        self.project = self.root / 'project'; self.project.mkdir()
        (self.project / 'LICENSE').write_text('user license')
        self.player = self.root / 'KairoPlayer'; shutil.copyfile('/bin/true', self.player)
        self.index = {}
        self.put('build/game-clang/Runtime/KairoPlayer/KairoPlayer', self.player.read_bytes())
        lock = self.put('workspace.lock', b'fixture lock')
        first = self.put('KairoNotices/KairoGameEngine.txt', b'first party')
        self.put('KairoNotices/notice-index.json', json.dumps({'schema': 1, 'workspace_lock_sha256': lock,
            'notices': [{'path': 'KairoGameEngine.txt', 'sha256': first}]}).encode())
        deps = []
        for name in ('kairoassetsstb', 'kairoassetscgltf', 'nlohmann_json', 'kairoglad', 'dearimgui', 'imguizmo'):
            path = name + '/0000.txt'
            checksum = self.put('ThirdPartySourceNotices/' + path, name.encode())
            deps.append({'name': name, 'scope': 'editor' if name in ('dearimgui', 'imguizmo') else 'runtime',
                         'notices': [{'path': path, 'sha256': checksum}]})
        self.put('ThirdPartySourceNotices/source-notices.json', json.dumps({'schema': 1, 'distribution_scope': 'studio',
            'dependencies': deps, 'excluded_fetched_sources': ['catch2'], 'third_party_licenses_verified': False}).encode())
        (self.studio / export.INDEX).write_text(json.dumps(self.index))

    def put(self, name, data):
        path = self.studio / name; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(data)
        checksum = export.digest(path)
        self.index[name] = {'bytes': len(data), 'sha256': checksum}
        return checksum

    def test_scope_counts_and_user_license(self):
        count, size = export.stage_notices(self.studio, self.player, self.project)
        paths = [p for p in self.project.rglob('*') if p.is_file() and p.name != 'LICENSE']
        self.assertEqual((count, size), (len(paths), sum(p.stat().st_size for p in paths)))
        self.assertEqual(count, 7)
        self.assertEqual((self.project / 'LICENSE').read_text(), 'user license')
        self.assertFalse((self.project / 'ThirdPartySourceNotices/dearimgui').exists())
        self.assertIn(b'project-files 8\n', export.updated_manifest(b'project-files 1\nproject-bytes 12\n', count, size))

    def test_tampering_and_player_mismatch(self):
        self.player.write_bytes(b'wrong')
        with self.assertRaises(ValueError): export.stage_notices(self.studio, self.player, self.project)
        shutil.copyfile('/bin/true', self.player)
        (self.studio / 'KairoNotices/KairoGameEngine.txt').write_bytes(b'changed')
        with self.assertRaises(ValueError): export.stage_notices(self.studio, self.player, self.project)
        self.assertEqual(list(self.project.iterdir()), [self.project / 'LICENSE'])

    def test_authored_conflict_preserved(self):
        (self.project / 'kaironotices').mkdir()
        with self.assertRaises(ValueError): export.stage_notices(self.studio, self.player, self.project)
        self.assertTrue((self.project / 'kaironotices').exists())
        self.assertFalse((self.project / 'ThirdPartySourceNotices').exists())


if __name__ == '__main__':
    unittest.main()
