#!/usr/bin/env bash
set -euo pipefail

ENGINE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ENGINE_ROOT}"

CLEAN=0
VERIFY_LOCK=0
for argument in "$@"; do
    case "${argument}" in
        --clean) CLEAN=1 ;;
        --verify-lock) VERIFY_LOCK=1 ;;
        *)
            echo "usage: $0 [--clean] [--verify-lock]" >&2
            exit 2
            ;;
    esac
done

if [[ ${VERIFY_LOCK} -eq 1 ]]; then
    bash scripts/verify_workspace_lock.sh
fi

if [[ ${CLEAN} -eq 1 ]]; then
    rm -rf build/dev-clang
fi

mkdir -p build/dev-clang
BUILD_LOG="${ENGINE_ROOT}/build/dev-clang/integration-build.log"
: > "${BUILD_LOG}"

# Capture configure + compile/link output while preserving the actual pipeline
# status.  A build failure stops here; CTest is never run on missing binaries.
set +e
{
    cmake --preset dev-clang &&
    cmake --build --preset dev-clang --parallel
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
WARNING_LOG="${ENGINE_ROOT}/build/dev-clang/integration-warnings.log"
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

ctest --preset dev-clang --output-on-failure
