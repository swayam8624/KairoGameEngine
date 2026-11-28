import tempfile
import os
import unittest
from pathlib import Path
import studio_distribution as studio


class StudioTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / 'source'
        self.build = self.root / 'build'
        self.dest = self.root / 'staged'
        for name in studio.SCRIPTS:
            self.write(self.source / 'scripts' / name)
        for name in studio.SAMPLES:
            self.write(self.source / 'Samples' / name / 'Project.kproject')
        for folder, names in studio.TOOLS.items():
            for name in names:
                self.write(self.build / folder / (name + ('.exe' if os.name == 'nt' else '')))
        for name in ('LICENSE', 'workspace.lock'):
            self.write(self.source / name)

    @staticmethod
    def write(path):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('fixture\n')

    def test_relocation(self):
        studio.stage(self.source, self.build, self.dest)
        archive = self.root / 'studio.zip'
        installed = self.root / 'relocated Studio'
        expected = studio.archive_and_relocate(self.dest, archive, installed)
        self.assertEqual(studio.inventory(installed), expected)
        self.assertFalse(self.dest.exists())
        if os.name == 'nt':
            self.assertTrue((installed / 'launch-studio.cmd').is_file())
            self.assertTrue((installed / 'build/game-clang/components/KairoEditor/KairoEditorApp.exe').is_file())
        else:
            self.assertTrue((installed / 'launch-studio.sh').stat().st_mode & 0o100)
            self.assertTrue((installed / 'build/game-clang/components/KairoEditor/KairoEditorApp').stat().st_mode & 0o100)

    def test_missing_tool_rolls_back(self):
        (self.build / 'Runtime/KairoPlayer' / ('KairoWorld2DTool' + ('.exe' if os.name == 'nt' else ''))).unlink()
        with self.assertRaises(ValueError):
            studio.stage(self.source, self.build, self.dest)
        self.assertFalse(self.dest.exists())

    def test_transient_and_existing_destination(self):
        self.write(self.source / 'Samples/BeaconRun/Build/private')
        with self.assertRaises(ValueError):
            studio.stage(self.source, self.build, self.dest)
        self.assertFalse(self.dest.exists())
        self.dest.mkdir()
        self.write(self.dest / 'keep')
        with self.assertRaises(ValueError):
            studio.stage(self.source, self.build, self.dest)
        self.assertTrue((self.dest / 'keep').exists())


if __name__ == '__main__':
    unittest.main()
