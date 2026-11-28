#!/usr/bin/env python3
"""Audit a private Kairo game installation against its original ZIP and receipt.

Pass the receipt SHA-256 captured at installation time from a trusted location.
This is an immutable-file audit, not a signature, dependency audit or GPU test.
The installation and archive must remain stable while they are checked.
"""
import argparse
import json
import os
from pathlib import Path
import re
import stat
import zipfile

import kairo_release as release

RECEIPT = 'local-runtime.json'
SHA = re.compile(r'[0-9a-f]{64}')


def record(path):
    """Hash a regular file without following a final-component symlink."""
    release.require(stat.S_ISREG(path.lstat().st_mode), f'Not a regular file: {path}')
    with path.open('rb') as stream:
        size, checksum = release.copy_hash(stream)
    return {'bytes': size, 'sha256': checksum}


def json_file(path, limit):
    release.require(stat.S_ISREG(path.lstat().st_mode), f'Not a regular file: {path}')
    with path.open('rb') as stream:
        data = stream.read(limit + 1)
    release.require(len(data) <= limit, f'JSON exceeds limit: {path}')
    return json.loads(data)


def audit(archive, root, receipt_sha256):
    archive, root = Path(archive), Path(root)
    release.require(isinstance(receipt_sha256, str) and SHA.fullmatch(receipt_sha256),
                    'Expected a trusted receipt SHA-256')
    release.require(root.is_dir() and not root.is_symlink(), 'Invalid installation root')
    index = release.verify(archive)
    archive_hash = record(archive)['sha256']
    release.require(index['kind'] == 'release', 'Expected a release archive')
    receipt_path = root / RECEIPT
    release.require(record(receipt_path)['sha256'] == receipt_sha256, 'Receipt hash differs')
    receipt = json_file(receipt_path, 65536)
    release.require(isinstance(receipt, dict), 'Invalid receipt')
    target = index['target']
    expected_keys = {'schema', 'target', 'source_archive_sha256', 'local_runtime_files',
                     'private_installation_only', 'redistribution_approved', 'dependency_closure_verified'}
    windows = target in ('windows-x86_64', 'windows-arm64')
    if not windows:
        release.require(target == 'macos-arm64', 'Unsupported private install target')
        expected_keys |= {'source_library_hashes', 'adhoc_signed'}
    release.require(set(receipt) == expected_keys and receipt['schema'] == 1
                    and receipt['target'] == target
                    and receipt['source_archive_sha256'] == archive_hash
                    and receipt['private_installation_only'] is True
                    and receipt['redistribution_approved'] is False
                    and receipt['dependency_closure_verified'] is False, 'Receipt metadata differs')
    release.require(json_file(root / release.INDEX, release.MAX_INDEX) == index,
                    'Installed release index differs')
    # The archive has already verified its manifest and single executable path.
    with zipfile.ZipFile(archive) as source:
        player = release.manifest(source.read('package.kmanifest'))['player']
    changes = receipt['local_runtime_files']
    if windows:
        expected_changes = {'bin/glfw3.dll', 'bin/vulkan-1.dll'}
    else:
        expected_changes = {player, 'bin/libglfw.3.dylib', 'bin/libvulkan.1.dylib'}
        hashes = receipt['source_library_hashes']
        release.require(receipt['adhoc_signed'] is True and isinstance(hashes, dict)
                        and set(hashes) == {'libglfw.3.dylib', 'libvulkan.1.dylib'}
                        and all(isinstance(h, str) and SHA.fullmatch(h) for h in hashes.values()),
                        'Invalid macOS source library receipt')
    release.require(isinstance(changes, dict) and set(changes) == expected_changes,
                    'Unexpected local runtime inventory')
    for entry in changes.values():
        release.require(isinstance(entry, dict) and set(entry) == {'bytes', 'sha256'}
                        and type(entry['bytes']) is int and 0 < entry['bytes'] <= release.MAX_BYTES
                        and isinstance(entry['sha256'], str) and SHA.fullmatch(entry['sha256']),
                        'Invalid local runtime file record')
    expected = index['files'] | changes
    # Reject unexpected files and all linked/special entries before hashing.
    # User saves should live outside this immutable private installation.
    observed = set()
    directories = {str(Path(name).parent).replace(os.sep, '/') for name in expected}
    directories |= {p.as_posix() for name in expected for p in Path(name).parents}
    for current, dirs, files in os.walk(root, followlinks=False):
        for name in dirs + files:
            path = Path(current) / name
            relative = release.safe_name(path.relative_to(root).as_posix())
            mode = path.lstat().st_mode
            if name in dirs:
                release.require(stat.S_ISDIR(mode) and relative in directories,
                                f'Unexpected directory: {relative}')
            else:
                release.require(stat.S_ISREG(mode), f'Unexpected file type: {relative}')
                observed.add(relative)
    release.require(observed == set(expected) | {release.INDEX, RECEIPT},
                    'Installed file inventory differs')
    for name, value in expected.items():
        release.require(record(root / name) == value, f'Installed file integrity differs: {name}')
    release.require(record(receipt_path)['sha256'] == receipt_sha256
                    and record(archive)['sha256'] == archive_hash, 'Inputs changed during audit')
    return {'target': target, 'verified_files': len(expected), 'source_archive_sha256': archive_hash,
            'receipt_sha256': receipt_sha256, 'immutable_file_integrity_verified': True,
            'dependency_closure_verified': False, 'redistribution_approved': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    parser.add_argument('installation', type=Path)
    parser.add_argument('--receipt-sha256', required=True)
    args = parser.parse_args()
    try:
        print(json.dumps(audit(args.archive, args.installation, args.receipt_sha256), sort_keys=True))
    except (OSError, ValueError, RuntimeError, zipfile.BadZipFile) as error:
        parser.exit(1, f'Private installation audit rejected: {error}\n')


if __name__ == '__main__':
    main()
