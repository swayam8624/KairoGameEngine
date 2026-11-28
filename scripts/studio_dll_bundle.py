"""Stage only explicitly reviewed Windows DLL bytes and matching notices.

The policy is an operator-supplied approval record, not legal clearance. This
module never infers permission from a package name or copies system DLLs.
"""
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct

HEX = re.compile(r'[0-9a-f]{64}\Z')
DLL = re.compile(r'[a-z0-9][a-z0-9_.+-]{0,126}\.dll\Z')
NOTICE = re.compile(r'[A-Za-z0-9][A-Za-z0-9_.-]{0,126}\Z')
MAX_FILE = 256 * 1024**2
MAX_TOTAL = 2 * 1024**3


def require(ok, message):
    if not ok:
        raise ValueError(message)


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def dll_target(path):
    """Check PE32+ machine and IMAGE_FILE_DLL, not only filename extension."""
    with path.open('rb') as stream:
        dos = stream.read(64)
        require(len(dos) == 64 and dos[:2] == b'MZ', 'Invalid DLL DOS header')
        offset = struct.unpack_from('<I', dos, 60)[0]
        require(64 <= offset <= 1024 * 1024, 'Invalid DLL PE offset')
        stream.seek(offset)
        pe = stream.read(26)
    require(len(pe) == 26 and pe[:4] == b'PE\0\0' and pe[24:] == b'\x0b\x02', 'Invalid PE32+ DLL')
    machine = struct.unpack_from('<H', pe, 4)[0]
    flags = struct.unpack_from('<H', pe, 22)[0]
    require(machine in (0x8664, 0xaa64) and flags & 0x2000, 'Unsupported DLL machine/type')
    return {0x8664: 'windows-x86_64', 0xaa64: 'windows-arm64'}[machine]


def load_policy(path):
    with Path(path).open('rb') as stream:
        data = stream.read(65537)
    require(len(data) <= 65536, 'DLL policy exceeds limit')
    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, 'Duplicate JSON key: ' + key)
            result[key] = value
        return result
    policy = json.loads(data, object_pairs_hook=unique)
    require(isinstance(policy, dict) and set(policy) == {'schema', 'target', 'libraries'} and
            policy['schema'] == 1 and policy['target'] in ('windows-x86_64', 'windows-arm64') and
            isinstance(policy['libraries'], list) and len(policy['libraries']) <= 256, 'Invalid DLL policy')
    rows = {}
    for row in policy['libraries']:
        require(isinstance(row, dict) and set(row) == {'name', 'sha256', 'notice', 'notice_sha256', 'origin'}, 'Invalid approval row')
        name = row['name']
        require(isinstance(name, str) and DLL.fullmatch(name) and name not in rows, 'Invalid or duplicate DLL name')
        require(all(isinstance(row[k], str) and HEX.fullmatch(row[k]) for k in ('sha256', 'notice_sha256')), 'Invalid approval hash')
        require(isinstance(row['notice'], str) and NOTICE.fullmatch(row['notice']) and
                not row['notice'].endswith('.') and isinstance(row['origin'], str) and
                0 < len(row['origin']) <= 512 and row['origin'].isprintable(), 'Invalid notice/origin')
        rows[name] = row
    return policy['target'], rows


def stage(root, tool_paths, graph, policy_path, notice_root, dll_roots, target):
    """Preflight all bytes before exclusive app-local copies; roll back new files.

    The graph must come from inspection of the exact staged executables. Paths
    are constrained to explicit source roots; this assumes a private stage and
    trusted, stable build inputs, not an adversarial concurrent writer.
    """
    root = Path(root).resolve(strict=True)
    approved_target, approved = load_policy(policy_path)
    require(approved_target == target, 'Policy target mismatch')
    roots = {Path(p).resolve(strict=True) for p in dll_roots}
    notices = Path(notice_root).resolve(strict=True)
    require(notices.is_dir(), 'Invalid notice root')
    dirs = set()
    for relative in tool_paths:
        binary = (root / relative).resolve(strict=True)
        require(binary.is_relative_to(root) and binary.is_file(), 'Invalid tool path')
        dirs.add(binary.parent)
    require(dirs, 'No tool directories')
    rows = {}
    for row in graph:
        name = row['name']
        require(isinstance(name, str) and DLL.fullmatch(name) and name not in rows, 'Invalid graph name')
        rows[name] = row
    candidates = {name for name, row in rows.items() if row['status'] == 'explicit-root-candidate'}
    require(candidates == set(approved), 'Every external candidate requires exact approval; no unused approvals')
    require(all(row['status'] in ('explicit-root-candidate', 'host-system-directory-present', 'os-api-contract')
                for row in rows.values()), 'Unresolved or ambiguous DLL dependency')
    plans, evidence = [], []
    total = 0
    for name in sorted(candidates):
        row, approval = rows[name], approved[name]
        require(len(row['candidates']) == 1, 'Expected unique DLL candidate')
        source = Path(row['candidates'][0])
        require(not source.is_symlink() and source.is_file() and source.resolve().parent in roots and
                source.name.lower() == name, 'DLL outside explicit root')
        require(0 < source.stat().st_size <= MAX_FILE and dll_target(source) == target, 'Wrong DLL architecture or size')
        require(digest(source) == row['sha256'] == approval['sha256'], 'DLL bytes differ from approval')
        notice = notices / approval['notice']
        require(not notice.is_symlink() and notice.is_file() and notice.resolve().parent == notices and
                0 < notice.stat().st_size <= 2 * 1024**2 and digest(notice) == approval['notice_sha256'], 'Notice bytes differ from approval')
        destinations = [directory / name for directory in sorted(dirs)]
        notice_dest = root / 'ThirdPartyNotices' / (name + '.txt')
        for dest in destinations + [notice_dest]:
            require(not dest.exists() and not dest.is_symlink(), 'Existing bundle destination: ' + str(dest))
            if dest.parent.exists():
                require(not any(p.name.casefold() == dest.name.casefold() for p in dest.parent.iterdir()),
                        'Case-colliding bundle destination: ' + str(dest))
        total += source.stat().st_size * len(destinations) + notice.stat().st_size
        require(total <= MAX_TOTAL, 'DLL copy budget exceeded')
        plans.extend((source, dest, approval['sha256']) for dest in destinations)
        plans.append((notice, notice_dest, approval['notice_sha256']))
        evidence.append({'name': name, 'sha256': approval['sha256'], 'origin': approval['origin'],
                         'notice_sha256': approval['notice_sha256'], 'notice': notice_dest.relative_to(root).as_posix(),
                         'copies': [p.relative_to(root).as_posix() for p in destinations]})
    created = []
    notice_dir = root / 'ThirdPartyNotices'
    made_dir = False
    try:
        if plans:
            notice_dir.mkdir()  # never reuse a preexisting notice directory
            made_dir = True
        for source, dest, checksum in plans:
            with source.open('rb') as src, dest.open('xb') as out:
                created.append(dest)
                shutil.copyfileobj(src, out)
            require(digest(dest) == checksum, 'Copy changed approved bytes')
    except BaseException:
        for path in reversed(created):
            path.unlink()
        if made_dir:
            notice_dir.rmdir()
        raise
    return {'schema': 1, 'target': target, 'approval_policy_sha256': digest(Path(policy_path)),
            'libraries': evidence, 'third_party_licenses_verified': False,
            'dependency_closure_verified': False}
