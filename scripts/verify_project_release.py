#!/usr/bin/env python3
"""Verify a canonical Kairo release, including provenance and optional Debian launch."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import stat
import subprocess
import tempfile

from package_kairo_deb import package, payload, read_manifest


def require(ok, message):
    if not ok:
        raise ValueError(message)


def digest(path):
    """Stream SHA256 so large assets do not require a matching allocation."""
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def inventory(root):
    """No exclusions on a built payload: unexpected files are failures."""
    files, _ = payload(root)
    return {p.as_posix(): (root.joinpath(p).stat().st_size, digest(root/p)) for p in files}


def source_files(root):
    """Match the canonical recipe's reserved Build artifact tree.
    Keep compiled logic; omit only transient .kairo subtrees and root .git.
    Reject links and special files before copying any source payload.
    """
    require(root.is_dir() and not root.is_symlink(), 'Invalid source root')
    descriptor = root/'Project.kproject'
    require(descriptor.is_file() and not descriptor.is_symlink(), 'Invalid source descriptor')
    require(descriptor.stat().st_size <= 1024*1024, 'Descriptor too large')
    profiles = [shlex.split(line) for line in descriptor.read_text(encoding='utf-8').splitlines()
                if line.startswith('build-profile ')]
    require(sorted(profiles) == [['build-profile','Development','development','Build/Development'],
                                 ['build-profile','Release','release','Build/Release']], 'Gate requires canonical recipe profiles')
    files = []
    def excluded(p):
        parts = p.parts
        return parts[0] in ('.git','Build') or (
            parts[0] == '.kairo' and len(parts)>1 and parts[1] != 'compiled')
    for folder, dirs, names in os.walk(root, followlinks=False):
        current = Path(folder)
        for name in list(dirs):
            path = current/name; relative = path.relative_to(root)
            if excluded(relative): dirs.remove(name); continue
            require(stat.S_ISDIR(path.lstat().st_mode), f'Unsafe source directory: {relative}')
        for name in names:
            path = current/name; relative = path.relative_to(root)
            if excluded(relative): continue
            require(stat.S_ISREG(path.lstat().st_mode), f'Unsafe source file: {relative}')
            files.append(relative)
            require(len(files)<=100000,'Too many source files')
    require(sum((root/p).stat().st_size for p in files)<=16*1024**3,'Source exceeds byte budget')
    return sorted(files)


def authored_inventory(root):
    """Runtime may create transient cache/output files; authored bytes may not change."""
    return {p.as_posix(): ((root/p).stat().st_size, digest(root/p))
            for p in source_files(root)}


def verify_provenance(root, hashes, require_evidence=False):
    """Check actual packaged model/license content against recipe records.
    No claim that user-supplied attribution establishes legal permission.
    """
    imported={path for path in hashes if path.startswith('Content/Imported/') and path.endswith('.glb')}
    if 'ASSET_LICENSES.json' not in hashes:
        require(not imported,'Missing imported model provenance')
        return 0
    require(hashes['ASSET_LICENSES.json'][0]<=2*1024*1024,'Provenance exceeds budget')
    records=json.loads((root/'ASSET_LICENSES.json').read_text(encoding='utf-8'))
    require(isinstance(records,list) and len(records)<=128,'Invalid recipe provenance records')
    seen=set()
    for record in records:
        require(isinstance(record,dict),'Invalid provenance record')
        path=record.get('path')
        require(isinstance(path,str) and path not in seen and path in hashes and
                path.startswith('Content/Imported/') and path.endswith('.glb'),'Invalid provenance model path')
        seen.add(path)
        require(record.get('sha256')==hashes[path][1],'Model provenance checksum mismatch')
        require(('license_file' in record)==('license_sha256' in record),'Incomplete license evidence')
        require(not require_evidence or 'license_file' in record,'Catalog model missing license evidence')
        if 'license_file' in record:
            evidence=record['license_file']
            require(isinstance(evidence,str) and evidence.startswith('Licenses/') and evidence in hashes and
                    hashes[evidence][0]>0 and record['license_sha256']==hashes[evidence][1],'License evidence mismatch')
    require(seen==imported,'Unattributed imported model')
    return len(records)


def run(command, cwd, env=None):
    subprocess.run([str(p) for p in command], cwd=cwd, env=env, check=True, timeout=120)


def verify(player, project, smoke=False, deb=False, catalog_models=None):
    """Isolate source copy, package, delete source, verify and run relocation.
    Linux only: the Debian wrapper and native Vulkan gate are host-specific.
    """
    player=player.resolve(strict=True);project=project.resolve(strict=True)
    require(project.name=='Project.kproject','Gate requires canonical descriptor')
    with tempfile.TemporaryDirectory(prefix='kairo release ') as temp:
        root=Path(temp);source=root/'source project';source.mkdir()
        for relative in source_files(project.parent):
            dest=source/relative;dest.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(project.parent/relative,dest)
        expected=inventory(source);models=verify_provenance(source,expected,catalog_models is not None)
        require(catalog_models is None or models==catalog_models,'Catalog model count differs')
        run([player,source/project.name,'--validate'],root)
        run([player,source/project.name,'--package','Release'],root)
        relocated=root/'relocated release';shutil.move(source/'Build/Release',relocated);shutil.rmtree(source)
        manifest=read_manifest(relocated)
        require(manifest['profile']=='Release' and manifest['player']==f'bin/{player.name}','Wrong release profile/player')
        actual=inventory(relocated/'project')
        require(actual==expected,'Relocated release payload differs')
        require(int(manifest['project-files'])==len(actual) and int(manifest['project-bytes'])==sum(v[0] for v in actual.values()),'Manifest counts differ')
        require(digest(relocated/manifest['player'])==digest(player),'Packaged player differs')
        require(verify_provenance(relocated/'project',actual,catalog_models is not None)==models,'Provenance count differs')
        def launch(executable, env=None):
            run([executable,'--validate'],root,env)
            if smoke:run([executable,'--renderer','vulkan','--smoke'],root,env)
        # The launcher runs against a writable project. Never exercise it on
        # the pristine package input: runtime caches are not release payload.
        playable=root/'playable release'
        shutil.copytree(relocated,playable)
        launch(playable/'run.sh')
        require(authored_inventory(playable/'project')==expected,'Relocated launch changed authored payload')
        require(inventory(relocated/'project')==expected,'Pristine release payload changed')
        shutil.rmtree(playable)
        if deb:
            archive=package(relocated,root/'game.deb','kairo-release-fixture','0.1.0','Kairo release test')
            extracted=root/'installed tree';run(['dpkg-deb','-x',archive,extracted],root)
            shutil.rmtree(relocated)
            installed=extracted/'usr/lib/kairo/kairo-release-fixture'
            require(inventory(installed/'project')==expected and digest(installed/manifest['player'])==digest(player),'Debian payload differs')
            env=dict(os.environ,XDG_DATA_HOME=str(root/'user data'))
            launch(extracted/'usr/bin/kairo-release-fixture',env)
            user=root/'user data/kairo/kairo-release-fixture/0.1.0/project'
            require(authored_inventory(user)==expected and inventory(installed/'project')==expected,'Installed/user authored payload differs')
        result={'files':len(actual),'bytes':sum(v[0] for v in actual.values()),'licensed_models':models,'debian':deb,'native_smoke':smoke}
        print(json.dumps(result,sort_keys=True));return result


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('player',type=Path);parser.add_argument('project',type=Path)
    parser.add_argument('--smoke',action='store_true');parser.add_argument('--deb',action='store_true')
    parser.add_argument('--catalog-models',type=int,help='Require this many models, each with license evidence')
    args=parser.parse_args();verify(args.player,args.project,args.smoke,args.deb,args.catalog_models)
