"""Private macOS Studio relocation experiment; never publish its rewritten binaries."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

EXPECTED = {
    '/opt/homebrew/opt/glfw/lib/libglfw.3.dylib': 'libglfw.3.dylib',
    '/opt/homebrew/opt/vulkan-loader/lib/libvulkan.1.dylib': 'libvulkan.1.dylib',
}
ARCHIVE_SHA = '6957beef5dc7dc342c5d229ee61bad3546d8e5fe5d3f1cb06fba8b9c845d15f0'
IMPORT_SHA = '77a083ed7a39e52164ec4a0a4f2dede3e733abe5c83fe7970463bba7ffc89'


def require(ok, message):
    if not ok:
        raise ValueError(message)


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def system(name):
    return name.startswith(('/usr/lib/', '/System/Library/Frameworks/'))


def plan(evidence):
    """Allow only the two external imports observed in the exact source artifact."""
    require(evidence['target'] == 'macos-arm64' and len(evidence['tools']) == 7,
            'Unexpected native inventory')
    tools, libraries = {}, {}
    for row in evidence['tools']:
        path = row['path']
        parts = path.split('/')
        require(len(parts) == 5 and parts[:2] == ['build', 'game-clang'] and
                all(p not in ('', '.', '..') and '\\' not in p for p in parts), 'Unsafe tool path')
        require(path not in tools, 'Duplicate tool')
        changes = [n for n in row['imports'] if not system(n)]
        require(set(changes) <= set(EXPECTED), 'Unexpected non-system import')
        tools[path] = {'sha256': row['sha256'], 'changes': changes}
        for image in row['host_resolution']['images']:
            for edge in image['edges']:
                name = edge['import']
                if system(name):
                    continue
                require(name in EXPECTED and edge['status'] == 'unique-file-candidate' and
                        len(edge['candidates']) == 1, 'Unresolved or unexpected dylib edge')
                old = libraries.setdefault(name, edge['sha256'])
                require(old == edge['sha256'], 'Inconsistent candidate hashes')
    require(set(libraries) == set(EXPECTED), 'Missing runtime libraries')
    return tools, libraries


def command(*args):
    return subprocess.run([str(a) for a in args], check=True, text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=90).stdout


def imports(path):
    lines = command('/usr/bin/otool', '-L', path).splitlines()
    require(lines and lines[0].endswith(':'), 'Unexpected otool output')
    return [line.strip().split(' (compatibility version ', 1)[0]
            for line in lines[1:] if line.strip()]


def relocate(studio, evidence):
    """Mutate only a disposable private stage, not the original archive or Homebrew."""
    require(sys.platform == 'darwin' and os.uname().machine == 'arm64', 'Apple Silicon macOS required')
    tools, libraries = plan(evidence)
    studio = Path(studio).resolve(strict=True)
    index_path = studio / 'studio-inventory.json'
    index = json.loads(index_path.read_bytes())
    source_receipts = {}
    sources = {}
    for old, name in EXPECTED.items():
        source = Path(old).resolve(strict=True)
        require(source.is_relative_to('/opt/homebrew/Cellar') and source.is_file(), 'Expected Homebrew runtime source')
        require('arm64' in command('/usr/bin/lipo', '-archs', source).split(), 'Wrong dylib architecture')
        require(set(imports(source)) <= {old} | {n for n in imports(source) if system(n)}, 'Unexpected dylib imports')
        sources[old] = source
        actual = digest(source)
        source_receipts[name] = {'sha256': actual, 'matches_original_candidate': actual == libraries[old], 'source': str(source)}
    # Preflight all exact original tools and destinations before changing bytes.
    dirs = set()
    for relative, row in tools.items():
        path = studio / relative
        require(path.is_file() and digest(path) == row['sha256'] == index[relative]['sha256'], 'Native tool bytes differ')
        require(set(n for n in imports(path) if not system(n)) == set(row['changes']), 'Native imports differ')
        if row['changes']:
            dirs.add(path.parent)
    for directory in dirs:
        for name in EXPECTED.values():
            require(not (directory / name).exists(), 'Unexpected existing dylib')
    rewritten = []
    for directory in sorted(dirs):
        for old, name in EXPECTED.items():
            dest = directory / name
            shutil.copyfile(sources[old], dest)
            dest.chmod(0o755)
            command('/usr/bin/install_name_tool', '-id', '@loader_path/' + name, dest)
            command('/usr/bin/codesign', '--force', '--sign', '-', dest)
            command('/usr/bin/codesign', '--verify', '--strict', dest)
            require(all(system(n) or n == '@loader_path/' + name for n in imports(dest)), 'Dylib retains external import')
            rewritten.append(dest)
    for relative, row in tools.items():
        if not row['changes']:
            continue
        path = studio / relative
        path.chmod(0o755)
        for old in row['changes']:
            command('/usr/bin/install_name_tool', '-change', old, '@loader_path/' + EXPECTED[old], path)
        command('/usr/bin/codesign', '--force', '--sign', '-', path)
        command('/usr/bin/codesign', '--verify', '--strict', path)
        require(set(n for n in imports(path) if not system(n)) == {'@loader_path/' + EXPECTED[n] for n in row['changes']}, 'Relocated imports differ')
        rewritten.append(path)
    # The private tree is no longer the original artifact: record the new hashes.
    for path in rewritten:
        index[path.relative_to(studio).as_posix()] = {'bytes': path.stat().st_size, 'sha256': digest(path), 'executable': True}
    index_path.chmod(0o644)
    index_path.write_text(json.dumps(index, sort_keys=True, indent=2) + '\n')
    return {'source_libraries': source_receipts, 'rewritten_files': len(rewritten),
            'adhoc_signed': True, 'redistribution_approved': False, 'dependency_closure_verified': False}


if __name__ == '__main__':
    source, studio = map(Path, sys.argv[1:3])
    require(digest(source / 'KairoStudio.zip') == ARCHIVE_SHA, 'Unexpected source archive')
    require(digest(source / 'native-imports.json') == IMPORT_SHA, 'Unexpected source import inventory')
    print(json.dumps(relocate(studio, json.loads((source / 'native-imports.json').read_bytes())), sort_keys=True))
