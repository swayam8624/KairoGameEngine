#!/usr/bin/env bash
set -euo pipefail

ENGINE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EVIDENCE="${ENGINE_ROOT}/build/portfolio-evidence/accepted.env"

bash "${ENGINE_ROOT}/scripts/verify_workspace_lock.sh"

if [[ ! -f "${EVIDENCE}" ]]; then
    echo >&2
    echo "KAIRO portfolio is NOT accepted at this revision." >&2
    echo "No exact-head acceptance evidence exists." >&2
    echo "Run: bash scripts/run_portfolio_acceptance.sh" >&2
    exit 1
fi

# shellcheck disable=SC1090
source "${EVIDENCE}"
CURRENT_ENGINE_SHA="$(git -C "${ENGINE_ROOT}" rev-parse HEAD)"
CURRENT_LOCK_SHA256="$(shasum -a 256 "${ENGINE_ROOT}/workspace.lock" | awk '{print $1}')"

if [[ "${KAIRO_ACCEPTED_ENGINE_SHA:-}" != "${CURRENT_ENGINE_SHA}" ]]; then
    echo "KAIRO acceptance evidence is stale: GameEngine revision changed." >&2
    echo "accepted: ${KAIRO_ACCEPTED_ENGINE_SHA:-missing}" >&2
    echo "current:  ${CURRENT_ENGINE_SHA}" >&2
    exit 1
fi
if [[ "${KAIRO_ACCEPTED_LOCK_SHA256:-}" != "${CURRENT_LOCK_SHA256}" ]]; then
    echo "KAIRO acceptance evidence is stale: workspace.lock changed." >&2
    echo "accepted: ${KAIRO_ACCEPTED_LOCK_SHA256:-missing}" >&2
    echo "current:  ${CURRENT_LOCK_SHA256}" >&2
    exit 1
fi

echo "KAIRO exact-head acceptance evidence: PASS"
echo "GameEngine: ${CURRENT_ENGINE_SHA}"
echo "workspace.lock sha256: ${CURRENT_LOCK_SHA256}"
echo "Host: ${KAIRO_ACCEPTED_HOST:-unknown}"
echo
echo "This proves only the gates executed by run_portfolio_acceptance.sh on this host."
echo "Platform-gated hosts remain unverified."
