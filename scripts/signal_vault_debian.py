#!/usr/bin/env python3
"""Qualify a Debian installer from the exact previously tested native ZIP.

Trusted build evidence only. No sudo or installation is performed; APT is
simulated and the real .deb is extracted into a fresh user-space tree.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

import debian_dependencies
import kairo_release as release
from package_kairo_deb import package


def digest(path):
    with path.open('rb') as stream:
        return release.copy_hash(stream)[1]


def run(*args, cwd, env=None, capture=False):
    return subprocess.run([str(arg) for arg in args], cwd=cwd, env=env,
                          check=True, timeout=180, text=True, capture_output=capture)


def qualify(archive, source_receipt, output):
    archive = Path(archive).resolve(strict=True)
    source_receipt = Path(source_receipt).resolve(strict=True)
    output = Path(output).absolute()
    release.require(not output.exists() and not output.is_symlink(), 'Output exists')
    previous = json.loads(source_receipt.read_text(encoding='utf-8'))
    index = release.verify(archive)
    release.require(index['target'].startswith('linux-') and index['kind'] == 'release',
                    'Expected host-native Linux release')
    release.require(previous['archive_sha256'] == digest(archive) and previous['target'] == index['target']
                    and previous['relocated_launcher_validation'] and previous['campaign_replay'],
                    'ZIP and qualification receipt disagree')
    with tempfile.TemporaryDirectory(prefix='kairo-deb-qualification-', dir=output.parent) as temp:
        root = Path(temp)
        bundle = root / 'source bundle'
        release.unpack(archive, bundle)
        fields = release.manifest((bundle / 'package.kmanifest').read_bytes())
        binary = bundle / fields['player']
        release.require(digest(binary) == previous['player_sha256'], 'Player differs from receipt')
        depends = debian_dependencies.resolve(binary)
        name = 'kairo-signal-vault'
        version = '0.1.0'
        installer = package(bundle, root / 'signal-vault.deb', name, version,
                            'Kairo project', depends)
        metadata = run('dpkg-deb', '-f', installer, 'Depends', cwd=root, capture=True).stdout.strip()
        release.require(metadata == ', '.join(depends), 'Debian Depends metadata differs')
        # Solver check on the build host. No network request or privileged
        # installation is needed, and this is not a clean-host assertion.
        run('apt-get', '-s', 'install', installer, cwd=root)
        installed = root / 'extracted installation'
        run('dpkg-deb', '-x', installer, installed, cwd=root)
        base = installed / 'usr/lib/kairo' / name
        release.require(digest(base / fields['player']) == previous['player_sha256'], 'Installed Player differs')
        for path, record in index['files'].items():
            if path.startswith('project/'):
                release.require(digest(base / path) == record['sha256'], 'Installed project differs: ' + path)
        shutil.rmtree(bundle)
        data = root / 'fresh user data'
        env = {**os.environ, 'XDG_DATA_HOME': str(data)}
        launcher = installed / 'usr/bin' / name
        run(launcher, '--validate', cwd=root, env=env)
        user_project = data / 'kairo' / name / version / 'project'
        release.require((user_project / 'Project.kproject').is_file(), 'No writable project copy')
        sentinel = user_project / '.user-fixture'
        sentinel.write_text('retain', encoding='ascii')
        run(launcher, '--renderer', 'vulkan', '--smoke', cwd=root, env=env)
        run(launcher, '--validate', cwd=root, env=env)
        release.require(sentinel.read_text() == 'retain', 'Subsequent launch replaced user project')
        release.require(not (base / 'project/.user-fixture').exists(), 'User data entered install tree')
        for path, record in index['files'].items():
            if path.startswith('project/'):
                release.require(digest(base / path) == record['sha256'], 'Launch changed install tree')
        receipt = {'schema': 1, 'commit': previous['commit'], 'target': index['target'],
                   'workspace_lock_sha256': previous['workspace_lock_sha256'],
                   'first_party_notice_count': previous.get('first_party_notice_count', 0),
                   'first_party_notice_index_sha256': previous.get('first_party_notice_index_sha256'),
                   'source_zip_sha256': previous['archive_sha256'], 'installer_sha256': digest(installer),
                   'depends': depends, 'apt_simulation': True, 'extracted_launcher_validation': True,
                   'vulkan_smoke': True, 'user_copy_retained': True, 'clean_machine_qualified': False,
                   'third_party_licenses_verified': False, 'signed': False}
        output.mkdir()
        try:
            with installer.open('rb') as src, (output / 'SignalVault.deb').open('xb') as dst:
                shutil.copyfileobj(src, dst)
            release.require(digest(output / 'SignalVault.deb') == receipt['installer_sha256'], 'Copy differs')
            (output / 'receipt.json').write_text(json.dumps(receipt, indent=2, sort_keys=True) + '\n')
        except BaseException:
            shutil.rmtree(output)
            raise
    return receipt


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    parser.add_argument('source_receipt', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    print(json.dumps(qualify(args.archive, args.source_receipt, args.output), sort_keys=True))
