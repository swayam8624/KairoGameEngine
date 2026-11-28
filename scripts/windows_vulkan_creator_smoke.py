"""Qualify actual exported World2D pixels with an explicit local Vulkan ICD."""
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import stat
import struct
import subprocess
import sys
import zipfile

import kairo_release as release

ARCHIVE_SHA = '4dbe093c70a9676f32cf399c0c093bbd740afff49f2597102ffe02db0cf68316'
IMPORT_SHA = '009c9eed1e95881db048c6a7f0121c24aa25518a8ccc90677438a0e1c995cd18'
COMMIT = '24f9c4975b2df0d46c96b01017bd761bf0238e94'
PLAYER = 'build/game-clang/Runtime/KairoPlayer/KairoPlayer.exe'
PLAYER_SHA = 'd58f791fa3e5d8c9bf73c8a86d35611eb7b84c2758c2e3dee51eaea7a9e22525'
DLLS = ('glfw3.dll', 'vulkan-1.dll')


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def inventory(root):
    return {p.relative_to(root).as_posix(): digest(p) for p in root.rglob('*') if p.is_file()}


def extract(source, target):
    source, target = Path(source), Path(target)
    receipt = json.loads((source / 'receipt.json').read_bytes())
    archive = source / 'KairoStudio.zip'
    imports = source / 'native-imports.json'
    release.require(receipt['commit'] == COMMIT and receipt['native_target'] == 'windows-x86_64'
                    and receipt['hub_template_validation'] and receipt['installed_creator_export_validated'], 'Unexpected Studio receipt')
    release.require(digest(archive) == receipt['archive_sha256'] == ARCHIVE_SHA, 'Studio archive hash differs')
    release.require(digest(imports) == receipt['native_import_inventory_sha256'] == IMPORT_SHA, 'Native import inventory differs')
    evidence = json.loads(imports.read_bytes())
    tools = {row['path']: row['sha256'] for row in evidence['tools']}
    release.require(len(tools) == 7 and evidence['target'] == 'windows-x86_64' and tools[PLAYER] == PLAYER_SHA, 'Unexpected native tools')
    release.require(not target.exists() and not target.is_symlink(), 'Studio destination already exists')
    with zipfile.ZipFile(archive) as z:
        entries = z.infolist()
        names = [release.safe_name(e.filename) for e in entries]
        release.require(0 < len(names) <= 20000 and len(set(n.casefold() for n in names)) == len(names)
                        and sum(e.file_size for e in entries) <= 4 * 1024**3, 'Unsafe Studio archive bounds')
        release.require(all(stat.S_ISREG(e.external_attr >> 16) and not e.flag_bits & 1 for e in entries), 'Invalid Studio entry type')
        index_name = 'studio-inventory.json'
        release.require(index_name in names and z.getinfo(index_name).file_size <= 32 * 1024**2, 'Invalid Studio inventory')
        index = json.loads(z.read(index_name))
        release.require(set(index) == set(names) - {index_name} and all(index[p]['sha256'] == h for p, h in tools.items()), 'Studio inventory differs')
        target.mkdir()
        try:
            for entry in entries:
                path = target / entry.filename
                path.parent.mkdir(parents=True, exist_ok=True)
                with z.open(entry) as src, path.open('xb') as dst:
                    size, checksum = release.copy_hash(src, dst)
                if entry.filename != index_name:
                    row = index[entry.filename]
                    release.require(size == row['bytes'] and checksum == row['sha256'], 'Studio content differs: ' + entry.filename)
        except BaseException:
            shutil.rmtree(target)
            raise
    return len(index)


def check_dll(path):
    release.require(path.is_file() and not path.is_symlink() and 0 < path.stat().st_size <= 256 * 1024**2, 'Invalid runtime DLL')
    with path.open('rb') as stream:
        release.require(release.platform_of(stream) == 'windows-x86_64', 'Wrong DLL architecture')
        stream.seek(60)
        offset = struct.unpack('<I', stream.read(4))[0]
        stream.seek(offset + 22)
        release.require(struct.unpack('<H', stream.read(2))[0] & 0x2000, 'PE file is not a DLL')
    return digest(path)


