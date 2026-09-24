#!/usr/bin/env bash
set -euo pipefail

# Clone any missing Kairo sibling repositories beside KairoGameEngine.
# Existing repositories are never overwritten or reset.

ENGINE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE_ROOT="${KAIRO_WORKSPACE_ROOT:-$(cd "${ENGINE_ROOT}/.." && pwd)}"

repos=(
  KairoMath KairoGeometry KairoSpatial KairoPhysicsMath KairoPhysicsEngine
  KairoAssets KairoAI KairoECS KairoReflection KairoEngineCore KairoRenderer
  KairoEditor KairoRayTracer KairoGPU KairoSIMD KairoScheduler KairoONNX
  KairoTransformers KairoHub KairoMacPerception
)

for repo in "${repos[@]}"; do
    target="${WORKSPACE_ROOT}/${repo}"
    if [[ -d "${target}/.git" || -f "${target}/.git" ]]; then
        echo "present  ${repo}"
        continue
    fi
    if [[ -e "${target}" ]]; then
        echo "ERROR: ${target} exists but is not a Git checkout; refusing to overwrite." >&2
        exit 1
    fi
    echo "clone    ${repo}"
    git clone "https://github.com/swayam8624/${repo}.git" "${target}"
done

echo
echo "Workspace ready at ${WORKSPACE_ROOT}"
