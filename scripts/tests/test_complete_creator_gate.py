"""Portable tests for the creator harness's fail-closed native execution gate."""
from contextlib import redirect_stdout
import io
from pathlib import Path
import sys
import tempfile
import unittest

from qualify_complete_creator_workflow import inventory, native


class CreatorGateTests(unittest.TestCase):
    def test_native_requires_non_skipped_evidence_and_exit_zero(self):
        with tempfile.TemporaryDirectory() as directory, redirect_stdout(io.StringIO()):
            work = Path(directory)
            native([sys.executable, '-c', "print('real marker')"], work, marker='real marker')
            with self.assertRaisesRegex(RuntimeError, 'did not report'):
                native([sys.executable, '-c', "print('rendering skipped')"], work, marker='real marker')
            with self.assertRaisesRegex(RuntimeError, 'Native operation failed'):
                native([sys.executable, '-c', "print('real marker'); raise SystemExit(7)"],
                       work, marker='real marker')

    def test_inventory_detects_mutation_and_missing_files(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            source = work / 'file.txt'
            source.write_bytes(b'original')
            before = inventory(work)
            source.write_bytes(b'changed')
            self.assertNotEqual(inventory(work), before)
            source.unlink()
            self.assertEqual(inventory(work), {})


if __name__ == '__main__':
    unittest.main()
