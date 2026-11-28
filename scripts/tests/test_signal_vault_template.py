"""Verify the complete reference game is an independent editable Hub starter."""
import hashlib
from pathlib import Path
import tempfile
import time
import tkinter as tk
import unittest

from kairo_hub import Hub
from kairo_hub_core import ROOT, TEMPLATES, create, editor_command


def inventory(root):
    return {p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in root.rglob('*') if p.is_file()}


class RecordingHub(Hub):
    def open_project(self, path):
        self.opened = Path(path)


class Tests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='kairo signal vault starter ')
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        self.sample = ROOT / 'Samples/SignalVault'
        self.original = inventory(self.sample)
        self.build = self.work / 'Build tools'
        self.build.mkdir()

    def test_independent_complete_game_copy(self):
        template = next(t for t in TEMPLATES if t.key == 'signalvault')
        self.assertTrue(template.available)
        project = create(template.key, 'My Signal Vault', self.work, ROOT)
        copied = inventory(project.parent)
        self.assertEqual(set(copied), set(self.original))
        self.assertEqual({k: v for k, v in copied.items() if k != 'Project.kproject'},
                         {k: v for k, v in self.original.items() if k != 'Project.kproject'})
        self.assertIn('name "My Signal Vault"', project.read_text())
        for name in ('Config/Campaign.kcampaign', 'Config/Logic.klogic2d',
                     'Config/Audio.kaudio', 'Config/UI.kui', 'Config/VFX.kvfx'):
            self.assertIn(name, copied)
        command = editor_command(project, self.build, 'auto', ROOT)
        self.assertEqual(Path(command[1]).name, 'kairo_world2d_editor.py')
        self.assertEqual(command[2], str(project))
        (project.parent / 'README.md').write_text('My independently edited game')
        self.assertEqual(inventory(self.sample), self.original)
        with self.assertRaises(FileExistsError):
            create(template.key, 'My Signal Vault', self.work, ROOT)
        self.assertEqual((project.parent / 'README.md').read_text(), 'My independently edited game')

    def test_real_hub_create_complete_game(self):
        window = tk.Tk()
        hub = RecordingHub(window, self.build, self.work / 'recent.json', ROOT)
        self.addCleanup(hub.close)
        window.geometry('850x690')
        hub.show('new')
        hub.template.set('signalvault')
        hub.name.set('GUI Signal Vault')
        hub.location.set(str(self.work))
        window.update()
        self.assertTrue(hub.create_button.winfo_viewable())
        self.assertLessEqual(hub.create_button.winfo_rooty() + hub.create_button.winfo_height(),
                             window.winfo_rooty() + window.winfo_height())
        hub.create_button.invoke()
        deadline = time.monotonic() + 10
        while not hasattr(hub, 'opened') and time.monotonic() < deadline:
            window.update()
            time.sleep(0.01)
        self.assertEqual(hub.opened, self.work / 'GUI Signal Vault/Project.kproject')
        self.assertIn(str(hub.opened), hub.recent.paths)
        self.assertTrue((hub.opened.parent / 'Config/Campaign.kcampaign').is_file())
        self.assertEqual(inventory(self.sample), self.original)


if __name__ == '__main__':
    unittest.main()
