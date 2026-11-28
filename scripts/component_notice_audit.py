#!/usr/bin/env python3
"""Collect exact committed notices from the locked Kairo source workspace.

Evidence only: filenames and hashes do not establish licensing or coverage of
statically linked, fetched, transitive, or dynamically loaded dependencies.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess

MAX_NOTICE = 2 * 1024 * 1024
MAX_TOTAL = 64 * 1024 * 1024
MAX_NOTICES = 2048
NAME = re.compile(r'Kairo[A-Za-z0-9]+\Z')
SHA = re.compile(r'[0-9a-f]{40}\Z')
NOTICE = re.compile(r'(?:licen[cs]e|copying|copyright|notice|unlicense|third[-_ ]party[-_ ]notices)(?:[._ -].*)?\Z', re.I)


def require(ok, message):
    if not ok:
        raise ValueError(message)


def git(repo, *args):
    return subprocess.run(['git', '-C', str(repo), *args], check=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=60).stdout


def parse_lock(data):
    require(len(data) <= 65536, 'Workspace lock exceeds budget')
    rows = {}
    for line in data.decode('ascii').splitlines():
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        fields = line.split()
        require(len(fields) == 2 and NAME.fullmatch(fields[0]) and SHA.fullmatch(fields[1]),
                'Malformed workspace lock row: ' + line)
        require(fields[0] not in rows, 'Duplicate component: ' + fields[0])
        rows[fields[0]] = fields[1]
    require(rows, 'Empty workspace lock')
    return rows


def candidates(repo, commit):
    require(repo.is_dir() and not repo.is_symlink(), 'Missing or linked checkout: ' + str(repo))
    actual_root = Path(git(repo, 'rev-parse', '--show-toplevel').decode().strip()).resolve()
    require(actual_root == repo.resolve(), 'Not a standalone checkout: ' + str(repo))
    require(git(repo, 'rev-parse', 'HEAD').decode().strip() == commit, 'Checkout differs from pin: ' + str(repo))
    tree = git(repo, 'ls-tree', '-r', '-z', '--full-tree', commit)
    found, submodules, skipped = [], [], []
    for record in tree.split(b'\0'):
        if not record:
            continue
        meta, raw_path = record.split(b'\t', 1)
        mode, kind, oid = meta.decode('ascii').split()
        path = raw_path.decode('utf-8', errors='surrogateescape')
        if kind == 'commit':
            submodules.append(path)
        if not NOTICE.fullmatch(PurePosixPath(path).name):
            continue
        if kind != 'blob' or mode not in ('100644', '100755'):
            skipped.append(path)
            continue
        found.append((path, oid))
    return sorted(found), sorted(submodules), sorted(skipped)


def collect(engine, workspace, output, names):
    engine, workspace, output = Path(engine).resolve(), Path(workspace).resolve(), Path(output).absolute()
    lock = engine / 'workspace.lock'
    data = lock.read_bytes()
    pins = parse_lock(data)
    require(len(names) == len(set(names)) and all(name in pins for name in names), 'Unknown or duplicate selected component')
    commit = git(engine, 'rev-parse', 'HEAD').decode().strip()
    require(git(engine, 'show', commit + ':workspace.lock') == data, 'Workspace lock differs from committed bytes')
    require(not output.exists() and not output.is_symlink(), 'Output exists')
    require(not output.resolve().is_relative_to(engine) and not any(output.resolve().is_relative_to(workspace / name) for name in names),
            'Evidence output must be outside audited checkouts')
    sources = [('KairoGameEngine', engine, commit)] + [(name, workspace / name, pins[name]) for name in names]
    receipt = {'schema': 1, 'engine_commit': commit, 'workspace_lock_sha256': hashlib.sha256(data).hexdigest(),
               'scope': 'committed source notice candidates for selected locked components',
               'license_coverage_verified': False, 'third_party_licenses_verified': False, 'components': []}
    output.mkdir()
    total = count = 0
    try:
        for name, repo, pin in sources:
            files, submodules, skipped = candidates(repo, pin)
            records = []
            for number, (path, oid) in enumerate(files):
                size = int(git(repo, 'cat-file', '-s', oid))
                total += size
                count += 1
                require(0 < size <= MAX_NOTICE and total <= MAX_TOTAL and count <= MAX_NOTICES,
                        'Notice inventory exceeds bounds: ' + name + '/' + path)
                blob = git(repo, 'cat-file', 'blob', oid)
                require(len(blob) == size, 'Notice size mismatch')
                require(not blob.startswith(b'version https://git-lfs.github.com/spec/v1\n'), 'Notice is an unresolved LFS pointer')
                relative = f'notices/{name}/{number:04d}.txt'
                dest = output / relative
                dest.parent.mkdir(parents=True, exist_ok=True)
                dest.write_bytes(blob)
                records.append({'source_path': path, 'git_blob': oid, 'sha256': hashlib.sha256(blob).hexdigest(),
                                'bytes': size, 'evidence_path': relative, 'root_candidate': '/' not in path})
            receipt['components'].append({'name': name, 'commit': pin, 'notice_candidates': records,
                                          'root_notice_found': any(r['root_candidate'] for r in records),
                                          'submodules_not_inspected': submodules, 'nonregular_candidates': skipped})
        (output / 'component-notices.json').write_text(json.dumps(receipt, sort_keys=True, indent=2) + '\n', encoding='utf-8')
    except BaseException:
        shutil.rmtree(output)
        raise
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine', type=Path, required=True)
    parser.add_argument('--workspace', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('components', nargs='+')
    args = parser.parse_args()
    result = collect(args.engine, args.workspace, args.output, args.components)
    for row in result['components']:
        print(row['name'], row['commit'], 'root notice candidates:', row['root_notice_found'],
              'total candidates:', len(row['notice_candidates']))


if __name__ == '__main__':
    main()
