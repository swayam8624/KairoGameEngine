#!/usr/bin/env python3
"""Native integration gate for data-only proximity triggers in generic Player."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile


def run(player, sample, backend):
    player, sample = player.resolve(strict=True), sample.resolve(strict=True)

    def invoke(root, *args):
        result = subprocess.run([str(player), str(root/'Project.kproject'), *args],
                                cwd=root, capture_output=True, text=True, timeout=90)
        print(result.stdout, end='')
        return result

    validated = invoke(sample, '--validate')
    if validated.returncode or 'proximity triggers: 4' not in validated.stdout:
        raise RuntimeError(f'Trigger validation failed: {validated.stderr}')
    smoke = invoke(sample, '--renderer', backend, '--smoke')
    expected = ('Trigger 1: hide 12', 'Trigger 2: show 13', 'Native viewport smoke passed')
    if smoke.returncode or not all(value in smoke.stdout for value in expected):
        raise RuntimeError(f'Native trigger smoke failed: {smoke.stderr}')
    with tempfile.TemporaryDirectory(prefix='kairo trigger ') as work:
        target = Path(work)/'invalid'
        shutil.copytree(sample,target)
        path = target/'Config/Triggers.ktrigger'
        # A validly formed rule may not change the physics-backed player.
        path.write_text('kairo-triggers 1\ntrigger 1 0 0 0 1 once 1 hide\n',encoding='utf-8')
        rejected = invoke(target,'--validate')
        if not rejected.returncode or 'Trigger target must be' not in rejected.stderr:
            raise RuntimeError('Unsafe trigger target accepted')
    print('Native trigger registration, entry effects, capture and rejection passed.')


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('player',type=Path)
    parser.add_argument('sample',type=Path)
    parser.add_argument('backend',choices=('metal','vulkan'))
    args=parser.parse_args()
    run(args.player,args.sample,args.backend)
