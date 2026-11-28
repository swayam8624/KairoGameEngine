#!/usr/bin/env bash
# Fast local iteration: retain the existing CMake/Ninja/module cache.
# Build only the programs needed for ordinary editor/player authoring.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
WORKSPACE="${KAIRO_WORKSPACE_ROOT:-$(cd "$ROOT/.." && pwd)}"
JOBS="${KAIRO_BUILD_JOBS:-4}"
if [[ ! "$JOBS" =~ ^[1-9][0-9]*$ ]]; then
    echo "KAIRO_BUILD_JOBS must be a positive integer." >&2
    exit 2
fi
if [[ "$(uname -s)" == Darwin ]] && command -v brew >/dev/null; then
    LLVM="$(brew --prefix llvm)"
    export PATH="$LLVM/bin:$PATH"
    export KAIRO_CXX_COMPILER="${KAIRO_CXX_COMPILER:-$LLVM/bin/clang++}"
fi
# Do not force materialization or switch compilers over a local edit.
source "$ROOT/scripts/workspace_profiles.sh"
bash scripts/verify_workspace_lock.sh "${kairo_game_repos[@]}"
mkdir -p build/game-clang
LOG="$ROOT/build/game-clang/studio-build.log"
{
    cmake --preset game-clang -DKAIRO_WORKSPACE_ROOT="$WORKSPACE"
    cmake --build --preset game-clang --parallel "$JOBS" --target \
        KairoEditorApp KairoPlayer KairoProjectCompiler KairoMenuDocumentTool
} 2>&1 | tee "$LOG"
echo "Studio programs built. Full CTest acceptance is separate."
echo "Log: $LOG"
