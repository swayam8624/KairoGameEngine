#!/usr/bin/env python3
"""Wrap a validated Linux Kairo release bundle in an unprivileged-launch .deb."""
import argparse
import os
from pathlib import Path
import re
import shlex
import shutil
import stat
import struct
import subprocess
import tempfile

from debian_dependencies import valid_dependency

MAX_BYTES = 16 * 1024**3
MAX_FILES = 100000


def require(condition, message):
    if not condition:
        raise ValueError(message)


def regular(path):
    """Only ordinary files are accepted; reject symbolic links and devices."""
    require(path.exists() and stat.S_ISREG(path.lstat().st_mode), f'Not a regular file: {path}')
    return path


def read_manifest(bundle):
    """Parse the exact RuntimePackaging v1 record grammar, without executing it."""
    path = regular(bundle / 'package.kmanifest')
    with path.open('rb') as stream:
        data = stream.read(16385)
    require(len(data) <= 16384, 'Manifest exceeds limit')
    lines = data.decode('utf-8').splitlines()
    require(lines and lines[0] == 'kairo-package 1', 'Unsupported package format')
    fields = {}
    for line in lines[1:]:
        tokens = shlex.split(line)
        require(len(tokens) == 2 and tokens[0] not in fields, 'Malformed or duplicate manifest field')
        fields[tokens[0]] = tokens[1]
    require(set(fields) == {'project', 'profile', 'kind', 'engine-version', 'player', 'project-files', 'project-bytes'}, 'Incomplete package manifest')
    require(fields['project'] == 'project/Project.kproject' and fields['kind'] == 'release', 'Expected a release bundle')
    require(re.fullmatch(r'bin/[A-Za-z0-9][A-Za-z0-9._-]{0,127}', fields['player']) is not None, 'Unsafe player path')
    require(re.fullmatch(r'[0-9]{1,12}', fields['project-files']) and re.fullmatch(r'[0-9]{1,20}', fields['project-bytes']), 'Invalid payload counts')
    return fields


def elf_arch(path):
    """Check Linux ELF class, endian and machine; do not trust a filename."""
    with regular(path).open('rb') as stream:
        header = stream.read(64)
    require(len(header) == 64 and header[:7] == b'\x7fELF\x02\x01\x01', 'Expected little-endian 64-bit ELF player')
    kind, machine = struct.unpack_from('<HH', header, 16)
    require(kind in (2, 3) and machine in (62, 183), 'Unsupported ELF executable architecture')
    require(path.stat().st_mode & 0o111, 'Player is not executable')
    return {62: 'amd64', 183: 'arm64'}[machine]


def payload(project):
    """Enumerate bounded regular files, including hidden compiled project assets."""
    require(project.is_dir() and not project.is_symlink(), 'Invalid project directory')
    files, total = [], 0
    for root, dirs, names in os.walk(project, followlinks=False):
        for name in dirs:
            p = Path(root) / name
            require(stat.S_ISDIR(p.lstat().st_mode), f'Unsafe project directory: {p}')
        for name in names:
            p = regular(Path(root) / name)
            total += p.stat().st_size
            files.append(p.relative_to(project))
            require(len(files) <= MAX_FILES and total <= MAX_BYTES, 'Project exceeds package budget')
    require(Path('Project.kproject') in files, 'Project descriptor missing')
    return sorted(files), total


def launcher(name, version, player):
    """Resolve install prefix relatively and create a versioned writable user copy.
    No maintainer script, sudo, shell evaluation of manifest or automatic upgrade.
    The lock protects cooperating launchers; an interrupted lock is explicit.
    """
    return f'''#!/bin/sh
set -eu
PREFIX=$(CDPATH= cd -- "$(dirname -- "$0")/../lib/kairo/{name}" && pwd)
DATA=${{XDG_DATA_HOME:-"$HOME/.local/share"}}/kairo/{name}/{version}
mkdir -p -- "$DATA"
if [ ! -d "$DATA/project" ]; then
    LOCK="$DATA/.first-launch-lock"
    if ! mkdir -- "$LOCK"; then
        echo "Project initialization is already running or needs lock recovery: $LOCK" >&2
        exit 1
    fi
    trap 'rm -rf -- "$LOCK"' EXIT HUP INT TERM
    if [ ! -d "$DATA/project" ]; then
        cp -R -- "$PREFIX/project" "$LOCK/project"
        chmod -R u+rwX -- "$LOCK/project"
        mv -T -- "$LOCK/project" "$DATA/project"
    fi
    rmdir -- "$LOCK"
    trap - EXIT HUP INT TERM
fi
exec "$PREFIX/{player}" "$DATA/project/Project.kproject" "$@"
'''


