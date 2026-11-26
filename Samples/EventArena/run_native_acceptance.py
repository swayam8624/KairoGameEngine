#!/usr/bin/env python3
"""Run native event-chain smoke and malformed binding rejection."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile


def run(player, sample, backend):
    player, sample = player.resolve(strict=True), sample.resolve(strict=True)

    def invoke(root, *args):
        return subprocess.run([str(player), str(root/'Project.kproject'), *args],
                              cwd=root, capture_output=True, text=True, timeout=90)

    valid = invoke(sample, '--validate')
    if valid.returncode or 'gameplay event rules: 4' not in valid.stdout:
        raise RuntimeError(f'Event validation failed: {valid.stdout} {valid.stderr}')
    native = invoke(sample, '--renderer', backend, '--smoke')
    expected = ('Gameplay event 1: show 36', 'Gameplay event 2: show 38',
                'Gameplay event 3: hide 36', 'Gameplay event 4: show 39',
                'Native viewport smoke passed')
    if native.returncode or not all(token in native.stdout for token in expected):
        raise RuntimeError(f'Event chain failed: {native.stdout} {native.stderr}')
    positions = [native.stdout.index(token) for token in expected]
    if positions != sorted(positions):
        raise RuntimeError('Event order differs')
    with tempfile.TemporaryDirectory(prefix='kairo events ') as tmp:
        root = Path(tmp)/'game'
        shutil.copytree(sample, root)
        config = root/'Config/Events.kevent'
        original = config.read_text(encoding='utf-8')
        for record, expected_error in (
            ('on 5 collected 999 1 once show 36', 'Unknown gameplay event source.'),
            ('on 5 begin 0 1 once hide 1', 'Gameplay event target must be'),
        ):
            config.write_text(original+record+'\n', encoding='utf-8')
            result = invoke(root, '--validate')
            if not result.returncode or expected_error not in result.stderr:
                raise RuntimeError(f'Invalid event accepted: {result.stdout} {result.stderr}')
    print('Native event-chain order, capture and invalid binding rejection passed.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('player', type=Path)
    parser.add_argument('sample', type=Path)
    parser.add_argument('backend', choices=('metal','vulkan'))
    args = parser.parse_args()
    run(args.player,args.sample,args.backend)
