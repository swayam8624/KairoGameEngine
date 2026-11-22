#!/usr/bin/env bash
# Fast, GPU-free build and execution of seven exact checked-in KairoPlayer,
# KairoRenderer and shared asset-bridge C++23 regression programs. No CMake superbuild, GLFW, Vulkan loader, GPU
# device, shader tools or C++23 module dependency scanner is needed.
#
# Usage (KairoGameEngine and KairoRenderer are sibling repositories):
#   bash scripts/run_renderer_resource_lifetime_regressions.sh
#   CXX=clang++ bash scripts/run_renderer_resource_lifetime_regressions.sh
#   KAIRO_RENDERER_ROOT=/path/to/KairoRenderer bash scripts/run_renderer_resource_lifetime_regressions.sh
#
# Why this exists: native GPU error-path tests depend on real SDK/driver
# availability, but exception-safety of the C++ ownership journal and
# backend map-publication helper can be verified independently first.

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
game_root="$(cd -- "$script_dir/.." && pwd)"
renderer_root="${KAIRO_RENDERER_ROOT:-"$game_root/../KairoRenderer"}"
cxx="${CXX:-c++}"

player_test="$game_root/Runtime/KairoPlayer/tests/RuntimeRenderResourceTrackingTests.cpp"
renderer_test="$renderer_root/tests/NativeResourcePublicationTests.cpp"
frame_test="$renderer_root/tests/VulkanFrameRecoveryJournalTests.cpp"
transaction_test="$renderer_root/tests/TransactionalResourceGenerationTests.cpp"
texture_test="$renderer_root/tests/TextureMipUploadPlanTests.cpp"
upload_retirement_test="$renderer_root/tests/VulkanUploadRetirementPolicyTests.cpp"
material_test="$game_root/Runtime/KairoRealtimeRenderBridge/tests/MaterialFileSizeBudgetTests.cpp"

if ! command -v "$cxx" >/dev/null 2>&1; then
  printf 'Missing C++ compiler: %s\n' "$cxx" >&2
  exit 1
fi

if [[ ! -f "$player_test" || ! -f "$renderer_test" || ! -f "$frame_test" || ! -f "$transaction_test" || ! -f "$texture_test" || ! -f "$upload_retirement_test" || ! -f "$material_test" ]]; then
  printf 'Expected sibling KairoGameEngine and KairoRenderer source trees.\n' >&2
  printf 'Missing CPU regression source:\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n' "$player_test" "$renderer_test" "$frame_test" "$transaction_test" "$texture_test" "$upload_retirement_test" "$material_test" >&2
  exit 1
fi

output_dir="$(mktemp -d "${TMPDIR:-/tmp}/kairo-gpu-lifetime.XXXXXXXX")"
trap 'rm -rf -- "$output_dir"' EXIT

printf '%s\n' '=== Compiler ==='
"$cxx" --version | head -n 1 || true

printf '%s\n' '=== Player handle-adoption and teardown failure injection ==='
"$cxx" -std=c++23 -O0 -g -Wall -Wextra -Werror -pedantic \
  "$player_test" -o "$output_dir/kairo-player-resource-test"
"$output_dir/kairo-player-resource-test"

printf '%s\n' '=== Renderer native-publication rollback failure injection ==='
"$cxx" -std=c++23 -O0 -g -Wall -Wextra -Werror -pedantic \
  "$renderer_test" -o "$output_dir/kairo-renderer-publication-test"
"$output_dir/kairo-renderer-publication-test"

printf '%s\n' '=== Vulkan acquire/record/submit/present failure state machine ==='
"$cxx" -std=c++23 -O0 -g -Wall -Wextra -Werror -pedantic \
  "$frame_test" -o "$output_dir/kairo-vulkan-frame-journal-test"
"$output_dir/kairo-vulkan-frame-journal-test"

printf '%s\n' '=== Transactional Vulkan descriptor generation fault injection ==='
"$cxx" -std=c++23 -O0 -g -Wall -Wextra -Werror -pedantic \
  "$transaction_test" -o "$output_dir/kairo-generation-transaction-test"
"$output_dir/kairo-generation-transaction-test"

printf '%s\n' '=== Validated Vulkan mip staging layout and payload offsets ==='
"$cxx" -std=c++23 -O0 -g -Wall -Wextra -Werror -pedantic \
  "$texture_test" -o "$output_dir/kairo-texture-upload-plan-test"
"$output_dir/kairo-texture-upload-plan-test"

printf '%s\n' '=== Vulkan upload queue completion and loss-state fault injection ==='
"$cxx" -std=c++23 -O0 -g -Wall -Wextra -Werror -pedantic \
  "$upload_retirement_test" -o "$output_dir/kairo-vulkan-upload-retirement-test"
"$output_dir/kairo-vulkan-upload-retirement-test"

printf '%s\n' '=== Bounded material artifact file pre-allocation validation ==='
"$cxx" -std=c++23 -O0 -g -Wall -Wextra -Werror -pedantic \
  "$material_test" -o "$output_dir/kairo-material-budget-test"
"$output_dir/kairo-material-budget-test"

printf '%s\n' 'PASS: all seven checked-in CPU-only C++23 regression programs compiled and ran.'
printf '%s\n' 'NOTE: these checks do not prove native Vulkan/Metal/OpenGL/D3D12 GPU execution.'
