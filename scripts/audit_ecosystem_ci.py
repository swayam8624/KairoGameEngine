#!/usr/bin/env python3
"""Audit all 27 Kairo repositories against their *current default-branch heads*.

A prior successful GitHub Actions run does not prove that a newer revision was
tested. This script records exact-head, workflow-specific evidence and never
converts that evidence into an invented engine completion percentage.

Public repository metadata is readable without a token, but GITHUB_TOKEN is
recommended to avoid unauthenticated API rate limits.
"""

from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime, timezone
import json
import os
from pathlib import Path
from typing import Any
from urllib.error import HTTPError, URLError
from urllib.parse import quote, urlencode
from urllib.request import Request, urlopen


OWNER = "swayam8624"
REPOSITORIES = (
    "KairoMath", "KairoGeometry", "KairoSpatial", "KairoSIMD",
    "KairoPhysicsMath", "KairoPhysicsEngine", "KairoScheduler", "KairoECS",
    "KairoReflection", "KairoAssets", "KairoEngineCore", "KairoGPU",
    "KairoRenderer", "KairoRayTracer", "KairoEditor", "KairoHub",
    "KairoMacPerception", "KairoONNX", "KairoTransformers", "KairoAI",
    "KairoGameEngine", "KairoPipelineCore", "KairoBlender", "KairoMaya",
    "KairoHoudini", "KairoNuke", "KairoProductionTools",
)


def github_json(url: str, token: str | None) -> dict[str, Any]:
    """Read one GitHub REST JSON object without silently hiding API failures."""
    headers = {
        "Accept": "application/vnd.github+json",
        "X-GitHub-Api-Version": "2022-11-28",
        "User-Agent": "KairoExactHeadAudit/1.0",
    }
    if token:
        headers["Authorization"] = f"Bearer {token}"
    try:
        with urlopen(Request(url, headers=headers), timeout=30) as response:
            payload = json.load(response)
    except HTTPError as error:
        raise RuntimeError(f"GitHub HTTP {error.code} for {url}") from error
    except (URLError, TimeoutError) as error:
        raise RuntimeError(f"GitHub request failed for {url}: {error}") from error
    if not isinstance(payload, dict):
        raise ValueError(f"GitHub returned a non-object response for {url}")
    return payload


def summarize_runs(head_sha: str, runs: list[dict[str, Any]],
                   total_count: int) -> dict[str, Any]:
    """Classify latest attempts for each observed workflow at an exact SHA.

    A green status proves only that the *observed workflows* for this SHA
    succeeded. It cannot prove runtime correctness or that every possible
    platform/integration workflow was configured or triggered.
    """
    exact = [run for run in runs if run.get("head_sha") == head_sha]
    newest: dict[str, dict[str, Any]] = {}
    for run in exact:
        key = str(run.get("workflow_id") or run.get("path") or run.get("name"))
        existing = newest.get(key)
        order = (str(run.get("created_at", "")), int(run.get("run_attempt", 1)),
                 int(run.get("id", 0)))
        old_order = ((str(existing.get("created_at", "")),
                      int(existing.get("run_attempt", 1)),
                      int(existing.get("id", 0))) if existing else None)
        if old_order is None or order > old_order:
            newest[key] = run

    workflows = []
    for key, run in sorted(newest.items()):
        workflows.append({
            "workflow_id": key,
            "name": run.get("name"),
            "run_id": run.get("id"),
            "url": run.get("html_url"),
            "status": run.get("status"),
            "conclusion": run.get("conclusion"),
            "created_at": run.get("created_at"),
        })
    if total_count > len(runs):
        state = "incomplete_pagination"
    elif not workflows:
        state = "no_exact_head_runs"
    elif any(w["status"] != "completed" for w in workflows):
        state = "pending"
    elif any(w["conclusion"] != "success" for w in workflows):
        state = "failed_or_cancelled"
    else:
        state = "observed_workflows_green"
    return {
        "state": state,
        "observed_workflow_count": len(workflows),
        "matching_run_count": len(exact),
        "api_total_count": total_count,
        "workflows": workflows,
    }


def audit_one(name: str, owner: str, token: str | None) -> dict[str, Any]:
    """Fetch repo metadata, its default branch's head, and runs for that SHA."""
    base = f"https://api.github.com/repos/{quote(owner, safe='')}/{quote(name, safe='')}"
    metadata = github_json(base, token)
    branch = metadata["default_branch"]
    branch_doc = github_json(f"{base}/branches/{quote(branch, safe='')}", token)
    head_sha = branch_doc["commit"]["sha"]
    query = urlencode({"head_sha": head_sha, "branch": branch, "per_page": 100})
    run_doc = github_json(f"{base}/actions/runs?{query}", token)
    result = summarize_runs(head_sha, run_doc.get("workflow_runs", []),
                            int(run_doc.get("total_count", 0)))
    return {
        "repository": f"{owner}/{name}",
        "default_branch": branch,
        "head_sha": head_sha,
        "html_url": metadata["html_url"],
        **result,
    }


def run_audit(owner: str, token: str | None, workers: int = 4) -> dict[str, Any]:
    """Record every repo, including API failures, in deterministic repo order."""
    results: dict[str, dict[str, Any]] = {}
    with ThreadPoolExecutor(max_workers=workers) as executor:
        futures = {
            executor.submit(audit_one, name, owner, token): name
            for name in REPOSITORIES
        }
        for future in as_completed(futures):
            name = futures[future]
            try:
                results[name] = future.result()
            except Exception as error:
                results[name] = {
                    "repository": f"{owner}/{name}",
                    "state": "api_error",
                    "error": str(error),
                }
    ordered = [results[name] for name in REPOSITORIES]
    counts: dict[str, int] = {}
    for item in ordered:
        counts[item["state"]] = counts.get(item["state"], 0) + 1
    return {
        "schema": "kairo.portfolio-ci-audit.v1",
        "created_at_utc": datetime.now(timezone.utc).isoformat(),
        "owner": owner,
        "repository_count": len(ordered),
        "state_counts": dict(sorted(counts.items())),
        "limitations": (
            "Only GitHub Actions runs observed at each current default-branch "
            "head are evaluated. Green observed workflows are not full runtime "
            "acceptance, platform parity, benchmark validation or completeness."
        ),
        "repositories": ordered,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--owner", default=OWNER)
    parser.add_argument("--output", type=Path, default=Path("build/portfolio-ci-audit.json"))
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument("--strict", action="store_true",
                        help="Fail unless all 27 exact-head observed-workflow states are green")
    args = parser.parse_args()
    if not 1 <= args.workers <= 8:
        parser.error("--workers must be between 1 and 8")
    report = run_audit(args.owner, os.environ.get("GITHUB_TOKEN"), args.workers)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                           encoding="utf-8")
    print(f"Wrote {args.output}: {report['repository_count']} Kairo repositories")
    for state, count in report["state_counts"].items():
        print(f"  {state}: {count}")
    if report["state_counts"].get("api_error", 0):
        return 2
    if args.strict and report["state_counts"] != {
        "observed_workflows_green": len(REPOSITORIES)
    }:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
