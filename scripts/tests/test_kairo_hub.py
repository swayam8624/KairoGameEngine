"""Exercise real template generation, safe copying and actual hub widgets."""
import json
import os
from pathlib import Path
import tempfile
import time
import unittest

from kairo_hub_core import ROOT, RecentProjects, create, descriptor, editor_command, project_name


class HubTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="kairo hub ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()

    def test_portable_names(self):
        for value in ("Game", "A game_2", "A-1"):
            self.assertEqual(project_name(value), value)
        for value in ("", "../escape", "a/b", "a\\b", "CON", "NUL", "COM1", "a ", " a", "a\nb", "a"*65):
            with self.subTest(value=value), self.assertRaises(ValueError):
                project_name(value)

    def test_recent_missing_dedup_and_no_delete(self):
        a = self.root / "A.kproject"
        b = self.root / "B.kproject"
        a.write_text("a"); b.write_text("b")
        path = self.root / "state/recent.json"
        rec = RecentProjects(path)
        rec.touch(a); rec.touch(b); rec.touch(a)
        self.assertEqual(RecentProjects(path).paths, [str(a), str(b)])
        rec.remove(str(a))
        self.assertTrue(a.exists())
        b.unlink()
        self.assertEqual(RecentProjects(path).paths, [str(b)])
        with self.assertRaises(ValueError):
            descriptor(b)
        path.write_text("{broken")
        rec = RecentProjects(path)
        self.assertTrue(rec.warning)
        with self.assertRaises(ValueError):
            rec.touch(a)
        self.assertEqual(path.read_text(), "{broken")
        backup = rec.recover()
        self.assertEqual(backup.read_text(), "{broken")
        rec.touch(a)
        self.assertEqual(RecentProjects(path).paths, [str(a)])
        self.assertEqual(backup.read_text(), "{broken")

    def test_recent_limit(self):
        recent = RecentProjects(self.root / "recents.json")
        for i in range(40):
            file = self.root / f"{i}.kproject"
            file.touch()
            recent.touch(file)
        self.assertEqual(len(RecentProjects(recent.path).paths), 32)
        self.assertEqual(recent.paths[0], str(self.root / "39.kproject"))
        self.assertEqual(recent.paths[-1], str(self.root / "8.kproject"))

    def test_actual_repository_templates(self):
        for kind in ("empty", "arena", "beacon", "keygate"):
            with self.subTest(kind=kind):
                name = "My " + kind
                p = create(kind, name, self.root)
                self.assertEqual(p, self.root / name / "Project.kproject")
                self.assertIn('name "' + name + '"', p.read_text())
                self.assertTrue((p.parent / "Assets.kassets").is_file())
                self.assertTrue(any((p.parent / "Scenes").glob("*.kscene")))
                before = p.read_bytes()
                with self.assertRaises(FileExistsError):
                    create(kind, name, self.root)
                self.assertEqual(p.read_bytes(), before)
        with self.assertRaises(ValueError):
            create("2d", "Not native yet", self.root)
        self.assertFalse((self.root / "Not native yet").exists())

    def test_sample_copy_rejects_links_and_excludes_runtime(self):
        source = self.root / "engine/Samples/KeyGateArena"
        source.mkdir(parents=True)
        (source / "Project.kproject").write_text('kairo-project 2\nname "Original"\n')
        (source / "Assets.kassets").write_text("asset")
        (source / "Licenses").mkdir()
        (source / "Licenses/asset.txt").write_text("license")
        for directory in ("Build", ".kairo", "__pycache__"):
            (source / directory).mkdir()
            (source / directory / "generated").touch()
        if os.name != "nt":
            (source / "linked").symlink_to(source / "Assets.kassets")
            with self.assertRaises(ValueError):
                create("keygate", "Copy", self.root, self.root / "engine")
            self.assertFalse((self.root / "Copy").exists())
            (source / "linked").unlink()
        p = create("keygate", "Copy", self.root, self.root / "engine")
        self.assertIn('name "Copy"', p.read_text())
        self.assertIn('name "Original"', (source / "Project.kproject").read_text())
        self.assertEqual((p.parent / "Licenses/asset.txt").read_text(), "license")
        for directory in ("Build", ".kairo", "__pycache__"):
            self.assertFalse((p.parent / directory).exists())
        with self.assertRaises(ValueError):
            create("keygate", "Nested", source, self.root / "engine")

    @unittest.skipIf(os.name == "nt", "POSIX executable fixture")
    def test_real_editor_command(self):
        p = self.root / "A.kproject"; p.touch()
        binary = self.root / "build/components/KairoEditor/KairoEditorApp"
        binary.parent.mkdir(parents=True)
        binary.touch(); binary.chmod(0o755)
        self.assertEqual(editor_command(p, self.root / "build", "metal"),
                         [str(binary), "--project", str(p), "--renderer", "metal"])

    @unittest.skipUnless(os.environ.get("DISPLAY") and os.name != "nt", "Requires Xvfb and POSIX fixture")
    def test_real_hub_create_open_and_remove(self):
        import tkinter as tk
        from kairo_hub import Hub
        binary = self.root / "build/components/KairoEditor/KairoEditorApp"
        binary.parent.mkdir(parents=True)
        binary.write_text('#!/usr/bin/env python3\nimport sys,json\nfrom pathlib import Path\nPath("launch.json").write_text(json.dumps(sys.argv[1:]))\n')
        binary.chmod(0o755)
        window = tk.Tk()
        window.geometry("1024x760")
        hub = Hub(window, self.root / "build", self.root / "recents.json", renderer="metal")
        try:
            hub.show("new")
            hub.name.set("GUI Game")
            hub.location.set(str(self.root))
            hub.template.set("empty")
            hub.create_button.invoke()
            deadline = time.monotonic() + 12
            while hub.future is not None or hub.process is not None:
                self.assertLess(time.monotonic(), deadline)
                window.update()
                time.sleep(.02)
            p = self.root / "GUI Game/Project.kproject"
            self.assertTrue(p.is_file())
            self.assertEqual(json.loads((p.parent / "launch.json").read_text()),
                             ["--project", str(p), "--renderer", "metal"])
            self.assertIn("Editor closed", hub.status.get())
            self.assertEqual(hub.recent.paths, [str(p)])
            hub.tree.selection_set(hub.tree.get_children()[0])
            hub.remove_selected()
            self.assertTrue(p.exists())
            self.assertEqual(hub.recent.paths, [])
        finally:
            hub.close()


if __name__ == "__main__":
    unittest.main()
