# KAIRO — integrated game engine reading order

KairoGameEngine is the **workspace superbuild and application integration layer**. Core implementation lives in separately versioned sibling repositories. The right order for learning is to understand one vertical slice from `.kproject` → scene → content → renderer/physics → Player/Editor → packaged game, then investigate backends individually. The [source catalog](docs/SOURCE_CATALOG.md) links every source/build file tracked inside this integration repo.

## First principle — how repositories work together

```text
KairoMath → KairoGeometry → KairoSpatial
      ↘         ↓             ↘
       KairoPhysicsMath → KairoPhysicsEngine
KairoAssets → KairoEngineCore (project, scene, persistence, input)
                  ↓                   ↓
         KairoRenderer  ←  KairoRealtimeRenderBridge
                  ↑                   ↑
              KairoEditor        KairoPlayer
                  ↑                   ↑
               KairoHub         Phase1Game sample
```

**Persistent data** (asset IDs, paths, project descriptors, scene components) belongs to KairoAssets/EngineCore; **ephemeral GPU resources** belong to the renderer/Player; **simulation state** belongs to PhysicsEngine and runtime bridges; **editor selection/undo** does not belong in a shipped runtime. The cross-repo head versions are pinned by [`workspace.lock`](workspace.lock), and they can differ from each sibling's latest main branch.

## Stage 0 — scope and acceptance evidence

1. [`README.md`](README.md): architecture and sibling checkout workflow.
2. [`STATUS.md`](STATUS.md), [`STATUS.yaml`](STATUS.yaml), [`docs/V1_95_COMPLETION.md`](docs/V1_95_COMPLETION.md): **95% is a target, not a verified score.** The project explicitly retracts an earlier completion claim.
3. [`workspace.lock`](workspace.lock): exact revision pins of the Kairo siblings; never assume a standalone green build proves these pins compose.
4. [`CMakeLists.txt`](CMakeLists.txt) and [`CMakePresets.json`](CMakePresets.json): superbuild targets and platform/compiler configuration.
5. [`scripts/run_portfolio_acceptance.sh`](scripts/run_portfolio_acceptance.sh), [`scripts/verify_portfolio_95.sh`](scripts/verify_portfolio_95.sh), [`scripts/audit_ecosystem_ci.py`](scripts/audit_ecosystem_ci.py): the difference between observed CI runs and **strict exact-head integrated acceptance**. A weekly non-strict audit is not itself proof that all 27 repos passed.

## Stage 1 — portable foundations in sibling repositories

Study the following sibling public contracts before entering Player files:

