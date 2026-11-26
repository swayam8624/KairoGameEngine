#!/usr/bin/env python3
"""Exercise authored objectives through the actual generic KairoPlayer."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile


def run(player, sample, backend):
    player = player.resolve(strict=True)
    sample = sample.resolve(strict=True)

    def invoke(project, *args, expected=None, rejected=False):
        result = subprocess.run([str(player), str(project), *args],
                                cwd=project.parent, capture_output=True,
                                text=True, timeout=90)
        print(result.stdout, end='')
        if rejected:
            if result.returncode == 0 or expected not in result.stderr:
                raise RuntimeError(f'Invalid authored objective accepted: {result.stdout} {result.stderr}')
        elif result.returncode or (expected is not None and expected not in result.stdout):
            raise RuntimeError(f'Native objective gate failed: {result.stdout} {result.stderr}')
        return result.stdout

    # The actual authored six-beacon game must validate as-is.
    invoke(sample/'Project.kproject', '--validate', expected='collectible goals: 6')
    with tempfile.TemporaryDirectory(prefix='kairo beacon smoke ') as tmp:
        root = Path(tmp)/'game'
        shutil.copytree(sample, root)
        scene = root/'Scenes/BeaconRun.kscene'
        original = scene.read_text(encoding='utf-8')
        old = 'transform -4.6 0.92 3.8 0 0 0 1 0.55 0.55 0.55'
        new = 'transform 0 0.92 3.4 0 0 0 1 0.55 0.55 0.55'
        if original.count(old) != 1:
            raise RuntimeError('Unexpected Beacon Run fixture')
        # Place only one goal at the player's spawn, so the generic runtime
        # must collect it without injected input or fake gameplay APIs.
        scene.write_text(original.replace(old,new),encoding='utf-8')
        out = invoke(root/'Project.kproject','--renderer',backend,'--smoke',
                     expected='Beacons 1/6')
        if 'Native viewport smoke passed' not in out:
            raise RuntimeError('Missing native capture evidence')
        # A collectible on a rigid-body player must be rejected by validation.
        scene.write_text(original.replace('tag "kairo.player-controller"',
                         'tag "kairo.player-controller"\ntag "kairo.collectible"',1),encoding='utf-8')
        invoke(root/'Project.kproject','--validate',rejected=True,
               expected='Collectibles must be leaf visual-only entities.')
    print('Generic Player validated six goals, collected one at spawn, captured a native frame, and rejected invalid collectible.')


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('player',type=Path)
    parser.add_argument('sample',type=Path)
    parser.add_argument('backend',choices=('metal','vulkan'))
    args=parser.parse_args()
    run(args.player,args.sample,args.backend)
