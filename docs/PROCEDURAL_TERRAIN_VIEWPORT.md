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

## Kairo Craft: bounded creative prototype (review candidate)

Build with the same opt-in procedural terrain preset, then build target
`KairoCraft`. Run:

```sh
./build/game-player-clang/Runtime/KairoPlayer/KairoCraft --seed 42 --renderer auto --save craft.kworld
````

WASD moves in creative flight, arrows look, Space ascends, Left Ctrl descends,
left mouse removes a block, right mouse places a colored block, F5 saves to
the explicit path, and Escape exits. An existing save supplies the seed and
edits on startup. No automatic writes; use F5. The finite 48 x 16 x 48 world
uses nine 16-cubed chunks, grass/dirt/stone and placed-block materials. Edits
rebuild only the owning chunk and affected face neighbors, and occupancy is
committed only after the candidate render scene is successfully submitted.
The save is versioned and checksummed, with bounded signed-coordinate edits.
The file replacement assumes one writer and is not a power-loss durability
guarantee. The demo currently uses creative flight, not voxel body physics;
no infinite streaming, inventory, texture atlas or multiplayer yet.
Native acceptance is pending. Do not treat the candidate dependency override
as an accepted release lock.

## Survival and streamed Craft (review candidates)

`KairoCraft --survival --seed 42 --renderer auto --save craft.kworld`
uses an upright AABB player body, gravity, Space jump, wall sliding, and
Shift sprint. The finite sandbox has invisible boundary containment and a
solid floor beneath the editable world. Placing blocks inside the body is
rejected. Creative flight remains the default.

```sh
cmake --build --preset game-player-clang --parallel 4 --target KairoCraftStream
./build/game-player-clang/Runtime/KairoPlayer/KairoCraftStream --seed 42 --renderer auto
```

The separate streamed prototype uses a 5 x 5 window of 16-cubed chunks.
Overlapping chunks retain their native slots; new chunks are generated and
uploaded synchronously, then published together. Cell-relative render
coordinates keep geometry near the camera even far from world origin.
Block edits remain in the bounded in-memory journal when a chunk is evicted.
WASD/arrow/Space/Ctrl/mouse controls match creative mode. The streamed sample
has one vertical band, no streamed-world disk persistence or survival body
integration yet. Native acceptance is pending; do not infer frame latency or
hardware performance from planned CI cases.

## Streamed survival, background meshing, and inventory

```sh
./build/game-player-clang/Runtime/KairoPlayer/KairoCraftStream --seed 42 --renderer auto --survival --workers 4 --save craft-stream.kworld
```

WASD walks, arrows look, Space jumps, Shift sprints, mouse buttons mine/place,
1–4 select grass/dirt/stone/building material, F5 saves and Esc exits. Counts
are shown in the window title; mining credits the material only after a
successful edit, and placing consumes it only after native publication.
Creative mode remains unrestricted and is not an anti-cheat boundary.

Workers produce dirty chunk meshes against an immutable occupancy snapshot.
The render thread polls nonblocking and publishes the complete next window;
obsolete queued jobs are cancelled, stale completions discarded. Existing
meshes stay visible until the replacement window is ready. Native uploads
and edit remeshes remain synchronous. Startup and regression settle with a
bounded timeout. This is not a measured frame-time improvement.

Streamed saves have distinct v2 world and v3 session formats. V3 includes
inventory, selected material, and the v2 world payload. V2 loads with empty
inventory; finite v1 is intentionally rejected. F5 writes through a temporary
file and same-directory rename; no concurrent-writer or power-loss guarantee.
No player pose persistence, recipes, multiple vertical chunk bands, textured
HUD or multiplayer yet. Native validation of these latest modes is pending.
