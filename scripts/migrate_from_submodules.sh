#!/usr/bin/env bash
set -euo pipefail

# Migrate an old KairoGameEngine clone that still contains nested component
# submodules into the sibling-repository workspace layout.
#
# Safe default: dry-run. Pass --apply only after reviewing the report.
# The script NEVER deletes a nested repository that has uncommitted changes.

APPLY=0
if [[ "\${1:-}" == "--apply" ]]; then
    APPLY=1
elif [[ $# -gt 0 ]]; then
    echo "usage: $0 [--apply]" >&2
    exit 2
fi

ENGINE_ROOT="$(cd "$(dirname "\${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE_ROOT="\${KAIRO_WORKSPACE_ROOT:-$(cd "\${ENGINE_ROOT}/.." && pwd)}"

declare -a COMPONENTS=(
    "Foundation/KairoMath:KairoMath"
    "Foundation/KairoGeometry:KairoGeometry"
    "Foundation/Spatial:KairoSpatial"
    "Foundation/KairoPhysicsMath:KairoPhysicsMath"
    "Foundation/KairoPhysicsEngine:KairoPhysicsEngine"
    "KairoAssets:KairoAssets"
    "KairoAI:KairoAI"
    "KairoECS:KairoECS"
    "KairoEditor:KairoEditor"
    "KairoEngineCore:KairoEngineCore"
    "KairoGPU:KairoGPU"
    "KairoHub:KairoHub"
    "KairoMacPerception:KairoMacPerception"
    "KairoONNX:KairoONNX"
    "KairoRayTracer:KairoRayTracer"
    "KairoReflection:KairoReflection"
    "KairoRenderer:KairoRenderer"
    "KairoSIMD:KairoSIMD"
    "KairoScheduler:KairoScheduler"
    "KairoTransformers:KairoTransformers"
)

echo "KairoGameEngine: \${ENGINE_ROOT}"
echo "Workspace root:  \${WORKSPACE_ROOT}"
echo

failures=0
for mapping in "\${COMPONENTS[@]}"; do
    nested="\${mapping%%:*}"
    sibling="\${mapping#*:}"
    nested_path="\${ENGINE_ROOT}/\${nested}"
    sibling_path="\${WORKSPACE_ROOT}/\${sibling}"

    if [[ ! -d "\${sibling_path}" || ! -f "\${sibling_path}/CMakeLists.txt" ]]; then
        echo "ERROR: missing sibling repository: \${sibling_path}" >&2
        failures=$((failures + 1))
        continue
    fi
    if ! git -C "\${sibling_path}" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        echo "ERROR: sibling is not a Git worktree: \${sibling_path}" >&2
        failures=$((failures + 1))
        continue
    fi

    if [[ -e "\${nested_path}" ]]; then
        if ! git -C "\${nested_path}" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
            echo "ERROR: legacy nested path is not a Git worktree; refusing to remove: \${nested_path}" >&2
            failures=$((failures + 1))
            continue
        fi

        dirty="$(git -C "\${nested_path}" status --porcelain --untracked-files=all)"
        if [[ -n "\${dirty}" ]]; then
            echo "ERROR: nested copy has local changes; preserve/commit them first:" >&2
            echo "       \${nested_path}" >&2
            echo "\${dirty}" | sed 's/^/       /' >&2
            failures=$((failures + 1))
            continue
        fi

        nested_head="$(git -C "\${nested_path}" rev-parse HEAD)"
        sibling_head="$(git -C "\${sibling_path}" rev-parse HEAD)"
        echo "legacy \${nested}"
        echo "  nested HEAD : \${nested_head}"
        echo "  sibling HEAD: \${sibling_head}"

        if ! git -C "\${sibling_path}" cat-file -e "\${nested_head}^{commit}" 2>/dev/null; then
            echo "  NOTE: sibling has not fetched nested HEAD; run 'git -C \"\${sibling_path}\" fetch --all --tags' if you need that historical revision."
        fi

        if [[ \${APPLY} -eq 1 ]]; then
            rm -rf "\${nested_path}"
            echo "  removed nested copy"
        else
            echo "  would remove nested copy"
        fi
    fi
done

if [[ \${failures} -ne 0 ]]; then
    echo
    echo "Migration stopped: \${failures} safety check(s) failed." >&2
    echo "No unsafe path was deleted. Resolve the items above and rerun." >&2
    exit 1
fi

echo
if [[ \${APPLY} -eq 0 ]]; then
    echo "Dry-run passed."
    echo "Run again with:"
    echo "  bash scripts/migrate_from_submodules.sh --apply"
    exit 0
fi

rm -rf "\${ENGINE_ROOT}/.git/modules/Foundation" \
       "\${ENGINE_ROOT}/.git/modules/KairoAssets" \
       "\${ENGINE_ROOT}/.git/modules/KairoAI" \
       "\${ENGINE_ROOT}/.git/modules/KairoECS" \
       "\${ENGINE_ROOT}/.git/modules/KairoEditor" \
       "\${ENGINE_ROOT}/.git/modules/KairoEngineCore" \
       "\${ENGINE_ROOT}/.git/modules/KairoGPU" \
       "\${ENGINE_ROOT}/.git/modules/KairoHub" \
       "\${ENGINE_ROOT}/.git/modules/KairoMacPerception" \
       "\${ENGINE_ROOT}/.git/modules/KairoONNX" \
       "\${ENGINE_ROOT}/.git/modules/KairoRayTracer" \
       "\${ENGINE_ROOT}/.git/modules/KairoReflection" \
       "\${ENGINE_ROOT}/.git/modules/KairoRenderer" \
       "\${ENGINE_ROOT}/.git/modules/KairoSIMD" \
       "\${ENGINE_ROOT}/.git/modules/KairoScheduler" \
       "\${ENGINE_ROOT}/.git/modules/KairoTransformers" 2>/dev/null || true

rmdir "\${ENGINE_ROOT}/Foundation" 2>/dev/null || true

# CMake caches absolute source directories from the old nested layout.
rm -rf "\${ENGINE_ROOT}/build"

echo
echo "Migration complete. There is now one physical checkout per Kairo repository."
echo
echo "Reconfigure from a clean cache:"
echo "  cd \"\${ENGINE_ROOT}\""
echo "  cmake --preset dev-clang"
echo "  cmake --build --preset dev-clang --parallel"
echo "  ctest --preset dev-clang --output-on-failure"
