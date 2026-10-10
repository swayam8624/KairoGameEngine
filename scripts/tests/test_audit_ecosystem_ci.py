"""Offline exact-head audit classification tests (no GitHub calls)."""

import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from audit_ecosystem_ci import REPOSITORIES, summarize_runs


class AuditTests(unittest.TestCase):
    def test_inventory_has_exactly_27_unique_repositories(self) -> None:
        self.assertEqual(len(REPOSITORIES), 27)
        self.assertEqual(len(set(REPOSITORIES)), 27)

    def test_older_green_commit_is_not_evidence_for_current_head(self) -> None:
        old = {"workflow_id": 1, "head_sha": "old", "status": "completed",
               "conclusion": "success", "id": 1}
        report = summarize_runs("new", [old], 1)
        self.assertEqual(report["state"], "no_exact_head_runs")

    def test_later_failure_supersedes_success_for_same_workflow(self) -> None:
        runs = [
            {"workflow_id": 3, "head_sha": "sha", "status": "completed",
             "conclusion": "success", "id": 2, "created_at": "2026-10-09T00:00:00Z"},
            {"workflow_id": 3, "head_sha": "sha", "status": "completed",
             "conclusion": "failure", "id": 3, "created_at": "2026-10-10T00:00:00Z"},
        ]
        report = summarize_runs("sha", runs, 2)
        self.assertEqual(report["state"], "failed_or_cancelled")
        self.assertEqual(report["workflows"][0]["run_id"], 3)

    def test_pending_workflow_blocks_green(self) -> None:
        runs = [
            {"workflow_id": 1, "head_sha": "x", "status": "completed",
             "conclusion": "success", "id": 7},
            {"workflow_id": 2, "head_sha": "x", "status": "in_progress",
             "conclusion": None, "id": 8},
        ]
        self.assertEqual(summarize_runs("x", runs, 2)["state"], "pending")

    def test_all_observed_exact_head_workflows_green(self) -> None:
        runs = [
            {"workflow_id": 1, "head_sha": "x", "status": "completed",
             "conclusion": "success", "id": 7},
            {"workflow_id": 2, "head_sha": "x", "status": "completed",
             "conclusion": "success", "id": 8},
        ]
        self.assertEqual(summarize_runs("x", runs, 2)["state"],
                         "observed_workflows_green")

    def test_truncation_cannot_be_reported_as_green(self) -> None:
        run = {"workflow_id": 1, "head_sha": "x", "status": "completed",
               "conclusion": "success", "id": 1}
        self.assertEqual(summarize_runs("x", [run], 101)["state"],
                         "incomplete_pagination")


if __name__ == "__main__":
    unittest.main()
