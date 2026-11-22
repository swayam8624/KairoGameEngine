"""Focused standard-library tests for the documentation link auditor."""

from __future__ import annotations

from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from scripts.audit_teaching_links import scan


class TeachingLinkAuditTests(unittest.TestCase):
    """Use tiny throwaway checkouts so no repository/network is required."""

    def test_relative_links_images_and_references(self) -> None:
        with TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "docs").mkdir()
            (root / "images").mkdir()
            (root / "README.md").write_text("# Root\n", encoding="utf-8")
            (root / "images" / "pet.png").write_bytes(b"fake")
            (root / "docs" / "guide.md").write_text(
                "[Readme](../README.md)\n"
                "![Pet](../images/pet.png)\n"
                "[reference]: ../README.md\n"
                "[External](https://example.org/file.md)\n"
                "[Section](#summary)\n",
                encoding="utf-8",
            )
            report = scan(root)
            self.assertEqual(report.markdown_files, 2)
            self.assertEqual(report.local_links, 3)
            self.assertEqual(report.skipped_links, 2)
            self.assertEqual(report.broken, ())

    def test_broken_local_target_reports_document_and_line(self) -> None:
        with TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "docs").mkdir()
            (root / "docs" / "guide.md").write_text(
                "# Intro\n[Missing](../never-there.md)\n",
                encoding="utf-8",
            )
            report = scan(root)
            self.assertEqual(len(report.broken), 1)
            self.assertEqual(report.broken[0].source, Path("docs/guide.md"))
            self.assertEqual(report.broken[0].line, 2)

    def test_skips_fenced_examples_and_vendor_folders(self) -> None:
        with TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "README.md").write_text(
                "# Intro\n"
                + chr(96) * 3 + "md\n"
                "[Not a real link](missing.md)\n"
                + chr(96) * 3 + "\n",
                encoding="utf-8",
            )
            (root / "vendor").mkdir()
            (root / "vendor" / "README.md").write_text(
                "[Missing](third-party.md)", encoding="utf-8"
            )
            result = scan(root)
            self.assertEqual(result.markdown_files, 1)
            self.assertEqual(result.local_links, 0)
            self.assertEqual(result.broken, ())

    def test_valid_percent_encoded_path(self) -> None:
        with TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "file name.md").write_text("# File\n", encoding="utf-8")
            (root / "README.md").write_text(
                "[File](file%20name.md#heading)\n", encoding="utf-8"
            )
            result = scan(root)
            self.assertEqual(result.local_links, 1)
            self.assertFalse(result.broken)


if __name__ == "__main__":
    unittest.main()
