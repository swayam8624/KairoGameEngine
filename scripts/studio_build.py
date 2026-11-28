#!/usr/bin/env python3
"""Incremental Studio build driver; no clean, update, sync or dependency rewrite.

Run from any directory. First use configures the focused preset. Later use
lets Ninja regenerate only when its real CMake dependencies change.
"""
import argparse
from pathlib import Path
import subprocess
import sys
import time

ROOT=Path(__file__).resolve().parents[1]
PRESETS={'all':'studio-fast','editor':'studio-editor','player':'studio-player'}


def run(command,root):
    start=time.perf_counter()
    print('+ '+' '.join(command),flush=True)
    subprocess.run(command,cwd=root,check=True)
    print(f'Elapsed: {time.perf_counter()-start:.3f} s',flush=True)


def build(root,target='all',jobs=None,configure=False,repeat=False):
    root=Path(root).resolve(strict=True)
    directory=root/'build/studio-fast'
    cache=directory/'CMakeCache.txt'
    ninja=directory/'build.ninja'
    if cache.is_file():
        with cache.open('r',encoding='utf-8',errors='replace') as stream:
            home=next((line.strip().split('=',1)[1] for line in stream
                       if line.startswith('CMAKE_HOME_DIRECTORY:INTERNAL=')),None)
        if home is None or Path(home).resolve()!=root:
            raise ValueError('Build directory belongs to a different source tree')
    if configure or not cache.is_file() or not ninja.is_file():
        run(['cmake','--preset','studio-fast'],root)
    command=['cmake','--build','--preset',PRESETS[target]]
    if jobs is not None:
        if not 1<=jobs<=64:raise ValueError('Jobs must be 1–64')
        command+=['--parallel',str(jobs)]
    run(command,root)
    if repeat:
        print('Repeat build (measure actual incremental/no-op behavior):',flush=True)
        run(command,root)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target',choices=PRESETS,default='all')
    parser.add_argument('--jobs',type=int)
    parser.add_argument('--configure',action='store_true')
    parser.add_argument('--repeat',action='store_true')
    args=parser.parse_args()
    try:
        build(ROOT,args.target,args.jobs,args.configure,args.repeat)
    except (ValueError,OSError,subprocess.CalledProcessError) as error:
        print(f'Studio build failed: {error}',file=sys.stderr)
        return 1
    return 0


if __name__=='__main__':
    sys.exit(main())
