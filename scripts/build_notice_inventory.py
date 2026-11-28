"""Collect committed notices from actual FetchContent checkouts, not the internet."""
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess

# The pinned Kairo component CMake declarations are the source of this list.
DEPENDENCIES = {
    'kairoassetsstb': ('nothings/stb', '2c980bb59875b0d32144a71867fbdebb2f77cd20', 'runtime'),
    'kairoassetscgltf': ('jkuhlmann/cgltf', '85cd62382dfea638278962690cf515023f33ed00', 'runtime'),
    'nlohmann_json': ('nlohmann/json', 'v3.11.3', 'runtime'),
    'kairoglad': ('Dav1dde/glad', 'v0.1.36', 'runtime'),
    'dearimgui': ('ocornut/imgui', '572f249ce1975f98ad9f8aabce512ffa12a52d6c', 'editor'),
    'imguizmo': ('CedricGuillemet/ImGuizmo', 'fd138a7ce089cfcbfee16da8e8692f37a0e9481e', 'editor'),
    'catch2': ('catchorg/Catch2', None, 'test'),
}
NOTICE = re.compile(r'(?:licen[cs]e|copying|copyright|notice|unlicense)(?:[._ -].*)?\Z', re.I)
MAX_TOTAL = 64 * 1024**2


def require(ok, message):
    if not ok:
        raise ValueError(message)


def git(repo, *args):
    return subprocess.run(['git', '-C', str(repo), *args], check=True,
                          capture_output=True, timeout=60).stdout


def collect(build, dest, editor=False, dependencies=DEPENDENCIES, distribution=False):
    deps = Path(build).resolve(strict=True) / '_deps'
    dest = Path(dest).absolute()
    require(deps.is_dir() and not deps.is_symlink(), 'Missing FetchContent directory')
    require(not dest.exists() and not dest.is_symlink(), 'Notice output exists')
    require(not dest.resolve().is_relative_to(deps), 'Output inside dependency sources')
    found = {p.name.removesuffix('-src') for p in deps.glob('*-src') if p.is_dir()}
    records, blobs, excluded = [], [], []
    total = 0
    for name, (origin, revision, scope) in sorted(dependencies.items()):
        if distribution and (scope == 'test' or (scope == 'editor' and not editor)):
            if name in found:
                excluded.append(name)
            continue
        repo = deps / (name + '-src')
        if name not in found:
            require(scope == 'test' or (scope == 'editor' and not editor), 'Missing fetched source: ' + name)
            continue
        require(not repo.is_symlink() and Path(git(repo, 'rev-parse', '--show-toplevel').decode().strip()).resolve() == repo.resolve(), 'Not a standalone source checkout: ' + name)
        remote = git(repo, 'remote', 'get-url', 'origin').decode().strip()
        require(remote.removesuffix('.git').lower() == ('https://github.com/' + origin).lower(), 'Unexpected dependency origin: ' + name)
        commit = git(repo, 'rev-parse', 'HEAD').decode().strip()
        if revision and re.fullmatch(r'[0-9a-f]{40}', revision):
            require(commit == revision, f'Unexpected dependency pin: {name}: expected {revision}, actual {commit}')
        elif revision:
            require(git(repo, 'rev-parse', revision + '^{commit}').decode().strip() == commit, 'Unexpected dependency tag: ' + name)
        notices = []
        for raw in git(repo, 'ls-tree', '-r', '-z', 'HEAD').split(b'\0'):
            if not raw:
                continue
            meta, path = raw.split(b'\t', 1)
            path = path.decode('utf-8', errors='surrogateescape')
            mode, kind, oid = meta.decode('ascii').split()
            if kind != 'blob' or mode not in ('100644', '100755') or not NOTICE.fullmatch(PurePosixPath(path).name):
                continue
            size = int(git(repo, 'cat-file', '-s', oid))
            total += size
            require(0 < size <= 2 * 1024**2 and total <= MAX_TOTAL and len(blobs) < 2048, 'Notice budget exceeded')
            blob = git(repo, 'cat-file', 'blob', oid)
            require(len(blob) == size and not blob.startswith(b'version https://git-lfs.github.com/spec/v1\n'), 'Invalid notice blob')
            relative = f'{name}/{len(notices):04d}.txt'
            notices.append({'source_path': path, 'path': relative, 'git_blob': oid, 'sha256': hashlib.sha256(blob).hexdigest()})
            blobs.append((relative, blob))
        require(any('/' not in n['source_path'] for n in notices), 'Missing root notice: ' + name)
        records.append({'name': name, 'origin': origin, 'declared_revision': revision, 'commit': commit,
                        'scope': scope, 'tracked_worktree_changes': git(repo, 'diff', '--name-only', 'HEAD').decode(errors='replace').splitlines(),
                        'notices': notices})
    receipt = {'schema': 1, 'scope': 'actual fetched source notices; not linked-binary or redistribution coverage',
               'dependencies': records, 'unclassified_fetched_sources': sorted(found - dependencies.keys()),
               'distribution_scope': ('studio' if editor else 'game') if distribution else 'build-audit',
               'excluded_fetched_sources': excluded,
               'system_and_generated_code_reviewed': False, 'third_party_licenses_verified': False}
    dest.mkdir()
    try:
        for relative, blob in blobs:
            path = dest / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(blob)
        (dest / 'source-notices.json').write_bytes((json.dumps(receipt, indent=2, sort_keys=True) + '\n').encode())
    except BaseException:
        shutil.rmtree(dest)
        raise
    return receipt
