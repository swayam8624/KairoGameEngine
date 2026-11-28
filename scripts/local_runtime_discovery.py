#!/usr/bin/env python3
"""Inspect explicitly selected local runtime sources for private Kairo games.

No network, package manager, executable launch or implicit trust. The reported
hashes identify observed bytes; they do not authenticate their publisher.
"""
import argparse
from contextlib import contextmanager
import json
from pathlib import Path
import platform
import re
import tempfile

import kairo_release as release
from launch_private_game import host_target
from verify_private_game_install import linked, record

MAX_LIBRARY = 256 * 1024**2


def candidates(root, target):
    """Find the fixed pair in a flat directory or supported package layout."""
    root = Path(root).resolve(strict=True)
    release.require(root.is_dir(), 'Runtime source must be a directory')
    if target in ('windows-x86_64', 'windows-arm64'):
        from install_local_windows_game import DLLS
        names = DLLS
        triplet = 'x64-windows' if target.endswith('x86_64') else 'arm64-windows'
        layouts = [('flat', [root / name for name in names]),
                   ('vcpkg', [root / 'installed' / triplet / 'bin' / name for name in names])]
    elif target == 'macos-arm64':
        from install_local_macos_game import LIBRARIES
        names = tuple(LIBRARIES.values())
        layouts = [('flat', [root / name for name in names]),
                   ('homebrew', [root / 'opt/glfw/lib' / names[0],
                                 root / 'opt/vulkan-loader/lib' / names[1]])]
    else:
        raise ValueError('Unsupported native runtime target: ' + target)
    results = []
    for layout, paths in layouts:
        if not all(p.is_file() for p in paths):
            continue
        resolved = [p.resolve(strict=True) for p in paths]
        # Homebrew opt links are conventional, but every resolved source must
        # remain within the explicitly selected prefix. No arbitrary PATH scan.
        release.require(all(p.is_relative_to(root) and p.is_file() and not linked(p.lstat())
                            for p in resolved), 'Runtime source escapes selected root or is linked')
        if layout != 'homebrew':
            release.require(all(not linked(p.lstat()) and p == actual
                                for p, actual in zip(paths, resolved)),
                            'Unexpected link in runtime directory')
        results.append((layout, dict(zip(names, resolved))))
    release.require(results, 'No complete GLFW/Vulkan pair found. Select a flat runtime directory, '
                    'a vcpkg root, or the Apple Silicon Homebrew prefix.')
    release.require(len(results) == 1, 'Ambiguous runtime layouts; select one explicit flat directory')
    return root, results[0]


def inspect_runtime(root, target):
    """Check native format and report actual hashes without loading libraries."""
    root, (layout, sources) = candidates(root, target)
    files = {}
    for name, path in sources.items():
        release.require(0 < path.stat().st_size <= MAX_LIBRARY, 'Invalid runtime file size: ' + name)
        if target.startswith('windows-'):
            from install_local_windows_game import dll_target
            release.require(dll_target(path) == target, 'Wrong DLL architecture: ' + name)
        else:
            from install_local_macos_game import LIBRARIES, inspect, system
            identity = next(old for old, leaf in LIBRARIES.items() if leaf == name)
            data = inspect(path, 6)
            release.require(data['identities'] == [identity] and not data['rpaths']
                            and all(system(n) for n in data['imports']),
                            'Unexpected dylib identity or dependency: ' + name)
        files[name] = {'source': str(path), **record(path)}
    return {'target': target, 'root': str(root), 'layout': layout, 'files': files,
            'observed_hashes_only': True, 'publisher_authenticity_verified': False,
            'dependency_closure_verified': False, 'gpu_execution_verified': False}


@contextmanager
def staged_runtime(root, target, glfw_hash, vulkan_hash):
    """Copy only validated and explicitly pinned bytes to an isolated directory."""
    report = inspect_runtime(root, target)
    files = report['files']
    pins = (glfw_hash, vulkan_hash)
    release.require(all(isinstance(pin, str) and re.fullmatch(r'[0-9a-f]{64}', pin)
                        for pin in pins), 'Expected exact SHA-256 for both local libraries')
    release.require(tuple(entry['sha256'] for entry in files.values()) == pins,
                    'Local runtime SHA-256 differs from supplied pin')
    with tempfile.TemporaryDirectory(prefix='kairo-private-runtime-') as directory:
        staged = Path(directory)
        for name, entry in files.items():
            source = Path(entry['source'])
            release.require(record(source) == {'bytes': entry['bytes'], 'sha256': entry['sha256']},
                            'Runtime source changed before staging: ' + name)
            destination = staged / name
            with source.open('rb') as src, destination.open('xb') as out:
                size, checksum = release.copy_hash(src, out)
            release.require(size == entry['bytes'] and checksum == entry['sha256']
                            and record(destination) == {'bytes': size, 'sha256': checksum},
                            'Runtime source changed during staging: ' + name)
        yield staged


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root', type=Path)
    parser.add_argument('--target')
    args = parser.parse_args()
    try:
        target = args.target or host_target(platform.system(), platform.machine())
        print(json.dumps(inspect_runtime(args.root, target), indent=2, sort_keys=True))
    except (OSError, ValueError, RuntimeError) as error:
        parser.exit(1, f'Runtime inspection rejected: {error}\n')


if __name__ == '__main__':
    main()
