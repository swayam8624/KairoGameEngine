"""Inventory all Studio native imports and explicit Windows DLL candidates.

Candidate graphs are not an emulation of the platform loader or clearance to
redistribute libraries. No dependency is loaded, copied or executed here.
"""
import hashlib
from pathlib import Path
import re

MAX_DLLS = 256
MAX_IMPORTS = 4096
DLL = re.compile(r'[A-Za-z0-9_.+-]+\.dll\Z', re.I)


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def index_dlls(directories):
    """Index only immediate regular files in caller-selected trusted roots."""
    index = {}
    for directory in directories:
        root = Path(directory).resolve(strict=True)
        if not root.is_dir():
            raise ValueError('DLL search root is not a directory')
        for path in sorted(root.iterdir()):
            if not DLL.fullmatch(path.name) or not path.is_file() or path.is_symlink():
                continue
            choices = index.setdefault(path.name.lower(), set())
            choices.add(path.resolve(strict=True))
            if sum(map(len, index.values())) > MAX_IMPORTS:
                raise ValueError('DLL candidate index exceeds budget')
    return index


def windows_candidates(imports, roots, system, inspect):
    """Traverse unique explicit-root candidates, never system or ambiguous DLLs."""
    index = index_dlls(roots)
    system = Path(system).resolve(strict=True)
    system_names = {p.name.lower() for p in system.iterdir() if p.is_file() and DLL.fullmatch(p.name)}
    queue = sorted(set(name.lower() for name in imports))
    records = {}
    while queue:
        name = queue.pop(0)
        if name in records:
            continue
        if not DLL.fullmatch(name):
            raise ValueError('Invalid PE import: ' + name)
        if len(records) >= MAX_DLLS:
            raise ValueError('Dependency graph exceeds budget')
        paths = sorted(index.get(name, ()))
        row = {'name': name, 'candidates': [str(p) for p in paths]}
        if name.startswith(('api-ms-win-', 'ext-ms-win-')):
            row['status'] = 'os-api-contract'
        elif name in system_names:
            row['status'] = 'host-system-directory-present' if not paths else 'system-and-external-candidates'
        elif not paths:
            row['status'] = 'unresolved'
        elif len(paths) != 1:
            row['status'] = 'ambiguous'
        else:
            path = paths[0]
            deps = inspect(path)
            row.update(status='explicit-root-candidate', sha256=digest(path), imports=deps['imports'])
            queue.extend(n.lower() for n in deps['imports'])
            if len(queue) > MAX_IMPORTS:
                raise ValueError('Dependency edges exceed budget')
        records[name] = row
    return [records[name] for name in sorted(records)]


def collect(root, tools, inspect, target_of, dll_roots=(), system_dir=None):
    """Inspect each exact relocated tool; require one native target throughout."""
    root = Path(root).resolve(strict=True)
    records = []
    imports = set()
    target = None
    for relative in sorted(tools):
        path = (root / relative).resolve(strict=True)
        if not path.is_relative_to(root) or not path.is_file():
            raise ValueError('Tool escapes distribution: ' + str(relative))
        actual = target_of(path)
        if target is not None and actual != target:
            raise ValueError('Mixed Studio native targets')
        target = actual
        evidence = inspect(path, actual)
        imports.update(evidence['imports'])
        if len(imports) > MAX_IMPORTS:
            raise ValueError('Too many direct imports')
        records.append({'path': str(relative).replace('\\', '/'), 'sha256': digest(path),
                        'imports': evidence['imports'], 'search_paths': evidence['search_paths'],
                        'interpreter': evidence['interpreter'], 'tool': evidence['tool']})
    if not records:
        raise ValueError('Empty native tool inventory')
    candidates = []
    if target.startswith('windows-'):
        if system_dir is None:
            raise ValueError('Windows system directory required')
        candidates = windows_candidates(imports, dll_roots, system_dir, lambda p: inspect(p, target))
    return {'schema': 1, 'target': target, 'tools': records, 'direct_imports': sorted(imports),
            'windows_candidate_graph': candidates, 'dependency_closure_verified': False,
            'third_party_licenses_verified': False, 'dynamic_loads_inspected': False}
