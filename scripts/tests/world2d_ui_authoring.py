#!/usr/bin/env python3
"""Exercise the real designer model, native validator and optional Tk controls."""
from pathlib import Path
import sys
import tempfile
import unittest
import subprocess

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from kairo_ui2d_designer import parse,encode,layout,open_designer,DEFAULT,LIMIT
from kairo_world2d_editor import bounded_config,publish_config
VALIDATOR=Path(sys.argv.pop(1)).resolve(strict=True)
SAMPLE=Path(sys.argv.pop(1)).resolve(strict=True)
GUI='--gui' in sys.argv
if GUI:sys.argv.remove('--gui')


class UITests(unittest.TestCase):
    def setUp(self):
        temp=tempfile.TemporaryDirectory();self.addCleanup(temp.cleanup)
        self.target=Path(temp.name)/'UI.kui';self.data=SAMPLE.read_bytes()
        self.target.write_bytes(self.data)

    def test_roundtrip_and_layout(self):
        widgets=parse(self.data);self.assertEqual(encode(widgets),self.data.replace(b"\r\n",b"\n"))
        self.assertEqual(parse(encode(parse(DEFAULT))),parse(DEFAULT))
        self.assertEqual(layout(widgets[4],1280,720),(1142,52,126,40))
        self.assertEqual(layout(widgets[4],720,1280),(582,52,126,40))
        widgets[1].text='COINS "GOLD" \\ '
        self.assertEqual(parse(encode(widgets)),widgets)

    def test_native_transaction_and_stale_write(self):
        widgets=parse(self.data);widgets[4].x=-30
        data=encode(widgets)
        publish_config(self.target,self.data,data,LIMIT,[VALIDATOR,'--ui'])
        self.assertEqual(self.target.read_bytes(),data)
        widgets[4].action='resume' # play screen cannot resume
        with self.assertRaises(ValueError):publish_config(self.target,data,encode(widgets),LIMIT,[VALIDATOR,'--ui'])
        self.assertEqual(self.target.read_bytes(),data)
        with self.assertRaises(ValueError):publish_config(self.target,self.data,self.data,LIMIT,[VALIDATOR,'--ui'])
        self.assertEqual(list(self.target.parent.iterdir()),[self.target])

    @unittest.skipUnless(GUI,'GUI requires an actual display')
    def test_designer_widgets_and_save(self):
        import tkinter as tk
        from tkinter import ttk
        root=tk.Tk();self.addCleanup(root.destroy);root.update()
        errors=[];root.report_callback_exception=lambda *args:errors.append(str(args[1]))
        open_designer(root,self.target,VALIDATOR,bounded_config,publish_config,lambda e:errors.append(str(e)))
        root.update()
        def walk(widget):
            for child in widget.winfo_children():
                yield child;yield from walk(child)
        controls=list(walk(root));buttons={w.cget('text'):w for w in controls if isinstance(w,ttk.Button)}
        for name in ('Add widget','Undo','Redo','Save UI'):
            buttons[name].invoke();root.update()
        saved=parse(self.target.read_bytes());self.assertEqual(len(saved),16)
        canvas=next(w for w in controls if isinstance(w,tk.Canvas))
        center=(canvas.winfo_width()//2,canvas.winfo_height()//2)
        canvas.event_generate('<ButtonPress-1>',x=center[0],y=center[1]);root.update()
        canvas.event_generate('<B1-Motion>',x=center[0]+20,y=center[1]+12);root.update()
        canvas.event_generate('<ButtonRelease-1>',x=center[0]+20,y=center[1]+12);root.update()
        buttons['Save UI'].invoke();root.update()
        self.assertNotEqual(parse(self.target.read_bytes())[-1].x,0)
        self.assertEqual(errors,[])
        subprocess.run([str(VALIDATOR),'--ui',str(self.target)],check=True,capture_output=True)


if __name__=='__main__':unittest.main()
