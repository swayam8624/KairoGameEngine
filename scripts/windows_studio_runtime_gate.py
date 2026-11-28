"""Verify and exercise an exact Windows Studio artifact with a restricted PATH."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import zipfile

INDEX = 'studio-inventory.json'
ARCHIVE_SHA = '1fea8376d343e804b71648d2cb3508ad4c73c328818d48ef372b1f5fb9607c1c'
IMPORT_SHA = '80b8880170bd8300205578b630e8eb1e542771c697ccb3926f99b1a60920b9fd'
COMMIT = 'ca1941939ee6935bf3d8ec090bb2465f103b237d'
DLLS = ('glfw3.dll', 'vulkan-1.dll')


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def extract(source, target):
    source, target = Path(source), Path(target)
    receipt = json.loads((source / 'receipt.json').read_bytes())
    require(receipt['commit'] == COMMIT and receipt['native_target'] == 'windows-x86_64'
            and receipt['hub_template_validation'] and receipt['installed_creator_export_validated'], 'Unexpected qualification receipt')
    archive = source / 'KairoStudio.zip'
    imports = source / 'native-imports.json'
    require(digest(archive) == receipt['archive_sha256'] == ARCHIVE_SHA, 'Unexpected Studio archive')
    require(digest(imports) == receipt['native_import_inventory_sha256'] == IMPORT_SHA, 'Unexpected import inventory')
    evidence = json.loads(imports.read_bytes())
    require(len(evidence['tools']) == 7 and evidence['target'] == 'windows-x86_64', 'Unexpected native tools')
    rows = {r['name']: r for r in evidence['windows_candidate_graph']}
    require(rows['glfw3.dll']['status'] == 'explicit-root-candidate' and rows['vulkan-1.dll']['status'] == 'system-and-external-candidates', 'Unexpected DLL evidence')
    require(not target.exists() and not target.is_symlink(), 'Installation exists')
    with zipfile.ZipFile(archive) as z:
        entries = z.infolist()
        names = [e.filename for e in entries]
        require(0 < len(entries) <= 20000 and len(set(n.casefold() for n in names)) == len(names) and sum(e.file_size for e in entries) <= 4 * 1024**3, 'Invalid archive bounds')
        for entry in entries:
            require(entry.filename.isascii() and len(entry.filename) <= 1024 and stat.S_ISREG(entry.external_attr >> 16)
                    and all(p not in ('', '.', '..') and not p.endswith((' ', '.')) and all(32 <= ord(c) < 127 and c not in '\\:<>"|?*' for c in p) for p in entry.filename.split('/')), 'Unsafe archive path')
        require(INDEX in names and z.getinfo(INDEX).file_size <= 32 * 1024**2, 'Invalid inventory')
        index = json.loads(z.read(INDEX))
        require(set(index) == set(names) - {INDEX}, 'Inventory mismatch')
        target.mkdir()
        try:
            for entry in entries:
                path = target / entry.filename
                path.parent.mkdir(parents=True, exist_ok=True)
                with z.open(entry) as src, path.open('xb') as out:
                    shutil.copyfileobj(src, out)
                if entry.filename != INDEX:
                    row = index[entry.filename]
                    require(path.stat().st_size == row['bytes'] and digest(path) == row['sha256'], 'Content differs: ' + entry.filename)
            for row in evidence['tools']:
                require(digest(target / row['path']) == row['sha256'], 'Native tool hash differs')
        except BaseException:
            shutil.rmtree(target)
            raise
    return evidence


def inventory(root):
    return {p.relative_to(root).as_posix(): digest(p) for p in root.rglob('*') if p.is_file()}


def qualify(source, work, runtime):
    require(os.name == 'nt', 'Actual Windows host required')
    work, runtime = Path(work).resolve(strict=True), Path(runtime).resolve(strict=True)
    studio = work / 'Installed Studio'
    evidence = extract(source, studio)
    require({p.name.lower() for p in runtime.iterdir()} == set(DLLS), 'Runtime directory must contain only explicit prerequisite DLLs')
    for name in DLLS:
        require((runtime / name).is_file() and not (runtime / name).is_symlink(), 'Invalid prerequisite DLL')
    before = inventory(studio)
    system = Path(os.environ['SystemRoot'])
    env = {k: v for k, v in os.environ.items() if k.upper() not in ('PATH', 'PYTHONPATH', 'PYTHONHOME', 'CMAKE_PREFIX_PATH', 'VCPKG_ROOT', 'VCPKG_INSTALLATION_ROOT', 'VK_DRIVER_FILES', 'VK_ICD_FILENAMES', 'VK_LAYER_PATH')}
    env.update(PATH=os.pathsep.join(map(str, (system / 'System32', system, runtime))), PYTHONDONTWRITEBYTECODE='1', PYTHONPATH=str(studio / 'scripts'), KAIRO_PYTHON=sys.executable)
    projects = work / 'User Projects'; projects.mkdir()
    code = '''from pathlib import Path
import sys
from kairo_hub_core import TEMPLATES, create, editor_command
from kairo_project_workflow import ProjectTools, run_cli
from kairo_release import verify, unpack
studio, projects = map(Path, sys.argv[1:3])
build = studio / 'build/game-clang'
for template in TEMPLATES:
    project = create(template.key, 'Runtime ' + template.key, projects, studio)
    assert run_cli(ProjectTools(project, build, studio), 'validate') == 0
    assert editor_command(project, build, 'auto', studio)
for key in ('world2d', 'arena'):
    project = projects / f'Runtime {key}/Project.kproject'
    archive = projects / f'Creator {key}.zip'
    assert run_cli(ProjectTools(project, build, studio), 'package', archive=archive) == 0
    files = verify(archive)['files']
    assert 'project/KairoNotices/notice-index.json' in files
    assert 'project/ThirdPartySourceNotices/source-notices.json' in files
    unpack(archive, projects / f'Exported {key}')
'''
    def run(*args):
        subprocess.run([str(a) for a in args], cwd=work, env=env, check=True, timeout=180)
    run(sys.executable, '-B', '-c', code, studio, projects)
    run(system / 'System32/cmd.exe', '/d', '/c', studio / 'launch-studio.cmd', '--help')
    run(studio / 'build/game-clang/components/KairoEditor/KairoEditorApp.exe', '--project', projects / 'Runtime empty/Project.kproject', '--renderer', 'd3d12', '--frames', '3', '--no-layout-persistence')
    world2d = projects / 'Exported world2d'
    run(world2d / 'bin/KairoPlayer.exe', world2d / 'project/Project.kproject', '--validate')
    # World2D rendering requires a Metal or Vulkan overlay. Exercise the
    # same relocated Player on the D3D12-supported 3D Arena template.
    arena = projects / 'Exported arena'
    run(arena / 'bin/KairoPlayer.exe', arena / 'project/Project.kproject', '--renderer', 'd3d12', '--smoke')
    repeat_gate = Path(__file__).resolve().with_name('repeat_creator_export.py')
    repeat_work = work / 'Repeat packaging'; repeat_work.mkdir()
    run(sys.executable, '-B', repeat_gate, studio, repeat_work)
    # The prior gate used a dedicated prerequisite PATH. Now install the
    # exported ZIP privately with explicit app-local DLL bytes, then drop
    # even that runtime PATH. The original archive stays untouched.
    from install_local_windows_game import install
    from verify_private_game_install import audit
    from launch_private_game import preflight
    local = work / 'Private installed game'
    archive = projects / 'Creator arena.zip'
    original_archive = digest(archive)
    receipt = install(archive, local, runtime, {name: digest(runtime / name) for name in DLLS})
    require(receipt['source_archive_sha256'] == original_archive and
            digest(archive) == original_archive, 'Original game archive changed')
    receipt_hash = digest(local / 'local-runtime.json')
    before_audit = audit(archive, local, receipt_hash)
    readiness = preflight(archive, local, receipt_hash, 'd3d12')
    require(readiness['native_import_preflight_passed'], 'Private game launch preflight failed')
    print(json.dumps(readiness, sort_keys=True), flush=True)
    local_before = inventory(local)
    local_env = {**env, 'PATH': os.pathsep.join(map(str, (system / 'System32', system)))}
    subprocess.run([str(local / 'bin/KairoPlayer.exe'), str(local / 'project/Project.kproject'),
                    '--renderer', 'd3d12', '--smoke'], cwd=work, env=local_env, check=True, timeout=180)
    require(inventory(local) == local_before, 'Private game install changed during smoke')
    after_audit = audit(archive, local, receipt_hash)
    require(after_audit == before_audit, 'Private game audit changed during smoke')
    print(json.dumps(after_audit, sort_keys=True), flush=True)
    print(json.dumps(receipt, sort_keys=True), flush=True)
    require(inventory(studio) == before, 'Installed Studio changed')
    return {'schema': 1, 'source_commit': COMMIT, 'archive_sha256': ARCHIVE_SHA, 'native_import_inventory_sha256': IMPORT_SHA,
            'runtime_prerequisite_hashes': {name: digest(runtime / name) for name in DLLS},
            'glfw_matches_original_candidate': digest(runtime / 'glfw3.dll') == next(r['sha256'] for r in evidence['windows_candidate_graph'] if r['name'] == 'glfw3.dll'),
            'system_vulkan_present': (system / 'System32/vulkan-1.dll').is_file(),
            'developer_path_removed': True, 'seven_templates_validated': True, 'creator_export_validated': True,
            'world2d_exported_player_validated': True, 'd3d12_exported_arena_smoke': True,
            'native_repeat_export_validated': True, 'private_game_app_local_runtime_smoke': True,
            'private_game_immutable_integrity_verified': True, 'private_game_receipt_sha256': receipt_hash,
            'd3d12_editor_and_player_smoke': True, 'installed_tree_unchanged': True,
            'fresh_machine_qualified': False, 'dependency_closure_verified': False, 'third_party_licenses_verified': False}


if __name__ == '__main__':
    print(json.dumps(qualify(*sys.argv[1:4]), sort_keys=True))
