# Game-first scope

## Product

Kairo exists to let people author, test, debug and distribute games. The default engine is a desktop Editor + Player + foundational libraries, not the entire Kairo portfolio. Keep native C++ runtime code that works; select languages per subsystem rather than rewrite for fashion.

## Dependency boundary

The default workspace has twelve siblings: Math, Geometry, Spatial, PhysicsMath, PhysicsEngine, Assets, AI, ECS, Reflection, EngineCore, Renderer and Editor (all prefixed Kairo). AI remains a sibling because it contains the deterministic GameplayAI module used by navigation, entity lifecycle and world persistence. The companion KairoAI PR #6 splits that module into Kairo::GameplayAI; the Player prefers this target while the Editor retains the aggregate assistant API. Older AI checkouts still work in normal builds via the compatibility link. Strict player-only builds reject those older checkouts.

The other fourteen siblings are opt-in: RayTracer, GPU, SIMD, Scheduler, ONNX, Transformers, Hub, MacPerception, PipelineCore, Blender, ProductionTools, Houdini, Maya and Nuke. Their repositories and lock entries are preserved. Hub may become a useful onboarding tool later; no DCC host or ML framework should be required to make a game. The native racing sample, offline renderer, physics sandboxes, migration framework and experimental compute build remain explicit opt-ins. Keep Phase1Game and shared content as regression fixtures.

The game-clang preset has its own build directory. Legacy full developer and portfolio scripts remain separate. Release builds default to the narrower feature set. Existing local checkouts are not deleted, reset or moved.

## Player-only build

With a KairoAI checkout containing the gameplay split (see [KairoAI PR #6](https://github.com/swayam8624/KairoAI/pull/6)), configure:

```bash
cmake --preset game-player-clang
cmake --build --preset game-player-clang --parallel 4
ctest --preset game-player-clang --no-tests=error
```

This profile does not configure the Editor, assistant, device agent, cloud provider, migration library, offline renderer, or standalone scale benchmark. The native Player and Phase1 gameplay fixture remain. An old KairoAI checkout fails the strict configuration rather than silently linking assistant code. The historical workspace lock has not been updated; do not represent this new combination as an accepted exact-head snapshot.

## Capability roadmap (not claims of existing completeness)

| Gate | Deliverable | Acceptance |
| --- | --- | --- |
| G0 | Stable game-first build | Clean bootstrap, compile, CTest, native frame and package checks |
| G1 | Beginner creation loop | New project, import assets, edit scene, play, debug, undo, save, package without code |
| G2 | Reusable gameplay | Input, audio, UI, animation, composition, scripting and state persistence |
| G3 | Seeded worlds | Versioned deterministic seed, chunk-addressed generation, bounded streaming, persistent edits; voxel and space templates |
| G4 | Open worlds and RPGs | Terrain, LOD, navigation, inventory, dialogue, quests and persistent state as composable systems |
| G5 | Multiplayer | Authoritative simulation, replication, interpolation, reconciliation, interest management and dedicated-server packaging |
| G6 | Release readiness | Installable desktop packages, crash diagnostics, profiling, accessibility and reproducible sample games |

Existing runtime code and tests for world streaming, saves, navigation, character movement, audio bridges and gameplay are retained and must be audited before replacing them. A registered test or source file does not establish native execution or usability.

## Acceptance policy

Do not use a portfolio-wide percentage as product readiness evidence. A gate requires recorded exact-revision results and explicit host/backend coverage. A skipped native test is not a pass. Start with Metal on the primary macOS development host; preserve existing Vulkan, OpenGL and D3D12 backends pending native parity evidence. Do not remove their source blindly.

Do not introduce dependencies that make a genre-specific template mandatory for every game. Prefer a small set of reusable primitives and optional modules; choose scripting and editor technology based on iteration speed, safety and interoperability.

## Commands

- `bash scripts/bootstrap_workspace.sh` — core dependencies only
- `bash scripts/build_and_test.sh` — game-first build and tests
- `bash scripts/build_and_test.sh --verify-lock` — validate game dependency revisions only
- `bash scripts/sync_workspace.sh` — fast-forward game dependencies only
- `bash scripts/bootstrap_workspace.sh --portfolio` — explicitly fetch every historical sibling
- `bash scripts/run_portfolio_acceptance.sh` — historical portfolio campaign, not the game release gate

`workspace.lock` remains the historical full snapshot. Bootstrap clones default branches; lock verification can fail if they differ from recorded revisions. No script silently resets an existing checkout.
