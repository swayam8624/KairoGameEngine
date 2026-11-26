import tempfile
from pathlib import Path
import tkinter as tk
import unittest
from unittest.mock import patch
from kairo_gameplay_editor import Document, GameplayEditor, parse


class EditorTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name); self.project=self.root/'Project.kproject'
        self.project.write_text('kairo-project 2\n'); (self.root/'Config').mkdir()
        self.path=self.root/'Config/Doors.kdoor'
        self.path.write_text('kairo-doors 1\ndoor 40 3.2 2 4.2 locked\n')

    def test_shapes(self):
        fixtures={'Doors.kdoor':'kairo-doors 1\ndoor 40 3 2 4\n',
            'Events.kevent':'kairo-events 1\non 1 collected 50 1 once door-open 40\n',
            'Health.khealth':'kairo-health 1\nhealth 100 30 -10\nhazard 1 3 0 0 1 20\ncheckpoint 7 -3 0 0 1\n',
            'Combat.kcombat':'kairo-combat 1\nattack 30 12 6\nprojectile 12\ntarget 40 45\n',
            'Enemies.kenemy':'kairo-enemies 1\nenemy 40 2 8 1.5 120 45 10 3 0.92 1.6\n',
            'EnemyNav.knav':'kairo-enemy-nav 1\nheight 0\nrect -3 -3 1 1\n',
            'Triggers.ktrigger':'kairo-triggers 1\ntrigger 1 0 0.92 3.4 1.5 enter 12 hide\n'}
        for name,source in fixtures.items():
            with self.subTest(name=name): self.assertTrue(parse(name,source.encode()))
        with self.assertRaises(ValueError): parse('Doors.kdoor',b'kairo-doors 1\ndoor 40 3\n')

    def test_external_change_and_symlink(self):
        doc=Document(self.project,'Doors.kdoor'); doc.rows[0][2]='4'
        self.path.write_text('kairo-doors 1\ndoor 40 5 2 4.2\n')
        with self.assertRaises(ValueError): doc.save()
        self.assertIn('40 5',self.path.read_text())
        outside=self.root/'outside'; outside.write_text('untouched')
        self.path.unlink(); self.path.symlink_to(outside)
        with self.assertRaises(ValueError): Document(self.project,'Doors.kdoor')
        self.assertEqual(outside.read_text(),'untouched')

    def test_discard_after_conflict(self):
        window=tk.Tk(); self.addCleanup(window.destroy)
        editor=GameplayEditor(window,self.project); editor.load()
        editor.record.set('door 41 3 2 4.2'); editor.add()
        self.path.write_text('kairo-doors 1\ndoor 40 5 2 4.2\n')
        editor.save(); self.assertIn('changed externally',editor.status.get())
        with patch('kairo_gameplay_editor.messagebox.askyesno',return_value=False): editor.discard()
        self.assertEqual(editor.list.size(),2)
        with patch('kairo_gameplay_editor.messagebox.askyesno',return_value=True): editor.discard()
        self.assertEqual(editor.list.size(),1); self.assertIn('40 5',editor.list.get(0))

    def test_actual_widgets(self):
        window=tk.Tk(); self.addCleanup(window.destroy)
        editor=GameplayEditor(window,self.project); window.update()
        editor.load(); editor.list.selection_set(0); editor.select()
        self.assertIn('locked',editor.record.get())
        editor.record.set('door 40 4 2 4.2 open'); editor.replace()
        editor.record.set('door 41 3 2 4.2'); editor.add()
        self.assertEqual(editor.list.size(),2)
        editor.record.set('door 42'); editor.add(); self.assertEqual(editor.list.size(),2)
        editor.list.selection_set(1); editor.remove(); editor.save()
        self.assertEqual(self.path.read_text(),'kairo-doors 1\ndoor 40 4 2 4.2 open\n')
        self.assertEqual(self.project.read_text(),'kairo-project 2\n')
        self.assertFalse(list((self.root/'Config').glob('.kairo-edit-*')))


if __name__=='__main__': unittest.main()
