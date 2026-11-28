"""Use a real ELF fixture but mock the native packaging command only."""
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import kairo_release as release
import signal_vault_release as gate


class DistributionTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.sample = self.root / "sample"
        self.sample.mkdir()
        (self.sample / "LICENSE").write_text("fixture notice\n")
        (self.sample / "Project.kproject").write_text("fixture descriptor\n")
        self.player = self.root / "KairoPlayer"
        shutil.copy2("/bin/true", self.player)
        self.lock = self.root / "workspace.lock"
        self.lock.write_text("fixture lock\n")
        self.output = self.root / "evidence"
        self.executed = []
        self.real_run = gate.run

    def mock_packager(self, command, cwd):
        command = [str(part) for part in command]
        if command[-2:] == ["--package", "Release"]:
            source = Path(command[1]).parent
            bundle = source / "Build/Release"
            (bundle / "bin").mkdir(parents=True)
            (bundle / "project").mkdir()
            shutil.copy2(self.player, bundle / "bin/KairoPlayer")
            for path in self.sample.iterdir():
                shutil.copy2(source / path.name, bundle / "project" / path.name)
            files = list((bundle / "project").iterdir())
            (bundle / "package.kmanifest").write_text(
                'kairo-package 1\nproject "project/Project.kproject"\n'
                'profile "Release"\nkind release\nengine-version "0.1.0"\n'
                'player "bin/KairoPlayer"\n'
                f'project-files {len(files)}\nproject-bytes {sum(p.stat().st_size for p in files)}\n')
            (bundle / "run.sh").write_text(
                '#!/bin/sh\nset -eu\ncd -- "$(dirname -- "$0")"\n'
                'test -f project/Project.kproject\nexec bin/KairoPlayer "$@"\n')
        else:
            self.executed.append(command)
            self.real_run(command, cwd)

    def qualify(self, replay="/bin/true"):
        with patch.object(gate, "run", side_effect=self.mock_packager):
            return gate.qualify(self.player, self.sample, self.output, self.lock, "a" * 40, replay)

    def test_exact_release_and_relocated_execution(self):
        receipt = self.qualify()
        archive = self.output / ("SignalVault-" + receipt["target"] + ".zip")
        self.assertEqual(gate.digest(archive), receipt["archive_sha256"])
        self.assertEqual(release.verify(archive)["files"]["project/LICENSE"]["sha256"],
                         receipt["license_sha256"])
        self.assertEqual(json.loads((self.output / "receipt.json").read_text()), receipt)
        self.assertTrue(any("Relocated Signal Vault/bin/KairoPlayer" in c[0] for c in self.executed))
        self.assertTrue(any("Relocated Signal Vault/run.sh" in c[0] for c in self.executed))
        inventory = self.output / "native-dependencies.json"
        self.assertEqual(gate.digest(inventory), receipt["native_dependencies_sha256"])
        self.assertEqual(json.loads(inventory.read_text())["tool"], "readelf")
        self.assertFalse(receipt["dependency_closure_verified"])
        self.assertFalse(receipt["third_party_licenses_verified"])
        self.assertFalse(receipt["fresh_machine_qualified"])
        self.assertFalse(receipt["signed"])

    def test_failed_replay_publishes_nothing(self):
        with self.assertRaises(subprocess.CalledProcessError):
            self.qualify("/bin/false")
        self.assertFalse(self.output.exists())

    def test_no_overwrite_and_missing_license(self):
        self.output.mkdir()
        sentinel = self.output / "keep"
        sentinel.write_text("keep")
        with self.assertRaises(ValueError):
            self.qualify()
        self.assertEqual(sentinel.read_text(), "keep")
        shutil.rmtree(self.output)
        (self.sample / "LICENSE").unlink()
        with self.assertRaises(ValueError):
            self.qualify()
        self.assertFalse(self.output.exists())


if __name__ == "__main__":
    unittest.main()
