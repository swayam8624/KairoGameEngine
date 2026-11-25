#!/usr/bin/env bash
# No GPU, graphics SDK, sibling checkout or CMake build is needed.
# Usage: CXX=clang++ bash scripts/run_craft_cpu_contracts.sh [TestName ...]
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
compiler="${CXX:-clang++}"
command -v "$compiler" >/dev/null || { echo "Compiler not found: $compiler" >&2; exit 2; }
contracts=(
    VoxelInteractionTests VoxelChunkSurfaceTests CraftWorldTests
    VoxelMeshPublicationTests CraftSaveTests VoxelPlayerTests
    CraftStreamingWindowTests CraftStreamSaveTests CraftMeshBatchTests
    CraftInventoryTests CraftCaveWorldTests CraftRecipesHudTests
    CraftHudProjectionTests CraftHudPublicationTests CraftVitalsTests
    CraftMiningTests CraftEquipmentTests CraftEquipmentEnvelopeTests
    CraftMouseLookTests CraftNutritionTests CraftSurfaceSpawnTests
    CraftActionLatchTests CraftSurvivalJourneyTests CraftSaveFileTests CraftDaylightTests CraftTargetFeedbackTests CraftAutosaveTests CraftCreatureTests
)
if (( $# )); then
    requested=("$@")
    for name in "${requested[@]}"; do
        valid=false
        for contract in "${contracts[@]}"; do
            if [[ "$name" == "$contract" ]]; then valid=true; break; fi
        done
        if [[ "$valid" != true ]]; then echo "Unknown CPU contract: $name" >&2; exit 2; fi
    done
    contracts=("${requested[@]}")
fi
for name in "${contracts[@]}"; do
    test -f "$root/Runtime/KairoPlayer/tests/$name.cpp" || { echo "Missing contract: $name" >&2; exit 2; }
done
scratch="$(mktemp -d "${TMPDIR:-/tmp}/kairo-craft-cpu.XXXXXXXX")"
trap 'rm -rf -- "$scratch"' EXIT
export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=1}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=1}"
"$compiler" --version
passed=0
for name in "${contracts[@]}"; do
    printf '\n=== %s ===\n' "$name"
    "$compiler" -std=c++23 -Wall -Wextra -Wpedantic -Werror -g -O1 -pthread \
        -fsanitize=address,undefined -fno-omit-frame-pointer \
        "$root/Runtime/KairoPlayer/tests/$name.cpp" -o "$scratch/$name"
    "$scratch/$name"
    passed=$((passed+1))
done
printf '\nCPU contract acceptance: %d/%d passed (no skipped tests).\n' "$passed" "${#contracts[@]}"
