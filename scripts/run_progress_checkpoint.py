#!/usr/bin/env python3
"""Run checkpoint write/read/rejection in three independent OS processes."""
import argparse
from pathlib import Path
import subprocess
import tempfile

def run(binary):
    binary=binary.resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix='kairo progress ') as temp:
        path=Path(temp)/'fixture.kprogress'
        for action in ('save','restore','reject'):
            subprocess.run([str(binary),action,str(path)],check=True,timeout=15)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary',type=Path)
    run(parser.parse_args().binary)
