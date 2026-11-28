#!/usr/bin/env bash
# Launch an authored Kairo project through the real project compiler and player.
# By default, bootstrap the sibling workspace, build/test it, validate the
# project descriptor, and open an interactive player window.
set -euo pipefail

ENGINE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT="Samples/PlayableVerticalSlice/Project.kproject"
PROJECT_SET=0
SKIP_BUILD=0
PLAYER_OPTIONS=()

usage() {
    echo "Usage: $0 [path/to/Project.kproject] [--skip-build] [--renderer auto|metal|vulkan|d3d12|opengl]" >&2
    exit 2
}

while (( $# > 0 )); do
    case "$1" in
        --skip-build)
            SKIP_BUILD=1
            shift
            ;;
        --renderer)
            (( $# >= 2 )) || usage
            case "$2" in
                auto|metal|vulkan|d3d12|opengl) PLAYER_OPTIONS=(--renderer "$2") ;;
                *) usage ;;
            esac
            shift 2
            ;;
        --help|-h)
            usage
            ;;
        -*)
            usage
            ;;
        *)
            if (( PROJECT_SET )); then usage; fi
            PROJECT="$1"
            PROJECT_SET=1
            shift
            ;;
    esac
done

if [[ "$PROJECT" != /* ]]; then
    if [[ -f "$PROJECT" ]]; then
        PROJECT="$(cd "$(dirname "$PROJECT")" && pwd)/$(basename "$PROJECT")"
    else
        PROJECT="$ENGINE_ROOT/$PROJECT"
    fi
fi

if [[ ! -f "$PROJECT" || "$PROJECT" != *.kproject ]]; then
    echo "Kairo project not found or not a .kproject: $PROJECT" >&2
    exit 2
fi

PLAYER="$ENGINE_ROOT/build/dev-clang/Runtime/KairoPlayer/KairoPlayer"
COMPILER="$ENGINE_ROOT/build/dev-clang/components/KairoEditor/KairoProjectCompiler"

if (( ! SKIP_BUILD )); then
    echo "Bootstrapping missing sibling repositories without changing existing work..."
    bash "$ENGINE_ROOT/scripts/bootstrap_workspace.sh"
    echo "Building and testing the native Kairo engine..."
    bash "$ENGINE_ROOT/scripts/build_and_test.sh"
fi

if [[ ! -x "$PLAYER" || ! -x "$COMPILER" ]]; then
    echo "KairoPlayer or KairoProjectCompiler is missing. Build the engine first." >&2
    echo "Expected player: $PLAYER" >&2
    echo "Expected compiler: $COMPILER" >&2
    exit 3
fi

echo "Validating and compiling: $PROJECT"
bash "$ENGINE_ROOT/scripts/validate_and_run_kairo_project.sh" "$PROJECT" --validate

echo "Starting interactive KairoPlayer: $PROJECT"
exec "$PLAYER" "$PROJECT" "${PLAYER_OPTIONS[@]}"
