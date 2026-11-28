"""Exercise the immutable installation audit using real ZIP/filesystem fixtures."""
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

import kairo_release as release
from install_local_windows_game import DLLS, install
from test_install_local_windows_game import pe
from test_install_local_macos_game import image, string_command
from verify_private_game_install import audit, record


class Tests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def bundle(self, windows):
        bundle = self.root / 'bundle'
        (bundle / 'bin').mkdir(parents=True)
        (bundle / 'project' / 'Assets').mkdir(parents=True)
        player = 'bin/Player.exe' if windows else 'bin/Player'
        binary = pe() if windows else image(2, [])
        (bundle / player).write_bytes(binary)
        (bundle / 'project/Project.kproject').write_bytes(b'fixture')
        (bundle / 'project/Assets/texture').write_bytes(b'pixels')
        (bundle / ('run.cmd' if windows else 'run.sh')).write_text('fixture')
        (bundle / 'package.kmanifest').write_text(
            'kairo-package 1\nproject project/Project.kproject\nprofile Release\nkind release\n'
            f'engine-version 0.1.0\nplayer {player}\nproject-files 2\nproject-bytes 13\n')
        archive = self.root / 'Game.zip'
        release.export(bundle, archive)
        return archive, player

    def windows(self):
        archive, player = self.bundle(True)
        runtime = self.root / 'runtime'
        runtime.mkdir()
        hashes = {}
        for name in DLLS:
            (runtime / name).write_bytes(pe(dll=True) + name.encode())
            hashes[name] = record(runtime / name)['sha256']
        dest = self.root / 'installed'
        install(archive, dest, runtime, hashes)
        return archive, dest, record(dest / 'local-runtime.json')['sha256']

    def test_real_windows_install(self):
        archive, dest, digest = self.windows()
        result = audit(archive, dest, digest)
        self.assertEqual(result['verified_files'], 7)
        self.assertTrue(result['immutable_file_integrity_verified'])
        self.assertFalse(result['dependency_closure_verified'])
        self.assertFalse(result['redistribution_approved'])

    def test_modified_files_and_unexpected_inventory(self):
        archive, dest, digest = self.windows()
        for name in ('project/Assets/texture', 'bin/Player.exe', 'bin/glfw3.dll'):
            path = dest / name
            original = path.read_bytes()
            path.write_bytes(original + b'changed')
            with self.assertRaisesRegex(ValueError, 'integrity differs'):
                audit(archive, dest, digest)
            path.write_bytes(original)
        extra = dest / 'bin/unexpected.dll'
        extra.write_bytes(b'extra')
        with self.assertRaisesRegex(ValueError, 'inventory differs'):
            audit(archive, dest, digest)
        extra.unlink()
        (dest / 'unknown').mkdir()
        with self.assertRaisesRegex(ValueError, 'Unexpected directory'):
            audit(archive, dest, digest)

    def test_receipt_and_index_tamper(self):
        archive, dest, digest = self.windows()
        path = dest / 'local-runtime.json'
        original = path.read_bytes()
        path.write_bytes(original + b' ')
        with self.assertRaisesRegex(ValueError, 'Receipt hash differs'):
            audit(archive, dest, digest)
        path.write_bytes(original)
        index = dest / 'release.json'
        data = json.loads(index.read_text())
        data['profile'] = 'Debug'
        index.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError, 'index differs'):
            audit(archive, dest, digest)

    def test_symlink_rejected(self):
        archive, dest, digest = self.windows()
        path = dest / 'project/Assets/texture'
        external = self.root / 'external'
        external.write_bytes(path.read_bytes())
        path.unlink()
        try:
            path.symlink_to(external)
        except OSError:
            self.skipTest('Host does not allow symlink creation')
        with self.assertRaisesRegex(ValueError, 'Unexpected file type'):
            audit(archive, dest, digest)

    def test_macos_rewritten_player_receipt_fixture(self):
        # Portable fixture tests accounting for changed bytes, not Mach-O
        # relocation or code signing; those are separate native gates.
        archive, player = self.bundle(False)
        dest = self.root / 'installed'
        release.unpack(archive, dest)
        (dest / player).write_bytes(image(2, [string_command(0xc, '@loader_path/libglfw.3.dylib', 24)]))
        names = ('libglfw.3.dylib', 'libvulkan.1.dylib')
        for name in names:
            (dest / 'bin' / name).write_bytes(name.encode())
        receipt = {
            'schema': 1, 'target': 'macos-arm64',
            'source_archive_sha256': record(archive)['sha256'],
            'source_library_hashes': {name: hashlib.sha256(name.encode()).hexdigest() for name in names},
            'local_runtime_files': {name: record(dest / name) for name in
                                    [player, *('bin/' + n for n in names)]},
            'private_installation_only': True, 'adhoc_signed': True,
            'redistribution_approved': False, 'dependency_closure_verified': False,
        }
        path = dest / 'local-runtime.json'
        path.write_text(json.dumps(receipt))
        digest = record(path)['sha256']
        self.assertEqual(audit(archive, dest, digest)['verified_files'], 7)
        # Even with an updated receipt hash, an extra local-file claim cannot
        # authorize changing an authored asset or bypass the original ZIP.
        receipt['local_runtime_files']['project/Assets/texture'] = record(dest / 'project/Assets/texture')
        path.write_text(json.dumps(receipt))
        with self.assertRaisesRegex(ValueError, 'Unexpected local runtime inventory'):
            audit(archive, dest, record(path)['sha256'])


if __name__ == '__main__':
    unittest.main()
