#!/usr/bin/env python3
"""Independent Linux C++23 regression gate for M50's portable implementations."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PLAYER = ROOT / 'Runtime/KairoPlayer'
TESTS = ('Combat', 'Health', 'SlidingDoor', 'ProximityTriggers', 'CollectibleRun', 'QuestDialogue', 'QuestSave', 'ItemInventory', 'RpgSession', 'TwoBoneIK', 'AnimationRetarget', 'SkeletonPose', 'PoseLab', 'ImportedTwoBone',
         'NetworkProtocol', 'NetworkPrediction', 'LoopbackTcp', 'CoopArena', 'CoopWorker', 'Weather', 'RainStreaks',
         'WaterGrid', 'SoftBody', 'TetraSurface', 'TetraPublication', 'BreakableCube', 'MenuDocument', 'MenuFocus', 'MenuPresentation',
         'Platformer', 'Strategy', 'GridCanvas', 'GameLabView')
HEADERS = ('RuntimeCombat.hpp', 'RuntimeHealth.hpp', 'RuntimeSlidingDoor.hpp', 'RuntimeProgressCheckpoint.hpp', 'RuntimeProximityTriggers.hpp', 'RuntimeCollectibleRun.hpp', 'CraftSaveFile.hpp', 'RuntimeQuestDialogue.hpp', 'RuntimeQuestSave.hpp', 'RuntimeItemInventory.hpp', 'RuntimeRpgSession.hpp',
           'RuntimeTwoBoneIK.hpp', 'RuntimeAnimationRetarget.hpp', 'RuntimeSkeletonPose.hpp', 'RuntimePoseLab.hpp', 'RuntimeImportedTwoBone.hpp', 'RuntimeNetworkProtocol.hpp',
           'RuntimeNetworkPrediction.hpp', 'RuntimeLoopbackTcp.hpp', 'RuntimeCoopArena.hpp', 'RuntimeCoopSession.hpp', 'RuntimeCoopWorker.hpp', 'RuntimeCoopView.hpp', 'RuntimeWeather.hpp',
           'RuntimeRainStreaks.hpp', 'RuntimeWaterGrid.hpp', 'RuntimeSoftBody.hpp', 'RuntimeTetraSurface.hpp', 'VoxelMeshPublication.hpp',
           'RuntimeBreakableCube.hpp', 'RuntimeMenuDocument.hpp', 'RuntimeMenuFocus.hpp', 'RuntimeMenuPresentation.hpp',
           'RuntimePlatformerCore.hpp', 'RuntimePlatformer.hpp', 'RuntimeStrategy.hpp', 'RuntimeGridCanvas.hpp', 'RuntimeGameLabView.hpp',
           'RuntimePluginABI.h', 'RuntimePluginLoader.hpp')
SAMPLES = ('Samples/NetworkLab/NetworkLab.cpp', 'Samples/NetworkLab/run_pair.py',
           'Samples/NetworkLab/run_matchmaking.py', 'Samples/PluginLab/ExamplePlugin.cpp',
           'Samples/PlatformerLab/PlatformerDemo.cpp', 'Samples/StrategyLab/StrategyDemo.cpp',
           'Samples/QuestDialogue/QuestDialogueDemo.cpp', 'scripts/kairo_asset_catalog.py',
           'scripts/kairo_matchmaking.py', 'scripts/tests/test_kairo_asset_catalog.py',
           'scripts/tests/test_kairo_matchmaking.py', 'Samples/NetworkLab/CoopLab.cpp', 'Samples/NetworkLab/run_coop.py',
           'Samples/NetworkLab/coop_directory.py',
           'Samples/AnimationLab/run_imported_ik_native.py', 'Samples/AnimationLab/test_imported_ik_fixture.py')


def fingerprint(path):
    """Hash actual source bytes; Git blob ID permits comparison with reviewed trees."""
    data = path.read_bytes()
    return {'sha256': hashlib.sha256(data).hexdigest(),
            'git_blob': hashlib.sha1(f'blob {len(data)}\0'.encode() + data).hexdigest()}


def run_suite(compiler, report):
    """Isolate binaries and saves, stop on failure, and retain command evidence."""
    if not sys.platform.startswith('linux'):
        raise RuntimeError('Portable gate currently supports Linux only')
    paths = [PLAYER / name for name in HEADERS]
    paths += [PLAYER / 'tests' / f'Runtime{name}Tests.cpp' for name in (*TESTS, 'PluginLoader', 'ProgressCheckpoint')]
    paths += [ROOT / name for name in SAMPLES]
    paths.extend((ROOT / 'scripts/run_progress_checkpoint.py', PLAYER / 'CraftSaveFile.hpp'))
    paths.append(Path(__file__).resolve())
    report['sources'] = {str(p.relative_to(ROOT)): fingerprint(p) for p in paths}
    env = dict(os.environ, PYTHONPATH=str(ROOT / 'scripts'))
    with tempfile.TemporaryDirectory(prefix='kairo-m50-') as work:
        work = Path(work)

        def step(name, args, input_text=None, expected=None):
            """Capture stdout/stderr and nonzero status; timeout is a failure."""
            args = [str(a) for a in args]
            entry = {'name': name, 'command': args, 'status': 'running'}
            report['steps'].append(entry)
            try:
                p = subprocess.run(args, cwd=work, env=env, input=input_text,
                                   capture_output=True, text=True, timeout=90)
                entry.update(returncode=p.returncode, stdout=p.stdout, stderr=p.stderr)
                if p.returncode or (expected is not None and expected not in p.stdout):
                    raise RuntimeError(f'{name} failed: {p.stdout} {p.stderr}')
                entry['status'] = 'passed'
                print(f'PASS {name}', flush=True)
            except BaseException:
                entry['status'] = 'failed'
                raise

        def build(name, source, output, *flags):
            """Compile using consistent strict C++23 settings."""
            step('build ' + name, [compiler, '-std=c++23', '-O2', '-Wall', '-Wextra', '-Werror',
                                   '-I', PLAYER, *flags, source, '-o', output])

        step('compiler', [compiler, '--version'])
        for name in TESTS:
            binary = work / name
            build(name, PLAYER / 'tests' / f'Runtime{name}Tests.cpp', binary, *(['-pthread'] if name in ('LoopbackTcp', 'CoopWorker') else []))
            step('test ' + name, [binary])
        checkpoint = work / 'progress-checkpoint'
        build('progress checkpoint', PLAYER / 'tests/RuntimeProgressCheckpointTests.cpp', checkpoint)
        step('cold progress restore', [sys.executable, ROOT / 'scripts/run_progress_checkpoint.py', checkpoint])
        network = work / 'network-lab'
        build('network lab', ROOT / SAMPLES[0], network)
        step('native loopback', [sys.executable, ROOT / SAMPLES[1], network])
        step('matchmaking connection', [sys.executable, ROOT / SAMPLES[2], network, ROOT / 'scripts/kairo_matchmaking.py'])
        coop = work / 'coop-lab'
        build('co-op lab', ROOT / 'Samples/NetworkLab/CoopLab.cpp', coop)
        step('three process co-op', [sys.executable, ROOT / 'Samples/NetworkLab/run_coop.py', coop])
        good, bad = work / 'good.so', work / 'bad.so'
        build('good plugin', ROOT / SAMPLES[3], good, '-shared', '-fPIC')
        build('bad plugin', ROOT / SAMPLES[3], bad, '-shared', '-fPIC', '-DKAIRO_PLUGIN_BAD_ABI')
        plugin_test = work / 'plugin-test'
        build('plugin tests', PLAYER / 'tests/RuntimePluginLoaderTests.cpp', plugin_test, '-ldl')
        step('native plugin ABI', [plugin_test, good, bad])
        for name, source, inputs, expected in (
            ('platformer', SAMPLES[4], 'r 31\nj 17\nr 80\n', 'Level complete!'),
            ('strategy', SAMPLES[5], 'move 2 1\nselect 1\ngather 8 1\ntick 100\n', 'Strategy objective complete!'),
            ('quest', SAMPLES[6], '1\n1\n2\n', 'Quest complete!')):
            binary = work / name
            build(name + ' demo', ROOT / source, binary)
            step(name + ' playthrough', [binary], inputs, expected)
        step('imported IK GLB byte fixture', [sys.executable, ROOT / 'Samples/AnimationLab/test_imported_ik_fixture.py'])
        for name in ('test_kairo_asset_catalog.py', 'test_kairo_matchmaking.py'):
            step(name, [sys.executable, '-m', 'unittest', 'discover', '-s', ROOT / 'scripts/tests', '-p', name])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True, help='GCC/Clang compiler executable')
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    report = {'schema': 1, 'scope': 'Linux portable M50; not native rendering or full workspace',
              'compiler': args.compiler, 'status': 'failed', 'steps': []}
    try:
        run_suite(args.compiler, report)
        report['status'] = 'passed'
    except Exception as error:
        report['error'] = str(error)
        print(str(error), file=sys.stderr)
    finally:
        args.report.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return 0 if report['status'] == 'passed' else 1


if __name__ == '__main__':
    sys.exit(main())
