"""Exercise real subprocess sequencing using explicit fixture executables."""
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from kairo_project_workflow import ProjectTools, run_cli


@unittest.skipIf(os.name == 'nt', 'POSIX executable fixture; Windows requires native acceptance')
class WorkflowTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='kairo workflow ')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.build = self.root/'build'
        self.project = self.root/'game folder'/'Project.kproject'
        self.project.parent.mkdir()
        self.project.write_text('fixture only')
        self.trace = self.project.parent/'trace'
        for name,folder in (('KairoProjectCompiler','components/KairoEditor'),
                            ('KairoEditorApp','components/KairoEditor'),
                            ('KairoPlayer','Runtime/KairoPlayer'),
                            ('KairoMenuDocumentTool','Runtime/KairoPlayer')):
            binary=self.build/folder/name
            binary.parent.mkdir(parents=True,exist_ok=True)
            binary.write_text('#!/usr/bin/env python3\nimport os,sys,json\nfrom pathlib import Path\n'
                              'with open("trace","a") as f: f.write(json.dumps([Path(sys.argv[0]).name,*sys.argv[1:]])+"\\n")\n'
                              'sys.exit(9 if os.environ.get("KAIRO_TEST_FAIL")==Path(sys.argv[0]).name else 0)\n')
            binary.chmod(0o755)
        self.tools=ProjectTools(self.project,self.build)

    def commands(self):
        import json
        return [json.loads(line) for line in self.trace.read_text().splitlines()]

    def test_package_sequence(self):
        self.assertEqual(run_cli(self.tools,'package'),0)
        p=str(self.project)
        self.assertEqual(self.commands(),[['KairoProjectCompiler',p],['KairoPlayer',p,'--validate'],['KairoPlayer',p,'--package','Release']])

    def test_named_profile_and_archive_sequence(self):
        self.project.write_text('build-profile "Preview" development "Build/Preview"\n')
        archive=self.root/'game.zip'
        commands=self.tools.commands('package',profile='Preview',archive=archive,replace=True)
        self.assertEqual(commands[2], [self.tools.binary('KairoPlayer','Runtime/KairoPlayer'),str(self.project),'--package','Preview','--replace'])
        self.assertEqual(commands[3][2:],['export',str(self.project.parent/'Build/Preview'),str(archive)])
        with self.assertRaises(ValueError):self.tools.commands('package',profile='Release')
        archive.touch()
        with self.assertRaises(ValueError):self.tools.commands('package',profile='Preview',archive=archive)
        self.project.write_text('build-profile "Unsafe" release "Assets"\n')
        with self.assertRaises(ValueError):self.tools.commands('package',profile='Unsafe')

    def test_failure_stops_later_steps(self):
        with patch.dict(os.environ,{'KAIRO_TEST_FAIL':'KairoProjectCompiler'}):
            self.assertEqual(run_cli(self.tools,'play'),9)
        self.assertEqual(len(self.commands()),1)
        self.trace.unlink()
        with patch.dict(os.environ,{'KAIRO_TEST_FAIL':'KairoPlayer'}):
            self.assertEqual(run_cli(self.tools,'package'),9)
        self.assertEqual(len(self.commands()),2)

    def test_smoke_and_editor(self):
        self.assertEqual(run_cli(self.tools,'smoke','metal'),0)
        self.assertEqual(self.commands()[-1],['KairoPlayer',str(self.project),'--renderer','metal','--smoke'])
        self.assertEqual(run_cli(self.tools,'editor'),0)
        self.assertEqual(self.commands()[-1],['KairoEditorApp','--project',str(self.project)])
        self.assertEqual(run_cli(self.tools,'editor','metal'),0)
        self.assertEqual(self.commands()[-1],['KairoEditorApp','--project',str(self.project),'--renderer','metal'])

    @unittest.skipUnless(os.environ.get('DISPLAY'), 'Requires real graphical display/Xvfb')
    def test_gui_actual_buttons(self):
        import tkinter as tk
        import time
        from kairo_project_workflow import gui
        def exercise(root):
            panel=root.winfo_children()[0]
            def wait():
                deadline=time.monotonic()+10
                while panel.process is not None or panel.pending:
                    self.assertLess(time.monotonic(),deadline)
                    root.update();time.sleep(.01)
            # Real Tk widgets drive the same subprocess path as the CLI.
            bar=next(w for w in panel.winfo_children() if isinstance(w,tk.ttk.Frame))
            buttons={w['text']:w for w in bar.winfo_children()}
            buttons['Validate'].invoke()
            self.assertEqual(str(buttons['Play']['state']),'disabled')
            wait()
            self.assertIn('successfully',panel.status.get())
            self.assertEqual(len(self.commands()),2)
            with patch.dict(os.environ,{'KAIRO_TEST_FAIL':'KairoProjectCompiler'}), \
                    patch('tkinter.simpledialog.askstring',return_value='Release'), \
                    patch('tkinter.messagebox.askyesno',return_value=False):
                buttons['Package Release'].invoke();wait()
            self.assertIn('failed',panel.status.get())
            self.assertEqual(len(self.commands()),3)
            self.assertEqual(str(buttons['Play']['state']),'normal')
            panel.close()
        with patch.object(tk.Tk,'mainloop',exercise):
            self.assertEqual(gui(self.project,self.build,'auto'),0)

    def test_gameplay_authoring_command(self):
        root=self.root/'engine';(root/'scripts').mkdir(parents=True)
        script=root/'scripts/kairo_gameplay_editor.py';script.touch()
        tools=ProjectTools(self.project,self.build,root)
        command=tools.commands('gameplay')[0]
        self.assertEqual(command[1:],[str(script),str(self.project)])
        script.unlink()
        with self.assertRaises(ValueError): tools.commands('gameplay')

    def test_missing_and_menu_contract(self):
        (self.build/'Runtime/KairoPlayer/KairoPlayer').unlink()
        with self.assertRaises(ValueError): self.tools.commands('play')
        self.assertFalse(self.trace.exists())
        root=self.root/'engine';(root/'scripts').mkdir(parents=True)
        script=root/'scripts/kairo_menu_designer.py';script.touch()
        command=ProjectTools(self.project,self.build,root).commands('menu')[0]
        self.assertEqual(command[1:], [str(script),str(self.project.parent),'--validator',str(self.build/'Runtime/KairoPlayer/KairoMenuDocumentTool')])
        with self.assertRaises(ValueError):self.tools.commands('unknown')


if __name__=='__main__':
    unittest.main()
