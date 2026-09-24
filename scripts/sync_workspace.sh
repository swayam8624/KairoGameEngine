#!/usr/bin/env bash
set -euo pipefail

ENGINE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE_ROOT="${KAIRO_WORKSPACE_ROOT:-$(cd "${ENGINE_ROOT}/.." && pwd)}"

repos=(
  KairoMath KairoGeometry KairoSpatial KairoPhysicsMath KairoPhysicsEngine
  KairoAssets KairoAI KairoECS KairoReflection KairoEngineCore KairoRenderer
  KairoEditor KairoRayTracer KairoGPU KairoSIMD KairoScheduler KairoONNX
  KairoTransformers KairoHub KairoMacPerception
)

# Preflight the complete workspace before changing any repository.
for repo in "${repos[@]}"; do
    path="${WORKSPACE_ROOT}/${repo}"
    if [[ ! -e "${path}/.git" ]]; then
        echo "ERROR: missing Git checkout: ${path}" >&2
        exit 1
    fi
    if [[ -n "$(git -C "${path}" status --porcelain --untracked-files=all)" ]]; then
        echo "ERROR: ${repo} has local changes; commit/stash them before workspace sync." >&2
        git -C "${path}" status --short >&2
        exit 1
    fi
    branch="$(git -C "${path}" symbolic-ref --quiet --short HEAD || true)"
    if [[ -z "${branch}" ]]; then
        echo "ERROR: ${repo} is detached; switch to its development branch before syncing." >&2
        exit 1
    fi
    if ! git -C "${path}" rev-parse --abbrev-ref --symbolic-full-name "@{u}" >/dev/null 2>&1; then
        echo "ERROR: ${repo} branch '${branch}' has no upstream." >&2
        exit 1
    fi
done

for repo in "${repos[@]}"; do
    path="${WORKSPACE_ROOT}/${repo}"
    printf '%-24s ' "${repo}"
    git -C "${path}" pull --ff-only
done

echo
echo "All sibling repositories are fast-forward synchronized."
