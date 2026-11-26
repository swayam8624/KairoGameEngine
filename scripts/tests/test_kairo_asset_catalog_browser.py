"""Exercise real Tk selection, filtering and verified staging under Xvfb."""
import hashlib
import json
from pathlib import Path
import struct
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
        self.browser.worker.shutdown(wait=True)
        self.root.destroy()

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
