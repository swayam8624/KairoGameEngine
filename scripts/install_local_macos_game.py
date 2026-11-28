#!/usr/bin/env python3
"""Install a verified ARM64 macOS game with explicitly provided local dylibs.

Private installation only. The original ZIP and release.json describe the
unmodified distribution. local-runtime.json records the rewritten bytes.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys

import kairo_release as release

LIBRARIES = {
    '/opt/homebrew/opt/glfw/lib/libglfw.3.dylib': 'libglfw.3.dylib',
    '/opt/homebrew/opt/vulkan-loader/lib/libvulkan.1.dylib': 'libvulkan.1.dylib',
}
MAX_LIBRARY = 256 * 1024**2
LOADS = {0xc, 0x80000018, 0x8000001f, 0x20, 0x80000023}


def digest(path):
    with Path(path).open('rb') as stream:
        return release.copy_hash(stream)[1]


def system(name):
    return name.startswith(('/usr/lib/', '/System/Library/Frameworks/'))


def inspect(path, kind):
    """Read bounded thin little-endian ARM64 Mach-O load commands on any host."""
    with Path(path).open('rb') as stream:
        header = stream.read(32)
        release.require(len(header) == 32, 'Truncated Mach-O header')
        magic, cpu, _, filetype, count, size, _, _ = struct.unpack('<8I', header)
        release.require(magic == 0xfeedfacf and cpu == 0x0100000c and filetype == kind,
                        'Expected thin ARM64 Mach-O of requested type')
        release.require(count <= 4096 and size <= 16 * 1024**2 and count * 8 <= size,
                        'Invalid load command bounds')
        data = stream.read(size)
    release.require(len(data) == size, 'Truncated load commands')
    offset = 0
    imports, identities, rpaths = [], [], []
    for _ in range(count):
        release.require(offset + 8 <= size, 'Truncated load command')
        cmd, length = struct.unpack_from('<II', data, offset)
        release.require(length >= 8 and length % 8 == 0 and offset + length <= size,
                        'Invalid load command length')
        if cmd in LOADS or cmd in (0xd, 0x8000001c):
            minimum = 12 if cmd == 0x8000001c else 24
            release.require(length >= minimum, 'Truncated load command string')
            start = struct.unpack_from('<I', data, offset + 8)[0]
            release.require(minimum <= start < length, 'Invalid load command string offset')
            field = data[offset + start:offset + length]
            release.require(b'\0' in field, 'Unterminated load command string')
            name = field.split(b'\0', 1)[0].decode('utf-8')
            release.require(bool(name) and name.isprintable(), 'Invalid load command string')
            (identities if cmd == 0xd else rpaths if cmd == 0x8000001c else imports).append(name)
        offset += length
    release.require(offset == size and len(identities) == (1 if kind == 6 else 0),
                    'Invalid load command inventory')
    return {'imports': imports, 'identities': identities, 'rpaths': rpaths}


def preflight(player, runtime, hashes):
    """Refuse unknown native imports or external dependencies of the dylibs."""
    runtime = Path(runtime).resolve(strict=True)
    release.require(runtime.is_dir(), 'Runtime source must be a directory')
    release.require(set(hashes) == set(LIBRARIES.values()) and
                    all(isinstance(h, str) and re.fullmatch('[0-9a-f]{64}', h) for h in hashes.values()),
                    'Expected exact SHA-256 for both local dylibs')
    image = inspect(player, 2)
    external = {name for name in image['imports'] if not system(name)}
    release.require(external == set(LIBRARIES), 'Unexpected Player runtime imports')
    release.require(len(set(image['rpaths'])) == len(image['rpaths']), 'Duplicate Player rpaths')
    sources = {}
    for old, name in LIBRARIES.items():
        path = runtime / name
        release.require(path.is_file() and not path.is_symlink() and path.resolve().parent == runtime,
                        'Missing or linked runtime dylib: ' + name)
        release.require(0 < path.stat().st_size <= MAX_LIBRARY and digest(path) == hashes[name],
                        'Runtime dylib size or hash differs: ' + name)
        lib = inspect(path, 6)
        release.require(lib['identities'] == [old] and all(system(n) for n in lib['imports']),
                        'Unexpected dylib identity or dependency: ' + name)
        release.require(not lib['rpaths'], 'Unexpected dylib rpath: ' + name)
        sources[old] = path
    return image, sources


def command(*args):
    subprocess.run([str(a) for a in args], check=True, timeout=90)


def rewrite(path, changes, rpaths, identity=None):
    args = ['/usr/bin/install_name_tool']
    if identity is not None:
        args += ['-id', identity]
    for old, new in changes.items():
        args += ['-change', old, new]
    for rpath in rpaths:
        args += ['-delete_rpath', rpath]
    command(*args, path)
    command('/usr/bin/codesign', '--force', '--sign', '-', path)
    command('/usr/bin/codesign', '--verify', '--strict', path)


def install(archive, destination, runtime, hashes):
    release.require(sys.platform == 'darwin' and os.uname().machine == 'arm64',
                    'Apple Silicon macOS required for native relocation')
    archive = Path(archive).resolve(strict=True)
    destination = Path(destination).absolute()
    runtime = Path(runtime).resolve(strict=True)
    release.require(not destination.resolve().is_relative_to(runtime), 'Destination must be outside runtime source')
    index = release.verify(archive)
    release.require(index['target'] == 'macos-arm64' and index['kind'] == 'release', 'Expected ARM64 macOS release')
    archive_hash = digest(archive)
    extracted = release.unpack(archive, destination)
    try:
        release.require(extracted == index and digest(archive) == archive_hash, 'Archive changed during installation')
        fields = release.manifest((destination / 'package.kmanifest').read_bytes())
        player = destination / fields['player']
        image, sources = preflight(player, runtime, hashes)
        changes = {old: '@loader_path/' + name for old, name in LIBRARIES.items()}
        records = {}
        for old, source in sources.items():
            name = LIBRARIES[old]
            target = player.parent / name
            with source.open('rb') as src, target.open('xb') as out:
                size, checksum = release.copy_hash(src, out)
            release.require(0 < size <= MAX_LIBRARY and checksum == hashes[name] == digest(target),
                            'Runtime dylib changed during copy')
            target.chmod(0o755)
            rewrite(target, {}, [], changes[old])
            lib = inspect(target, 6)
            release.require(lib['identities'] == [changes[old]] and not lib['rpaths'] and
                            all(system(n) for n in lib['imports']), 'Relocated dylib still has external references')
            records[target.relative_to(destination).as_posix()] = {'sha256': digest(target), 'bytes': target.stat().st_size}
        rewrite(player, changes, image['rpaths'])
        linked = inspect(player, 2)
        release.require(not linked['rpaths'] and {n for n in linked['imports'] if not system(n)} == set(changes.values()),
                        'Relocated Player imports differ')
        records[fields['player']] = {'sha256': digest(player), 'bytes': player.stat().st_size}
        receipt = {'schema': 1, 'target': index['target'], 'source_archive_sha256': archive_hash,
                   'source_library_hashes': hashes, 'local_runtime_files': records,
                   'private_installation_only': True, 'adhoc_signed': True,
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
        print(json.dumps(install(args.archive, args.destination, args.runtime,
              dict(zip(LIBRARIES.values(), (args.glfw_sha256, args.vulkan_sha256)))), sort_keys=True))
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        parser.exit(1, f'Local game installation rejected: {error}\n')


if __name__ == '__main__':
    main()
