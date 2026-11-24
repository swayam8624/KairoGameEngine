#!/usr/bin/env bash
set -euo pipefail

ENGINE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ENGINE_ROOT}"

CLEAN=0
VERIFY_LOCK=0
PROFILE=game-clang
for argument in "$@"; do
    case "${argument}" in
        --clean) CLEAN=1 ;;
        --verify-lock) VERIFY_LOCK=1 ;;
        --portfolio) PROFILE=dev-clang ;;
        *)
            echo "usage: $0 [--clean] [--verify-lock] [--portfolio]" >&2
            exit 2
            ;;
    esac
done

if [[ ${VERIFY_LOCK} -eq 1 ]]; then
    if [[ "$PROFILE" == "dev-clang" ]]; then
        bash scripts/verify_workspace_lock.sh
    else
        source "${ENGINE_ROOT}/scripts/workspace_profiles.sh"
        bash scripts/verify_workspace_lock.sh "${kairo_game_repos[@]}"
    fi
fi

if [[ ${CLEAN} -eq 1 ]]; then
    rm -rf build/${PROFILE}
fi

mkdir -p build/${PROFILE}
BUILD_LOG="${ENGINE_ROOT}/build/${PROFILE}/integration-build.log"
: > "${BUILD_LOG}"

# Capture configure + compile/link output while preserving the actual pipeline
# status.  A build failure stops here; CTest is never run on missing binaries.
set +e
{
    cmake --preset "${PROFILE}" &&
    cmake --build --preset "${PROFILE}" --parallel
} 2>&1 | tee "${BUILD_LOG}"
BUILD_STATUS=${PIPESTATUS[0]}
set -e

if [[ ${BUILD_STATUS} -ne 0 ]]; then
    echo >&2
    echo "KAIRO BUILD FAILED (exit ${BUILD_STATUS}). Tests were not started." >&2
    echo "Full log: ${BUILD_LOG}" >&2
    exit "${BUILD_STATUS}"
fi

# Zero-warning integration policy. Kairo-owned targets use warnings-as-errors,
# while this gate also catches linker/toolchain/third-party diagnostics that
# compiler target properties cannot see.
WARNING_LOG="${ENGINE_ROOT}/build/${PROFILE}/integration-warnings.log"
grep -E -i     '(^|[[:space:]])(ld: )?warning:|CMake (Deprecation )?Warning'     "${BUILD_LOG}" > "${WARNING_LOG}" || true

if [[ -s "${WARNING_LOG}" ]]; then
    echo >&2
    echo "KAIRO ZERO-WARNING GATE FAILED." >&2
    echo "A successful integration build emitted warning diagnostics:" >&2
    cat "${WARNING_LOG}" >&2
    echo >&2
    echo "Full log: ${BUILD_LOG}" >&2
    exit 3
fi

rm -f "${WARNING_LOG}"
echo
echo "KAIRO zero-warning build gate: PASS"

ctest --preset "${PROFILE}" --output-on-failure
