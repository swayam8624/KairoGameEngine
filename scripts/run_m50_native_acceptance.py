#!/usr/bin/env python3
"""Require the exact native M50 CTest gates and preserve their evidence."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
GATES = frozenset((
    'KairoGameLabPlatformerNativeSmoke', 'KairoGameLabStrategyNativeSmoke',
    'KairoGameLabRPGNativeSmoke', 'KairoGameLabAnimationNativeSmoke', 'KairoSimulationWaterNativeSmoke',
    'KairoSimulationSoftNativeSmoke', 'KairoSimulationDestructionNativeSmoke'))


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def check_junit(path, expected):
    """CTest process success alone is insufficient: skipped tests also return 0.
    Require each named test exactly once and an explicit run status with no
    failure/error/skip child. CTest emits the actual execution status in XML.
    """
    root = ET.parse(path).getroot()
    results = {}
    for case in root.iter('testcase'):
        name = case.get('name')
        require(name in expected and name not in results, f'Unexpected or duplicate result: {name}')
        require(case.get('status') == 'run' and not any(case.find(tag) is not None for tag in ('failure', 'error', 'skipped')),
                f'Native gate did not run successfully: {name}')
        results[name] = {'status': 'passed', 'seconds': float(case.get('time', '0'))}
    require(set(results) == expected, f'Missing native results: {sorted(expected-set(results))}')
    return results


def run_ctest(build, evidence, expected=GATES):
    """Inspect test discovery, then run only the selected native label.
    A missing test, skip, failed process, timeout or malformed XML fails closed.
    The evidence directory must be new; never overwrite an earlier run.
    """
    build = build.resolve(strict=True)
    evidence.mkdir()
    evidence = evidence.resolve()
    base = ['ctest', '--test-dir', str(build), '-L', 'm50-native']
    discovery = subprocess.run(base + ['--show-only=json-v1'], capture_output=True, text=True, timeout=30, check=True)
    (evidence/'discovery.json').write_text(discovery.stdout, encoding='utf-8')
    names = [item['name'] for item in json.loads(discovery.stdout)['tests']]
    require(len(names) == len(expected) and set(names) == expected, f'Wrong native gate inventory: {names}')
    xml = evidence/'results.xml'
    with (evidence/'ctest.log').open('w', encoding='utf-8') as log:
        result = subprocess.run(base + ['--no-tests=error', '--output-on-failure', '--output-junit', str(xml)],
                                stdout=log, stderr=subprocess.STDOUT, text=True, timeout=480)
    require(result.returncode == 0, f'Native CTest failed ({result.returncode}); inspect {evidence}/ctest.log')
    return check_junit(xml, expected)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--evidence', type=Path, required=True)
    parser.add_argument('--jobs', type=int, default=2)
    args = parser.parse_args()
    try:
        require(sys.platform.startswith('linux') or sys.platform == 'darwin', 'Native gate requires Linux or macOS')
        require(1 <= args.jobs <= 16, 'Invalid build parallelism')
        build = args.build.resolve(strict=True)
        # Bind verification to the same source and sibling roots CMake uses.
        cache = {}
        for line in (build/'CMakeCache.txt').read_text(encoding='utf-8').splitlines():
            if line.startswith(('#','//')) or '=' not in line: continue
            key,value = line.split('=',1)
            cache[key.split(':',1)[0]] = value
        require(Path(cache['CMAKE_HOME_DIRECTORY']).resolve() == ROOT, 'Build uses a different engine source tree')
        workspace = Path(cache['KAIRO_WORKSPACE_ROOT']).resolve(strict=True)
        env = dict(os.environ, KAIRO_WORKSPACE_ROOT=str(workspace))
        revision = subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip()
        subprocess.run(['git', '-C', str(ROOT), 'diff', '--quiet', 'HEAD', '--'], check=True)
        lock_digest = hashlib.sha256((ROOT/'workspace.lock').read_bytes()).hexdigest()
        # Check exact sibling revisions before and after the native tests.
        repos = ['KairoMath','KairoGeometry','KairoSpatial','KairoPhysicsMath','KairoPhysicsEngine',
                 'KairoAssets','KairoAI','KairoECS','KairoReflection','KairoEngineCore','KairoRenderer','KairoEditor']
        command = ['bash', str(ROOT/'scripts/verify_workspace_lock.sh'), *repos]
        subprocess.run(command, cwd=ROOT, env=env, check=True)
        args.evidence.mkdir()
        # Build the actual targets before running tests; do not certify a stale
        # binary based solely on the current source checkout's revision.
        with (args.evidence/'build.log').open('w', encoding='utf-8') as log:
            subprocess.run(['cmake','--build',str(build),'--parallel',str(args.jobs),'--target','KairoGameLab','KairoSimulationLab'],
                           stdout=log,stderr=subprocess.STDOUT,text=True,timeout=1800,check=True)
        results = run_ctest(build, args.evidence/'ctest')
        subprocess.run(command, cwd=ROOT, env=env, check=True)
        require(subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip() == revision,
                'Engine revision changed during test')
        subprocess.run(['git','-C',str(ROOT),'diff','--quiet','HEAD','--'],check=True)
        require(hashlib.sha256((ROOT/'workspace.lock').read_bytes()).hexdigest() == lock_digest,'Lock changed during test')
        report = {'status':'passed','engine_revision':revision,'workspace_lock_sha256':lock_digest,'gates':results,
                  'binaries_sha256': {name: hashlib.sha256((build/'Runtime/KairoPlayer'/name).read_bytes()).hexdigest()
                                      for name in ('KairoGameLab','KairoSimulationLab')},
                  'scope':'Seven native sample objective/capture gates; not complete M50 or a full release certification'}
        (args.evidence/'summary.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        print(f'PASS: {len(results)} required native sample gates')
        return 0
    except (RuntimeError, OSError, ValueError, KeyError, ET.ParseError, subprocess.SubprocessError) as error:
        print(f'Native acceptance incomplete: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
