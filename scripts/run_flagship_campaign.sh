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
HOST_OS="$(uname -s)"
WAVE_C_EVIDENCE="${ENGINE_ROOT}/build/wave-c-scale/summary.json"
WAVE_D_EVIDENCE="${ENGINE_ROOT}/build/wave-d-authoring/accepted.env"
WAVE_E_EVIDENCE="${ENGINE_ROOT}/build/wave-e-compute/summary.json"

for evidence in "${WAVE_C_EVIDENCE}" "${WAVE_D_EVIDENCE}" "${WAVE_E_EVIDENCE}"; do
    if [[ ! -f "${evidence}" ]]; then
        echo "ERROR: flagship campaign is missing evidence file: ${evidence}" >&2
        exit 7
    fi
done

WAVE_C_SHA="$(shasum -a 256 "${WAVE_C_EVIDENCE}" | awk '{print $1}')"
WAVE_D_SHA="$(shasum -a 256 "${WAVE_D_EVIDENCE}" | awk '{print $1}')"
WAVE_E_SHA="$(shasum -a 256 "${WAVE_E_EVIDENCE}" | awk '{print $1}')"

cat > "${EVIDENCE_DIR}/manifest.json" <<EOF
{
  "schema": "kairo.flagship.evidence.v2",
  "engine_sha": "${ENGINE_SHA}",
  "workspace_lock_sha256": "${LOCK_SHA}",
  "host_os": "${HOST_OS}",
  "portfolio_acceptance": "PASS",
  "wave_c_scale": {
    "status": "PASS",
    "evidence": "build/wave-c-scale/summary.json",
    "sha256": "${WAVE_C_SHA}"
  },
  "wave_d_authoring": {
    "status": "PASS",
    "evidence": "build/wave-d-authoring/accepted.env",
    "sha256": "${WAVE_D_SHA}"
  },
  "wave_e_compute": {
    "status": "PASS",
    "evidence": "build/wave-e-compute/summary.json",
    "sha256": "${WAVE_E_SHA}"
  }
}
EOF

echo
echo "KAIRO flagship campaign: PASS"
echo "Evidence: ${EVIDENCE_DIR}/manifest.json"
