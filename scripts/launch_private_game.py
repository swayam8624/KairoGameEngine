#!/usr/bin/env python3
"""Preflight or launch a trusted private Kairo installation on its native host.

Requires the original release ZIP and a receipt digest captured separately at
installation time. No downloading, dependency loading, rebuilding or modifying
the installation is performed by preflight. Integrity is not authenticity.
"""
import argparse
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import zipfile

import kairo_release as release
from verify_private_game_install import audit

RENDERERS = ('auto', 'metal', 'vulkan', 'd3d12', 'opengl')


def host_target(system, machine):
    architecture = {'amd64': 'x86_64', 'x86_64': 'x86_64',
                    'aarch64': 'arm64', 'arm64': 'arm64'}.get(machine.lower())
    release.require(architecture is not None, 'Unsupported CPU architecture: ' + machine)
    family = {'Windows': 'windows', 'Darwin': 'macos'}.get(system)
    release.require(family is not None, 'Private installer supports Windows and Apple Silicon macOS')
    return family + '-' + architecture


def renderer_for(target, renderer, overlay):
    release.require(renderer in RENDERERS, 'Unknown renderer')
    allowed = {'auto', 'vulkan', 'd3d12', 'opengl'} if target.startswith('windows-') else {'auto', 'vulkan', 'metal', 'opengl'}
    if overlay:
        allowed &= {'auto', 'metal', 'vulkan'}
    release.require(renderer in allowed,
                    'Renderer is incompatible with this host or 2D project; choose ' + ', '.join(sorted(allowed)))
    return renderer


def preflight(archive, installation, receipt_sha256, renderer='auto', *,
              system=None, machine=None):
    """Static native readiness checks; do not infer actual GPU or loader support."""
    archive, root = Path(archive).resolve(strict=True), Path(installation).absolute()
    report = audit(archive, root, receipt_sha256)
    target = report['target']
    actual = host_target(system if system is not None else platform.system(),
                         machine if machine is not None else platform.machine())
    release.require(actual == target, f'Native host mismatch: game is {target}, host is {actual}')
    with zipfile.ZipFile(archive) as source:
        fields = release.manifest(source.read('package.kmanifest'))
    player = root / fields['player']
    descriptor = root / fields['project']
    with descriptor.open('rb') as stream:
        data = stream.read(1024 * 1024 + 1)
    release.require(len(data) <= 1024 * 1024, 'Project descriptor exceeds preflight limit')
    project_root = descriptor.parent
    # Match native dispatch: existing World2D or legacy Platformer config
    # is sufficient to classify an overlay project. Do not parse gameplay.
    overlay = any((project_root / name).is_file()
                  for name in ('Config/World.k2d', 'Config/Platformer.k2d'))
    overlay |= any(line.strip() in (b'startup-scene "Scenes/World2D.kscene"',
                                   b'startup-scene "Scenes/Platformer.kscene"')
                   for line in data.splitlines())
    renderer_for(target, renderer, overlay)
    if target.startswith('windows-'):
        from install_local_windows_game import DLLS, dll_target
        for name in DLLS:
            release.require(dll_target(player.parent / name) == target, 'Wrong app-local DLL architecture: ' + name)
        prerequisites = ['Windows system libraries and supported GPU driver',
                         'Vulkan ICD for Vulkan/automatic 2D rendering']
    else:
        from install_local_macos_game import LIBRARIES, inspect, system as os_library
        imports = {'@loader_path/' + n for n in LIBRARIES.values()}
        image = inspect(player, 2)
        release.require(not image['rpaths'] and {n for n in image['imports'] if not os_library(n)} == imports,
                        'Private Player does not have expected relative imports')
        for name in LIBRARIES.values():
            library = inspect(player.parent / name, 6)
            release.require(library['identities'] == ['@loader_path/' + name]
                            and not library['rpaths'] and all(os_library(n) for n in library['imports']),
                            'Unexpected private dylib imports: ' + name)
        prerequisites = ['Supported macOS version and Metal GPU',
                         'External Vulkan driver/ICD if Vulkan is requested']
    return {**report, 'host_target': actual, 'overlay_project': overlay,
            'requested_renderer': renderer, 'native_import_preflight_passed': True,
            'remaining_host_prerequisites': prerequisites,
            'gpu_execution_verified': False, 'actual_loader_selection_verified': False,
            'player': str(player), 'descriptor': str(descriptor)}


def launch(archive, installation, receipt_sha256, renderer='auto', action='play'):
    report = preflight(archive, installation, receipt_sha256, renderer)
    release.require(action in ('play', 'validate', 'smoke'), 'Unknown launch action')
    command = [report['player'], report['descriptor']]
    if action == 'validate':
        command.append('--validate')
    else:
        command += ['--renderer', renderer]
        if action == 'smoke':
            command.append('--smoke')
    print(json.dumps(report, sort_keys=True), flush=True)
    # Use an explicit executable and argv rather than a shell. Private runtime
    # libraries are adjacent to Player. Keep user display and driver environment
    # intact; this is not a sandbox or an isolated loader qualification.
    result = subprocess.run(command, cwd=Path(report['descriptor']).parent)
    return result.returncode if result.returncode >= 0 else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    parser.add_argument('installation', type=Path)
    parser.add_argument('--receipt-sha256', required=True)
    parser.add_argument('--renderer', choices=RENDERERS, default='auto')
    parser.add_argument('--action', choices=('check', 'validate', 'smoke', 'play'), default='check')
    args = parser.parse_args()
    try:
        if args.action == 'check':
            print(json.dumps(preflight(args.archive, args.installation, args.receipt_sha256, args.renderer), sort_keys=True, indent=2))
            return 0
        return launch(args.archive, args.installation, args.receipt_sha256, args.renderer, args.action)
    except (OSError, ValueError, RuntimeError, zipfile.BadZipFile) as error:
        parser.exit(1, f'Private game launch rejected: {error}\n')


if __name__ == '__main__':
    sys.exit(main())
