import os
from pathlib import Path
import plistlib
import subprocess
import tempfile
import unittest
from desktop_launchers import write_mac_launcher


class LauncherTests(unittest.TestCase):
    def test_mac_bundle_user_copy_and_relaunch(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            app = root / 'Signal Vault.app'
            launcher = write_mac_launcher(app)
            contents = app / 'Contents'
            project = contents / 'Resources/project'
            project.mkdir(parents=True)
            (project / 'Project.kproject').write_text('original')
            player = contents / 'MacOS/KairoPlayer'
            player.write_text('#!/bin/sh\nset -eu\ntest -f "$1"\ntest "$2" = --validate\n')
            player.chmod(0o755)
            data = root / 'user data'
            env = dict(os.environ, KAIRO_DATA_HOME=str(data))
            subprocess.run([launcher, '--validate'], cwd=root, env=env, check=True)
            copied = data / 'project/Project.kproject'
            self.assertEqual(copied.read_text(), 'original')
            copied.write_text('user edit')
            subprocess.run([launcher, '--validate'], cwd=root, env=env, check=True)
            self.assertEqual(copied.read_text(), 'user edit')
            self.assertEqual((project / 'Project.kproject').read_text(), 'original')
            self.assertFalse((data / '.first-launch-lock').exists())
            with (contents / 'Info.plist').open('rb') as stream:
                self.assertEqual(plistlib.load(stream)['CFBundleExecutable'], 'SignalVault')

    def test_existing_lock_blocks_initialization(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            launcher = write_mac_launcher(root / 'test.app')
            data = root / 'data'
            (data / '.first-launch-lock').mkdir(parents=True)
            result = subprocess.run([launcher], env=dict(os.environ, KAIRO_DATA_HOME=str(data)), capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse((data / 'project').exists())
            self.assertTrue((data / '.first-launch-lock').exists())


if __name__ == '__main__':
    unittest.main()
