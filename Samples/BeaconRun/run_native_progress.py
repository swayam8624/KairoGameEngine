#!/usr/bin/env python3
"""Exercise actual KairoPlayer progress across two cold native launches."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

def run(player,sample,backend):
    player,sample=player.resolve(strict=True),sample.resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix='kairo progress native ') as tmp:
        root=Path(tmp)/'game';shutil.copytree(sample,root)
        scene=root/'Scenes/BeaconRun.kscene'
        original=scene.read_text(encoding='utf-8')
        old='transform -4.6 0.92 3.8 0 0 0 1 0.55 0.55 0.55'
        if original.count(old)!=1:raise RuntimeError('Unexpected sample scene')
        authored=original.replace(old,'transform 0 0.92 3.4 0 0 0 1 0.55 0.55 0.55')
        authored+='\nentity 36 "Progress lamp"\nenabled false\nlayer 0\ntag "kairo.trigger-visual"\ntransform 4 1 3 0 0 0 1 0.55 0.55 0.55\nmesh-renderer 20000000-0000-4000-8000-000000000003 true true true 18446744073709551615 1 20000000-0000-4000-8000-000000000010\nend\n'
        scene.write_text(authored,encoding='utf-8')
        (root/'Config/Triggers.ktrigger').write_text('kairo-triggers 1\ntrigger 1 0 0.92 3.4 1.5 once 36 show\n',encoding='utf-8')
        path=Path(tmp)/'player.kprogress'
        def invoke():
            return subprocess.run([str(player),str(root/'Project.kproject'),'--renderer',backend,
                 '--smoke','--progress-file',str(path)],cwd=root,capture_output=True,text=True,timeout=90)
        first=invoke()
        if first.returncode or not all(s in first.stdout for s in ('Beacons 1/6','Trigger 1: show 36','Native viewport smoke passed','Saved objective progress.')):
            raise RuntimeError(f'First native launch failed: {first.stdout} {first.stderr}')
        if not path.is_file():raise RuntimeError('Missing saved progress')
        second=invoke()
        if second.returncode or not all(s in second.stdout for s in ('Restored objective progress: 1/6','Native viewport smoke passed','Saved objective progress.')) or 'Trigger 1: show 36' in second.stdout or 'Beacons 1/6' in second.stdout:
            raise RuntimeError(f'Cold restore failed: {second.stdout} {second.stderr}')
        before=path.read_bytes()
        scene.write_text(authored+'\n',encoding='utf-8')
        changed=invoke()
        if not changed.returncode or 'Progress belongs to another authored revision.' not in changed.stderr or path.read_bytes()!=before:
            raise RuntimeError('Changed authored revision accepted or checkpoint overwritten')
    print('Native cold restore and authored revision rejection passed.')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('player',type=Path)
    parser.add_argument('sample',type=Path)
    parser.add_argument('backend',choices=('vulkan','metal'))
    args=parser.parse_args();run(args.player,args.sample,args.backend)
