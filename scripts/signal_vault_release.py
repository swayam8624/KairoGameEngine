#!/usr/bin/env python3
"""Build, verify and execute the exact host-native reference-game release.

Run only on trusted build outputs. The archive checksum is integrity evidence,
not a signature. System graphics/runtime libraries remain host prerequisites.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

import kairo_release as release


def digest(path):
    with path.open("rb") as stream:
        return release.copy_hash(stream)[1]


def run(command, cwd):
    subprocess.run([str(part) for part in command], cwd=cwd, check=True, timeout=120)


def qualify(player, sample, output, lock, commit, gameplay_test, smoke=False):
    """Publish an exclusive evidence directory only after relocation passes."""
    player = Path(player).resolve(strict=True)
    sample = Path(sample).resolve(strict=True)
    lock = Path(lock).resolve(strict=True)
    gameplay_test = Path(gameplay_test).resolve(strict=True)
    output = Path(output).absolute()
    release.require(re.fullmatch(r"[0-9a-f]{40}", commit), "Expected exact candidate commit")
    release.require(not output.exists() and not output.is_symlink(), "Output already exists")
    release.require(not output.resolve().is_relative_to(sample), "Output must be outside sample")
    release.require((sample / "LICENSE").is_file(), "Missing distribution license")
    release.require((sample / "Project.kproject").is_file(), "Missing project descriptor")
    release.require(not (sample / "Build").exists() and not (sample / ".kairo").exists(),
                    "Use a clean authored reference project")
    expected = {name: digest(path) for name, path in release.entries(sample).items()}
    lock_hash = digest(lock)
    player_hash = digest(player)
    with tempfile.TemporaryDirectory(prefix="kairo-distribution-", dir=output.parent) as temp:
        root = Path(temp)
        source = root / "Source Project"
        shutil.copytree(sample, source)
        run([player, source / "Project.kproject", "--validate"], root)
        run([player, source / "Project.kproject", "--package", "Release"], root)
        archive = root / "candidate.zip"
        index = release.export(source / "Build/Release", archive)
        release.require(index == release.verify(archive), "Archive verification disagreed")
        release.require(index["kind"] == "release" and index["profile"] == "Release", "Wrong profile")
        packaged = {name.removeprefix("project/"): record["sha256"]
                    for name, record in index["files"].items() if name.startswith("project/")}
        release.require(packaged == expected, "Release project inventory differs")
        release.require(index["files"]["bin/" + player.name]["sha256"] == player_hash,
                        "Release executable differs from build output")
        shutil.rmtree(source)
        relocated = root / "Relocated Signal Vault"
        release.require(release.unpack(archive, relocated) == index, "Extraction index differs")
        descriptor = relocated / "project/Project.kproject"
        packaged_player = relocated / "bin" / player.name
        # Both the actual extracted executable and its generated launcher
        # must work without the source tree or a build-tree executable path.
        run([packaged_player, descriptor, "--validate"], root)
        launcher = relocated / ("run.cmd" if os.name == "nt" else "run.sh")
        command = ["cmd.exe", "/d", "/c", launcher] if os.name == "nt" else [launcher]
        run([*command, "--validate"], root)
        if smoke:
            run([*command, "--renderer", "metal" if sys.platform == "darwin" else "vulkan", "--smoke"], root)
        run([gameplay_test, descriptor.parent], root)
        for name, checksum in expected.items():
            release.require(digest(descriptor.parent / name) == checksum,
                            "Launch changed authored content: " + name)
        release.require(not (descriptor.parent / ".kairo/progress").exists(),
                        "Validation or smoke wrote a personal checkpoint")
        receipt = {"schema": 1, "commit": commit, "workspace_lock_sha256": lock_hash,
                   "target": index["target"], "profile": index["profile"],
                   "archive_sha256": digest(archive), "player_sha256": player_hash,
                   "file_count": len(index["files"]), "project_file_count": len(expected),
                   "license_sha256": expected["LICENSE"],
                   "extracted_player_validation": True, "relocated_launcher_validation": True,
                   "campaign_replay": True, "native_graphics_smoke": smoke,
                   "fresh_machine_qualified": False, "signed": False}
        # Publish only the tested archive and receipt. Never overwrite an
        # unrelated directory, even when another process won the name race.
        output.mkdir()
        try:
            name = "SignalVault-" + index["target"] + ".zip"
            with (output / name).open("xb") as dst, archive.open("rb") as src:
                shutil.copyfileobj(src, dst)
            release.require(digest(output / name) == receipt["archive_sha256"],
                            "Published archive differs")
            with (output / "receipt.json").open("x", encoding="utf-8") as stream:
                json.dump(receipt, stream, sort_keys=True, indent=2)
                stream.write("\n")
        except BaseException:
            shutil.rmtree(output)
            raise
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("player", type=Path)
    parser.add_argument("sample", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--lock", type=Path, required=True)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--gameplay-test", type=Path, required=True)
    parser.add_argument("--smoke", action="store_true")
    args = parser.parse_args()
    receipt = qualify(args.player, args.sample, args.output, args.lock,
                      args.commit, args.gameplay_test, args.smoke)
    print(json.dumps(receipt, sort_keys=True))


if __name__ == "__main__":
    main()
