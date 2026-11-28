"""Exercise the real Hub and child Workbench using Tk, without engine binaries."""
from pathlib import Path
import sys
import tempfile
import time
import tkinter as tk
import unittest

from kairo_hub import Hub, ROOT
from kairo_hub_core import create, workbench_command


class Tests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='kairo hub fixture ')
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        self.build = self.work / 'Built Tools'
        self.build.mkdir()
        self.project = create('world2d', 'Workbench Fixture', self.work, ROOT)
        self.window = tk.Tk()
        self.hub = Hub(self.window, self.build, self.work / 'recent.json', ROOT, 'vulkan')
        self.addCleanup(self.close)
        self.window.geometry('850x690')
        self.hub.recent.touch(self.project)
        self.hub.refresh()
        self.hub.tree.selection_set('0')
        self.window.update()

    def close(self):
        child = self.hub.process
        if child is not None:
            child.terminate()
            child.wait(timeout=10)
        self.hub.close()

    def test_command_and_rejection(self):
        args = workbench_command(self.project, self.build, 'vulkan', ROOT)
        self.assertEqual(args, [sys.executable, '-B', str(ROOT / 'scripts/kairo_project_workflow.py'),
                               str(self.project), '--build', str(self.build), '--renderer', 'vulkan'])
        for project, build, renderer in ((self.work / 'Missing.kproject', self.build, 'auto'),
                                         (self.project, self.work / 'missing', 'auto'),
                                         (self.project, self.build, 'invalid')):
            with self.assertRaises((OSError, ValueError)):
                workbench_command(project, build, renderer, ROOT)

    def test_search_selection_and_missing_project_actions(self):
        other = create('2d', 'Platformer Fixture', self.work, ROOT)
        self.hub.recent.touch(other)
        self.hub.refresh()
        self.window.update()
        self.assertEqual(self.hub.selected(), str(self.project))
        self.hub.search.set('platformer fixture')
        self.window.update()
        self.assertEqual(len(self.hub.tree.get_children()), 1)
        self.hub.focus_first_result()
        self.assertEqual(self.hub.selected(), str(other))
        self.assertFalse(self.hub.open_button.instate(('disabled',)))
        self.hub.search.set('no matching project')
        self.window.update()
        self.assertEqual(len(self.hub.tree.get_children()), 0)
        self.assertTrue(self.hub.open_button.instate(('disabled',)))
        self.assertIn('No matching projects', self.hub.project_summary.get())
        other.unlink()
        self.hub.search.set('missing')
        self.window.update()
        self.hub.focus_first_result()
        self.assertEqual(self.hub.selected(), str(other))
        self.assertTrue(self.hub.open_button.instate(('disabled',)))
        self.assertTrue(self.hub.tools_button.instate(('disabled',)))
        self.assertFalse(self.hub.remove_button.instate(('disabled',)))
        self.assertEqual(len(self.hub.recent.paths), 2)
        self.hub.remove_selected()
        self.assertEqual(len(self.hub.recent.paths), 1)
        self.assertTrue(self.project.is_file())
        self.assertTrue(other.parent.is_dir())

    def test_real_child_and_busy_gate(self):
        button = self.hub.tools_button
        self.assertTrue(button.winfo_viewable())
        self.assertLessEqual(button.winfo_rootx() + button.winfo_width(),
                             self.window.winfo_rootx() + self.window.winfo_width())
        button.invoke()
        child = self.hub.process
        self.assertIsNotNone(child)
        deadline = time.monotonic() + 1.0
        while time.monotonic() < deadline:
            self.window.update()
            time.sleep(0.02)
        self.assertIsNone(child.poll(), 'Workbench must stay open as a real GUI child')
        self.assertEqual(self.hub.process_label, 'Project Workbench')
        self.hub.open_project(self.project)
        self.assertIs(self.hub.process, child)
        self.assertIn('already active', self.hub.status.get())
        child.terminate()
        child.wait(timeout=10)
        deadline = time.monotonic() + 5
        while self.hub.process is not None and time.monotonic() < deadline:
            self.window.update()
            time.sleep(0.02)
        self.assertIsNone(self.hub.process)
        self.assertIsNone(self.hub.log)
        self.assertIn('Project Workbench', self.hub.status.get())
        self.assertTrue(self.project.is_file())


if __name__ == '__main__':
    unittest.main()
