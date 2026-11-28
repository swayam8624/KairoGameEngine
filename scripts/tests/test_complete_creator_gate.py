"""Portable tests for the creator harness's fail-closed native execution gate."""
from contextlib import redirect_stdout
import io
import os
from unittest.mock import patch
from pathlib import Path
import sys
import tempfile
import unittest

from qualify_complete_creator_workflow import inventory, native, isolated_runtime_path


class CreatorGateTests(unittest.TestCase):
    def test_native_requires_non_skipped_evidence_and_exit_zero(self):
        with tempfile.TemporaryDirectory() as directory, redirect_stdout(io.StringIO()):
            work = Path(directory)
            native([sys.executable, '-c', "print('real marker')"], work, marker='real marker')
            with self.assertRaisesRegex(RuntimeError, 'did not report'):
                native([sys.executable, '-c', "print('rendering skipped')"], work, marker='real marker')
            with self.assertRaisesRegex(RuntimeError, 'Native operation failed'):
                native([sys.executable, '-c', "print('real marker'); raise SystemExit(7)"],
                       work, marker='real marker')

    def test_private_launch_environment_is_restored_after_error(self):
        with patch.dict(os.environ, {'PATH': 'developer path', 'DYLD_LIBRARY_PATH': 'override',
                                    'VCPKG_ROOT': 'developer root', 'PYTHONPATH': 'source modules',
                                    'VK_DRIVER_FILES': 'explicit driver'}, clear=True):
            original = os.environ.copy()
            # Exercise the macOS policy portably without running native binaries.
            with patch('qualify_complete_creator_workflow.os.name', 'posix'), \
                 patch('qualify_complete_creator_workflow.sys.platform', 'darwin'):
                with self.assertRaisesRegex(RuntimeError, 'fixture failure'):
                    with isolated_runtime_path():
                        self.assertEqual(os.environ['PATH'], '/usr/bin:/bin:/usr/sbin:/sbin')
                        self.assertNotIn('DYLD_LIBRARY_PATH', os.environ)
                        self.assertNotIn('VCPKG_ROOT', os.environ)
                        self.assertNotIn('PYTHONPATH', os.environ)
                        self.assertEqual(os.environ['VK_DRIVER_FILES'], 'explicit driver')
                        raise RuntimeError('fixture failure')
            self.assertEqual(os.environ.copy(), original)

    def test_windows_environment_snapshot_accepts_uppercase_systemroot(self):
        # Windows os.environ.copy() returns an ordinary dict with uppercase
        # keys. Patch only this module's platform view and Path factory;
        # pathlib itself must retain the actual host semantics.
        from types import SimpleNamespace
        import qualify_complete_creator_workflow as gate
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp).resolve()
            values = {'SYSTEMROOT': str(root), 'PATH': 'developer path',
                      'VCPKG_ROOT': 'developer root', 'VK_DRIVER_FILES': 'explicit driver'}
            fake_os = SimpleNamespace(name='nt', environ=values, pathsep=';')
            with patch.object(gate, 'os', fake_os):
                with isolated_runtime_path():
                    self.assertEqual(values['PATH'], str(root / 'System32') + ';' + str(root))
                    self.assertNotIn('VCPKG_ROOT', values)
                    self.assertEqual(values['VK_DRIVER_FILES'], 'explicit driver')
                self.assertEqual(values['PATH'], 'developer path')
                self.assertEqual(values['VCPKG_ROOT'], 'developer root')
                for key in ('SystemRoot', 'systemroot'):
                    values[key] = values.pop('SYSTEMROOT' if key == 'SystemRoot' else 'SystemRoot')
                    with isolated_runtime_path():
                        self.assertNotIn('VCPKG_ROOT', values)
                values.pop('systemroot')
                with self.assertRaisesRegex(RuntimeError, 'SystemRoot is missing'):
                    with isolated_runtime_path():
                        self.fail('Missing root accepted')

    def test_inventory_detects_mutation_and_missing_files(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            source = work / 'file.txt'
            source.write_bytes(b'original')
            before = inventory(work)
            source.write_bytes(b'changed')
            self.assertNotEqual(inventory(work), before)
            source.unlink()
            self.assertEqual(inventory(work), {})


if __name__ == '__main__':
    unittest.main()
