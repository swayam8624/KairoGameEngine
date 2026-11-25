# Procedural terrain viewport (review candidate)

This standalone demo is deliberately opt-in until the EngineCore procedural
stack (#26–30) is reviewed and integrated. The normal KairoPlayer and release
workspace lock remain unchanged.

For a disposable review workspace, materialize the game workspace lock, then
check out EngineCore candidate `ab7aac6b4df24d8bcdaf5aa8508c9ada296bef2e` in the
sibling KairoEngineCore repository. Do not switch a dirty working tree or
overwrite an accepted lock. The candidate CI does this in a clean runner.

```sh
cmake --preset game-player-clang -DKAIRO_PLAYER_BUILD_PROCEDURAL_TERRAIN=ON -DKAIRO_GAME_ENGINE_BUILD_PHASE1_SAMPLE=OFF
cmake --build --preset game-player-clang --parallel 4 --target KairoTerrainDemo
./build/game-player-clang/Runtime/KairoPlayer/KairoTerrainDemo --seed 42 --renderer auto
```

Use `--smoke --renderer vulkan` for the bounded three-region travel regression.
It requires three nonuniform captures, actual mesh loads and unloads, matching
resident/committed counts and successful shutdown. The candidate CI executes
it directly under Lavapipe/Xvfb and fails on detected validation errors.

The demo is an automatic camera circuit, not a controllable game or physics
test. By default it generates meshes synchronously. Add `--async` for a bounded
CPU worker (eight pending meshes, four completion slots consumed per update).
Use `--workers 4` to enable parallel generation; queued obsolete jobs can be cancelled while running jobs retain their slots. Only the render thread uploads/destroys native meshes. Obsolete completions
outside the keep region are discarded and acknowledged. The async smoke waits
for three settled regions before capturing, with a 60-second deadline.
Both paths directly upload canonical artifacts,
and releases native handles only after removing terrain draws from the submitted
scene. Individual failed retirements retain handles for retry. All requests
are acknowledged, including failures. Shutdown is terminal and retryable;
the planner's reservation snapshot is no longer an active runtime after shutdown.

The demo now chooses a cell-aligned double-precision render origin and rebases its camera and terrain together. `--far-origin` exercises a world centered near (+1e9,-1e9) meters; the smoke compares captures at two render origins for the same world location and requires unchanged residency. The caller must still independently rebase authored objects, general player state and physics. Full floating-origin orchestration, picking terrain, physics heightfield shapes, dynamic job reprioritization and measured GPU memory budgets are not implemented. No hardware performance claims.
