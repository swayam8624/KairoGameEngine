#!/usr/bin/env bash
set -euo pipefail

ENGINE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE_ROOT="${KAIRO_WORKSPACE_ROOT:-$(cd "${ENGINE_ROOT}/.." && pwd)}"

repos=(
  KairoMath KairoGeometry KairoPhysicsMath KairoPipelineCore KairoBlender
  KairoECS KairoReflection KairoScheduler KairoGPU
  KairoSpatial KairoPhysicsEngine KairoAssets KairoRenderer KairoEngineCore KairoRayTracer
  KairoEditor KairoGameEngine KairoProductionTools KairoHub KairoHoudini KairoMaya KairoNuke
  KairoSIMD KairoONNX KairoTransformers KairoAI KairoMacPerception
)

failures=0
for repo in "${repos[@]}"; do
    if [[ "${repo}" == "KairoGameEngine" ]]; then
        root="${ENGINE_ROOT}"
    else
        root="${WORKSPACE_ROOT}/${repo}"
    fi
    status="${root}/STATUS.yaml"
    if [[ ! -f "${status}" ]]; then
        echo "MISSING  ${repo}: STATUS.yaml" >&2
        failures=$((failures + 1))
        continue
    fi

    score="$(awk -F': *' '$1=="completion_score"{print $2; exit}' "${status}")"
    target="$(awk -F': *' '$1=="target_score"{print $2; exit}' "${status}")"
    gate="$(awk -F': *' '$1=="source_gate"{print $2; exit}' "${status}")"

    if [[ "${score}" != "95" || "${target}" != "95" || "${gate}" != "complete" ]]; then
        echo "FAIL     ${repo}: completion=${score:-?} target=${target:-?} source_gate=${gate:-?}" >&2
        failures=$((failures + 1))
    else
        printf 'OK       %-24s 95%% source-complete\n' "${repo}"
    fi
done

if [[ ${failures} -ne 0 ]]; then
    echo >&2
    echo "Portfolio 95% source gate failed for ${failures} repository/repositories." >&2
    exit 1
fi

echo
echo "KAIRO portfolio source gate: 27/27 repositories at frozen-v1 95%."
echo "Native/exact-head execution remains a separate verification gate."
