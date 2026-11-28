#!/usr/bin/env python3
"""Export a private native bundle with notices from an installed Studio."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import tempfile

INDEX = 'studio-inventory.json'
MAX_INDEX = 32 * 1024**2
MAX_NOTICE = 2 * 1024**2
MAX_TOTAL = 64 * 1024**2


def require(ok, message):
    if not ok:
        raise ValueError(message)


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def read_json(path):
    require(path.is_file() and not path.is_symlink() and path.stat().st_size <= MAX_INDEX, 'Invalid index: ' + str(path))
    return json.loads(path.read_bytes())


def checked_bytes(root, name, index):
    require(isinstance(name, str) and re.fullmatch(r'[A-Za-z0-9_.-]+(?:/[A-Za-z0-9_.-]+)*', name)
            and all(p not in ('.', '..') for p in name.split('/')), 'Unsafe notice path')
    path = root / name
    require(path.resolve().is_relative_to(root) and not path.is_symlink() and path.is_file(), 'Invalid notice path')
    row = index.get(name)
    require(isinstance(row, dict) and row.get('bytes') == path.stat().st_size and 0 < path.stat().st_size <= MAX_NOTICE,
            'Notice size differs from Studio inventory: ' + name)
    data = path.read_bytes()
    require(hashlib.sha256(data).hexdigest() == row.get('sha256'), 'Notice hash differs: ' + name)
    return data


def stage_notices(studio, player, project):
    """Preflight all notices and exact Player identity, then write private output."""
    studio, player, project = Path(studio).resolve(strict=True), Path(player), Path(project)
    index = read_json(studio / INDEX)
    require(isinstance(index, dict), 'Invalid Studio inventory')
    key = 'build/game-clang/Runtime/KairoPlayer/' + player.name
    row = index.get(key)
    require(isinstance(row, dict) and row.get('bytes') == player.stat().st_size and row.get('sha256') == digest(player),
            'Packaged Player does not match installed Studio')
    first = json.loads(checked_bytes(studio, 'KairoNotices/notice-index.json', index))
    third = json.loads(checked_bytes(studio, 'ThirdPartySourceNotices/source-notices.json', index))
    require(first.get('schema') == 1 and third.get('schema') == 1 and third.get('distribution_scope') == 'studio', 'Invalid notice metadata')
    require(first.get('workspace_lock_sha256') == digest(studio / 'workspace.lock'), 'Notice lock mismatch')
    plans = {}
    def add(name, expected):
        data = checked_bytes(studio, name, index)
        require(hashlib.sha256(data).hexdigest() == expected and name not in plans, 'Invalid or duplicate notice record')
        plans[name] = data
    for row in first['notices']:
        add('KairoNotices/' + row['path'], row['sha256'])
    runtime = [row for row in third['dependencies'] if row['scope'] == 'runtime']
    require({row['name'] for row in runtime} == {'kairoassetsstb', 'kairoassetscgltf', 'nlohmann_json', 'kairoglad'}, 'Missing runtime notice collection')
    for dep in runtime:
        for row in dep['notices']:
            add('ThirdPartySourceNotices/' + row['path'], row['sha256'])
    third['dependencies'] = runtime
    third['distribution_scope'] = 'game'
    third['excluded_fetched_sources'] = sorted(set(third['excluded_fetched_sources']) | {'dearimgui', 'imguizmo'})
    for name, data in [('KairoNotices/notice-index.json', first), ('ThirdPartySourceNotices/source-notices.json', third)]:
        require(name not in plans, 'Notice index collision')
        plans[name] = (json.dumps(data, sort_keys=True, indent=2) + '\n').encode('utf-8')
    require(len(plans) <= 2048 and sum(map(len, plans.values())) <= MAX_TOTAL, 'Notice budget exceeded')
    roots = ('KairoNotices', 'ThirdPartySourceNotices')
    require(not any(p.name.casefold() in {n.casefold() for n in roots} for p in project.iterdir()), 'Authored project already contains reserved notice directory')
    created = []
    try:
        for name in roots:
            dest = project / name
            dest.mkdir()
            created.append(dest)
        for name, data in plans.items():
            dest = project / name
            dest.parent.mkdir(parents=True, exist_ok=True)
            with dest.open('xb') as out:
                out.write(data)
    except BaseException:
        for dest in created:
            shutil.rmtree(dest)
        raise
    return len(plans), sum(map(len, plans.values()))


def updated_manifest(data, files, size):
    text = data.decode('utf-8')
    for key, amount in [('project-files', files), ('project-bytes', size)]:
        text, count = re.subn(r'(?m)^' + key + r' ([0-9]+)\r?$', lambda m: key + ' ' + str(int(m[1]) + amount), text)
        require(count == 1, 'Malformed native manifest count')
    return text.encode('utf-8')


def export(bundle, output, studio):
    import kairo_release as release
    bundle, output, studio = Path(bundle).resolve(strict=True), Path(output).absolute(), Path(studio).resolve(strict=True)
    require(not output.exists() and not output.is_symlink() and output.suffix == '.zip', 'Expected unused ZIP output')
    require(not output.resolve().is_relative_to(bundle), 'Output must be outside package')
    files = release.entries(bundle)
    fields = release.manifest(files['package.kmanifest'].read_bytes())
    with files[fields['player']].open('rb') as stream:
        target = release.platform_of(stream)
    release.check_layout(fields, {name: p.stat().st_size for name, p in files.items()}, target)
    with tempfile.TemporaryDirectory(prefix='kairo-user-export-', dir=output.parent) as temp:
        copy = Path(temp) / 'private bundle'
        shutil.copytree(bundle, copy)
        count, size = stage_notices(studio, copy / fields['player'], copy / 'project')
        manifest = copy / 'package.kmanifest'
        manifest.write_bytes(updated_manifest(manifest.read_bytes(), count, size))
        return release.export(copy, output)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('bundle', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('studio', type=Path)
    args = parser.parse_args()
    print(json.dumps(export(args.bundle, args.output, args.studio), sort_keys=True))
