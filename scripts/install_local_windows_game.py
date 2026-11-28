#!/usr/bin/env python3
"""Install a verified Kairo Windows game with explicitly supplied local DLLs.

This creates a private installation, not a redistributable archive. The original
release.json continues to describe the verified ZIP; local-runtime.json records
only the extra local runtime files. Requires trusted, stable input files and an
unused destination under the caller's control.
"""
import argparse
import json
from pathlib import Path
import re
import shutil
import struct

import kairo_release as release

DLLS = ('glfw3.dll', 'vulkan-1.dll')
MAX_DLL = 256 * 1024**2


def digest(path):
    with Path(path).open('rb') as stream:
        return release.copy_hash(stream)[1]


def dll_target(path):
    with path.open('rb') as stream:
        target = release.platform_of(stream)
        release.require(target.startswith('windows-'), 'Expected a Windows DLL')
        stream.seek(60)
        offset = struct.unpack('<I', stream.read(4))[0]
        stream.seek(offset + 22)
        flags = struct.unpack('<H', stream.read(2))[0]
        release.require(flags & 0x2000, 'PE image is not a DLL')
    return target


def install(archive, destination, runtime, hashes):
    """Preflight explicit local DLLs, then extract and copy to a new directory."""
    archive = Path(archive).resolve(strict=True)
    destination = Path(destination).absolute()
    runtime = Path(runtime).resolve(strict=True)
    release.require(runtime.is_dir(), 'Runtime source must be a directory')
    release.require(set(hashes) == set(DLLS) and all(isinstance(h, str) and re.fullmatch('[0-9a-f]{64}', h) for h in hashes.values()), 'Expected exact SHA-256 for both local DLLs')
    release.require(not destination.exists() and not destination.is_symlink(), 'Destination already exists')
    release.require(not destination.resolve().is_relative_to(runtime), 'Destination must be outside runtime source')
    index = release.verify(archive)
    target = index['target']
    release.require(target in ('windows-x86_64', 'windows-arm64') and index['kind'] == 'release', 'Expected a Windows release archive')
    archive_hash = digest(archive)
    sources = {}
    for name in DLLS:
        source = runtime / name
        release.require(source.is_file() and not source.is_symlink() and source.resolve().parent == runtime, 'Missing or linked runtime DLL: ' + name)
        release.require(0 < source.stat().st_size <= MAX_DLL and dll_target(source) == target, 'Wrong DLL size or architecture: ' + name)
        release.require(digest(source) == hashes[name], 'Runtime DLL hash differs: ' + name)
        sources[name] = source
    # unpack reserves the destination exclusively and removes it if extraction
    # fails. From this point onward only our newly created directory is touched.
    extracted = release.unpack(archive, destination)
    try:
        release.require(extracted == index and digest(archive) == archive_hash, 'Archive changed during installation')
        installed = {}
        for name, source in sources.items():
            dest = destination / 'bin' / name
            with source.open('rb') as src, dest.open('xb') as out:
                size, checksum = release.copy_hash(src, out)
            release.require(0 < size <= MAX_DLL and checksum == hashes[name] == digest(dest), 'Local DLL changed during copy')
            installed['bin/' + name] = {'bytes': size, 'sha256': checksum}
        receipt = {'schema': 1, 'target': target, 'source_archive_sha256': archive_hash,
                   'local_runtime_files': installed, 'private_installation_only': True,
                   'redistribution_approved': False, 'dependency_closure_verified': False}
        with (destination / 'local-runtime.json').open('x', encoding='utf-8') as stream:
            json.dump(receipt, stream, sort_keys=True, indent=2)
            stream.write('\n')
    except BaseException:
        shutil.rmtree(destination)
        raise
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    parser.add_argument('destination', type=Path)
    parser.add_argument('runtime', type=Path)
    parser.add_argument('--glfw-sha256', required=True)
    parser.add_argument('--vulkan-sha256', required=True)
    args = parser.parse_args()
    try:
        result = install(args.archive, args.destination, args.runtime,
                         dict(zip(DLLS, (args.glfw_sha256, args.vulkan_sha256))))
        print(json.dumps(result, sort_keys=True))
    except (OSError, ValueError, RuntimeError) as error:
        parser.exit(1, f'Local game installation rejected: {error}\n')


if __name__ == '__main__':
    main()
