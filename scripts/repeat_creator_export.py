#!/usr/bin/env python3
"""Exercise repeat exports using the actual installed native Player, never mocks."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def inventory(root):
    return {p.relative_to(root).as_posix(): digest(p)
            for p in root.rglob('*') if p.is_file()}


def qualify(studio, work):
    studio, work = Path(studio).resolve(strict=True), Path(work).resolve(strict=True)
    sys.path.insert(0, str(studio / 'scripts'))
    from kairo_hub_core import create
    from kairo_project_workflow import ProjectTools, run_cli
    from kairo_release import verify, unpack
    build = studio / 'build/game-clang'
    project = create('world2d', 'Repeat Export Fixture', work, studio)
    tools = ProjectTools(project, build, studio)
    bundle = project.parent / tools.profiles()['Release']
    readme = project.parent / 'README.md'
    original = readme.read_bytes()
    private = project.parent / '.kairo/progress/private.kprogress'
    private.parent.mkdir(parents=True)
    private.write_bytes(b'private user state')
    first, second = work / 'First release.zip', work / 'Second release.zip'
    require(run_cli(tools, 'package', archive=first) == 0, 'First native export failed')
    first_hash = digest(first)
    first_index = verify(first)['files']
    first_bundle = inventory(bundle)
    require(first_index['project/README.md']['sha256'] == hashlib.sha256(original).hexdigest(), 'First export does not match authored content')
    require(not any('/.kairo/' in name for name in first_index), 'Private checkpoint entered archive')
    require(run_cli(tools, 'package') != 0, 'Unapproved native replacement succeeded')
    require(inventory(bundle) == first_bundle and digest(first) == first_hash, 'Rejected replacement changed prior output')
    edited = original + b'\nRepeat export fixture: revised authored content.\n'
    readme.write_bytes(edited)
    require(run_cli(tools, 'package', archive=second, replace=True) == 0, 'Approved native replacement failed')
    second_hash = digest(second)
    second_index = verify(second)['files']
    require(second_hash != first_hash and digest(first) == first_hash, 'First ZIP was overwritten or second export is stale')
    require(second_index['project/README.md']['sha256'] == hashlib.sha256(edited).hexdigest(), 'Second export does not reflect authored edit')
    require(readme.read_bytes() == edited and private.read_bytes() == b'private user state', 'Export changed authored/user state')
    require(not any('/.kairo/' in name for name in second_index), 'Private checkpoint entered second archive')
    require(all(name in second_index for name in ('project/KairoNotices/notice-index.json', 'project/ThirdPartySourceNotices/source-notices.json')), 'Second export lost notices')
    require(not any((project.parent / name).exists() or (bundle / 'project' / name).exists() for name in ('KairoNotices', 'ThirdPartySourceNotices')), 'Export notices leaked into project/native bundle')
    second_bundle = inventory(bundle)
    try:
        tools.commands('package', archive=first, replace=True)
    except ValueError:
        pass
    else:
        raise RuntimeError('Existing ZIP destination accepted')
    require(digest(first) == first_hash and inventory(bundle) == second_bundle, 'ZIP rejection changed prior outputs')
    foreign = create('world2d', 'Foreign Output Fixture', work, studio)
    foreign_tools = ProjectTools(foreign, build, studio)
    unknown = foreign.parent / foreign_tools.profiles()['Release']
    unknown.mkdir(parents=True)
    sentinel = unknown / 'unrelated.txt'
    sentinel.write_bytes(b'do not delete')
    require(run_cli(foreign_tools, 'package', replace=True) != 0, 'Native Player replaced unrecognized output')
    require(inventory(unknown) == {'unrelated.txt': hashlib.sha256(b'do not delete').hexdigest()}, 'Unrecognized output changed')
    suffix = '.exe' if sys.platform == 'win32' else ''
    for archive, name, expected in ((first, 'First relocated', original), (second, 'Second relocated', edited)):
        destination = work / name
        unpack(archive, destination)
        require((destination / 'project/README.md').read_bytes() == expected, 'Relocated authored content differs')
        subprocess.run([str(destination / 'bin' / ('KairoPlayer' + suffix)), str(destination / 'project/Project.kproject'), '--validate'],
                       cwd=work, check=True, timeout=90)
    return {'first_archive_sha256': first_hash, 'second_archive_sha256': second_hash,
            'native_replace_validated': True, 'no_implicit_replace': True, 'unknown_output_preserved': True,
            'previous_zip_preserved': True, 'both_relocated_players_validated': True,
            'authored_and_private_state_preserved': True, 'runtime_notices_present': True}


if __name__ == '__main__':
    print(json.dumps(qualify(*sys.argv[1:3]), sort_keys=True))
