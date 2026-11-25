# Procedural terrain viewport

This standalone demo remains opt-in. The proposed workspace lock now pins
the merged EngineCore procedural stack and renderer. Materialize and verify
the game workspace lock in a clean checkout; no review-only sibling override
is needed. Do not overwrite a dirty working tree.

The exact merged combination must pass the native acceptance workflow before
being called validated.

```sh
cmake --preset game-player-clang -DKAIRO_PLAYER_BUILD_PROCEDURAL_TERRAIN=ON -DKAIRO_GAME_ENGINE_BUILD_PHASE1_SAMPLE=OFF
cmake --build --preset game-player-clang --parallel 4 --target KairoTerrainDemo
./build/game-player-clang/Runtime/KairoPlayer/KairoTerrainDemo --seed 42 --renderer auto
```

Use `--smoke --renderer vulkan` for the bounded three-region travel regression.
It requires three nonuniform captures, actual mesh loads and unloads, matching
resident/committed counts and successful shutdown. The locked-workspace CI executes
it directly under Lavapipe/Xvfb and fails on detected validation errors.

The demo is an automatic camera circuit, not a controllable game or physics
test. By default it generates meshes synchronously. Add `--async` for a bounded
CPU worker (eight pending meshes, four completion slots consumed per update).
Use `--workers 4` to enable parallel generation; queued obsolete jobs can be cancelled while running jobs retain their slots. Only the render thread uploads/destroys native meshes. Obsolete completions
outside the keep region are discarded and acknowledged. The async smoke waits
for three settled regions before capturing, with a 60-second deadline.
Both paths directly upload canonical artifacts
and release native handles only after removing terrain draws from the submitted
scene. Individual failed retirements retain handles for retry. All requests
are acknowledged, including failures. Shutdown is terminal and retryable;
the planner's reservation snapshot is no longer an active runtime after shutdown.

The demo now chooses a cell-aligned double-precision render origin and rebases its camera and terrain together. `--far-origin` exercises a world centered near (+1e9,-1e9) meters; the smoke compares captures at two render origins for the same world location and requires unchanged residency. The caller must still independently rebase authored objects, general player state and physics. Full floating-origin orchestration, picking terrain, physics heightfield shapes, dynamic job reprioritization and measured GPU memory budgets are not implemented. No hardware performance claims.

## Error-controlled terrain LOD (review candidate)

`--workers 4 --lod` enables distance/hysteresis-based mixed detail and
resident replacements. `--workers 4 --error-mm 1000` additionally caps the
maximum nominal vertical deviation from the full-resolution heightfield at
1000 mm. An initial worker measures all supported levels once per load;
resident meshes retain their small fixed-size error profile and replacements
reuse it. Unloading a cell discards its profile. The bound does not cover
GPU float rounding, shading or projected screen-space error. The LOD stack
requires the merged EngineCore #31–33 revisions to be pinned before acceptance;
the temporary native CI candidate override is only for review.

## Perspective-aware terrain and playable explorer

`KairoTerrainDemo --workers 4 --screen-px 20` uses a perspective camera and
cached vertical errors to estimate projected displacement. Near-plane cells
fall back to full detail. Coarsening has 20% headroom; asynchronously replacing
meshes means a moving camera may transiently exceed the target. The estimate
is not a bound on visibility, shading, GPU rounding or rasterization.

Build and run the separate first-person exploration sample:

```sh
cmake --preset game-player-clang -DKAIRO_PLAYER_BUILD_PROCEDURAL_TERRAIN=ON -DKAIRO_GAME_ENGINE_BUILD_PHASE1_SAMPLE=OFF
cmake --build --preset game-player-clang --parallel 4 --target KairoTerrainExplorer
./build/game-player-clang/Runtime/KairoPlayer/KairoTerrainExplorer --seed 42 --renderer auto
```

WASD moves, arrows look, left Shift sprints, Space jumps, Escape quits. The camera follows
the canonical heightfield while movement is blocked by missing/full-detail
residency or excessive sampled grade. This is a point-grounded walking sample,
not a physical character controller. Jumping uses ballistic vertical motion and sampled terrain landing. No capsule collision, inventory,
object interaction, or multiplayer yet. Native smoke runs scripted travel and
three captures. Build instructions require the final merged LOD dependency pin;
review CI uses the explicit disposable EngineCore candidate.

## Craft prototype foundations (CPU contracts)

`VoxelInteraction.hpp` provides signed unit-grid material queries, bounded
baseline-relative overrides, and first-solid ray traversal. Air is material 0.
Unknown cells stop rays; exact simultaneous-axis crossings have no unique
placement face. This is not yet connected to the explorer's input.

`VoxelChunkSurface.hpp` builds opaque chunk-local greedy quads with a one-cell
halo. Faces between solid voxels are omitted, including across chunks. Adjacent
coplanar faces merge only when material IDs match. Unknown exterior samples
produce temporary faces and set MissingNeighbors; unknown interior samples
reject the build. A native adapter and transactional remesh/invalidation on
block edits are required next. No persistent save format or rendered voxel
game is claimed by these CPU helpers.
