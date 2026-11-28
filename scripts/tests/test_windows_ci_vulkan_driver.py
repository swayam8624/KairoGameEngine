"""Portable tests of the hosted-only registry lifetime policy."""
from contextlib import contextmanager
from pathlib import Path
import unittest
from unittest.mock import patch

from windows_ci_vulkan_driver import registered, hosted_windows, run, KEY


class Registry:
    KEY_WOW64_64KEY = 0x100
    KEY_QUERY_VALUE = 1
    KEY_SET_VALUE = 2
    REG_DWORD = 4
    HKEY_LOCAL_MACHINE = object()

    def __init__(self, values=None):
        self.values = dict(values or {})
        self.closed = False

    def OpenKey(self, root, name, reserved, access):
        assert root is self.HKEY_LOCAL_MACHINE and name == KEY
        assert access & self.KEY_WOW64_64KEY
        return self

    def QueryValueEx(self, key, value):
        if value not in self.values:
            raise FileNotFoundError(value)
        return self.values[value]

    def SetValueEx(self, key, value, reserved, kind, data):
        self.values[value] = (data, kind)

    def DeleteValue(self, key, value):
        del self.values[value]

    def CloseKey(self, key):
        self.closed = True


class RegistryTests(unittest.TestCase):
    def test_temporary_value_removed_on_exception(self):
        manifest = Path('vk_swiftshader_icd.json')
        unrelated = {'other.json': (0, Registry.REG_DWORD)}
        registry = Registry(unrelated)
        with self.assertRaisesRegex(RuntimeError, 'child failure'):
            with registered(registry, manifest) as added:
                self.assertTrue(added)
                self.assertEqual(registry.values[str(manifest)], (0, Registry.REG_DWORD))
                raise RuntimeError('child failure')
        self.assertEqual(registry.values, unrelated)
        self.assertTrue(registry.closed)

    def test_existing_enabled_registration_is_preserved(self):
        manifest = Path('vk_swiftshader_icd.json')
        values = {str(manifest): (0, Registry.REG_DWORD)}
        registry = Registry(values)
        with registered(registry, manifest) as added:
            self.assertFalse(added)
        self.assertEqual(registry.values, values)

    def test_disabled_registration_never_overwritten(self):
        manifest = Path('vk_swiftshader_icd.json')
        values = {str(manifest): (1, Registry.REG_DWORD)}
        registry = Registry(values)
        with self.assertRaisesRegex(RuntimeError, 'refusing to replace'):
            with registered(registry, manifest):
                self.fail('Disabled registration accepted')
        self.assertEqual(registry.values, values)

    def test_changed_registration_not_deleted(self):
        manifest = Path('vk_swiftshader_icd.json')
        registry = Registry()
        with self.assertRaisesRegex(RuntimeError, 'changed externally'):
            with registered(registry, manifest):
                registry.values[str(manifest)] = (1, Registry.REG_DWORD)
        self.assertEqual(registry.values[str(manifest)], (1, Registry.REG_DWORD))

    def test_non_hosted_runs_are_rejected_before_registry_access(self):
        with patch('windows_ci_vulkan_driver.hosted_windows', return_value=False):
            with self.assertRaisesRegex(RuntimeError, 'restricted'):
                run('missing.json', ['unused'])


if __name__ == '__main__':
    unittest.main()
