#!/usr/bin/env bash
set -euo pipefail

ENGINE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT="${1:-}"
MODE="${2:---validate}"

if [[ -z "${PROJECT}" ]]; then
    echo "usage: $0 <path/to/Project.kproject> [--validate|--smoke]" >&2
    exit 2
fi
if [[ "${MODE}" != "--validate" && "${MODE}" != "--smoke" ]]; then
    echo "mode must be --validate or --smoke" >&2
    exit 2
fi

if [[ "${PROJECT}" != /* ]]; then
    PROJECT="${ENGINE_ROOT}/${PROJECT}"
fi
if [[ ! -f "${PROJECT}" || "${PROJECT##*.}" != "kproject" ]]; then
    echo "Kairo project descriptor is missing or not a .kproject: ${PROJECT}" >&2
    exit 2
fi

COMPILER="${ENGINE_ROOT}/build/dev-clang/KairoEditor/KairoProjectCompiler"
PLAYER="${ENGINE_ROOT}/build/dev-clang/Runtime/KairoPlayer/KairoPlayer"

for binary in "${COMPILER}" "${PLAYER}"; do
    if [[ ! -x "${binary}" ]]; then
        echo "Required built executable is missing: ${binary}" >&2
        exit 3
    fi
done

echo "Kairo project gate: ${PROJECT}"
echo "1/2 compile attached project logic"
"${COMPILER}" "${PROJECT}"

echo "2/2 run KairoPlayer ${MODE}"
"${PLAYER}" "${PROJECT}" "${MODE}"

echo "Kairo project gate: PASS"
