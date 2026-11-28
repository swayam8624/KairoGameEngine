"""Native launch preflight tests with real ZIPs and static binary fixtures."""
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import kairo_release as release
import test_verify_private_game_install as fixtures
from install_local_macos_game import LIBRARIES
from test_install_local_macos_game import image, string_command
from launch_private_game import host_target, renderer_for, preflight, launch
from verify_private_game_install import record


class LaunchTests(unittest.TestCase):
    bundle = fixtures.Tests.bundle
    windows = fixtures.Tests.windows

    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def test_host_and_renderer_policy(self):
        self.assertEqual(host_target('Windows', 'AMD64'), 'windows-x86_64')
        self.assertEqual(host_target('Darwin', 'arm64'), 'macos-arm64')
        with self.assertRaises(ValueError):
            host_target('Linux', 'x86_64')
        with self.assertRaises(ValueError):
            host_target('Windows', 'i386')
        self.assertEqual(renderer_for('windows-x86_64', 'vulkan', True), 'vulkan')
        self.assertEqual(renderer_for('macos-arm64', 'metal', True), 'metal')
        self.assertEqual(renderer_for('windows-x86_64', 'd3d12', False), 'd3d12')
        for target, renderer in [('windows-x86_64', 'metal'), ('windows-x86_64', 'd3d12'),
                                 ('macos-arm64', 'd3d12'), ('macos-arm64', 'opengl')]:
            with self.assertRaises(ValueError):
                renderer_for(target, renderer, True)

    def test_windows_preflight_and_mismatch(self):
        archive, dest, digest = self.windows()
        result = preflight(archive, dest, digest, 'd3d12', system='Windows', machine='AMD64')
        self.assertTrue(result['native_import_preflight_passed'])
        self.assertFalse(result['gpu_execution_verified'])
        self.assertFalse(result['actual_loader_selection_verified'])
        self.assertEqual(Path(result['player']), dest / 'bin/Player.exe')
        with self.assertRaisesRegex(ValueError, 'Native host mismatch'):
            preflight(archive, dest, digest, system='Darwin', machine='arm64')
        with self.assertRaisesRegex(ValueError, 'Renderer is incompatible'):
            preflight(archive, dest, digest, 'metal', system='Windows', machine='AMD64')
        (dest / 'bin/glfw3.dll').write_bytes(b'corrupt')
        with self.assertRaisesRegex(ValueError, 'integrity differs'):
            preflight(archive, dest, digest, system='Windows', machine='AMD64')

    def test_macos_relocated_imports(self):
        archive, player = self.bundle(False)
        dest = self.root / 'installed'
        release.unpack(archive, dest)
        names = tuple(LIBRARIES.values())
        imports = [string_command(0xc, '@loader_path/' + name, 24) for name in names]
        (dest / player).write_bytes(image(2, imports))
        for name in names:
            file = dest / 'bin' / name
            file.write_bytes(image(6, [string_command(0xd, '@loader_path/' + name, 24)]))
            file.chmod(0o755)
        receipt = {'schema': 1, 'target': 'macos-arm64',
                   'source_archive_sha256': record(archive)['sha256'],
                   'source_library_hashes': {name: 'a' * 64 for name in names},
                   'local_runtime_files': {name: record(dest / name) for name in
                                           [player, *('bin/' + n for n in names)]},
                   'private_installation_only': True, 'adhoc_signed': True,
                   'redistribution_approved': False, 'dependency_closure_verified': False}
        path = dest / 'local-runtime.json'
        path.write_text(json.dumps(receipt))
        result = preflight(archive, dest, record(path)['sha256'], 'metal', system='Darwin', machine='arm64')
        self.assertTrue(result['native_import_preflight_passed'])
        (dest / player).write_bytes(image(2, [string_command(0xc, '/tmp/unknown.dylib', 24)]))
        receipt['local_runtime_files'][player] = record(dest / player)
        path.write_text(json.dumps(receipt))
        with self.assertRaisesRegex(ValueError, 'expected relative imports'):
            preflight(archive, dest, record(path)['sha256'], 'metal', system='Darwin', machine='arm64')

    def test_launch_uses_exact_arguments_and_propagates_status(self):
        archive, dest, digest = self.windows()
        with patch('launch_private_game.platform.system', return_value='Windows'), \
             patch('launch_private_game.platform.machine', return_value='AMD64'), \
             patch('launch_private_game.subprocess.run') as run:
            run.return_value.returncode = 7
            self.assertEqual(launch(archive, dest, digest, 'd3d12', 'smoke'), 7)
            run.assert_called_once_with([str(dest / 'bin/Player.exe'), str(dest / 'project/Project.kproject'),
                                         '--renderer', 'd3d12', '--smoke'], cwd=dest / 'project')
            run.reset_mock()
            (dest / 'project/Assets/texture').write_bytes(b'tampered')
            with self.assertRaises(ValueError):
                launch(archive, dest, digest, 'd3d12')
            run.assert_not_called()


if __name__ == '__main__':
    unittest.main()
