"""Check the declared editor dependency revisions without compiling Kairo."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile


SOURCES = {
    'DearImGui': ('ocornut/imgui', 'LICENSE.txt'),
    'ImGuizmo': ('CedricGuillemet/ImGuizmo', 'LICENSE'),
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def git(repo, *args, timeout=60):
    result = subprocess.run(['git', '-C', str(repo), *args], capture_output=True,
                            timeout=timeout, check=False)
    require(result.returncode == 0, 'Git command failed: ' + ' '.join(args) + ': ' +
            result.stderr.decode('utf-8', errors='replace')[-1500:])
    return result.stdout


def declarations(text):
    result = {}
    for name, (origin, license_path) in SOURCES.items():
        blocks = re.findall(r'FetchContent_Declare\(\s*' + name + r'\b([^)]*)\)', text, re.I)
        require(len(blocks) == 1, 'Expected one literal declaration: ' + name)
        urls = re.findall(r'\bGIT_REPOSITORY\s+(\S+)', blocks[0])
        pins = re.findall(r'\bGIT_TAG\s+(\S+)', blocks[0])
        url = 'https://github.com/' + origin + '.git'
        require(urls == [url] and len(pins) == 1 and re.fullmatch(r'[0-9a-f]{40}', pins[0]),
                'Unexpected origin or non-immutable declaration: ' + name)
        result[name] = (url, pins[0], license_path)
    return result


def probe(url, pin, license_path):
    require(re.fullmatch(r'[0-9a-f]{40}', pin), 'Expected exact commit')
    with tempfile.TemporaryDirectory(prefix='kairo-pin-') as temp:
        repo = Path(temp)
        git(repo, 'init', '-q')
        git(repo, 'fetch', '--no-tags', '--depth=1', url, pin, timeout=120)
        actual = git(repo, 'rev-parse', 'FETCH_HEAD^{commit}').decode().strip()
        require(actual == pin, f'Remote pin mismatch: expected {pin}, fetched {actual}')
        row = git(repo, 'ls-tree', actual, '--', license_path).decode().strip().split()
        require(len(row) == 4 and row[0] in ('100644', '100755') and row[1] == 'blob' and
                row[3] == license_path, 'Missing regular root notice: ' + license_path)
        size = int(git(repo, 'cat-file', '-s', row[2]))
        require(0 < size <= 2 * 1024**2, 'Invalid root notice size')
        data = git(repo, 'cat-file', 'blob', row[2])
        require(len(data) == size, 'Notice size differs')
        return {'origin': url, 'expected_commit': pin, 'actual_commit': actual,
                'notice_path': license_path, 'notice_blob': row[2],
                'notice_sha256': hashlib.sha256(data).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('editor_cmake', type=Path)
    args = parser.parse_args()
    for name, (url, pin, license_path) in declarations(args.editor_cmake.read_text(encoding='utf-8')).items():
        print(json.dumps({'dependency': name, 'declared_commit': pin, 'origin': url}), flush=True)
        print(json.dumps({'dependency': name, **probe(url, pin, license_path)}, sort_keys=True), flush=True)


if __name__ == '__main__':
    main()
