#!/usr/bin/env bash
# Reproduce the strict native Metal gameplay gate on an actual Mac.
# Does not clone, checkout or modify source trees: the caller supplies an
# exact, clean sibling workspace, and verification fails before building
# if any required revision differs from workspace.lock.
set -euo pipefail
if [[ "$(uname -s)" != Darwin ]]; then
    echo "This gate requires macOS and the native Metal backend." >&2
    exit 2
fi
if (( $# > 0 )); then
    echo "Usage: bash scripts/run_native_metal_gameplay_acceptance.sh" >&2
    exit 2
fi
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"
repos=(KairoMath KairoGeometry KairoSpatial KairoPhysicsMath KairoPhysicsEngine KairoAssets KairoAI KairoECS KairoReflection KairoEngineCore KairoRenderer KairoEditor)
# Keep evidence separate from the source tree and avoid overwriting logs from
# a previous run. CI may supply an explicit artifact directory.
logdir="${KAIRO_METAL_LOG_DIR:-$root/build/metal-acceptance/$(date -u +%Y%m%dT%H%M%SZ)-$$}"
mkdir -p "$logdir"
logdir="$(cd "$logdir" && pwd)"
trap 'echo "Metal acceptance evidence: $logdir"' EXIT
{
    echo "Engine revision: $(git rev-parse HEAD)"
    echo "Host: $(uname -a)"
    echo "Lock SHA256: $(shasum -a 256 workspace.lock)"
    bash scripts/verify_workspace_lock.sh "${repos[@]}"
} 2>&1 | tee "$logdir/revisions.log"
if [[ -z "${KAIRO_CXX_COMPILER:-}" ]]; then
    command -v brew >/dev/null || { echo "Install Homebrew LLVM or set KAIRO_CXX_COMPILER." >&2; exit 2; }
    export KAIRO_CXX_COMPILER="$(brew --prefix llvm)/bin/clang++"
fi
test -x "$KAIRO_CXX_COMPILER"
"$KAIRO_CXX_COMPILER" --version | tee "$logdir/compiler.log"
cmake --preset game-player-clang \
    -DKAIRO_PLAYER_BUILD_PROCEDURAL_TERRAIN=ON \
    -DKAIRO_GAME_ENGINE_BUILD_PHASE1_SAMPLE=OFF \
    -DCMAKE_BUILD_TYPE=Release 2>&1 | tee "$logdir/configure.log"
targets=(
    KairoCraftMouseLookTests KairoCraftEquipmentEnvelopeTests KairoCraftEquipmentTests KairoCraftMiningTests KairoCraftVitalsTests KairoCraftHudPublicationTests KairoCraftHudProjectionTests KairoCraftRecipesHudTests KairoCraftCaves KairoCraftCaveWorldTests
    KairoCraftStream KairoCraftInventoryTests KairoCraftMeshBatchTests
    KairoCraftStreamSaveTests KairoCraft KairoCraftStreamingWindowTests
    KairoVoxelPlayerTests KairoCraftSaveTests KairoVoxelMeshPublicationTests
    KairoCraftWorldTests KairoVoxelChunkSurfaceTests KairoVoxelInteractionTests
    KairoTerrainDemo KairoTerrainExplorer KairoTerrainWalkerTests
    KairoTerrainNativeStoreTests KairoTerrainGenerationQueueTests
    KairoTerrainRenderOriginTests KairoTerrainErrorProfileTests
    KairoTerrainScreenErrorTests KairoRendererMetalSkinningSmoke
    KairoRendererMetalInstancingSmoke
)
cmake --build --preset game-player-clang --parallel "${KAIRO_BUILD_JOBS:-2}" --target "${targets[@]}" 2>&1 | tee "$logdir/build.log"
build=build/game-player-clang
contracts='^Kairo(Terrain(NativeStore|GenerationQueue|RenderOrigin|ErrorProfile|ScreenError|Walker)|VoxelInteraction|VoxelChunkSurface|CraftWorld|VoxelMeshPublication|CraftSave|VoxelPlayer|CraftStreamingWindow|CraftStreamSave|CraftMeshBatch|CraftInventory|CraftCaveWorld|CraftRecipesHud|CraftHudProjection|CraftHudPublication|CraftVitals|CraftMining|CraftEquipment|CraftEquipmentEnvelope|CraftMouseLook)Tests$'
ctest --test-dir "$build" -R "$contracts" --no-tests=error --output-on-failure 2>&1 | tee "$logdir/contracts.log"
ctest --test-dir "$build" -R '^KairoRendererMetal(Skinning|Instancing)Smoke$' --no-tests=error --output-on-failure 2>&1 | tee "$logdir/renderer.log"
# CTest's skip status is useful in broad CI, but not evidence of native
# presentation. This acceptance command must fail if either test skips.
if grep -Ei 'Skipped|Not Run' "$logdir/renderer.log"; then
    echo "Native Metal renderer presentation unavailable: acceptance incomplete." >&2
    exit 1
fi
player="$build/Runtime/KairoPlayer"
"$player/KairoTerrainDemo" --renderer metal --seed 42 --smoke --workers 4 --screen-px 20 2>&1 | tee "$logdir/terrain.log"
"$player/KairoTerrainExplorer" --renderer metal --seed 42 --smoke 2>&1 | tee "$logdir/explorer.log"
"$player/KairoCraftStream" --renderer metal --seed 42 --survival --workers 4 --uploads 2 --smoke 2>&1 | tee "$logdir/craft.log"
"$player/KairoCraftCaves" --renderer metal --seed 42 --smoke 2>&1 | tee "$logdir/caves.log"
# Verify that the exact dependency snapshot remained intact after the run.
bash scripts/verify_workspace_lock.sh "${repos[@]}" 2>&1 | tee "$logdir/revisions-after.log"
echo "PASS: exact dependency snapshot, macOS contracts, native renderer and gameplay captures." | tee "$logdir/RESULT.txt"
