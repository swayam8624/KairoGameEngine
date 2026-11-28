#!/usr/bin/env python3
"""Exercise a real installed Studio's complete-game authoring and release path.

This is a qualification harness, not a product installer. It imports the
relocated Studio's actual Python authoring modules and executes its native
compiler, document validator and Player. All mutations occur in a new project
under a caller-selected scratch directory.
"""
import argparse
from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def inventory(root):
    return {p.relative_to(root).as_posix(): digest(p)
            for p in root.rglob('*') if p.is_file()}


def native(command, cwd, env=None, marker=None):
    result = subprocess.run(list(map(str, command)), cwd=cwd, env=env,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, errors='replace', timeout=180)
    print(result.stdout, flush=True)
    require(result.returncode == 0, 'Native operation failed: ' + str(command[0]))
    if marker:
        require(marker in result.stdout, 'Native execution did not report: ' + marker)


@contextmanager
def isolated_runtime_path():
    """Remove developer library paths only around the private native launch.

    Retain the explicitly configured graphics ICD and normal user environment.
    This tests the private installation under a narrower search environment;
    it does not prove actual OS loader selection or dependency closure.
    """
    previous = os.environ.copy()
    try:
        if os.name == 'nt':
            # os.environ is case-insensitive on Windows, but its copied dict
            # is not. Normalize the snapshot before looking up SystemRoot.
            system_root = next((value for key, value in previous.items()
                                if key.upper() == 'SYSTEMROOT'), None)
            require(system_root, 'Windows SystemRoot is missing')
            system = Path(system_root)
            require(system.is_absolute() and system.is_dir(), 'Invalid Windows SystemRoot')
            os.environ['PATH'] = os.pathsep.join(map(str, (system / 'System32', system)))
        elif sys.platform == 'darwin':
            os.environ['PATH'] = '/usr/bin:/bin:/usr/sbin:/sbin'
        else:
            raise RuntimeError('Private native path isolation is supported on Windows and macOS')
        for key in list(os.environ):
            if key.upper().startswith(('DYLD_', 'LD_', 'VCPKG_')) or key.upper() in ('CMAKE_PREFIX_PATH', 'PYTHONPATH', 'PYTHONHOME'):
                os.environ.pop(key, None)
        yield
    finally:
        os.environ.clear()
        os.environ.update(previous)


