#!/usr/bin/env bash
set -euo pipefail

ENGINE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE_ROOT="${KAIRO_WORKSPACE_ROOT:-$(cd "${ENGINE_ROOT}/.." && pwd)}"

run() {
    echo
    echo "==> $*"
    "$@"
}

resolve_clang() {
    if [[ -n "${KAIRO_CXX_COMPILER:-}" && -x "${KAIRO_CXX_COMPILER}" ]]; then
        printf '%s\n' "${KAIRO_CXX_COMPILER}"
        return
    fi
    if command -v brew >/dev/null 2>&1; then
        local prefix
        prefix="$(brew --prefix llvm 2>/dev/null || true)"
        if [[ -n "${prefix}" && -x "${prefix}/bin/clang++" ]]; then
            printf '%s\n' "${prefix}/bin/clang++"
            return
        fi
    fi
    command -v clang++ || true
}

cmake_gate() {
    local repo="$1"
    local source="${WORKSPACE_ROOT}/${repo}"
    local build="${source}/build/portfolio-95"
    local compiler="$2"
    if [[ ! -f "${source}/CMakeLists.txt" ]]; then
        echo "SKIP     ${repo}: no CMake project."
        return
    fi
    run cmake -S "${source}" -B "${build}" -G Ninja         -DCMAKE_BUILD_TYPE=Debug         -DCMAKE_CXX_COMPILER="${compiler}"
    run cmake --build "${build}" --parallel
    run ctest --test-dir "${build}" --output-on-failure
}

run bash "${ENGINE_ROOT}/scripts/verify_workspace_lock.sh"
run bash "${ENGINE_ROOT}/scripts/build_and_test.sh" --verify-lock

# Prove the supported import/run contract with actual Kairo projects and actual
# built compiler/player binaries. This is not Unity/Unreal migration.
run bash "${ENGINE_ROOT}/scripts/validate_and_run_kairo_project.sh" \
    "KairoEditor/examples/StarterProject/Project.kproject" --validate
run bash "${ENGINE_ROOT}/scripts/validate_and_run_kairo_project.sh" \
    "Samples/SharedContentShowcase/Project.kproject" --smoke
run "${ENGINE_ROOT}/build/dev-clang/Samples/Phase1Game/KairoPhase1Game" --smoke

CXX_COMPILER="$(resolve_clang)"
if [[ -z "${CXX_COMPILER}" ]]; then
    echo "SKIP     Standalone C++ repo gates: no clang++ found."
else
    # These repos are optional or not fully exercised by the default umbrella
    # configuration. Standalone gates prove their own CMake/test contracts.
    for repo in KairoSIMD KairoScheduler KairoGPU KairoONNX KairoTransformers; do
        cmake_gate "${repo}" "${CXX_COMPILER}"
    done
fi

# Host-neutral production-pipeline gates. Treat Python warnings as failures so
# deprecations and resource issues do not become permanent background noise.
run env PYTHONWARNINGS=error PYTHONPATH="${WORKSPACE_ROOT}/KairoPipelineCore/src"     python3 -m unittest discover -s "${WORKSPACE_ROOT}/KairoPipelineCore/tests" -v

for repo in KairoHoudini KairoMaya KairoNuke; do
    run env PYTHONWARNINGS=error         PYTHONPATH="${WORKSPACE_ROOT}/KairoPipelineCore/src:${WORKSPACE_ROOT}/${repo}/python:${WORKSPACE_ROOT}/${repo}/scripts/python"         python3 -m unittest discover -s "${WORKSPACE_ROOT}/${repo}/tests" -v
done

# Blender native gate when the standard macOS installation exists.
BLENDER="/Applications/Blender.app/Contents/MacOS/Blender"
if [[ -x "${BLENDER}" ]]; then
    run "${BLENDER}" --background --factory-startup         --python "${WORKSPACE_ROOT}/KairoBlender/tests/run_blender_tests.py"
else
    echo "SKIP     Blender native gate: ${BLENDER} not installed."
fi

# Hub and Mac perception are independent host stacks and enforce warning-clean
# compilation at their language toolchain boundaries too.
if command -v npm >/dev/null 2>&1 && command -v cargo >/dev/null 2>&1; then
    (cd "${WORKSPACE_ROOT}/KairoHub" && run npm run build)
    run env RUSTFLAGS="-Dwarnings" cargo test         --manifest-path "${WORKSPACE_ROOT}/KairoHub/src-tauri/Cargo.toml"
else
    echo "SKIP     KairoHub native gate: npm and/or cargo unavailable."
fi

if command -v swift >/dev/null 2>&1; then
    (cd "${WORKSPACE_ROOT}/KairoMacPerception" &&         run swift test -Xswiftc -warnings-as-errors)
else
    echo "SKIP     KairoMacPerception gate: Swift unavailable."
fi

EVIDENCE_DIR="${ENGINE_ROOT}/build/portfolio-evidence"
mkdir -p "${EVIDENCE_DIR}"
ENGINE_SHA="$(git -C "${ENGINE_ROOT}" rev-parse HEAD)"
LOCK_SHA256="$(shasum -a 256 "${ENGINE_ROOT}/workspace.lock" | awk '{print $1}')"
HOST_NAME="$(hostname)"
cat > "${EVIDENCE_DIR}/accepted.env" <<EOF
KAIRO_ACCEPTED_ENGINE_SHA='${ENGINE_SHA}'
KAIRO_ACCEPTED_LOCK_SHA256='${LOCK_SHA256}'
KAIRO_ACCEPTED_HOST='${HOST_NAME}'
EOF

echo
echo "KAIRO host acceptance completed for every gate available on this host."
echo "Evidence: ${EVIDENCE_DIR}/accepted.env"
echo "Platform-specific skipped gates remain platform-gated, never inferred as verified."
