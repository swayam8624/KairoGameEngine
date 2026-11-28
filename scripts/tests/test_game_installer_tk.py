"""Exercise actual Tk desktop installer layout and cancellation under Xvfb."""
from pathlib import Path
import tempfile
import time
import tkinter as tk
from tkinter import ttk
import unittest
from unittest.mock import patch

from kairo_game_installer import gui


def descendants(root):
    for widget in root.winfo_children():
        yield widget
        yield from descendants(widget)


class DesktopLayout(unittest.TestCase):
    def test_actions_visible_at_minimum_size_and_confirmation_cancels(self):
        root = tk.Tk()
        self.addCleanup(root.destroy)
        gui(root, run_loop=False)
        root.geometry('760x560')
        root.update()
        buttons = {w.cget('text'): w for w in descendants(root) if isinstance(w, ttk.Button)}
        entries = [w for w in descendants(root) if isinstance(w, ttk.Entry) and not isinstance(w, ttk.Combobox)]
        self.assertEqual(len(entries), 6)
        for label in ('Install', 'Open ticket…', 'Check', 'Validate', 'Smoke', 'Play'):
            self.assertTrue(buttons[label].winfo_ismapped(), label)
            self.assertLessEqual(buttons[label].winfo_rootx() + buttons[label].winfo_width(),
                                 root.winfo_rootx() + root.winfo_width())
        with tempfile.TemporaryDirectory() as directory:
            for entry, value in zip(entries, ('trusted.zip', directory + '/new', directory,
                                               'a' * 64, 'b' * 64, directory + '/ticket.json')):
                entry.insert(0, value)
            with patch('tkinter.messagebox.askyesno', return_value=False), \
                 patch('kairo_game_installer.install_game') as install:
                buttons['Install'].invoke()
                root.update()
                install.assert_not_called()
            self.assertFalse((Path(directory) / 'new').exists())
            self.assertFalse((Path(directory) / 'ticket.json').exists())
            with patch('kairo_game_installer.use_ticket', return_value={'verified': True}) as check:
                buttons['Check'].invoke()
                deadline = time.monotonic() + 3
                while time.monotonic() < deadline:
                    root.update()
                    if not buttons['Check'].instate(['disabled']):
                        break
                    time.sleep(.02)
                self.assertFalse(buttons['Check'].instate(['disabled']))
                check.assert_called_once_with(directory + '/ticket.json', 'auto', 'check')
            console = next(w for w in descendants(root) if isinstance(w, tk.Text))
            self.assertIn('"verified": true', console.get('1.0', 'end'))
        # Exercise the actual close callback to release its executor.
        root.tk.call(root.protocol('WM_DELETE_WINDOW'))
        self._cleanups.pop()


if __name__ == '__main__':
    unittest.main()
