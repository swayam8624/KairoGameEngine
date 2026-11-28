#!/usr/bin/env python3
"""Exercise native preview and safe VFX document publication."""
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from kairo_vfx2d_editor import preview,parse_preview,LIMIT
from kairo_world2d_editor import publish_config
TOOL=Path(sys.argv.pop(1)).resolve(strict=True)
SAMPLE=Path(sys.argv.pop(1)).resolve(strict=True)


class VFXTests(unittest.TestCase):
    def setUp(self):
        temp=tempfile.TemporaryDirectory();self.addCleanup(temp.cleanup)
        self.root=Path(temp.name)/"Project";shutil.copytree(SAMPLE,self.root)
        self.path=self.root/"Config/VFX.kvfx";self.baseline=self.path.read_bytes()

    def test_native_preview_and_transaction(self):
        first=preview(self.root,TOOL,self.baseline,"main","collect",12)
        self.assertEqual(first,preview(self.root,TOOL,self.baseline,"main","collect",12))
        self.assertEqual(first[0],28);self.assertGreater(len(first[2]),0)
        changed=self.baseline.replace(b"fff4bd",b"ffffff")
        publish_config(self.path,self.baseline,changed,LIMIT,[TOOL,"--vfx",self.root])
        self.assertEqual(self.path.read_bytes(),changed)
        with self.assertRaises(ValueError):
            publish_config(self.path,self.baseline,self.baseline,LIMIT,[TOOL,"--vfx",self.root])
        invalid=changed.replace(b"main 8 ambient",b"main 999 ambient")
        with self.assertRaises(ValueError):
            publish_config(self.path,changed,invalid,LIMIT,[TOOL,"--vfx",self.root])
        self.assertEqual(self.path.read_bytes(),changed)
        with self.assertRaises(ValueError):preview(self.root,TOOL,invalid,"main","collect",12)
        self.assertEqual(list(self.path.parent.glob(".vfx-preview-*")),[])

    def test_reject_invalid_preview_receipts(self):
        for bad in ("","vfx-preview 513 0 0","vfx-preview 1 0 1\n-1 0 2 2 0",
                    "vfx-preview 1 0 1\n0 0 641 2 0","vfx-preview 1 0 1",
                    "vfx-preview 1 0 1\n0 0 2 2 16777216"):
            with self.subTest(bad=bad),self.assertRaises(ValueError):parse_preview(bad)


if __name__=="__main__":unittest.main()
