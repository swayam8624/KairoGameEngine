"""Host dependency evidence for trusted native build artifacts, not redistribution."""
import hashlib
import os
from pathlib import Path
import re
import subprocess
import sys

MAX_RECORDS = 512
ADDRESS = r'\s+\(0x[0-9a-fA-F]+\)\s*$'
GLIBC_LOADERS = {'linux-x86_64': ('/lib64/ld-linux-x86-64.so.2', '/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2'),
                 'linux-arm64': ('/lib/ld-linux-aarch64.so.1', '/lib/aarch64-linux-gnu/ld-linux-aarch64.so.1')}


def require(ok, message):
    if not ok:
        raise ValueError(message)


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def parse_loader_list(output):
    records = []
    seen = set()
    for line in output.splitlines():
        line = line.strip()
        if not line:
            continue
        if line.endswith(' => not found'):
            name = line.removesuffix(' => not found')
            path, status = None, 'unresolved'
        else:
            match = re.search(ADDRESS, line)
            require(match is not None, 'Unexpected loader output: ' + line)
            body = line[:match.start()]
            if ' => ' in body:
                name, path = body.split(' => ', 1)
                status = 'resolved-file'
            elif body.startswith('/'):
                name, path, status = body, body, 'resolved-file'
            else:
                require(body in ('linux-vdso.so.1', 'linux-gate.so.1'), 'Unknown virtual object: ' + body)
                name, path, status = body, None, 'kernel-virtual-object'
        require(name and name not in seen and len(records) < MAX_RECORDS, 'Duplicate/excess loader record')
        seen.add(name)
        if path is not None:
            require(Path(path).is_absolute(), 'Relative dependency path: ' + path)
        records.append({'name': name, 'path': path, 'status': status})
    require(records, 'Empty loader output')
    return records


def linux_loader_inventory(binary, target, interpreter):
    """Run the host glibc loader in --list mode on a trusted built executable.

    No app main execution, no arbitrary PT_INTERP executable, no LD_* overrides.
    This reports the current host's actual resolution, including transitive ELF
    objects. Runtime dlopen, plugins and fresh-machine compatibility remain out.
    """
    require(sys.platform.startswith('linux') and target in GLIBC_LOADERS, 'Unsupported Linux host/target')
    binary = Path(binary).resolve(strict=True)
    require(binary.is_file() and interpreter in GLIBC_LOADERS[target], 'Unsupported glibc interpreter')
    loader = Path(interpreter).resolve(strict=True)
    require(loader.is_file() and any(loader == Path(p).resolve() for p in GLIBC_LOADERS[target]), 'Unexpected host loader')
    env = {k: v for k, v in os.environ.items() if not k.startswith('LD_') and k != 'GLIBC_TUNABLES'}
    env['LC_ALL'] = 'C'
    result = subprocess.run([str(loader), '--list', str(binary)], cwd=binary.parent, env=env,
                            text=True, capture_output=True, timeout=30)
    require(result.returncode == 0, 'glibc dependency resolution failed: ' + result.stderr[:2000])
    require(len(result.stdout) <= 1024 * 1024, 'Loader output exceeds budget')
    rows = parse_loader_list(result.stdout)
    for row in rows:
        require(row['status'] != 'unresolved', 'Unresolved ELF dependency: ' + row['name'])
        if row['path']:
            file = Path(row['path']).resolve(strict=True)
            require(file.is_file(), 'Resolved object is not a regular file')
            row.update(real_path=str(file), sha256=digest(file))
    return {'method': 'host glibc --list without LD_* overrides', 'objects': rows,
            'host_loader_resolution_verified': True, 'dynamic_loads_inspected': False,
            'fresh_machine_qualified': False, 'third_party_licenses_verified': False}


def macos_candidates(import_name, owner, executable, rpaths):
    """Resolve path candidates from one image's LC_RPATH and executable root.

    Not dyld emulation: inherited runpath stacks, weak imports, shared-cache
    availability and environment overrides are intentionally not inferred.
    """
    owner, executable = Path(owner).resolve(), Path(executable).resolve()
    def expand(value):
        for token, base in (('@loader_path', owner.parent), ('@executable_path', executable.parent)):
            if value == token:
                return base
            if value.startswith(token + '/'):
                return base / value[len(token) + 1:]
        if value.startswith('/'):
            return Path(value)
        return None
    if import_name.startswith(('/usr/lib/', '/System/Library/')):
        return {'import': import_name, 'status': 'os-library-reference', 'candidates': []}
    paths = []
    if import_name.startswith('@rpath/'):
        for entry in rpaths:
            base = expand(entry)
            if base is not None:
                paths.append(base / import_name.removeprefix('@rpath/'))
    else:
        path = expand(import_name)
        if path is not None:
            paths.append(path)
    found = sorted({str(p.resolve()) for p in paths if p.is_file()})
    status = 'unique-file-candidate' if len(found) == 1 else 'ambiguous' if found else 'unresolved'
    return {'import': import_name, 'status': status, 'candidates': found}


def macos_graph(executable, initial, inspect):
    """Walk unique non-system file candidates, preserving per-image edges."""
    executable = Path(executable).resolve(strict=True)
    pending = [(executable, initial)]
    seen = set()
    images = []
    edges = 0
    while pending:
        owner, evidence = pending.pop(0)
        if owner in seen:
            continue
        require(len(seen) < MAX_RECORDS, 'Mach-O candidate graph exceeds budget')
        seen.add(owner)
        rows = []
        for name in evidence['imports']:
            edges += 1
            require(edges <= 4096, 'Mach-O candidate edges exceed budget')
            row = macos_candidates(name, owner, executable, evidence['search_paths'])
            if row['status'] == 'unique-file-candidate':
                path = Path(row['candidates'][0])
                row['sha256'] = digest(path)
                if path not in seen and not any(p == path for p, _ in pending):
                    pending.append((path, inspect(path)))
            rows.append(row)
        images.append({'image': str(owner), 'sha256': digest(owner), 'edges': rows})
    return {'method': 'per-image LC_RPATH candidate traversal, not dyld resolution', 'images': images,
            'host_loader_resolution_verified': False, 'dynamic_loads_inspected': False,
            'fresh_machine_qualified': False, 'third_party_licenses_verified': False}
