#!/usr/bin/env bash
set -euo pipefail

ENGINE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EVIDENCE_DIR="${ENGINE_ROOT}/build/flagship-evidence"
mkdir -p "${EVIDENCE_DIR}"

run() {
    echo
    echo "==> $*"
    "$@"
}

run bash "${ENGINE_ROOT}/scripts/run_portfolio_acceptance.sh"
run bash "${ENGINE_ROOT}/scripts/verify_portfolio_95.sh"
run bash "${ENGINE_ROOT}/scripts/run_wave_c_scale_campaign.sh"
run bash "${ENGINE_ROOT}/scripts/run_authoring_workflow.sh"
run bash "${ENGINE_ROOT}/scripts/run_compute_stack_campaign.sh"

ENGINE_SHA="$(git -C "${ENGINE_ROOT}" rev-parse HEAD)"
LOCK_SHA="$(shasum -a 256 "${ENGINE_ROOT}/workspace.lock" | awk '{print $1}')"
cat > "${EVIDENCE_DIR}/manifest.json" <<EOF
{
  "schema": "kairo.flagship.evidence.v1",
  "engine_sha": "${ENGINE_SHA}",
  "workspace_lock_sha256": "${LOCK_SHA}",
  "portfolio_acceptance": "PASS",
  "authoring_workflow": "PASS",
  "compute_campaign": "PASS"
}
EOF

echo
echo "KAIRO flagship campaign: PASS"
echo "Evidence: ${EVIDENCE_DIR}/manifest.json"