def qualify(studio, work, renderer=None, runtime=None):
    studio, work = Path(studio).resolve(strict=True), Path(work).resolve(strict=True)
    require(not work.is_relative_to(studio), 'Work directory must be outside Studio')
    sys.path.insert(0, str(studio / 'scripts'))
    from kairo_hub_core import TEMPLATES, create
    from kairo_project_workflow import ProjectTools, run_cli
    from kairo_world2d_editor import bounded_config, publish_config
    from kairo_ui2d_designer import LIMIT, parse, encode
    from kairo_release import verify, unpack

    # Only use the exact installed Studio modules, never source-checkout copies.
    for module in ('kairo_hub_core', 'kairo_project_workflow', 'kairo_world2d_editor',
                   'kairo_ui2d_designer', 'kairo_release'):
        require(Path(sys.modules[module].__file__).resolve().is_relative_to(studio / 'scripts'),
                'Imported authoring module outside installed Studio: ' + module)
    studio_before = inventory(studio)
    require(any(t.key == 'signalvault' and t.available for t in TEMPLATES),
            'Complete reference-game template not available')
    project = create('signalvault', 'Creator Complete Game', work, studio)
    root = project.parent
    tools = ProjectTools(project, studio / 'build/game-clang', studio)
    validator = tools.binary('KairoWorld2DTool', 'Runtime/KairoPlayer')
    ui = root / 'Config/UI.kui'
    campaign = root / 'Config/Campaign.kcampaign'
    original_ui = bounded_config(ui, LIMIT)
    original_campaign = bounded_config(campaign, 8192)
    require(original_ui and original_campaign, 'Reference authoring documents missing')
    private = root / '.kairo/progress/qualification.kprogress'
    private.parent.mkdir(parents=True)
    private.write_bytes(b'private qualification state')
    first, second = work / 'Original Game.zip', work / 'Edited Game.zip'
    require(run_cli(tools, 'package', archive=first) == 0, 'Original game export failed')
    first_hash = digest(first)
    first_index = verify(first)
    bundle = root / tools.profiles()['Release']
    before_reject = inventory(bundle)

    # Use the same UI designer's parser/serializer and native-validated
    # atomic publication path that the graphical editor uses.
    widgets = parse(original_ui)
    coins = next(w for w in widgets if w.id == 'coins')
    coins.text = 'BEACONS '
    coins.fg = 'e4bc58'
    revised_ui = encode(widgets)
    require(revised_ui != original_ui, 'UI edit did not change the document')
    try:
        publish_config(ui, original_ui, b'not a UI document\n', LIMIT, [validator, '--ui'])
    except ValueError:
        pass
    else:
        raise RuntimeError('Native validator accepted invalid authored UI')
    require(ui.read_bytes() == original_ui, 'Rejected UI edit changed authored file')
    publish_config(ui, original_ui, revised_ui, LIMIT, [validator, '--ui'])
    require(ui.read_bytes() == revised_ui, 'UI edit was not published')
    require(next(w for w in parse(ui.read_bytes()) if w.id == 'coins').text == 'BEACONS ',
            'Persisted UI edit cannot be reopened')
    try:
        publish_config(ui, original_ui, original_ui, LIMIT, [validator, '--ui'])
    except ValueError:
        pass
    else:
        raise RuntimeError('Stale authoring baseline was accepted')

    revised_campaign = original_campaign.replace(b'title "SIGNAL VAULT"', b'title "CREATOR VAULT"', 1)
    require(revised_campaign != original_campaign, 'Campaign edit did not change the document')
    publish_config(campaign, original_campaign, revised_campaign, 8192,
                   [validator, '--campaign', root])
    require(campaign.read_bytes() == revised_campaign, 'Campaign edit was not published')

    require(run_cli(tools, 'package') != 0, 'Implicit package replacement succeeded')
    require(inventory(bundle) == before_reject and digest(first) == first_hash,
            'Rejected package replacement altered previous release')
    require(run_cli(tools, 'package', archive=second, replace=True) == 0,
            'Edited game export failed')
    second_hash = digest(second)
    second_index = verify(second)
    require(first_hash != second_hash and digest(first) == first_hash, 'Prior ZIP was changed or second is stale')
    for index, expected_ui, expected_campaign in ((first_index, original_ui, original_campaign),
                                                  (second_index, revised_ui, revised_campaign)):
        files = index['files']
        for name, data in (('Config/UI.kui', expected_ui), ('Config/Campaign.kcampaign', expected_campaign)):
            require(files['project/' + name]['sha256'] == hashlib.sha256(data).hexdigest(),
                    'Exported authored document differs: ' + name)
        for name in ('Config/Logic.klogic2d', 'Config/Audio.kaudio', 'Config/VFX.kvfx',
                     'KairoNotices/notice-index.json', 'ThirdPartySourceNotices/source-notices.json'):
            require('project/' + name in files, 'Export lost required authored file or notice: ' + name)
        require('project/.kairo/progress/qualification.kprogress' not in files,
                'Export leaked private checkpoint data')
    require(ui.read_bytes() == revised_ui and campaign.read_bytes() == revised_campaign
            and private.read_bytes() == b'private qualification state', 'Export altered authored/private state')
    suffix = '.exe' if os.name == 'nt' else ''
    for archive, name in ((first, 'Original relocated game'), (second, 'Edited relocated game')):
        dest = work / name
        unpack(archive, dest)
        native([dest / 'bin' / ('KairoPlayer' + suffix), dest / 'project/Project.kproject', '--validate'], work)
    relocated = work / 'Edited relocated game'
    before = inventory(relocated)
    if renderer:
        native([relocated / 'bin' / ('KairoPlayer' + suffix), relocated / 'project/Project.kproject',
                '--renderer', renderer, '--smoke'], work,
               marker='Native World2D textured atlas viewport smoke passed')
        require(inventory(relocated) == before, 'Graphical smoke altered relocated game')
    private_result = False
    runtime_hashes = None
    if runtime is not None:
        from local_runtime_discovery import inspect_runtime
        from kairo_game_installer import install_game, use_ticket
        from launch_private_game import host_target
        import platform
        report = inspect_runtime(runtime, host_target(platform.system(), platform.machine()))
        # CI supplies a trusted local package source. These observed pins
        # bind this specific test, not third-party publisher authenticity.
        runtime_hashes = {name: entry['sha256'] for name, entry in report['files'].items()}
        pins = list(runtime_hashes.values())
        destination, ticket = work / 'Private edited game', work / 'External launch ticket.json'
        install_game(second, destination, runtime, *pins, ticket)
        with isolated_runtime_path():
            require(use_ticket(ticket, renderer or 'auto', 'check')['native_import_preflight_passed'],
                    'Private installation preflight failed')
            use_ticket(ticket, renderer or 'auto', 'validate')
            if renderer:
                outcome = use_ticket(ticket, renderer, 'smoke')
                print(outcome['output_tail'], flush=True)
                require('Native World2D textured atlas viewport smoke passed' in outcome['output_tail'],
                        'Private smoke did not report real atlas rendering')
            require(use_ticket(ticket, renderer or 'auto', 'check')['immutable_file_integrity_verified'],
                    'Private installed game changed after launch')
        private_result = True
    require(inventory(studio) == studio_before, 'Creator workflow altered installed Studio')
    return {'schema': 1, 'template': 'signalvault', 'original_archive_sha256': first_hash,
            'edited_archive_sha256': second_hash, 'native_authoring_validation': True,
            'invalid_and_stale_edits_rejected': True, 'authored_ui_and_campaign_edits_exported': True,
            'previous_release_and_private_state_preserved': True, 'both_relocated_players_validated': True,
            'relocated_graphical_smoke': renderer is not None,
            'private_install_and_launch_validation': private_result,
            'private_graphical_smoke': private_result and renderer is not None,
            'private_developer_path_removed': private_result,
            'requested_renderer': renderer, 'runtime_library_sha256': runtime_hashes,
            'actual_loader_selection_verified': False,
            'hardware_gpu_qualified': False, 'dependency_closure_verified': False,
            'redistribution_approved': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('studio', type=Path)
    parser.add_argument('work', type=Path)
    parser.add_argument('--renderer', choices=('metal', 'vulkan', 'auto'))
    parser.add_argument('--runtime', type=Path)
    args = parser.parse_args()
    print(json.dumps(qualify(args.studio, args.work, args.renderer, args.runtime), sort_keys=True))


if __name__ == '__main__':
    main()
