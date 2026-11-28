"""Ship exact root notices from the locked native Kairo sources."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

NATIVE = ('KairoMath', 'KairoGeometry', 'KairoSpatial', 'KairoPhysicsMath',
          'KairoPhysicsEngine', 'KairoAssets', 'KairoAI', 'KairoECS', 'KairoReflection',
          'KairoEngineCore', 'KairoRenderer', 'KairoEditor', 'KairoRayTracer')
INDEX = 'notice-index.json'


def require(ok, message):
    if not ok:
        raise ValueError(message)


def git(repo, *args):
    return subprocess.run(['git', '-C', str(repo), *args], check=True,
                          capture_output=True, timeout=60).stdout


def stage(engine, lock, dest, names=NATIVE):
    engine, lock, dest = Path(engine).resolve(), Path(lock).resolve(), Path(dest).absolute()
    raw = lock.read_bytes()
    require(len(raw) <= 65536, 'Lock exceeds budget')
    pins = {}
    for line in raw.decode('ascii').splitlines():
        if not line.strip() or line.lstrip().startswith('#'):
            continue
        name, pin = line.split()
        require(name not in pins and name.startswith('Kairo') and name.isalnum() and
                len(pin) == 40 and all(c in '0123456789abcdef' for c in pin), 'Invalid lock row')
        pins[name] = pin
    commit = git(engine, 'rev-parse', 'HEAD').decode().strip()
    require(git(engine, 'show', commit + ':workspace.lock') == raw, 'Lock differs from committed bytes')
    require(len(set(names)) == len(names) and all(n in pins for n in names), 'Missing or duplicate component')
    records = []
    blobs = []
    for name, repo, pin in [('KairoGameEngine', engine, commit)] + [(n, engine.parent / n, pins[n]) for n in names]:
        require(not repo.is_symlink() and Path(git(repo, 'rev-parse', '--show-toplevel').decode().strip()).resolve() == repo.resolve(), 'Invalid checkout: ' + name)
        require(git(repo, 'rev-parse', 'HEAD').decode().strip() == pin, 'Wrong pin: ' + name)
        row = git(repo, 'ls-tree', pin, '--', 'LICENSE').decode().strip().split()
        require(len(row) == 4 and row[0] in ('100644', '100755') and row[1] == 'blob' and row[3] == 'LICENSE', 'Missing regular LICENSE: ' + name)
        oid = row[2]
        size = int(git(repo, 'cat-file', '-s', oid))
        require(0 < size <= 2 * 1024 * 1024, 'Invalid notice size: ' + name)
        blob = git(repo, 'cat-file', 'blob', oid)
        require(len(blob) == size and blob.startswith(b'MIT License'), 'Expected existing MIT notice: ' + name)
        records.append({'component': name, 'commit': pin, 'git_blob': oid,
                        'path': name + '.txt', 'sha256': hashlib.sha256(blob).hexdigest()})
        blobs.append(blob)
    receipt = {'schema': 1, 'engine_commit': commit, 'workspace_lock_sha256': hashlib.sha256(raw).hexdigest(),
               'notices': records, 'third_party_licenses_verified': False}
    dest.mkdir()  # exclusive; never replace a caller directory
    try:
        for row, blob in zip(records, blobs):
            (dest / row['path']).write_bytes(blob)
        (dest / INDEX).write_bytes((json.dumps(receipt, sort_keys=True, indent=2) + '\n').encode('utf-8'))
    except BaseException:
        shutil.rmtree(dest)
        raise
    return receipt
