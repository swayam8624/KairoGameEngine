"""Actual Tk canvas events and native parser-backed file authoring."""
from dataclasses import replace
import os
from pathlib import Path
import tempfile
import tkinter as tk
import unittest
from kairo_menu_designer import MenuModel, Designer


class DesignerTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.project=Path(self.tmp.name);(self.project/'Project.kproject').write_text('fixture')
        self.model=MenuModel(self.project,os.environ['KAIRO_MENU_VALIDATOR'])

    def test_model_roundtrip_and_conflict(self):
        old=self.model.buttons()
        with self.assertRaises(ValueError): self.model.apply([old[0],replace(old[1],x=old[0].x,y=old[0].y)])
        self.assertEqual(self.model.buttons(),old)
        self.model.apply([replace(old[0],label='Play "now" \\'),old[1]])
        self.model.save();reopened=MenuModel(self.project,os.environ['KAIRO_MENU_VALIDATOR'])
        self.assertEqual(reopened.buttons(),self.model.buttons())
        self.model.path.write_bytes(b'external edit')
        with self.assertRaises(ValueError): self.model.save()
        self.assertEqual(self.model.path.read_bytes(),b'external edit')

    def test_real_canvas_drag_and_save(self):
        root=tk.Tk();self.addCleanup(root.destroy)
        ui=Designer(root,self.model);root.update()
        ui.canvas.event_generate('<ButtonPress-1>',x=320,y=140);root.update()
        ui.canvas.event_generate('<B1-Motion>',x=256,y=104);root.update()
        ui.canvas.event_generate('<ButtonRelease-1>',x=256,y=104);root.update()
        self.assertEqual((self.model.buttons()[0].x,self.model.buttons()[0].y),(200,220))
        ui.values['label'].set('START');ui.apply_fields();self.assertEqual(self.model.buttons()[0].label,'START')
        ui.undo();self.assertEqual(self.model.buttons()[0].label,'PLAY')
        ui.add();self.assertEqual(len(self.model.buttons()),3)
        ui.delete();self.assertEqual(len(self.model.buttons()),2)
        ui.save();self.assertIn(b'200 220 400 140',self.model.path.read_bytes())
        self.assertEqual(MenuModel(self.project,os.environ['KAIRO_MENU_VALIDATOR']).buttons(),self.model.buttons())


if __name__=='__main__':
    unittest.main()
