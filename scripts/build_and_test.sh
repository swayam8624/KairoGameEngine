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

cmake --preset dev-clang
cmake --build --preset dev-clang --parallel
ctest --preset dev-clang --output-on-failure
