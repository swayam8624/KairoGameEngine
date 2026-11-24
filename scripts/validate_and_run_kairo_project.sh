#!/usr/bin/env bash
set -euo pipefail

ENGINE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT="${1:-}"
MODE="${2:---validate}"
PROFILE="${KAIRO_BUILD_PROFILE:-game-clang}"
case "$PROFILE" in
    game-clang|dev-clang) ;;
    *) echo "Unsupported KAIRO_BUILD_PROFILE: $PROFILE" >&2; exit 2 ;;
esac

if [[ -z "${PROJECT}" ]]; then
    echo "usage: $0 <path/to/Project.kproject> [--validate|--smoke]" >&2
    exit 2
fi
if [[ "${MODE}" != "--validate" && "${MODE}" != "--smoke" ]]; then
    echo "mode must be --validate or --smoke" >&2
    exit 2
fi

if [[ "${PROJECT}" != /* ]]; then
    if [[ -f "${ENGINE_ROOT}/${PROJECT}" ]]; then
        PROJECT="${ENGINE_ROOT}/${PROJECT}"
    elif [[ -f "${ENGINE_ROOT}/../${PROJECT}" ]]; then
        PROJECT="${ENGINE_ROOT}/../${PROJECT}"
    else
        PROJECT="${ENGINE_ROOT}/${PROJECT}"
    fi
fi
if [[ ! -f "${PROJECT}" || "${PROJECT##*.}" != "kproject" ]]; then
    echo "Kairo project descriptor is missing or not a .kproject: ${PROJECT}" >&2
    exit 2
fi

resolve_binary() {
    local first="$1"
    shift
    if [[ -x "${first}" ]]; then
        printf '%s\n' "${first}"
        return 0
    fi
    for candidate in "$@"; do
        if [[ -x "${candidate}" ]]; then
            printf '%s\n' "${candidate}"
            return 0
        fi
    done
    return 1
}

COMPILER="$(resolve_binary     "${ENGINE_ROOT}/build/${PROFILE}/components/KairoEditor/KairoProjectCompiler"     "${ENGINE_ROOT}/build/${PROFILE}/KairoEditor/KairoProjectCompiler")" || {
    echo "Built KairoProjectCompiler not found in current or legacy layout." >&2
    exit 3
}
PLAYER="$(resolve_binary     "${ENGINE_ROOT}/build/${PROFILE}/Runtime/KairoPlayer/KairoPlayer")" || {
    echo "Built KairoPlayer not found." >&2
    exit 3
}

echo "Kairo project gate: ${PROJECT}"
echo "1/2 compile attached project logic"
"${COMPILER}" "${PROJECT}"

echo "2/2 run KairoPlayer ${MODE}"
"${PLAYER}" "${PROJECT}" "${MODE}"

echo "Kairo project gate: PASS"
