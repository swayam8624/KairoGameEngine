#!/usr/bin/env python3
"""Exercise an actual collectible-to-door event and invalid target binding."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

def run(player, sample, backend):
    player, sample = player.resolve(strict=True), sample.resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix='kairo key gate ') as tmp:
        root = Path(tmp)/'game'
        shutil.copytree(sample, root)
        scene = root/'Scenes/PlayableArena.kscene'
        text = scene.read_text(encoding='utf-8')
        old = 'transform -3 0.92 3.4 0 0 0 1 0.55 0.55 0.55'
        if text.count(old) != 1:
            raise RuntimeError('Unexpected key scene')
        scene.write_text(text.replace(old, 'transform 0 0.92 3.4 0 0 0 1 0.55 0.55 0.55'),encoding='utf-8')
        def invoke(*args):
            return subprocess.run([str(player), str(root/'Project.kproject'), *args],
                                  cwd=root,capture_output=True,text=True,timeout=90)
        smoke = invoke('--renderer',backend,'--smoke')
        if smoke.returncode or not all(x in smoke.stdout for x in
                ('Gameplay event 1: door-open 40','Beacons 1/1','Native viewport smoke passed')):
            raise RuntimeError(f'Native key event failed: {smoke.stdout} {smoke.stderr}')
        (root/'Config/Events.kevent').write_text('kairo-events 1\non 1 collected 50 1 once door-open 1\n',encoding='utf-8')
        invalid = invoke('--validate')
        if not invalid.returncode or 'registered door' not in invalid.stderr:
            raise RuntimeError(f'Invalid door accepted: {invalid.stdout} {invalid.stderr}')
    print('Native collectible-to-door event and invalid binding passed')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('player',type=Path);p.add_argument('sample',type=Path)
    p.add_argument('backend',choices=('metal','vulkan'))
    a=p.parse_args();run(a.player,a.sample,a.backend)
