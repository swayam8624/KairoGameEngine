#!/usr/bin/env python3
"""Register one installed Chrome software Vulkan ICD for a hosted CI command.

The Windows Vulkan loader ignores environment overrides in elevated processes.
This helper is deliberately excluded from Studio distributions. It refuses
non-GitHub-hosted runners and never replaces or disables an existing driver.
"""
import argparse
from contextlib import contextmanager
import json
import os
from pathlib import Path
import subprocess
import sys

from windows_vulkan_creator_smoke import driver_manifest

KEY = r'SOFTWARE\Khronos\Vulkan\Drivers'


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def hosted_windows():
    return (sys.platform == 'win32' and os.environ.get('GITHUB_ACTIONS') == 'true'
            and os.environ.get('RUNNER_ENVIRONMENT') == 'github-hosted')


def chrome_manifest(path):
    path = Path(path).resolve(strict=True)
    roots = [Path(os.environ[name]) / 'Google/Chrome/Application'
             for name in ('ProgramFiles', 'ProgramFiles(x86)') if os.environ.get(name)]
    require(any(path.is_relative_to(root.resolve()) for root in roots),
            'Only an installed Chrome software ICD is eligible')
    return path, driver_manifest(path)


@contextmanager
def registered(registry, manifest):
    """Preserve existing values; remove only the exact temporary entry."""
    view = registry.KEY_WOW64_64KEY
    access = registry.KEY_QUERY_VALUE | registry.KEY_SET_VALUE | view
    value = str(manifest)
    try:
        key = registry.OpenKey(registry.HKEY_LOCAL_MACHINE, KEY, 0, access)
    except FileNotFoundError:
        key = registry.CreateKeyEx(registry.HKEY_LOCAL_MACHINE, KEY, 0, access)
    added = False
    try:
        try:
            previous, kind = registry.QueryValueEx(key, value)
        except FileNotFoundError:
            registry.SetValueEx(key, value, 0, registry.REG_DWORD, 0)
            added = True
        else:
            require(kind == registry.REG_DWORD and previous == 0,
                    'Existing ICD registration is not enabled; refusing to replace it')
        yield added
    finally:
        try:
            if added:
                current, kind = registry.QueryValueEx(key, value)
                require(current == 0 and kind == registry.REG_DWORD,
                        'Temporary ICD registration changed externally; refusing deletion')
                registry.DeleteValue(key, value)
        finally:
            registry.CloseKey(key)


def run(manifest, command):
    require(hosted_windows(), 'This helper is restricted to disposable GitHub-hosted Windows CI')
    require(command, 'Expected a qualification command')
    import winreg
    path, evidence = chrome_manifest(manifest)
    env = dict(os.environ)
    for name in ('VK_DRIVER_FILES', 'VK_ICD_FILENAMES', 'VK_ADD_DRIVER_FILES',
                 'VK_LOADER_DRIVERS_SELECT', 'VK_LOADER_DRIVERS_DISABLE'):
        env.pop(name, None)
    env['VK_LOADER_DEBUG'] = 'error,warn,driver'
    with registered(winreg, path) as added:
        print(json.dumps({'ci_software_driver': evidence, 'registered_manifest': str(path),
                          'temporary_registry_value_added': added,
                          'hardware_gpu_qualified': False}, sort_keys=True), flush=True)
        # Require independent loader enumeration before expensive creator
        # work. Actual game rendering remains a separate required gate.
        probe = subprocess.run(['vulkaninfo', '--summary'], env=env, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, text=True, errors='replace', timeout=60)
        print(probe.stdout, flush=True)
        require(probe.returncode == 0 and 'swiftshader' in probe.stdout.casefold(),
                'Software Vulkan device was not enumerated by the native loader')
        result = subprocess.run(command, env=env, check=False)
        return result.returncode if result.returncode >= 0 else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest', type=Path)
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command
    if command[:1] == ['--']:
        command = command[1:]
    try:
        return run(args.manifest, command)
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        parser.exit(1, f'Hosted software Vulkan qualification rejected: {error}\n')


if __name__ == '__main__':
    sys.exit(main())
