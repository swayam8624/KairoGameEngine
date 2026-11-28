#!/usr/bin/env python3
"""Build and qualify native desktop launch packages from an already tested ZIP."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import zipfile

import desktop_launchers as launch
import kairo_release as release


def digest(path):
    with path.open('rb') as stream:
        return release.copy_hash(stream)[1]


def inventory(root):
    return {name: digest(path) for name, path in release.entries(root).items()}


def run(*args, cwd, env=None, check=True):
    return subprocess.run([str(arg) for arg in args], cwd=cwd, env=env,
                          check=check, timeout=180)


def qualify(archive, source_receipt, output):
    archive = Path(archive).resolve(strict=True)
    output = Path(output).absolute()
    previous = json.loads(Path(source_receipt).read_text(encoding='utf-8'))
    index = release.verify(archive)
    target = index['target']
    mac = sys.platform == 'darwin' and target.startswith('macos-')
    windows = os.name == 'nt' and target.startswith('windows-')
    release.require(mac or windows, 'Expected matching macOS or Windows host')
    release.require(index['kind'] == 'release' and previous['target'] == target and
                    previous['archive_sha256'] == digest(archive) and
                    previous['relocated_launcher_validation'] and previous['campaign_replay'],
                    'Expected matching qualified release and receipt')
    release.require(not output.exists() and not output.is_symlink(), 'Output already exists')
    with tempfile.TemporaryDirectory(prefix='kairo-desktop-', dir=output.parent) as temp:
        root = Path(temp)
        bundle = root / 'source bundle'
        release.unpack(archive, bundle)
        fields = release.manifest((bundle / 'package.kmanifest').read_bytes())
        player = bundle / fields['player']
        release.require(digest(player) == previous['player_sha256'], 'Native Player differs')
        release.require(player.name == ('KairoPlayer' if mac else 'KairoPlayer.exe'), 'Unexpected Player name')
        data = root / 'fresh user data'
        env = {**os.environ, 'KAIRO_DATA_HOME': str(data)}
        stage = root / 'staging'
        stage.mkdir()
        if mac:
            app = stage / 'Signal Vault.app'
            launch.write_mac_launcher(app)
            base = app / 'Contents'
            shutil.copy2(player, base / 'MacOS/KairoPlayer')
            shutil.copytree(bundle / 'project', base / 'Resources/project')
            expected = inventory(app)
            package = root / ('SignalVault-' + target + '-app.zip')
            run('ditto', '-c', '-k', '--keepParent', app, package, cwd=root)
            shutil.rmtree(stage)
            shutil.rmtree(bundle)
            relocated = root / 'relocated application'
            relocated.mkdir()
            run('ditto', '-x', '-k', package, relocated, cwd=root)
            installed = relocated / app.name
            release.require(inventory(installed) == expected, 'Extracted app differs')
            executable = installed / 'Contents/MacOS/SignalVault'
            release.require(os.access(executable, os.X_OK), 'App launcher lacks execute permission')
            command = [executable, '--validate']
            native_player = installed / 'Contents/MacOS/KairoPlayer'
        else:
            payload = stage / 'Payload'
            shutil.copytree(bundle / 'project', payload / 'project')
            (payload / 'bin').mkdir()
            shutil.copy2(player, payload / 'bin/KairoPlayer.exe')
            (payload / 'Launch-SignalVault.ps1').write_text(launch.WINDOWS_LAUNCHER, encoding='utf-8')
            (stage / 'Install-SignalVault.ps1').write_text(launch.WINDOWS_INSTALLER, encoding='utf-8')
            expected = inventory(payload)
            records = [{'path': path, 'sha256': checksum} for path, checksum in sorted(expected.items())]
            (stage / 'payload-hashes.json').write_text(json.dumps(records, indent=2), encoding='utf-8')
            (stage / 'README.txt').write_text(
                'Extract this trusted package, run Install-SignalVault.ps1 in PowerShell, then run '
                'Launch-SignalVault.ps1 in the installed folder. No admin access required. '
                'Unsigned scripts must comply with your PowerShell execution policy. '
                'Native graphics/runtime dependencies remain host prerequisites.\n', encoding='utf-8')
            package = root / ('SignalVault-' + target + '-install.zip')
            stage_expected = inventory(stage)
            with zipfile.ZipFile(package, 'w', compression=zipfile.ZIP_DEFLATED, allowZip64=True) as zip_out:
                for name in sorted(stage_expected):
                    zip_out.write(stage / name, name)
            shutil.rmtree(stage)
            shutil.rmtree(bundle)
            relocated = root / 'relocated installer'
            relocated.mkdir()
            # This is our just-created archive, not an arbitrary incoming ZIP.
            with zipfile.ZipFile(package) as zip_in:
                release.require(sorted(zip_in.namelist()) == sorted(stage_expected), 'Archive inventory differs')
                zip_in.extractall(relocated)
            release.require(inventory(relocated) == stage_expected, 'Extracted installer differs')
            installed = root / 'installed app'
            ps = ['powershell.exe', '-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-File']
            install = [*ps, relocated / 'Install-SignalVault.ps1', '-Destination', installed]
            run(*install, cwd=root)
            release.require(inventory(installed) == expected, 'Installed payload differs')
            release.require(run(*install, cwd=root, check=False).returncode != 0, 'Installer overwrote existing destination')
            shutil.rmtree(relocated)
            command = [*ps, installed / 'Launch-SignalVault.ps1', '-Validate']
            native_player = installed / 'bin/KairoPlayer.exe'
        release.require(digest(native_player) == previous['player_sha256'], 'Installed executable differs')
        run(*command, cwd=root, env=env)
        user_project = data / 'project'
        release.require((user_project / 'Project.kproject').is_file(), 'Missing initialized user project')
        sentinel = user_project / '.user-fixture'
        sentinel.write_text('retain', encoding='ascii')
        run(*command, cwd=root, env=env)
        release.require(sentinel.read_text() == 'retain', 'Launch replaced user data')
        release.require(inventory(installed) == expected, 'Launch changed installed tree')
        receipt = {'schema': 1, 'commit': previous['commit'], 'target': target,
                   'source_zip_sha256': previous['archive_sha256'], 'package_sha256': digest(package),
                   'workspace_lock_sha256': previous['workspace_lock_sha256'],
                   'native_launcher_validation': True, 'user_copy_retained': True,
                   'windows_installer_no_overwrite': windows, 'native_graphics_smoke': False,
                   'fresh_machine_qualified': False, 'dependency_closure_verified': False,
                   'third_party_licenses_verified': False, 'signed': False, 'notarized': False}
        output.mkdir()
        try:
            with package.open('rb') as src, (output / package.name).open('xb') as dst:
                shutil.copyfileobj(src, dst)
            release.require(digest(output / package.name) == receipt['package_sha256'], 'Published copy differs')
            (output / 'receipt.json').write_text(json.dumps(receipt, sort_keys=True, indent=2) + '\n', encoding='utf-8')
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
