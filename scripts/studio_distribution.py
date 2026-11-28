#!/usr/bin/env python3
"""Stage a bounded relocatable Studio toolset; no implicit compilation."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import tempfile
import zipfile

import kairo_release
import native_dependency_audit
import studio_import_inventory
import studio_dll_bundle
import unix_dependency_resolution
import first_party_notices
import build_notice_inventory

SCRIPTS = ('kairo_hub.py', 'kairo_hub_core.py', 'kairo_project_workflow.py',
           'create_kairo_project.py', 'kairo_world2d_editor.py', 'kairo_platformer_editor.py',
           'kairo_gameplay_editor.py', 'kairo_menu_designer.py', 'kairo_ui2d_designer.py',
           'kairo_audio2d_editor.py', 'kairo_vfx2d_editor.py', 'kairo_release.py', 'studio_game_export.py')
SAMPLES = ('PlayableVerticalSlice', 'BeaconRun', 'KeyGateArena', 'PlatformerProject',
           'World2DProject', 'PhysicsWorld2DProject', 'SignalVault')
TOOLS = {'components/KairoEditor': ('KairoEditorApp', 'KairoProjectCompiler'),
         'Runtime/KairoPlayer': ('KairoPlayer', 'KairoWorld2DTool', 'KairoWorld2DInputTool',
                                 'KairoWorld2DAudioTool', 'KairoMenuDocumentTool')}
MAX_BYTES = 4 * 1024**3
MAX_FILES = 20000
INDEX = 'studio-inventory.json'


def require(ok, message):
    if not ok:
        raise ValueError(message)


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def safe_path(name):
    # Archive paths are relative, ASCII, and canonical across desktop hosts.
    parts = name.split('/')
    require(name.isascii() and len(name) <= 1024 and all(p not in ('', '.', '..') and
            not p.endswith((' ', '.')) and all(32 <= ord(c) < 127 and c not in '\\:<>"|?*' for c in p)
            for p in parts), 'Unsafe path: ' + name)
    return name


def inventory(root):
    files = {}
    seen = set()
    total = 0
    for current, dirs, names in os.walk(root, followlinks=False):
        for leaf in dirs + names:
            path = Path(current) / leaf
            name = safe_path(path.relative_to(root).as_posix())
            require(name.casefold() not in seen, 'Case alias: ' + name)
            seen.add(name.casefold())
            mode = path.lstat().st_mode
            if leaf in dirs:
                require(stat.S_ISDIR(mode), 'Linked directory: ' + name)
                continue
            require(stat.S_ISREG(mode), 'Nonregular file: ' + name)
            total += path.stat().st_size
            require(total <= MAX_BYTES and len(files) < MAX_FILES, 'Studio inventory exceeds limits')
            files[name] = {'sha256': digest(path), 'bytes': path.stat().st_size,
                           'executable': bool(mode & stat.S_IXUSR)}
    return files


def copy_file(source, dest, executable=False):
    require(source.is_file() and not source.is_symlink(), 'Missing or linked file: ' + str(source))
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, dest)
    dest.chmod(0o755 if executable else 0o644)


def stage(source, build, dest):
    source, build, dest = Path(source).resolve(strict=True), Path(build).resolve(strict=True), Path(dest).absolute()
    require(not dest.exists() and not dest.is_symlink(), 'Destination exists')
    dest.mkdir()
    try:
        for name in SCRIPTS:
            copy_file(source / 'scripts' / name, dest / 'scripts' / name)
        for sample in SAMPLES:
            origin = source / 'Samples' / sample
            require(origin.is_dir() and not origin.is_symlink(), 'Missing sample: ' + sample)
            for name in inventory(origin):
                require(not any(p in ('.git', '.kairo', 'Build', '__pycache__') for p in name.split('/')),
                        'Transient sample content: ' + name)
                copy_file(origin / name, dest / 'Samples' / sample / name)
        for directory, names in TOOLS.items():
            for name in names:
                binary = name + ('.exe' if os.name == 'nt' else '')
                copy_file(build / directory / binary, dest / 'build/game-clang' / directory / binary, True)
        for name in ('LICENSE', 'workspace.lock'):
            copy_file(source / name, dest / name)
        if os.name == 'nt':
            (dest / 'launch-studio.cmd').write_bytes(
                b'@echo off\r\nsetlocal\r\nif not defined KAIRO_PYTHON set "KAIRO_PYTHON=python"\r\n'
                b'"%KAIRO_PYTHON%" -B "%~dp0scripts\\kairo_hub.py" %*\r\nexit /b %errorlevel%\r\n')
        else:
            (dest / 'launch-studio.sh').write_text('#!/bin/sh\nset -eu\nROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\nexec "${KAIRO_PYTHON:-python3}" -B "$ROOT/scripts/kairo_hub.py" "$@"\n', encoding='utf-8')
            (dest / 'launch-studio.sh').chmod(0o755)
        (dest / 'README.txt').write_text('Kairo Studio developer-host distribution\nRequires Python 3.11+ with Tk and native graphics/runtime dependencies.\nOn macOS, install a matching Python/Tk pair (for example brew install python@3.13 python-tk@3.13), then set KAIRO_PYTHON to its full interpreter path before launching.\nRun launch-studio.cmd on Windows or ./launch-studio.sh on Unix; projects are stored outside this directory.\nNo compiler or build system is invoked by the launcher.\nUnsigned, not clean-machine qualified.\n', encoding='utf-8')
        files = inventory(dest)
        (dest / INDEX).write_text(json.dumps(files, sort_keys=True, indent=2) + '\n', encoding='utf-8')
        return files
    except BaseException:
        shutil.rmtree(dest)
        raise


def archive_and_relocate(source, archive, target):
    files = inventory(source)
    require(not archive.exists() and not target.exists(), 'Output exists')
    with zipfile.ZipFile(archive, 'x', compression=zipfile.ZIP_DEFLATED, allowZip64=True) as z:
        for name in sorted(files):
            z.write(source / name, name)
    shutil.rmtree(source)
    target.mkdir()
    with zipfile.ZipFile(archive) as z:
        require(sorted(z.namelist()) == sorted(files), 'Archive inventory differs')
        for name, record in files.items():
            path = target / safe_path(name)
            path.parent.mkdir(parents=True, exist_ok=True)
            with z.open(name) as src, path.open('xb') as dst:
                shutil.copyfileobj(src, dst)
            path.chmod(0o755 if record['executable'] else 0o644)
    require(inventory(target) == files, 'Relocated content differs')
    return files


def run(command, cwd, env=None):
    subprocess.run([str(x) for x in command], cwd=cwd, env=env, check=True, timeout=180)


def qualify(source, build, output, graphical=False, dll_roots=(), dll_policy=None, notice_root=None, notice_engine=None):
    source, build, output = Path(source).resolve(strict=True), Path(build).resolve(strict=True), Path(output).absolute()
    require(os.name == 'nt' or sys.platform == 'darwin' or sys.platform.startswith('linux'), 'Unsupported native Studio host')
    require(not output.exists() and not output.is_symlink(), 'Output exists')
    require((dll_policy is None) == (notice_root is None), 'DLL policy and notice root must be supplied together')
    require(dll_policy is None or os.name == 'nt', 'App-local DLL bundling requires Windows')
    with tempfile.TemporaryDirectory(prefix='kairo-studio-', dir=output.parent) as temp:
        root = Path(temp)
        staged = root / 'staged Studio'
        stage(source, build, staged)
        notices = None
        third_party = None
        if notice_engine is not None:
            notices = first_party_notices.stage(notice_engine, staged / 'workspace.lock', staged / 'KairoNotices')
            if os.environ.get('GITHUB_SHA'):
                require(notices['engine_commit'] == os.environ['GITHUB_SHA'], 'Notice source commit differs')
            third_party = build_notice_inventory.collect(build, staged / 'ThirdPartySourceNotices',
                                                         editor=True, distribution=True)
            (staged / INDEX).unlink()
            (staged / INDEX).write_text(json.dumps(inventory(staged), sort_keys=True, indent=2) + '\n',
                                        encoding='utf-8')
        suffix = '.exe' if os.name == 'nt' else ''
        native_tools = [Path('build/game-clang') / directory / (name + suffix)
                        for directory, names in TOOLS.items() for name in names]
        def target_of(path):
            with path.open('rb') as stream:
                return kairo_release.platform_of(stream)
        system_dir = Path(os.environ['SystemRoot']) / 'System32' if os.name == 'nt' else None
        bundle_evidence = None
        if dll_policy is not None:
            staged_imports = studio_import_inventory.collect(
                staged, native_tools, native_dependency_audit.audit, target_of, dll_roots, system_dir)
            bundle_evidence = studio_dll_bundle.stage(
                staged, native_tools, staged_imports['windows_candidate_graph'],
                dll_policy, notice_root, dll_roots, staged_imports['target'])
            (staged / 'dll-bundle.json').write_text(json.dumps(bundle_evidence, sort_keys=True, indent=2) + '\n',
                                                  encoding='utf-8')
            (staged / INDEX).unlink()
            (staged / INDEX).write_text(json.dumps(inventory(staged), sort_keys=True, indent=2) + '\n',
                                        encoding='utf-8')
        archive = root / 'KairoStudio.zip'
        installed = root / 'relocated Studio'
        expected = archive_and_relocate(staged, archive, installed)
        projects = root / 'user projects'
        projects.mkdir()
        env = {**os.environ, 'PYTHONDONTWRITEBYTECODE': '1', 'PYTHONPATH': str(installed / 'scripts')}
        if bundle_evidence is not None:
            # Exercise explicit app-local copies without the build/vcpkg PATH.
            # Windows system libraries and the installed Python remain host prerequisites.
            env['PATH'] = os.pathsep.join((str(system_dir), os.environ['SystemRoot']))
        # Execute the installed Python modules from a different cwd, using
        # the real compiler and Player for all seven Hub template types.
        code = '''from pathlib import Path
import sys
from kairo_hub_core import TEMPLATES, create, editor_command
from kairo_project_workflow import ProjectTools, run_cli
root, projects = map(Path, sys.argv[1:3])
build = root / 'build/game-clang'
for template in TEMPLATES:
    project = create(template.key, 'Studio ' + template.key, projects, root)
    assert run_cli(ProjectTools(project, build, root), 'validate') == 0
    command = editor_command(project, build, 'auto', root)
    assert command
print('All installed Hub templates created and validated')
'''
        run([sys.executable, '-c', code, installed, projects], root, env)
        if notices is not None:
            # Test the same installed Workbench export path used by creators.
            export_code = '''from pathlib import Path
import sys
from kairo_project_workflow import ProjectTools, run_cli
from kairo_release import verify, unpack
root, projects = map(Path, sys.argv[1:3])
project = projects / 'Studio world2d/Project.kproject'
archive = projects / 'Creator game.zip'
tools = ProjectTools(project, root / 'build/game-clang', root)
assert run_cli(tools, 'package', archive=archive) == 0
index = verify(archive)
assert 'project/KairoNotices/notice-index.json' in index['files']
assert 'project/ThirdPartySourceNotices/source-notices.json' in index['files']
assert not any('ThirdPartySourceNotices/dearimgui/' in n or 'ThirdPartySourceNotices/imguizmo/' in n for n in index['files'])
for name in ('KairoNotices', 'ThirdPartySourceNotices'):
    assert not (project.parent / name).exists()
    assert not (project.parent / 'Build/Release/project' / name).exists()
unpack(archive, projects / 'Exported game')
'''
            run([sys.executable, '-c', export_code, installed, projects], root, env)
            run([projects / 'Exported game/bin' / ('KairoPlayer' + suffix),
                 projects / 'Exported game/project/Project.kproject', '--validate'], root, env)
        # Exercise the actual packaged launcher without opening a GUI.
        launcher = installed / ('launch-studio.cmd' if os.name == 'nt' else 'launch-studio.sh')
        launch_env = {**env, 'KAIRO_PYTHON': sys.executable}
        if os.name == 'nt':
            run(['cmd.exe', '/d', '/c', launcher, '--help'], root, launch_env)
        else:
            run([launcher, '--help'], root, launch_env)
        if graphical:
            require(os.name != 'nt', 'Windows graphics smoke is a separate qualification gate')
            editor = installed / ('build/game-clang/components/KairoEditor/KairoEditorApp' + suffix)
            project = projects / 'Studio empty/Project.kproject'
            run([editor, '--project', project, '--renderer', 'vulkan', '--frames', '3', '--no-layout-persistence'], root, env)
            player = installed / 'build/game-clang/Runtime/KairoPlayer/KairoPlayer'
            run([player, projects / 'Studio world2d/Project.kproject', '--renderer', 'vulkan', '--smoke'], root, env)
        require(inventory(installed) == expected, 'Studio changed installed tree')
        imports = studio_import_inventory.collect(
            installed, native_tools, native_dependency_audit.audit, target_of, dll_roots, system_dir)
        if imports['target'].startswith('linux-'):
            for tool in imports['tools']:
                if tool['interpreter'] is None and not tool['imports']:
                    tool['host_resolution'] = {'method': 'no ELF interpreter or direct imports',
                                               'host_loader_resolution_verified': False}
                else:
                    tool['host_resolution'] = unix_dependency_resolution.linux_loader_inventory(
                        installed / tool['path'], imports['target'], tool['interpreter'])
        elif imports['target'].startswith('macos-'):
            for tool in imports['tools']:
                tool['host_resolution'] = unix_dependency_resolution.macos_graph(
                    installed / tool['path'], tool,
                    lambda path: native_dependency_audit.audit(path, imports['target']))
        import_bytes = (json.dumps(imports, sort_keys=True, indent=2) + '\n').encode('utf-8')
        receipt = {'schema': 1, 'archive_sha256': digest(archive), 'workspace_lock_sha256': digest(installed / 'workspace.lock'),
                   'commit': os.environ.get('GITHUB_SHA'), 'platform': sys.platform,
                   'file_count': len(expected), 'hub_template_validation': True, 'launcher_help_validated': True,
                   'installed_creator_export_validated': notices is not None,
                   'editor_and_player_graphical_smoke': graphical,
                   'native_import_inventory_sha256': hashlib.sha256(import_bytes).hexdigest(),
                   'native_target': imports['target'], 'native_tools_inspected': len(imports['tools']),
                   'first_party_notice_count': len(notices['notices']) if notices else 0,
                   'first_party_notice_index_sha256': digest(installed / 'KairoNotices/notice-index.json') if notices else None,
                   'third_party_source_notice_dependencies': len(third_party['dependencies']) if third_party else 0,
                   'third_party_source_notice_index_sha256': digest(installed / 'ThirdPartySourceNotices/source-notices.json') if third_party else None,
                   'host_loader_resolved_tools': sum(bool(t.get('host_resolution', {}).get('host_loader_resolution_verified')) for t in imports['tools']),
                   'dependency_closure_verified': False,
                   'app_local_dll_policy_applied': bundle_evidence is not None,
                   'bundled_dll_count': len(bundle_evidence['libraries']) if bundle_evidence else 0,
                   'developer_path_removed_during_validation': bundle_evidence is not None,
                   'fresh_machine_qualified': False, 'signed': False, 'third_party_licenses_verified': False}
        output.mkdir()
        try:
            shutil.copyfile(archive, output / archive.name)
            require(digest(output / archive.name) == receipt['archive_sha256'], 'Published archive differs')
            (output / 'native-imports.json').write_bytes(import_bytes)
            (output / 'receipt.json').write_text(json.dumps(receipt, sort_keys=True, indent=2) + '\n', encoding='utf-8')
        except BaseException:
            shutil.rmtree(output)
            raise
    return receipt


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('build', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--graphical', action='store_true')
    parser.add_argument('--dll-root', type=Path, action='append', default=[])
    parser.add_argument('--dll-policy', type=Path)
    parser.add_argument('--notice-root', type=Path)
    parser.add_argument('--notice-engine', type=Path)
    args = parser.parse_args()
    print(json.dumps(qualify(args.source, args.build, args.output, args.graphical, args.dll_root,
                             args.dll_policy, args.notice_root, args.notice_engine), sort_keys=True))
