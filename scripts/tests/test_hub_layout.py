"""Real Tk layout and create-button regression; run with a display or Xvfb."""
import json
from pathlib import Path
import tempfile
import time
import tkinter as tk
from tkinter import ttk
import unittest

from kairo_hub import Hub, ROOT


class RecordingHub(Hub):
    def open_project(self, path):
        # Test actual asynchronous project creation and its handoff, without
        # requiring a native Editor executable on the GUI fixture host.
        self.opened = Path(path)


class HubLayoutTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        self.window = tk.Tk()
        self.hub = RecordingHub(self.window, ROOT / 'build/game-clang', self.work / 'recent.json', ROOT)
        self.addCleanup(self.close)
        self.hub.show('new')
        self.window.update()

    def close(self):
        if self.hub.future is not None:
            self.hub.future.result(timeout=10)
            self.window.update()
        self.hub.close()

    def test_primary_action_and_scroll(self):
        for geometry in ('1024x760', '850x690', '850x540'):
            self.window.geometry(geometry)
            self.window.update()
            button = self.hub.create_button
            self.assertTrue(button.winfo_viewable())
            self.assertLessEqual(button.winfo_rooty() + button.winfo_height(), self.window.winfo_rooty() + self.window.winfo_height())
            self.assertLessEqual(button.winfo_rootx() + button.winfo_width(), self.window.winfo_rootx() + self.window.winfo_width())
            canvas = self.hub.new_canvas
            canvas.yview_moveto(0)
            self.window.update()
            self.assertLess(canvas.yview()[1], 1.0)
            canvas.yview_moveto(1)
            self.window.update()
            self.assertAlmostEqual(canvas.yview()[1], 1.0)
            self.assertTrue(button.winfo_viewable())
        # Keyboard focus on the last editable field reveals it automatically.
        def entries(widget):
            for child in widget.winfo_children():
                if isinstance(child, ttk.Entry):
                    yield child
                yield from entries(child)
        fields = list(entries(self.hub.new_canvas))
        self.assertEqual(len(fields), 2)
        canvas.yview_moveto(0)
        self.window.update()
        fields[-1].event_generate('<FocusIn>')
        self.window.update()
        self.assertGreater(canvas.yview()[0], 0)
        self.assertLessEqual(fields[-1].winfo_rooty() + fields[-1].winfo_height(), canvas.winfo_rooty() + canvas.winfo_height())

    def test_create_button_preserves_real_project_and_recents(self):
        self.window.geometry('850x690')
        self.hub.name.set('GUI Fixture')
        self.hub.location.set(str(self.work))
        self.hub.template.set('world2d')
        self.hub.create_button.invoke()
        deadline = time.monotonic() + 10
        while not hasattr(self.hub, 'opened') and time.monotonic() < deadline:
            self.window.update()
            time.sleep(0.01)
        self.assertEqual(self.hub.opened, self.work / 'GUI Fixture/Project.kproject')
        self.assertTrue(self.hub.opened.is_file())
        self.assertIn(str(self.hub.opened), json.loads((self.work / 'recent.json').read_text())['projects'])
        self.assertFalse(self.hub.create_button.instate(('disabled',)))


if __name__ == '__main__':
    unittest.main()
