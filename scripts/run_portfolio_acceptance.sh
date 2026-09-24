#!/usr/bin/env bash
set -euo pipefail

ENGINE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE_ROOT="${KAIRO_WORKSPACE_ROOT:-$(cd "${ENGINE_ROOT}/.." && pwd)}"

run() {
    echo
    echo "==> $*"
    "$@"
}

run bash "${ENGINE_ROOT}/scripts/verify_portfolio_95.sh"
run bash "${ENGINE_ROOT}/scripts/verify_workspace_lock.sh"
run bash "${ENGINE_ROOT}/scripts/build_and_test.sh" --verify-lock

# Host-neutral production-pipeline gates.
run env PYTHONPATH="${WORKSPACE_ROOT}/KairoPipelineCore/src"     python3 -m unittest discover -s "${WORKSPACE_ROOT}/KairoPipelineCore/tests" -v

for repo in KairoHoudini KairoMaya KairoNuke; do
    run env PYTHONPATH="${WORKSPACE_ROOT}/KairoPipelineCore/src:${WORKSPACE_ROOT}/${repo}/python:${WORKSPACE_ROOT}/${repo}/scripts/python"         python3 -m unittest discover -s "${WORKSPACE_ROOT}/${repo}/tests" -v
done

# Blender native gate when the standard macOS installation exists.
BLENDER="/Applications/Blender.app/Contents/MacOS/Blender"
if [[ -x "${BLENDER}" ]]; then
    run "${BLENDER}" --background --factory-startup         --python "${WORKSPACE_ROOT}/KairoBlender/tests/run_blender_tests.py"
else
    echo "SKIP     Blender native gate: ${BLENDER} not installed."
fi

# Hub and Mac perception are independent host stacks.
if command -v npm >/dev/null 2>&1 && command -v cargo >/dev/null 2>&1; then
    (cd "${WORKSPACE_ROOT}/KairoHub" && run npm run build)
    run cargo test --manifest-path "${WORKSPACE_ROOT}/KairoHub/src-tauri/Cargo.toml"
else
    echo "SKIP     KairoHub native gate: npm and/or cargo unavailable."
fi

if command -v swift >/dev/null 2>&1; then
    (cd "${WORKSPACE_ROOT}/KairoMacPerception" && run swift test)
else
    echo "SKIP     KairoMacPerception gate: Swift unavailable."
fi

echo
echo "KAIRO portfolio acceptance completed for every gate available on this host."
echo "Platform-specific skipped gates remain platform-gated, never inferred as verified."
