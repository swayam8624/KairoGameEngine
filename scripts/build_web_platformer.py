#!/usr/bin/env python3
"""Build the shared Kairo platformer core as a self-contained static WASM site."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'Samples/WebPlatformer'
EXPORTS = ('game_reset', 'game_step', 'game_tile', 'game_x', 'game_y',
           'game_coins', 'game_deaths', 'game_won')


def build(output, compiler='clang++'):
    """No SDK/sysroot required: compile a freestanding C++ module, then copy
    the static host. All files are staged before publishing a fresh directory.
    """
    output = output.absolute()
    if output.exists() or output.is_symlink():
        raise ValueError('Web output must be a fresh directory')
    with tempfile.TemporaryDirectory(prefix='.kairo-web-', dir=output.parent) as tmp:
        stage = Path(tmp) / 'site'
        stage.mkdir()
        command = [compiler, '--target=wasm32', '-std=c++23', '-O2', '-Wall', '-Wextra', '-Werror',
                   '-nostdlib', '-fno-exceptions', '-fno-rtti', '-fno-builtin', '-I', str(ROOT/'Runtime/KairoPlayer'),
                   str(SOURCE/'PlatformerWasm.cpp'), '-Wl,--no-entry']
        command += [f'-Wl,--export={name}' for name in EXPORTS]
        subprocess.run(command + ['-o', str(stage/'platformer.wasm')], check=True, timeout=90)
        for name in ('index.html', 'game.js'):
            shutil.copyfile(SOURCE/name, stage/name)
        # Exclusive mkdir prevents overwriting a concurrently created site.
        output.mkdir()
        try:
            for path in stage.iterdir():
                shutil.copyfile(path, output/path.name)
        except BaseException:
            shutil.rmtree(output)
            raise
    return output


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--compiler', default='clang++')
    args = parser.parse_args()
    print(build(args.output, args.compiler))
