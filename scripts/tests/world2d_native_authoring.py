#!/usr/bin/env python3
"""Exercise the exact authoring model against the built native validator."""
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from kairo_world2d_editor import Document, World, Actor

VALIDATOR=Path(sys.argv.pop(1)).resolve(strict=True)
SAMPLE=Path(sys.argv.pop(1)).resolve(strict=True)


class WorldTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path=Path(self.temp.name)/"World.k2d"
        self.path.write_bytes(SAMPLE.read_bytes())
        self.doc=Document(self.path,VALIDATOR)

    def test_roundtrip_migration(self):
        world=self.doc.world
        self.assertEqual(world.size,(32,16))
        self.assertEqual(len(world.actors),8)
        self.assertTrue(world.encode().startswith(b"kairo-world2d 2\n"))
        self.assertEqual(World.parse(world.encode()),world)
        self.assertFalse(self.doc.dirty)
        self.doc.save()
        self.assertEqual(Document(self.path,VALIDATOR).world,world)

    def test_undo_redo_save(self):
        self.assertTrue(self.doc.edit(lambda w:w.colors.__setitem__(1,0xabcdef)))
        self.assertTrue(self.doc.dirty)
        self.doc.undo();self.assertFalse(self.doc.dirty)
        self.doc.redo();self.assertTrue(self.doc.dirty)
        self.doc.save();self.assertFalse(self.doc.dirty)
        self.assertEqual(Document(self.path,VALIDATOR).world.colors[1],0xabcdef)

    def test_native_validation_rolls_back(self):
        original=self.path.read_bytes()
        self.doc.edit(lambda w:setattr(w.actors[0],"x",0))
        with self.assertRaises(ValueError):self.doc.save()
        self.assertEqual(self.path.read_bytes(),original)
        self.assertEqual(list(self.path.parent.iterdir()),[self.path])
        self.assertTrue(self.doc.dirty)

    def test_stale_save(self):
        self.doc.edit(lambda w:w.colors.__setitem__(3,0x123456))
        external=self.path.read_bytes()+b"\n"
        self.path.write_bytes(external)
        with self.assertRaises(ValueError):self.doc.save()
        self.assertEqual(self.path.read_bytes(),external)

    def test_sprite_dimensions_and_actors(self):
        def edit(w):
            w.sprites["new"]=(8,8,4,("1"*64,"2"*64))
            w.actors.append(Actor(99,"decor",50,50,16,16,"new",0))
        self.doc.edit(edit);self.doc.save()
        reopened=Document(self.path,VALIDATOR).world
        self.assertEqual(reopened.sprites["new"],(8,8,4,("1"*64,"2"*64)))
        self.assertEqual(reopened.actors[-1].id,99)

    def test_physics_roundtrip_and_invalid_body_rollback(self):
        config="physics 0 180\nbodies 1\n6 dynamic box 2 0.3 0.7 4 3 0 0 0\njoints 0\n"
        self.doc.edit(lambda w:setattr(w,"physics",config))
        self.assertTrue(self.doc.world.encode().startswith(b"kairo-world2d 3\n"))
        self.doc.undo();self.assertFalse(self.doc.dirty)
        self.doc.redo();self.doc.save()
        reopened=Document(self.path,VALIDATOR)
        self.assertEqual(reopened.world.physics,config)
        before=self.path.read_bytes()
        reopened.edit(lambda w:setattr(w,"physics",config.replace("6 dynamic","99 dynamic")))
        with self.assertRaises(ValueError):reopened.save()
        self.assertEqual(self.path.read_bytes(),before)
        reopened.edit(lambda w:setattr(w,"physics",""))
        reopened.save()
        self.assertTrue(self.path.read_bytes().startswith(b"kairo-world2d 2\n"))

    def test_invalid_projection(self):
        with self.assertRaises(ValueError):World.parse(b"kairo-world2d 1\n")
        with self.assertRaises(ValueError):World.parse(self.doc.world.encode()+b"trailing")


if __name__=="__main__":
    unittest.main()
