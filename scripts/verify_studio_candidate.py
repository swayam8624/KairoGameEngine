#!/usr/bin/env python3
"""Verify a qualified Studio artifact and its exact-commit evidence, offline.

A successful result certifies the recorded hosted qualification and archive
integrity only. It does not certify signatures, hardware GPU compatibility,
third-party redistribution, or physical clean-machine installation.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import stat
import zipfile

import kairo_release as release

TARGETS = ('linux-x86_64', 'macos-arm64', 'windows-x86_64')
MAX_FILES = 20000
MAX_BYTES = 4 * 1024**3
MAX_JSON = 32 * 1024**2


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def read_json(path):
    release.require(path.is_file() and not path.is_symlink(), 'Missing evidence file: ' + str(path))
    with path.open('rb') as stream:
        raw = stream.read(MAX_JSON + 1)
    release.require(len(raw) <= MAX_JSON, 'Evidence JSON exceeds size limit')
    data = json.loads(raw)
    release.require(isinstance(data, dict), 'Expected evidence object')
    return data


def sha(value):
    return isinstance(value, str) and re.fullmatch(r'[0-9a-f]{64}', value) is not None


def verify_candidate(folder, target, commit):
    folder = Path(folder).resolve(strict=True)
    release.require(target in TARGETS and re.fullmatch(r'[0-9a-f]{40}', commit),
                    'Expected supported target and exact commit SHA')
    receipt_path = folder / 'receipt.json'
    imports_path = folder / 'native-imports.json'
    archive = folder / 'KairoStudio.zip'
    receipt, imports = read_json(receipt_path), read_json(imports_path)
    release.require(archive.is_file() and not archive.is_symlink(), 'Missing Studio archive')
    release.require(receipt.get('schema') == 1 and receipt.get('commit') == commit
                    and receipt.get('native_target') == target, 'Receipt commit or target differs')
    release.require(digest(archive) == receipt.get('archive_sha256')
                    and digest(imports_path) == receipt.get('native_import_inventory_sha256'),
                    'Archive or native inventory digest differs')
    for key in ('hub_template_validation', 'installed_creator_export_validated', 'launcher_help_validated'):
        release.require(receipt.get(key) is True, 'Missing required Studio evidence: ' + key)
    creator = receipt.get('complete_creator_workflow')
    release.require(isinstance(creator, dict) and creator.get('schema') == 1
                    and creator.get('template') == 'signalvault', 'Missing complete creator receipt')
    for key in ('native_authoring_validation', 'invalid_and_stale_edits_rejected',
                'authored_ui_and_campaign_edits_exported', 'previous_release_and_private_state_preserved',
                'both_relocated_players_validated', 'relocated_graphical_smoke'):
        release.require(creator.get(key) is True, 'Missing required creator evidence: ' + key)
    release.require(sha(creator.get('original_archive_sha256')) and sha(creator.get('edited_archive_sha256'))
                    and creator['original_archive_sha256'] != creator['edited_archive_sha256'],
                    'Creator release hashes missing or identical')
    if target != 'linux-x86_64':
        for key in ('private_install_and_launch_validation', 'private_graphical_smoke',
                    'private_developer_path_removed'):
            release.require(creator.get(key) is True, 'Missing private creator evidence: ' + key)
        hashes = creator.get('runtime_library_sha256')
        release.require(isinstance(hashes, dict) and len(hashes) == 2 and all(sha(v) for v in hashes.values()),
                        'Missing native runtime hashes')
    expected_renderer = {'linux-x86_64': 'vulkan', 'macos-arm64': 'metal', 'windows-x86_64': 'auto'}[target]
    release.require(creator.get('requested_renderer') == expected_renderer, 'Unexpected renderer qualification')
    release.require(imports.get('target') == target and imports.get('schema') == 1,
                    'Native import inventory target differs')
    tools = imports.get('tools', [])
    release.require(len(tools) == receipt.get('native_tools_inspected') == 7
                    and len({row['path'] for row in tools}) == 7, 'Unexpected native tool inventory')
    with zipfile.ZipFile(archive) as z:
        entries = z.infolist()
        names = [release.safe_name(e.filename) for e in entries]
        release.require(0 < len(names) <= MAX_FILES and len(set(n.casefold() for n in names)) == len(names)
                        and sum(e.file_size for e in entries) <= MAX_BYTES, 'Invalid archive inventory bounds')
        release.require(all(stat.S_ISREG(e.external_attr >> 16) and not e.flag_bits & 1 for e in entries),
                        'Unexpected archive entry type')
        index_name = 'studio-inventory.json'
        release.require(index_name in names and z.getinfo(index_name).file_size <= MAX_JSON,
                        'Missing bounded Studio inventory')
        index = json.loads(z.read(index_name))
        release.require(isinstance(index, dict) and set(index) == set(names) - {index_name}
                        and len(index) == receipt.get('file_count'), 'Studio inventory differs')
        for entry in entries:
            if entry.filename == index_name:
                continue
            with z.open(entry) as stream:
                size, checksum = release.copy_hash(stream)
            row = index[entry.filename]
            release.require(size == row['bytes'] and checksum == row['sha256'],
                            'Archive file differs: ' + entry.filename)
        for row in tools:
            release.require(index[row['path']]['sha256'] == row['sha256'],
                            'Native tool evidence differs: ' + row['path'])
        for name, field in (('workspace.lock', 'workspace_lock_sha256'),
                            ('KairoNotices/notice-index.json', 'first_party_notice_index_sha256'),
                            ('ThirdPartySourceNotices/source-notices.json', 'third_party_source_notice_index_sha256')):
            release.require(index[name]['sha256'] == receipt.get(field), 'Notice or lock differs: ' + name)
    return {'schema': 1, 'commit': commit, 'target': target, 'archive_sha256': receipt['archive_sha256'],
            'receipt_sha256': digest(receipt_path), 'native_import_inventory_sha256': digest(imports_path),
            'files_verified': len(index), 'complete_creator_hosted_qualification': True,
            'physical_clean_host_qualified': False, 'hardware_gpu_qualified': False,
            'signed_release': False, 'redistribution_approved': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    parser.add_argument('--target', choices=TARGETS, required=True)
    parser.add_argument('--commit', required=True)
    args = parser.parse_args()
    try:
        print(json.dumps(verify_candidate(args.folder, args.target, args.commit), sort_keys=True))
    except (OSError, ValueError, KeyError, TypeError, RuntimeError, zipfile.BadZipFile) as error:
        parser.exit(1, f'Candidate rejected: {error}\n')


if __name__ == '__main__':
    main()
