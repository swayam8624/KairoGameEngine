#!/usr/bin/env python3
"""Qualify the real packaging path without mutating the checked-in sample."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

player = Path(sys.argv[1]).resolve(strict=True)
sample = Path(sys.argv[2]).resolve(strict=True)
gameplay_test = Path(sys.argv[3]).resolve(strict=True)

def run(*args):
    subprocess.run([str(a) for a in args], check=True, timeout=60)

with tempfile.TemporaryDirectory(prefix="kairo-signal-vault-") as temp:
    root = Path(temp)
    source = root / "Source Project"
    shutil.copytree(sample, source)
    # Per-user state must not enter the shipped game.
    progress = source / ".kairo/progress/world2d.kprogress"
    progress.parent.mkdir(parents=True)
    progress.write_text("private test checkpoint", encoding="ascii")
    run(player, source / "Project.kproject", "--validate")
    run(player, source / "Project.kproject", "--package", "Release")
    relocated = root / "Relocated Signal Vault"
    shutil.move(str(source / "Build/Release"), str(relocated))
    project = relocated / "project"
    for original in sample.rglob("*"):
        if original.is_file():
            copied = project / original.relative_to(sample)
            if copied.read_bytes() != original.read_bytes():
                raise RuntimeError(f"Package differs from source: {original}")
    if (project / ".kairo/progress").exists():
        raise RuntimeError("Private checkpoint entered package")
    shutil.rmtree(source)
    run(player, project / "Project.kproject", "--validate")
    run(gameplay_test, project)
    if not (relocated / "bin" / player.name).is_file():
        raise RuntimeError("Native executable missing from package")
    launcher = relocated / ("run.cmd" if sys.platform == "win32" else "run.sh")
    if not launcher.is_file():
        raise RuntimeError("Package launcher missing")
    print("Relocated reference game content, validation and full campaign replay passed")
