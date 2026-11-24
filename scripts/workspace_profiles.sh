#!/usr/bin/env bash
# Source this file, do not execute it. Keep game dependencies aligned with the
# required targets in CMakeLists.txt; AI remains a real Editor/Player dependency.
kairo_game_repos=(
  KairoMath KairoGeometry KairoSpatial KairoPhysicsMath KairoPhysicsEngine
  KairoAssets KairoAI KairoECS KairoReflection KairoEngineCore KairoRenderer KairoEditor
)
kairo_optional_repos=(
  KairoRayTracer KairoGPU KairoSIMD KairoScheduler KairoONNX KairoTransformers
  KairoHub KairoMacPerception KairoPipelineCore KairoBlender KairoProductionTools
  KairoHoudini KairoMaya KairoNuke
)
kairo_select_workspace_profile() {
    repos=("${kairo_game_repos[@]}")
    if (( $# == 0 )); then
        return 0
    fi
    if (( $# == 1 )) && [[ "$1" == "--portfolio" ]]; then
        repos+=("${kairo_optional_repos[@]}")
        return 0
    fi
    echo "usage: $0 [--portfolio]" >&2
    return 2
}