def driver_manifest(path):
    """Select only the installed Chrome SwiftShader ICD, not an arbitrary path."""
    path = Path(path).absolute()
    release.require(path.name == 'vk_swiftshader_icd.json' and path.is_file() and not path.is_symlink()
                    and 0 < path.stat().st_size <= 65536, 'Invalid SwiftShader manifest')
    data = json.loads(path.read_bytes())
    icd = data['ICD']
    release.require(data['file_format_version'] == '1.0.0' and isinstance(icd['api_version'], str)
                    and re.fullmatch(r'1\.[0-9]+\.[0-9]+', icd['api_version']), 'Unexpected ICD version')
    release.require(icd['library_path'] in ('vk_swiftshader.dll', './vk_swiftshader.dll', '.\\vk_swiftshader.dll'), 'Unexpected ICD library path')
    library = path.parent / 'vk_swiftshader.dll'
    return {'manifest_sha256': digest(path), 'driver_sha256': check_dll(library), 'api_version': icd['api_version']}


def qualify(source, work, runtime, driver):
    release.require(os.name == 'nt', 'Native Windows required')
    work, runtime, driver = map(lambda p: Path(p).resolve(strict=True), (work, runtime, driver))
    driver_evidence = driver_manifest(driver)
    release.require(set(p.name.lower() for p in runtime.iterdir()) == set(DLLS), 'Unexpected runtime directory contents')
    dlls = {name: check_dll(runtime / name) for name in DLLS}
    studio = work / 'Installed Studio'
    file_count = extract(source, studio)
    before = inventory(studio)
    projects = work / 'User Projects'; projects.mkdir()
    system = Path(os.environ['SystemRoot'])
    env = {k: v for k, v in os.environ.items() if not k.upper().startswith(('VK_', 'VULKAN_', 'VCPKG_'))
           and k.upper() not in ('PATH', 'PYTHONPATH', 'PYTHONHOME', 'CMAKE_PREFIX_PATH')}
    env.update(PATH=os.pathsep.join(map(str, (system / 'System32', system, runtime))),
               PYTHONDONTWRITEBYTECODE='1', PYTHONPATH=str(studio / 'scripts'),
               VK_DRIVER_FILES=str(driver), VK_ICD_FILENAMES=str(driver), VK_LOADER_DEBUG='error,warn,driver')
    code = '''from pathlib import Path
import sys
from kairo_hub_core import create
from kairo_project_workflow import ProjectTools, run_cli
from kairo_release import unpack
studio, projects = map(Path, sys.argv[1:])
project = create('world2d', 'Vulkan Fixture', projects, studio)
archive = projects / 'Exported 2D.zip'
assert run_cli(ProjectTools(project, studio / 'build/game-clang', studio), 'package', archive=archive) == 0
unpack(archive, projects / 'Exported 2D')
'''
    subprocess.run([sys.executable, '-B', '-c', code, str(studio), str(projects)], cwd=work, env=env, check=True, timeout=180)
    archive = projects / 'Exported 2D.zip'
    release.require(release.verify(archive)['files']['bin/KairoPlayer.exe']['sha256'] == PLAYER_SHA, 'Exported Player differs')
    exported = projects / 'Exported 2D'
    exported_before = inventory(exported)
    player = exported / 'bin/KairoPlayer.exe'
    project = exported / 'project/Project.kproject'
    result = subprocess.run([str(player), str(project), '--renderer', 'auto', '--smoke'], cwd=work, env=env,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace', timeout=180)
    print(result.stdout, flush=True)
    release.require(result.returncode == 0 and 'Native World2D textured atlas viewport smoke passed' in result.stdout,
                    'Automatic Windows Vulkan textured-atlas smoke failed')
    release.require(inventory(studio) == before and inventory(exported) == exported_before, 'Native smoke changed immutable files')
    return {'schema': 1, 'source_commit': COMMIT, 'studio_archive_sha256': ARCHIVE_SHA, 'native_tools': 7,
            'studio_files': file_count, 'export_archive_sha256': digest(archive), 'player_sha256': PLAYER_SHA,
            'software_driver': driver_evidence, 'runtime_dll_hashes': dlls, 'automatic_world2d_atlas_smoke': True,
            'hardware_gpu_qualified': False, 'redistribution_approved': False, 'dependency_closure_verified': False}


if __name__ == '__main__':
    print(json.dumps(qualify(*sys.argv[1:5]), sort_keys=True))
