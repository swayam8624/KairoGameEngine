#!/usr/bin/env python3
"""Package and relocate a sample; verify bytes and run its own launcher."""
import argparse
import hashlib
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import sys


def check(ok, message):
    if not ok:
        raise RuntimeError(message)


def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            h.update(chunk)
    return h.hexdigest()


def payload(root):
    files = {}
    for path in root.rglob("*"):
        relative = path.relative_to(root)
        if relative.parts[0] in ("Build", ".git", ".kairo"):
            continue
        check(not path.is_symlink(), f"Unexpected symlink: {relative}")
        if path.is_file():
            files[relative.as_posix()] = (path.stat().st_size, digest(path))
    return files


def verify(player, project, smoke=False):
    player = player.resolve(strict=True)
    project = project.resolve(strict=True)
    check(project.name == "Project.kproject", "Gate requires canonical Project.kproject name")
    with tempfile.TemporaryDirectory(prefix="kairo-release-") as temp:
        root = Path(temp)
        source = root / "source project"
        shutil.copytree(project.parent, source,
                        ignore=shutil.ignore_patterns("Build", ".git", ".kairo"))
        expected = payload(source)
        subprocess.run([str(player), str(source / project.name), "--validate"], check=True, cwd=root)
        subprocess.run([str(player), str(source / project.name), "--package", "Release"], check=True, cwd=root)
        original = source / "Build/Release"
        check(original.is_dir(), "Release package missing")
        relocated = root / "relocated release"
        shutil.move(str(original), str(relocated))
        # Remove the original source copy: no accidental source-relative reads.
        shutil.rmtree(source)
        manifest = {}
        lines = (relocated / "package.kmanifest").read_text(encoding="utf-8").splitlines()
        check(lines[0] == "kairo-package 1", "Unknown manifest version")
        for line in lines[1:]:
            parts = shlex.split(line)
            check(len(parts) == 2 and parts[0] not in manifest, "Malformed manifest")
            manifest[parts[0]] = parts[1]
        check(manifest.get("profile") == "Release", "Wrong packaged profile")
        check(manifest.get("project") == "project/Project.kproject", "Wrong descriptor path")
        actual = payload(relocated / "project")
        check(actual == expected, "Relocated project payload differs")
        check(int(manifest["project-files"]) == len(actual), "Manifest file count differs")
        check(int(manifest["project-bytes"]) == sum(v[0] for v in actual.values()),
              "Manifest byte count differs")
        packaged_player = relocated / "bin" / player.name
        check(manifest.get("player") == f"bin/{player.name}", "Wrong player path")
        check(digest(packaged_player) == digest(player), "Packaged player differs")
        launcher = relocated / ("run.cmd" if sys.platform == "win32" else "run.sh")
        check(launcher.is_file(), "Launcher missing")
        launch = (["cmd.exe", "/d", "/c", str(launcher)] if sys.platform == "win32"
                  else [str(launcher)])
        subprocess.run([*launch, "--validate"], check=True, cwd=root)
        if smoke:
            subprocess.run([*launch, "--renderer", "vulkan", "--smoke"], check=True, cwd=root)
        print(f"Release gate passed: {len(actual)} files, exact payload/player hashes, relocated validation"
              + (", native Vulkan smoke" if smoke else ""))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("player", type=Path)
    parser.add_argument("project", type=Path)
    parser.add_argument("--smoke", action="store_true")
    args = parser.parse_args()
    verify(args.player, args.project, args.smoke)


if __name__ == "__main__":
    main()
