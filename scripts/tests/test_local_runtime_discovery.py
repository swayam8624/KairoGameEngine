"""Portable native-library discovery and staging fixtures; no libraries loaded."""
import hashlib
from pathlib import Path
import tempfile
import unittest

from install_local_windows_game import DLLS
from install_local_macos_game import LIBRARIES
from test_install_local_windows_game import pe
from test_install_local_macos_game import image, string_command
from local_runtime_discovery import inspect_runtime, staged_runtime


class DiscoveryTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name).resolve()

    def windows(self, directory, machine=0x8664):
        directory.mkdir(parents=True, exist_ok=True)
        pins = []
        for name in DLLS:
            data = pe(machine=machine, dll=True) + name.encode()
            (directory / name).write_bytes(data)
            pins.append(hashlib.sha256(data).hexdigest())
        return pins

    def test_flat_pair_and_pinned_copy(self):
        pins = self.windows(self.root)
        report = inspect_runtime(self.root, 'windows-x86_64')
        self.assertEqual(report['layout'], 'flat')
        self.assertTrue(report['observed_hashes_only'])
        self.assertFalse(report['publisher_authenticity_verified'])
        with staged_runtime(self.root, 'windows-x86_64', *pins) as staged:
            for name in DLLS:
                self.assertEqual((staged / name).read_bytes(), (self.root / name).read_bytes())
        self.assertFalse(staged.exists())
        with self.assertRaisesRegex(ValueError, 'differs from supplied pin'):
            with staged_runtime(self.root, 'windows-x86_64', '0' * 64, pins[1]):
                self.fail('Wrong pins must not stage any runtime')

    def test_vcpkg_triplet_and_ambiguity(self):
        self.windows(self.root / 'installed/x64-windows/bin')
        self.assertEqual(inspect_runtime(self.root, 'windows-x86_64')['layout'], 'vcpkg')
        with self.assertRaisesRegex(ValueError, 'No complete'):
            inspect_runtime(self.root, 'windows-arm64')
        self.windows(self.root / 'installed/arm64-windows/bin', 0xaa64)
        self.assertEqual(inspect_runtime(self.root, 'windows-arm64')['layout'], 'vcpkg')
        self.windows(self.root)
        with self.assertRaisesRegex(ValueError, 'Ambiguous'):
            inspect_runtime(self.root, 'windows-x86_64')

    def test_wrong_architecture_and_missing_file(self):
        self.windows(self.root, 0xaa64)
        with self.assertRaisesRegex(ValueError, 'Wrong DLL architecture'):
            inspect_runtime(self.root, 'windows-x86_64')
        (self.root / DLLS[0]).unlink()
        with self.assertRaisesRegex(ValueError, 'No complete'):
            inspect_runtime(self.root, 'windows-arm64')

    def test_homebrew_resolved_sources(self):
        pins = []
        for (identity, name), package in zip(LIBRARIES.items(), ('glfw', 'vulkan-loader')):
            actual = self.root / 'Cellar' / package / '1.0'
            (actual / 'lib').mkdir(parents=True)
            data = image(6, [string_command(0xd, identity, 24)])
            (actual / 'lib' / name).write_bytes(data)
            pins.append(hashlib.sha256(data).hexdigest())
            link = self.root / 'opt' / package
            link.parent.mkdir(exist_ok=True)
            try:
                link.symlink_to(actual, target_is_directory=True)
            except OSError:
                self.skipTest('Host does not allow symlink creation')
        report = inspect_runtime(self.root, 'macos-arm64')
        self.assertEqual(report['layout'], 'homebrew')
        with staged_runtime(self.root, 'macos-arm64', *pins) as staged:
            self.assertEqual(set(p.name for p in staged.iterdir()), set(LIBRARIES.values()))

    def test_reject_link_outside_explicit_root(self):
        runtime = self.root / 'runtime'
        runtime.mkdir()
        self.windows(self.root)
        for name in DLLS:
            try:
                (runtime / name).symlink_to(self.root / name)
            except OSError:
                self.skipTest('Host does not allow symlink creation')
        with self.assertRaisesRegex(ValueError, 'escapes selected root'):
            inspect_runtime(runtime, 'windows-x86_64')


if __name__ == '__main__':
    unittest.main()
