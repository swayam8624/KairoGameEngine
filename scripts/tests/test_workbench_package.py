"""Real filesystem package intent and Tk Workbench layout regression."""
from pathlib import Path
from types import SimpleNamespace
import tempfile
import sys
import time
import tkinter as tk
from tkinter import ttk
import unittest
from unittest.mock import patch

from kairo_hub_core import create, ROOT
from kairo_project_workflow import ProjectTools, choose_package_options, gui


class Tests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='kairo package fixture ')
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        self.project = create('world2d', 'Workbench Fixture', self.work, ROOT)
        self.build = self.work / 'Built Tools'
        self.build.mkdir()
        self.tools = ProjectTools(self.project, self.build, ROOT)
        self.bundle = self.project.parent / self.tools.profiles()['Release']

    def choose(self, profile, answers, archive=''):
        calls = []
        def ask(title, message, **kwargs):
            calls.append(title)
            return next(answers)
        result = choose_package_options(self.tools, None,
            SimpleNamespace(askstring=lambda *a, **k: profile),
            SimpleNamespace(askyesno=ask),
            SimpleNamespace(asksaveasfilename=lambda **k: archive))
        return result, calls

    def test_new_and_repeat_package_intent(self):
        result, calls = self.choose('Release', iter([False]))
        self.assertEqual(result, ('Release', None, False))
        self.assertEqual(calls, ['Export ZIP'])
        self.bundle.mkdir(parents=True)
        sentinel = self.bundle / 'existing'
        sentinel.write_text('retain')
        result, calls = self.choose('Release', iter([False]))
        self.assertIsNone(result)
        self.assertEqual(calls, ['Replace existing build?'])
        output = str(self.work / 'New export.zip')
        result, calls = self.choose('Release', iter([True, True]), output)
        self.assertEqual(result, ('Release', output, True))
        self.assertEqual(calls, ['Replace existing build?', 'Export ZIP'])
        self.assertEqual(sentinel.read_text(), 'retain')
        self.assertFalse(Path(output).exists())

    def test_cancel_invalid_and_symlink(self):
        self.assertIsNone(self.choose(None, iter([]))[0])
        self.assertIsNone(self.choose('Release', iter([True]), '')[0])
        with self.assertRaises(ValueError):
            self.choose('Unknown', iter([]))
        self.bundle.parent.mkdir(parents=True)
        self.bundle.symlink_to(self.work / 'missing', target_is_directory=True)
        with self.assertRaises(ValueError):
            self.choose('Release', iter([]))

    def test_live_output_and_failure_stops_later_commands(self):
        marker = self.work / 'should-not-run'
        child = [sys.executable, '-c',
                 "import time; print('live diagnostic', flush=True); time.sleep(1.5); raise SystemExit(7)"]
        later = [sys.executable, '-c',
                 "from pathlib import Path; import sys; Path(sys.argv[1]).touch()", str(marker)]
        def exercise(window):
            frame = window.winfo_children()[0]
            with patch.object(ProjectTools, 'commands', return_value=[child, later]):
                frame.start('validate')
                deadline = time.monotonic() + 5
                while time.monotonic() < deadline:
                    window.update()
                    if 'live diagnostic' in frame.output.get('1.0', 'end'):
                        break
                    time.sleep(.01)
                self.assertIn('live diagnostic', frame.output.get('1.0', 'end'))
                self.assertIsNotNone(frame.process)
                self.assertIsNone(frame.process.poll(), 'Log must appear before process exit')
                deadline = time.monotonic() + 5
                while frame.process is not None and time.monotonic() < deadline:
                    window.update()
                    time.sleep(.01)
                self.assertIsNone(frame.process)
                self.assertIn('exit 7', frame.status.get())
                self.assertFalse(marker.exists())
                self.assertFalse(frame.pending)
                self.assertIsNone(frame.log_reader)
                self.assertIsNone(frame.log_directory)
            window.destroy()
        with patch.object(tk.Tk, 'mainloop', exercise):
            self.assertEqual(gui(self.project, self.build, 'auto'), 0)

    def test_real_tk_controls_fit_and_cancel(self):
        def exercise(window):
            def walk(widget):
                for child in widget.winfo_children():
                    yield child
                    yield from walk(child)
            for geometry in ('1020x560', '800x460'):
                window.geometry(geometry)
                window.update()
                buttons = {w.cget('text'): w for w in walk(window) if isinstance(w, ttk.Button)}
                for label in ('Open Editor','Design Menu','Edit Gameplay','Validate','Native Smoke','Play','Package Release'):
                    button = buttons[label]
                    self.assertTrue(button.winfo_viewable())
                    self.assertLessEqual(button.winfo_rootx()+button.winfo_width(), window.winfo_rootx()+window.winfo_width())
                    self.assertLessEqual(button.winfo_rooty()+button.winfo_height(), window.winfo_rooty()+window.winfo_height())
            frame = window.winfo_children()[0]
            with patch('tkinter.simpledialog.askstring', return_value=None), patch('subprocess.Popen') as spawn:
                buttons['Package Release'].invoke()
                spawn.assert_not_called()
                self.assertIn('cancelled', frame.status.get())
                self.assertFalse(frame.pending)
            window.destroy()
        with patch.object(tk.Tk, 'mainloop', exercise):
            self.assertEqual(gui(self.project, self.build, 'auto'), 0)


if __name__ == '__main__':
    unittest.main()
