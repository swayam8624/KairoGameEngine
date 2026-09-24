#!/usr/bin/env bash
set -euo pipefail

ENGINE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE_ROOT="${KAIRO_WORKSPACE_ROOT:-$(cd "${ENGINE_ROOT}/.." && pwd)}"
EVIDENCE_DIR="${ENGINE_ROOT}/build/wave-d-authoring"
mkdir -p "${EVIDENCE_DIR}"

run() {
    echo
    echo "==> $*"
    "$@"
}

run bash "${ENGINE_ROOT}/scripts/verify_workspace_lock.sh"
run cmake --preset dev-clang
run cmake --build --preset dev-clang --parallel

# Exercise the authored-data lifecycle, editor transactions and runtime
# persistence explicitly. A launch-only demo is not sufficient evidence for
# Wave D because save/recovery/package regressions can otherwise hide behind a
# successful executable build.
run ctest --test-dir "${ENGINE_ROOT}/build/dev-clang" \
    -R 'KairoEditorTests|KairoEditorPhase4Tests|KairoEditorDocumentCompilerTests|KairoEditorUITests|KairoEditorSharedContentSmoke|KairoPlayerSaveGameBridgeTests|KairoPlayerWorldSaveBridgeTests|KairoPhase1.Package' \
    --output-on-failure

run bash "${ENGINE_ROOT}/scripts/validate_and_run_kairo_project.sh"     "${WORKSPACE_ROOT}/KairoEditor/examples/StarterProject/Project.kproject" --validate
run bash "${ENGINE_ROOT}/scripts/validate_and_run_kairo_project.sh"     "${ENGINE_ROOT}/Samples/SharedContentShowcase/Project.kproject" --smoke
run "${ENGINE_ROOT}/build/dev-clang/Samples/Phase1Game/KairoPhase1Game" --smoke

if ! command -v npm >/dev/null 2>&1 || ! command -v cargo >/dev/null 2>&1; then
    echo "ERROR: authoring workflow requires npm and cargo for KairoHub." >&2
    exit 4
fi
(
    cd "${WORKSPACE_ROOT}/KairoHub"
    run npm ci --ignore-scripts
    run npm run build
    run env RUSTFLAGS="-Dwarnings" cargo test --locked --manifest-path src-tauri/Cargo.toml
)

IMPORT_TMP="$(mktemp -d "${TMPDIR:-/tmp}/kairo-wave-d-import.XXXXXX")"
trap 'rm -rf "${IMPORT_TMP}"' EXIT
cp "${ENGINE_ROOT}/Samples/SharedContentShowcase/Content/ToyCar/ToyCar.glb"     "${IMPORT_TMP}/ToyCar.glb"

IMPORTED_PROJECT="$(
    cd "${WORKSPACE_ROOT}/KairoHub"
    RUSTFLAGS="-Dwarnings" cargo run --quiet         --manifest-path src-tauri/Cargo.toml         --bin kairo-hub-import --         "${IMPORT_TMP}" "Wave D External Import" "0.1.0" "ToyCar.glb"
)"
if [[ ! -f "${IMPORTED_PROJECT}" ]]; then
    echo "ERROR: Hub import did not produce a Kairo project." >&2
    exit 5
fi
run bash "${ENGINE_ROOT}/scripts/validate_and_run_kairo_project.sh"     "${IMPORTED_PROJECT}" --validate
run bash "${ENGINE_ROOT}/scripts/validate_and_run_kairo_project.sh"     "${IMPORTED_PROJECT}" --smoke

run "${ENGINE_ROOT}/build/dev-clang/Samples/Phase1Game/KairoPhase1Game"     --package Release --replace

HOST_OS="$(uname -s)"
if [[ "${HOST_OS}" == "Darwin" ]]; then
    BLENDER="/Applications/Blender.app/Contents/MacOS/Blender"
    if [[ ! -x "${BLENDER}" ]]; then
        echo "ERROR: Blender is required for the macOS authoring workflow." >&2
        exit 6
    fi
    run "${BLENDER}" --background --factory-startup         --python "${WORKSPACE_ROOT}/KairoBlender/tests/run_blender_tests.py"
fi

ENGINE_SHA="$(git -C "${ENGINE_ROOT}" rev-parse HEAD)"
LOCK_SHA="$(shasum -a 256 "${ENGINE_ROOT}/workspace.lock" | awk '{print $1}')"
cat > "${EVIDENCE_DIR}/accepted.env" <<EOF
KAIRO_WAVE_D_ENGINE_SHA='${ENGINE_SHA}'
KAIRO_WAVE_D_LOCK_SHA256='${LOCK_SHA}'
KAIRO_WAVE_D_EXTERNAL_IMPORT='PASS'
KAIRO_WAVE_D_PACKAGE='PASS'
KAIRO_WAVE_D_HOST_OS='${HOST_OS}'
EOF

echo
echo "KAIRO Wave D authoring workflow: PASS"
echo "Evidence: ${EVIDENCE_DIR}/accepted.env"
