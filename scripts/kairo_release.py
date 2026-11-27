#!/usr/bin/env python3
"""Create and verify deterministic host-native Kairo ZIP release archives.

Integrity checks are not signatures. No executable in an archive is run here.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shlex
import stat
import struct
import tempfile
import zipfile

MAX_FILES = 100004
MAX_BYTES = 17 * 1024**3
MAX_INDEX = 32 * 1024**2
INDEX = 'release.json'


def require(ok, message):
    if not ok:
        raise ValueError(message)


def safe_name(name):
    # Canonical portable relative names, including Windows device restrictions.
    require(isinstance(name, str) and 0 < len(name) <= 1024 and name.isascii(), 'Unsafe archive name')
    parts = name.split('/')
    for part in parts:
        require(part not in ('', '.', '..') and len(part) <= 240 and not part.endswith(('.', ' '))
                and all(32 <= ord(c) < 127 and c not in '\\:<>"|?*' for c in part)
                and not re.fullmatch(r'CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9]', part.split('.')[0], re.I),
                f'Unsafe path: {name}')
    return name


def manifest(data):
    require(len(data) <= 16384, 'Manifest exceeds limit')
    lines = data.decode('utf-8').splitlines()
    require(lines and lines[0] == 'kairo-package 1', 'Unknown native package format')
    fields = {}
    for line in lines[1:]:
        tokens = shlex.split(line)
        require(len(tokens) == 2 and tokens[0] not in fields, 'Malformed manifest')
        fields[tokens[0]] = tokens[1]
    require(set(fields) == {'project','profile','kind','engine-version','player','project-files','project-bytes'}, 'Incomplete manifest')
    require(fields['project'] == 'project/Project.kproject' and fields['kind'] in ('release','development'), 'Invalid package type')
    require(fields['player'] in ('bin/KairoPlayer','bin/KairoPlayer.exe'), 'Invalid player path')
    for key in ('project-files','project-bytes'):
        require(re.fullmatch(r'[0-9]{1,20}', fields[key]), 'Invalid package count')
    return fields


def platform_of(stream):
    # Read actual binary headers. Universal Mach-O is identified as such,
    # not falsely advertised as one architecture. This is not an ABI audit.
    header = stream.read(64)
    require(len(header) >= 32, 'Truncated executable')
    if header[:7] == b'\x7fELF\x02\x01\x01':
        kind, machine = struct.unpack_from('<HH', header, 16)
        require(kind in (2,3) and machine in (62,183), 'Unsupported ELF')
        return 'linux-' + {62:'x86_64',183:'arm64'}[machine]
    if header[:4] == b'\xcf\xfa\xed\xfe':
        cpu, _, kind = struct.unpack_from('<III', header, 4)
        require(kind == 2 and cpu in (0x01000007,0x0100000c), 'Unsupported Mach-O')
        return 'macos-' + {0x01000007:'x86_64',0x0100000c:'arm64'}[cpu]
    if header[:4] in (b'\xca\xfe\xba\xbe',b'\xca\xfe\xba\xbf'):
        count = struct.unpack_from('>I', header, 4)[0]
        require(1 <= count <= 16, 'Invalid fat Mach-O')
        return 'macos-universal' if count > 1 else 'macos-fat'
    if header[:2] == b'MZ' and len(header) == 64:
        offset = struct.unpack_from('<I',header,60)[0]
        require(64 <= offset <= 1024*1024, 'Invalid PE header offset')
        # ZIP streams support seek but only bounded offsets are allowed.
        stream.seek(offset)
        pe = stream.read(26)
        require(len(pe)==26 and pe[:4]==b'PE\0\0', 'Invalid PE signature')
        machine=struct.unpack_from('<H',pe,4)[0]
        require(machine in (0x8664,0xaa64) and pe[24:26]==b'\x0b\x02', 'Unsupported PE')
        return 'windows-' + {0x8664:'x86_64',0xaa64:'arm64'}[machine]
    raise ValueError('Unsupported executable header')


def entries(root):
    require(root.is_dir() and not root.is_symlink(), 'Invalid bundle root')
    files = {}; seen=set(); total=0
    for current, dirs, names in os.walk(root,followlinks=False):
        for name in dirs+names:
            path=Path(current)/name; rel=safe_name(path.relative_to(root).as_posix())
            require(rel.lower() not in seen, 'Case-colliding archive paths'); seen.add(rel.lower())
            mode=path.lstat().st_mode
            if name in dirs:
                require(stat.S_ISDIR(mode), f'Linked directory: {rel}'); continue
            require(stat.S_ISREG(mode) and rel != INDEX, f'Unsafe bundle file: {rel}')
            total += path.stat().st_size
            files[rel]=path
            require(len(files)<=MAX_FILES and total<=MAX_BYTES, 'Bundle exceeds limits')
    return files


def check_layout(fields, sizes, target):
    player=fields['player']; windows=target.startswith('windows-')
    launcher='run.cmd' if windows else 'run.sh'
    require(player == ('bin/KairoPlayer.exe' if windows else 'bin/KairoPlayer'), 'Executable/target mismatch')
    project={p:n for p,n in sizes.items() if p.startswith('project/')}
    require(fields['project'] in project and {player,launcher,'package.kmanifest'} <= sizes.keys(), 'Incomplete bundle')
    require(len(project)==int(fields['project-files']) and sum(project.values())==int(fields['project-bytes']), 'Payload counts differ')
    # Additional native libraries are not silently copied or claimed bundled.
    require(set(sizes)==set(project)|{player,launcher,'package.kmanifest'}, 'Unexpected bundle payload')
    for name in project:
        parts=PurePosixPath(name).parts[1:]
        require(parts[0]!='.git' and not (parts[0]=='.kairo' and (len(parts)<2 or parts[1]!='compiled')), 'Transient data in bundle')
    return launcher


def zip_info(name, executable=False):
    info=zipfile.ZipInfo(name,(1980,1,1,0,0,0))
    info.create_system=3
    info.external_attr=(stat.S_IFREG | (0o755 if executable else 0o644))<<16
    info.compress_type=zipfile.ZIP_DEFLATED
    return info


def copy_hash(source, target=None):
    digest=hashlib.sha256(); size=0
    while chunk:=source.read(1024*1024):
        size+=len(chunk); require(size<=MAX_BYTES,'Stream exceeds limit'); digest.update(chunk)
        if target is not None: target.write(chunk)
    return size,digest.hexdigest()


def export(bundle, output):
    bundle=Path(bundle); output=Path(output)
    require(output.suffix=='.zip' and not output.exists() and not output.is_symlink(), 'Expected unused ZIP output')
    require(not output.resolve().is_relative_to(bundle.resolve()), 'Output must be outside bundle')
    files=entries(bundle)
    require('package.kmanifest' in files, 'Missing native manifest')
    with files['package.kmanifest'].open('rb') as stream: fields=manifest(stream.read(16385))
    require(fields['player'] in files, 'Missing player')
    with files[fields['player']].open('rb') as stream: target=platform_of(stream)
    sizes={name:path.stat().st_size for name,path in files.items()}
    launcher=check_layout(fields,sizes,target)
    with tempfile.TemporaryDirectory(prefix='.kairo-export-',dir=output.parent) as directory:
        staged=Path(directory)/'release.zip'; records={}
        with zipfile.ZipFile(staged,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6,allowZip64=True) as archive:
            for name,path in sorted(files.items()):
                require(stat.S_ISREG(path.lstat().st_mode), 'Bundle changed during export')
                with path.open('rb') as source, archive.open(zip_info(name,name in (fields['player'],launcher)), 'w',force_zip64=True) as dest:
                    size,digest=copy_hash(source,dest)
                require(size==sizes[name], 'Bundle file changed size')
                records[name]={'bytes':size,'sha256':digest}
            index={'schema':1,'target':target,'profile':fields['profile'],'kind':fields['kind'],'files':records}
            data=json.dumps(index,sort_keys=True,separators=(',',':')).encode('ascii')+b'\n'
            require(len(data)<=MAX_INDEX,'Integrity index exceeds limit')
            archive.writestr(zip_info(INDEX),data)
        verify(staged)
        os.link(staged,output)  # exclusive publication, no replacement
    return index


def verify(path):
    with zipfile.ZipFile(path) as archive:
        infos=archive.infolist(); names={}; folded=set(); total=0
        require(len(infos)<=MAX_FILES+1,'Too many archive entries')
        for info in infos:
            name=safe_name(info.filename)
            require(name.lower() not in folded,'Duplicate or case-alias entry'); folded.add(name.lower())
            require(stat.S_ISREG(info.external_attr>>16) and not info.is_dir() and not info.flag_bits&1,'Unsafe ZIP entry type')
            total+=info.file_size; require(total<=MAX_BYTES+MAX_INDEX,'Archive exceeds limit')
            names[name]=info
        # Detect file/directory conflicts and case aliases at every prefix.
        directories={}
        for name in names:
            parts=name.split('/')
            for i in range(1,len(parts)):
                prefix='/'.join(parts[:i]); key=prefix.lower()
                require(key not in folded and directories.setdefault(key,prefix)==prefix,'Conflicting archive path prefix')
        require(INDEX in names and names[INDEX].file_size<=MAX_INDEX,'Missing or oversized integrity index')
        index=json.loads(archive.read(INDEX)); names.pop(INDEX)
        require(isinstance(index,dict) and set(index)=={'schema','target','profile','kind','files'} and index['schema']==1,'Unsupported integrity index')
        require(isinstance(index['files'],dict) and set(index['files'])==set(names),'Integrity inventory mismatch')
        require('package.kmanifest' in names and names['package.kmanifest'].file_size<=16384,'Missing native manifest')
        fields=manifest(archive.read('package.kmanifest'))
        require(fields['player'] in names,'Missing player')
        with archive.open(fields['player']) as stream: target=platform_of(stream)
        launcher=check_layout(fields,{name:info.file_size for name,info in names.items()},target)
        require((target,fields['profile'],fields['kind'])==(index['target'],index['profile'],index['kind']),'Release metadata mismatch')
        for name,info in names.items():
            expected_mode=0o755 if name in (fields['player'],launcher) else 0o644
            require(stat.S_IMODE(info.external_attr>>16)==expected_mode,'Unexpected executable permission')
            with archive.open(info) as stream: size,digest=copy_hash(stream)
            require(index['files'][name]=={'bytes':size,'sha256':digest},f'Integrity mismatch: {name}')
        return index


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    sub=parser.add_subparsers(dest='action',required=True)
    create=sub.add_parser('export');create.add_argument('bundle',type=Path);create.add_argument('output',type=Path)
    check=sub.add_parser('verify');check.add_argument('archive',type=Path)
    args=parser.parse_args()
    try:
        result=export(args.bundle,args.output) if args.action=='export' else verify(args.archive)
        print(json.dumps({k:v for k,v in result.items() if k!='files'} | {'file_count':len(result['files'])},sort_keys=True))
    except (ValueError,OSError,RuntimeError,zipfile.BadZipFile) as error:
        parser.exit(1,f'Kairo release rejected: {error}\n')


if __name__=='__main__':
    main()
