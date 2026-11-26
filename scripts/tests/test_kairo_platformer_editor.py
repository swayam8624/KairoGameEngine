import os
from pathlib import Path
import tempfile
import unittest
from kairo_platformer_editor import TileDocument, parse, encode
from kairo_hub_core import create, editor_command, ROOT

class TileEditorTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix="kairo 2d ")
        self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name).resolve()
        self.project=create("2d","My Platformer",self.root)
        self.path=self.project.parent/"Config/Platformer.k2d"
        self.doc=TileDocument(self.path)

    def test_paint_undo_redo_and_marker_move(self):
        original=self.doc.rows
        self.assertFalse(self.doc.paint(0,1,"."))
        self.assertFalse(self.doc.paint(1,4,"."))
        self.assertTrue(self.doc.paint(2,3,"S"))
        self.assertEqual(sum(row.count("S") for row in self.doc.rows),1)
        self.assertEqual(self.doc.rows[4][1],".")
        self.doc.undo()
        self.assertEqual(self.doc.rows,original)
        self.assertFalse(self.doc.dirty)
        self.doc.redo()
        self.assertTrue(self.doc.dirty)
        self.doc.save()
        self.assertEqual(parse(self.path.read_bytes()),self.doc.rows)
        self.assertFalse(self.doc.dirty)
        self.doc.undo()
        self.assertTrue(self.doc.dirty)

    def test_external_edit_preserved(self):
        self.doc.paint(2,3,"C")
        data=self.path.read_bytes()+b"\n"
        self.path.write_bytes(data)
        with self.assertRaises(ValueError):
            self.doc.save()
        self.assertEqual(self.path.read_bytes(),data)

    def test_validation(self):
        self.assertEqual(parse(encode(self.doc.rows)),self.doc.rows)
        self.assertEqual(parse(encode(self.doc.rows).replace(b"\n",b"\r\n")),self.doc.rows)
        for value in (b"",b"kairo-platformer 2\n",b"kairo-platformer 1\n#####\n#SSG#\n#####",b"#"*4097):
            with self.assertRaises(ValueError):
                parse(value)

    def test_hub_routes_2d_editor(self):
        build=self.root/"build";build.mkdir()
        command=editor_command(self.project,build,"vulkan")
        self.assertEqual(Path(command[1]),ROOT/"scripts/kairo_platformer_editor.py")
        self.assertEqual(command[2:],[str(self.project),"--build",str(build),"--renderer","vulkan"])
        self.path.unlink()
        self.assertEqual(editor_command(self.project,build,"vulkan"),command)
        with self.assertRaises(ValueError):
            TileDocument(self.path)

    @unittest.skipUnless(os.environ.get("DISPLAY") and os.name!="nt","Needs Xvfb and POSIX fixture")
    def test_actual_canvas_paint_and_play(self):
        import tkinter as tk
        import time
        from kairo_platformer_editor import Editor
        build=self.root/"build"
        player=build/"Runtime/KairoPlayer/KairoPlayer"
        player.parent.mkdir(parents=True)
        player.write_text('#!/usr/bin/env python3\nfrom pathlib import Path\nimport sys\nPath("played.txt").write_text(sys.argv[1])\n')
        player.chmod(0o755)
        window=tk.Tk();window.geometry("1040x680")
        editor=Editor(window,self.project,build,"vulkan")
        try:
            window.update()
            editor.brush.set("C")
            editor.canvas.event_generate("<Button-1>",x=editor.ox+2*editor.scale+editor.scale//2,y=editor.oy+3*editor.scale+editor.scale//2)
            window.update()
            self.assertEqual(editor.doc.rows[3][2],"C")
            editor.undo();self.assertEqual(editor.doc.rows[3][2],".")
            editor.redo();editor.play()
            deadline=time.monotonic()+10
            while editor.child is not None:
                self.assertLess(time.monotonic(),deadline)
                window.update();time.sleep(.02)
            self.assertEqual((self.project.parent/"played.txt").read_text(),str(self.project))
            self.assertEqual(parse(self.path.read_bytes())[3][2],"C")
        finally:
            editor.close()

if __name__=="__main__":
    unittest.main()
