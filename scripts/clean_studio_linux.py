#!/usr/bin/env python3
"""Exercise an exact qualified Studio ZIP without a build checkout or compiler."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import zipfile

MAX_BYTES = 4 * 1024**3
MAX_FILES = 20000
INDEX = 'studio-inventory.json'


def require(ok, message):
    if not ok:
        raise ValueError(message)


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def extract(archive, target, expected_hash):
    require(digest(archive) == expected_hash, 'Unexpected Studio ZIP hash')
    target = Path(target)
    require(not target.exists() and not target.is_symlink(), 'Installation already exists')
    with zipfile.ZipFile(archive) as z:
        entries = z.infolist()
        names = [item.filename for item in entries]
        require(0 < len(entries) <= MAX_FILES and len(set(n.casefold() for n in names)) == len(names), 'Invalid inventory size or duplicate path')
        require(sum(item.file_size for item in entries) <= MAX_BYTES, 'Archive exceeds byte budget')
        for item in entries:
            parts = item.filename.split('/')
            require(item.filename.isascii() and len(item.filename) <= 1024 and
                    all(p not in ('', '.', '..') and not p.endswith((' ', '.')) and
                        all(32 <= ord(c) < 127 and c not in '\\:<>"|?*' for c in p) for p in parts),
                    'Unsafe archive path')
            require(stat.S_ISREG(item.external_attr >> 16), 'Nonregular ZIP entry')
        require(INDEX in names and z.getinfo(INDEX).file_size <= 32 * 1024**2, 'Missing or oversized inventory')
        inventory = json.loads(z.read(INDEX))
        require(isinstance(inventory, dict) and set(inventory) == set(names) - {INDEX}, 'Archive inventory mismatch')
        target.mkdir()
        try:
            for item in entries:
                name = item.filename
                path = target / name
                path.parent.mkdir(parents=True, exist_ok=True)
                with z.open(item) as src, path.open('xb') as dst:
                    shutil.copyfileobj(src, dst)
                if name != INDEX:
                    row = inventory[name]
                    require(path.stat().st_size == row['bytes'] and digest(path) == row['sha256'], 'Archive content differs: ' + name)
                    path.chmod(0o555 if row['executable'] else 0o444)
                else:
                    path.chmod(0o444)
            for directory in sorted((p for p in target.rglob('*') if p.is_dir()), reverse=True):
                directory.chmod(0o555)
            target.chmod(0o555)
        except BaseException:
            for directory in [target, *(p for p in target.rglob('*') if p.is_dir())]:
                directory.chmod(0o755)
            shutil.rmtree(target)
            raise
    return inventory


def run_user(studio, work):
    require(os.geteuid() != 0, 'Run Studio as a normal user')
    require(not os.access(studio, os.W_OK), 'Installation must not be writable')
    sys.path.insert(0, str(studio / 'scripts'))
    from kairo_hub_core import TEMPLATES, create, editor_command
    from kairo_project_workflow import ProjectTools, run_cli
    from kairo_release import verify, unpack
    build = studio / 'build/game-clang'
    for template in TEMPLATES:
        project = create(template.key, 'Clean ' + template.key, work, studio)
        require(run_cli(ProjectTools(project, build, studio), 'validate') == 0, 'Template validation failed')
        require(editor_command(project, build, 'auto', studio), 'Missing editor command')
    project = work / 'Clean world2d/Project.kproject'
    archive = work / 'Creator game.zip'
    require(run_cli(ProjectTools(project, build, studio), 'package', archive=archive) == 0, 'Creator export failed')
    files = verify(archive)['files']
    require('project/KairoNotices/notice-index.json' in files and 'project/ThirdPartySourceNotices/source-notices.json' in files, 'Missing exported notices')
    require(not any('ThirdPartySourceNotices/dearimgui/' in n or 'ThirdPartySourceNotices/imguizmo/' in n for n in files), 'Editor notices leaked into game')
    require(not (project.parent / 'KairoNotices').exists(), 'Authored project changed')
    extracted = work / 'Exported game'
    unpack(archive, extracted)
    def run(*args):
        subprocess.run([str(x) for x in args], cwd=work, check=True, timeout=180)
    run(studio / 'launch-studio.sh', '--help')
    run(build / 'components/KairoEditor/KairoEditorApp', '--project', work / 'Clean empty/Project.kproject', '--renderer', 'vulkan', '--frames', '3', '--no-layout-persistence')
    run(extracted / 'bin/KairoPlayer', extracted / 'project/Project.kproject', '--renderer', 'vulkan', '--smoke')
    print('All seven clean-host templates, installed Editor and exported Player passed')


def main():
    action = sys.argv[1]
    if action == 'extract':
        extract(Path(sys.argv[2]), Path(sys.argv[3]), sys.argv[4])
    elif action == 'run':
        run_user(Path(sys.argv[2]).resolve(strict=True), Path(sys.argv[3]).resolve(strict=True))
    else:
        raise ValueError('Unknown action')


if __name__ == '__main__':
    main()
