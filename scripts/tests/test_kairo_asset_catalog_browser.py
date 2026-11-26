"""Exercise real Tk selection, filtering and verified staging under Xvfb."""
import hashlib
import json
from pathlib import Path
import struct
import runpy
import sys
from unittest.mock import patch
import tempfile
import time
import tkinter as tk
import unittest
from kairo_asset_catalog_browser import CatalogBrowser


class BrowserTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path=Path(self.temp.name)
        doc=b'{"asset":{"version":"2.0"}}'
        doc+=b' ' * (-len(doc)%4)
        self.glb=struct.pack('<4sIIII',b'glTF',2,20+len(doc),len(doc),0x4e4f534a)+doc
        (self.path/'model.glb').write_bytes(self.glb)
        (self.path/'LICENSE.txt').write_text('Synthetic test-only license evidence')
        self.records=[dict(id=key,name=name,file='model.glb',author='Fixture',
                           license='CC0-1.0',source='generated-test',sha256=hashlib.sha256(self.glb).hexdigest(),
                           license_file='LICENSE.txt',tags=[tag])
                      for key,name,tag in [('cube','Blue Cube','geometry'),('tree','Green Tree','nature')]]
        self.manifest=self.path/'catalog.json'
        self.write()
        self.root=tk.Tk()
        self.browser=CatalogBrowser(self.root)
        self.addCleanup(self.cleanup_gui)
        self.browser.load(self.manifest)
        self.root.update()

    def write(self):
        self.manifest.write_text(json.dumps({'version':1,'assets':self.records}))

    def cleanup_gui(self):
        self.browser.close()

    def finish(self):
        deadline=time.monotonic()+5
        while self.browser.future is not None and time.monotonic()<deadline:
            self.root.update()
            time.sleep(.01)
        self.assertIsNone(self.browser.future)

    def select_cube(self):
        self.browser.query.set('blue geometry')
        self.root.update()
        self.assertEqual(self.browser.tree.get_children(),('cube',))
        self.browser.tree.selection_set('cube')
        self.root.update()
        self.assertIn('CC0-1.0',self.browser.details.get('1.0','end'))

    def test_search_selection_and_stage(self):
        self.select_cube()
        out=self.path/'stage'
        self.browser.start_stage(out)
        self.finish()
        self.assertIn('Staged recipe:',self.browser.status.get())
        self.assertEqual((out/'assets/cube.glb').read_bytes(),self.glb)
        self.assertEqual(set(json.loads((out/'recipe.json').read_text())['assets']),{'cube'})
        self.assertTrue((out/'licenses/cube.txt').is_file())
        self.browser.start_stage(out)
        self.finish()
        self.assertIn('Staging failed:',self.browser.status.get())

    def test_stale_metadata_rejected(self):
        self.select_cube()
        self.records[0]['author']='Changed after review'
        self.write()
        out=self.path/'rejected'
        self.browser.start_stage(out)
        self.finish()
        self.assertIn('Catalog metadata changed',self.browser.status.get())
        self.assertFalse(out.exists())

    def test_create_project_and_editor_handoff(self):
        # Use the actual renderable triangle fixture, not the minimal GLB
        # header used by catalog-only tests.
        generator=Path(__file__).resolve().parents[2]/'Samples/AssetCatalog/create_demo_catalog.py'
        manifest=runpy.run_path(str(generator))['generate'](self.path/'real-catalog')
        self.browser.load(manifest)
        self.browser.tree.selection_set('triangle')
        self.root.update()
        output=self.path/'new game'
        self.browser.start_stage(output,project=True)
        self.finish()
        self.assertIn('Created editable project:',self.browser.status.get())
        self.assertEqual(self.browser.generated_project,output/'Project.kproject')
        self.assertTrue(self.browser.generated_project.is_file())
        records=json.loads((output/'ASSET_LICENSES.json').read_text())
        self.assertEqual(len(records),1)
        self.assertTrue((output/records[0]['license_file']).is_file())
        self.assertEqual(hashlib.sha256((output/records[0]['path']).read_bytes()).hexdigest(),
                         records[0]['sha256'])
        self.browser.start_stage(output,project=True)
        self.finish()
        self.assertIn('Project creation failed:',self.browser.status.get())
        self.assertTrue((output/'Project.kproject').is_file())

        # Verify the actual argument boundary without pretending to launch
        # a native GPU editor on the GUI-only runner.
        self.browser.editor=Path(sys.executable)
        with patch('kairo_asset_catalog_browser.subprocess.Popen') as launch:
            launch.return_value.poll.return_value=None
            self.browser.open_editor()
            launch.assert_called_once_with([sys.executable,'--project',str(output/'Project.kproject')],
                                           cwd=output)
            self.browser.open_editor()
            self.assertEqual(launch.call_count,1)

        timer = self.browser.editor_poll
        self.assertIsNotNone(timer)
        with patch.object(self.root, 'destroy'):
            self.browser.close()
        with self.assertRaises(tk.TclError):
            self.root.tk.call('after', 'info', timer)

    def test_filter_clears_selection(self):
        self.select_cube()
        self.browser.query.set('nature')
        self.root.update()
        self.assertEqual(self.browser.tree.get_children(),('tree',))
        self.assertFalse(self.browser.tree.selection())
        with self.assertRaises(ValueError):
            self.browser.start_stage(self.path/'invalid')


if __name__=='__main__':
    unittest.main()
