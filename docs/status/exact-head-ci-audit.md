# Kairo exact-head CI audit

Run `GITHUB_TOKEN=<token> python3 scripts/audit_ecosystem_ci.py` from the
umbrella repository (public metadata can be read without a token, but it has a
lower rate limit). The script writes `build/portfolio-ci-audit.json`, records
the 27 known Kairo repositories in deterministic order, resolves the current
default-branch SHA for each and inspects only GitHub Actions runs for **that
exact SHA**. Its weekly/manual CI workflow uploads the JSON as an artifact.

The five meaningful states are `observed_workflows_green`,
`no_exact_head_runs`, `pending`, `failed_or_cancelled`, and
`incomplete_pagination`; unexpected API errors are reported separately as
`api_error` and fail the process. Use `--strict` to return a nonzero exit
code unless all 27 repositories have observed green runs. Strict mode is **not**
enabled in the weekly evidence-collection workflow: the purpose is to preserve
diagnostics even before portfolio-wide readiness has been achieved.

**Important:** A green result says the latest observed runs for workflows
that executed on that SHA passed. It cannot establish full workflow coverage,
correctness of untested features, native GPU availability, package validity,
cross-repository pin synchronization, or a completion percentage. Use
`scripts/run_portfolio_acceptance.sh` for host-specific integrated checks;
validate each unsupported platform independently.
