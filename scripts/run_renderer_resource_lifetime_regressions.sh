#!/usr/bin/env bash
# Fast, GPU-free build and execution of the exact KairoPlayer and KairoRenderer
# resource-publication tests. No CMake superbuild, GLFW, Vulkan loader, GPU
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

if ! command -v "$cxx" >/dev/null 2>&1; then
  printf 'Missing C++ compiler: %s\n' "$cxx" >&2
  exit 1
fi

if [[ ! -f "$player_test" || ! -f "$renderer_test" ]]; then
  printf 'Expected sibling KairoGameEngine and KairoRenderer source trees.\n' >&2
  printf 'Missing player or renderer test: %s | %s\n' "$player_test" "$renderer_test" >&2
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

printf '%s\n' 'PASS: both checked-in CPU-only C++23 regression programs compiled and ran.'
printf '%s\n' 'NOTE: these checks do not prove native Vulkan/Metal/OpenGL/D3D12 GPU execution.'