def package(bundle, output, name, version, maintainer, depends=()):
    """Build an actual Debian archive, never install or execute the player.
    Stage adjacent to the output, then publish by exclusive hard link. This
    assumes the source bundle does not change concurrently during packaging.
    """
    require(re.fullmatch(r'[a-z0-9][a-z0-9+.-]{1,63}', name) is not None, 'Invalid package name')
    require(re.fullmatch(r'[0-9][A-Za-z0-9.+~-]{0,63}', version) is not None, 'Invalid version')
    require(0 < len(maintainer) <= 200 and all(32 <= ord(c) <= 126 for c in maintainer), 'Invalid maintainer')
    require(len(depends) <= 128 and len(set(depends)) == len(depends) and all(map(valid_dependency, depends)), 'Invalid dependency relation')
    require(bundle.is_dir() and not bundle.is_symlink() and output.suffix == '.deb' and not output.exists() and not output.is_symlink(), 'Invalid bundle or output (no overwrite)')
    bundle = bundle.resolve(strict=True)
    manifest = read_manifest(bundle)
    require((bundle / 'bin').is_dir() and not (bundle / 'bin').is_symlink(), 'Unsafe binary directory')
    player = bundle / manifest['player']
    arch = elf_arch(player)
    files, size = payload(bundle / 'project')
    require(len(files) == int(manifest['project-files']) and size == int(manifest['project-bytes']), 'Project payload does not match manifest')
    require(player.stat().st_size <= MAX_BYTES, 'Player exceeds byte budget')
    require(not output.resolve().is_relative_to(bundle), 'Output must be outside source bundle')
    with tempfile.TemporaryDirectory(prefix='.kairo-deb-', dir=output.parent) as tmp:
        tmp = Path(tmp)
        stage = tmp / 'root'
        base = stage / 'usr/lib/kairo' / name
        for relative in files:
            dest = base / 'project' / relative
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(bundle / 'project' / relative, dest)
            dest.chmod(0o644)
        dest_player = base / manifest['player']
        dest_player.parent.mkdir(parents=True)
        shutil.copyfile(player, dest_player); dest_player.chmod(0o755)
        shutil.copyfile(bundle / 'package.kmanifest', base / 'package.kmanifest')
        bin_path = stage / 'usr/bin' / name
        bin_path.parent.mkdir(parents=True)
        bin_path.write_text(launcher(name, version, manifest['player']), encoding='utf-8'); bin_path.chmod(0o755)
        control = stage / 'DEBIAN/control'
        control.parent.mkdir()
        control.write_text(f'Package: {name}\nVersion: {version}\nArchitecture: {arch}\nMaintainer: {maintainer}\nSection: games\nPriority: optional\n' +
                           (f'Depends: {", ".join(depends)}\n' if depends else '') + 'Description: Kairo authored game release\n', encoding='utf-8')
        for p in stage.rglob('*'):
            if p.is_dir(): p.chmod(0o755)
        stage.chmod(0o755); control.chmod(0o644); (base / 'package.kmanifest').chmod(0o644)
        archive = tmp / 'package.deb'
        subprocess.run(['dpkg-deb', '--root-owner-group', '--build', str(stage), str(archive)], check=True, timeout=300)
        os.link(archive, output)  # fails atomically if another writer won
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('bundle', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--name', required=True)
    parser.add_argument('--version', required=True)
    parser.add_argument('--maintainer', required=True)
    parser.add_argument('--depends', action='append', default=[])
    args = parser.parse_args()
    try:
        print(package(args.bundle, args.output, args.name, args.version, args.maintainer, args.depends))
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        parser.exit(1, f'Installer rejected: {error}\n')


if __name__ == '__main__':
    main()