6. [KairoMath](https://github.com/swayam8624/KairoMath), [KairoGeometry](https://github.com/swayam8624/KairoGeometry), [KairoSpatial](https://github.com/swayam8624/KairoSpatial): vectors/transform conventions, geometry, acceleration structures and spatial queries.
7. [KairoAssets](https://github.com/swayam8624/KairoAssets), [KairoEngineCore](https://github.com/swayam8624/KairoEngineCore), [KairoECS](https://github.com/swayam8624/KairoECS): stable identity, versioned project/scene data, runtime component storage and serialization. A persistent `AssetID` is not a GPU buffer handle.
8. [KairoRenderer](https://github.com/swayam8624/KairoRenderer), [KairoPhysicsEngine](https://github.com/swayam8624/KairoPhysicsEngine): rendering backends and fixed-step rigid-body dynamics. Each has host-specific native validation gates.
9. [KairoEditor](https://github.com/swayam8624/KairoEditor), [KairoHub](https://github.com/swayam8624/KairoHub): document authoring, UI/session state, and project launching. The Hub is not the engine renderer.

## Stage 2 — project, rendering and physics integration

10. [`Runtime/KairoPlayer/RuntimeProject.cppm`](Runtime/KairoPlayer/RuntimeProject.cppm): load/validate `.kproject`, manifest and startup scene; reject unsafe relative paths.
11. [`Runtime/KairoRealtimeRenderBridge/RealtimeSceneBridge.cppm`](Runtime/KairoRealtimeRenderBridge/RealtimeSceneBridge.cppm): shared Editor/Player adaptation of portable meshes/materials/textures to renderer data. Follow asset ID → imported artifact → mesh/material resource.
12. [`Runtime/KairoPlayer/RuntimeRenderBridge.cppm`](Runtime/KairoPlayer/RuntimeRenderBridge.cppm): Player-owned mapping to GPU handles, scene draw extraction and resource lifetimes.
13. [`Runtime/KairoPlayer/RuntimePhysicsBridge.cppm`](Runtime/KairoPlayer/RuntimePhysicsBridge.cppm): instantiate an isolated physics world from scene descriptors, accumulate bounded fixed steps and return poses for interpolation.
14. [`Runtime/KairoPlayer/RuntimeInputBridge.cppm`](Runtime/KairoPlayer/RuntimeInputBridge.cppm), [`Runtime/KairoPlayer/RuntimeLogicBridge.cppm`](Runtime/KairoPlayer/RuntimeLogicBridge.cppm): input command routing and authored gameplay execution. The game's authored logic is not part of the renderer.
15. [`Runtime/KairoPlayer/RuntimeAudioBridge.cppm`](Runtime/KairoPlayer/RuntimeAudioBridge.cppm), [`Runtime/KairoPlayer/RuntimeSaveGameBridge.cppm`](Runtime/KairoPlayer/RuntimeSaveGameBridge.cppm), [`Runtime/KairoPlayer/RuntimeWorldStreamingBridge.cppm`](Runtime/KairoPlayer/RuntimeWorldStreamingBridge.cppm): auxiliary runtime services, with independent lifetime/serialization concerns.

## Stage 3 — executable and authoring verification

16. [`Runtime/KairoPlayer/main.cpp`](Runtime/KairoPlayer/main.cpp): CLI selection for validate, smoke, package or full runtime. Follow each imported `Kairo.Player.*` module and identify which mode can create a window or write files.
17. [`Samples/Phase1Game/Phase1GameState.cppm`](Samples/Phase1Game/Phase1GameState.cppm) and [`Samples/Phase1Game/main.cpp`](Samples/Phase1Game/main.cpp): end-to-end gameplay fixture with input, hazards/collectibles, physics, render and save/load.
18. [`Samples/Phase1Game/tests/Phase1GameStateTests.cpp`](Samples/Phase1Game/tests/Phase1GameStateTests.cpp), [`tests/SharedContentContractTests.cpp`](tests/SharedContentContractTests.cpp), and [`Runtime/KairoRealtimeRenderBridge/tests/RealtimeSceneBridgeTests.cpp`](Runtime/KairoRealtimeRenderBridge/tests/RealtimeSceneBridgeTests.cpp): verify the stated contracts rather than only admiring the rendering sample.
19. [`Runtime/KairoPlayer/RuntimePackaging.cppm`](Runtime/KairoPlayer/RuntimePackaging.cppm): host-specific staged package, atomic replacement, project validation, file trust and platform dependency limits.

## Stage 4 — optional migration and production tooling

20. [`Migration/KairoBridge/CanonicalIR.cppm`](Migration/KairoBridge/CanonicalIR.cppm), [`Migration/KairoBridge/GodotAdapter.cppm`](Migration/KairoBridge/GodotAdapter.cppm), [`Migration/KairoBridge/UnityAdapter.cppm`](Migration/KairoBridge/UnityAdapter.cppm): diagnostic conversion of source representations. **Unity/Unreal/Godot source projects are not magically runnable by Kairo.** Review conversion boundaries in README before claiming compatibility.
21. [`Samples/KairoRacingGame/main.cpp`](Samples/KairoRacingGame/main.cpp) and [`Samples/KairoRacingGame/tools/export_glb.py`](Samples/KairoRacingGame/tools/export_glb.py): extra showcase and asset conversion, not a substitute for exact-head end-to-end acceptance.

## APIs worth understanding deeply

| API/component | Used by | Contract / common mistake |
|---|---|---|
| `.kproject` (EngineCore) | Hub, Editor, Player | Versioned persistent startup/project configuration, not a running scene |
| `Kairo.Assets` | Renderer bridge, EngineCore | Portable identities/content and validation, not GPU handles |
| `Kairo.Runtime.RealtimeSceneBridge` | Editor/Player | Common authored-content to rendering adaptation |
| `Kairo.Player.RuntimeRenderBridge` | Player | Process-local GPU resources and draw submission |
| `Kairo.Player.RuntimePhysicsBridge` | Player / sample | Scene-to-physics binding, fixed timestep, interpolated poses |
| `Kairo.Player.RuntimeProject` | Player / packaging | Reads and validates project bootstrap |
| `Kairo.Player.RuntimePackaging` | CLI, Hub | Builds relocatable **host** runtime bundle; not universal installers |
| `workspace.lock` | Superbuild / acceptance | Exact sibling commit pins; must reconcile with newer main-branch fixes |

## Learning tasks

Take one `Phase1Game` scene entity from its stable asset ID and authored transform into a GPU mesh handle and physics rigid body. Change its position: which system stores the source state, which writes the physics result, and who reads its interpolated pose for rendering? Next, validate `--smoke` on each available backend and compare frame readbacks **only after** the exact locked revisions pass the proper native tests.

Kairo's integration README currently states that all 27 repo completion scores are **unverified** until accepted exact-head evidence. Preserve this boundary in any educational material. The root [MIT license](LICENSE) covers the original code as stated there; assets, imported scenes, third-party dependencies and sibling repos may have their own licensing terms.
