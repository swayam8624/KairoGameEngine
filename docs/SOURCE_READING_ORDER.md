# Kairo workspace — source reading order

For a complete narrative of the runtime's startup, fixed timestep, identity domains, rendering and save-game boundaries, read [PLAYER_EXECUTION_EXPLAINED.md](PLAYER_EXECUTION_EXPLAINED.md) after this guide.

This is a **concept-first, executable-path-first study plan**, not an instruction to read every repository alphabetically. KairoGameEngine is a superbuild over sibling repositories: its CMake configuration uses `KAIRO_WORKSPACE_ROOT` and can enable or omit editor, player, physics sandbox, migration, and compute targets. Follow [README](../README.md) and [CMakeLists](../CMakeLists.txt) to understand which dependencies are actually required in your chosen configuration.

All links to other repositories below point at their default GitHub branches; check out matching revisions for a reproducible workspace build.

## I. Learn underlying primitives before engine orchestration

1. [KairoMath](https://github.com/swayam8624/KairoMath) — vectors, matrices, transforms, tensor math and numerical algorithms.
2. [KairoGeometry](https://github.com/swayam8624/KairoGeometry), [KairoSpatial](https://github.com/swayam8624/KairoSpatial) — geometric representation and spatial indexing; inspect the package exports and tests.
3. [KairoPhysicsMath](https://github.com/swayam8624/KairoPhysicsMath) → [KairoPhysicsEngine](https://github.com/swayam8624/KairoPhysicsEngine) — shapes, contacts, broadphase/narrowphase, integration and world state.
4. [KairoECS](https://github.com/swayam8624/KairoECS) — entity identifiers, registry and sparse-set storage; compare this component to EngineCore's entity/scene-level contracts.
5. [KairoReflection](https://github.com/swayam8624/KairoReflection) and [KairoAssets](https://github.com/swayam8624/KairoAssets) — runtime metadata, importers, asset manifests, and persistent-to-runtime identity.

## II. Follow a real game from project file to player

6. [Runtime/KairoPlayer/main.cpp](../Runtime/KairoPlayer/main.cpp) — entry point, CLI options, validation, startup and fixed-frame dispatch. Start by tracing the sequence of module imports.
7. [RuntimeProject.cppm](../Runtime/KairoPlayer/RuntimeProject.cppm) — validates the `.kproject` path, reads descriptor, asset manifest, startup scene, and action map; compare [KairoEngineCore](https://github.com/swayam8624/KairoEngineCore) project/scene loader interfaces.
8. [RuntimeInputBridge.cppm](../Runtime/KairoPlayer/RuntimeInputBridge.cppm) → [RuntimeLogicBridge.cppm](../Runtime/KairoPlayer/RuntimeLogicBridge.cppm) → [RuntimeNativeGameplayBridge.cppm](../Runtime/KairoPlayer/RuntimeNativeGameplayBridge.cppm) — how a host input event becomes gameplay actions and authored or native logic.
9. [RuntimePhysicsBridge.cppm](../Runtime/KairoPlayer/RuntimePhysicsBridge.cppm) → [RuntimeCharacterMotorBridge.cppm](../Runtime/KairoPlayer/RuntimeCharacterMotorBridge.cppm) → [RuntimePlayerControllerBridge.cppm](../Runtime/KairoPlayer/RuntimePlayerControllerBridge.cppm) — fixed-step simulation, entity-to-body mapping and input-driven motion. Pair with the [PhysicsWorld](https://github.com/swayam8624/KairoPhysicsEngine/blob/main/PhysicsWorld.cppm) implementation.
10. [RuntimeRenderBridge.cppm](../Runtime/KairoPlayer/RuntimeRenderBridge.cppm), [RealtimeSceneBridge.cppm](../Runtime/KairoRealtimeRenderBridge/RealtimeSceneBridge.cppm) — asset-ID conversion, scene-instance mapping and renderer handles. Compare [KairoRenderer](https://github.com/swayam8624/KairoRenderer) modules.
11. [RuntimeAudioBridge.cppm](../Runtime/KairoPlayer/RuntimeAudioBridge.cppm), [RuntimeSceneAudioBridge.cppm](../Runtime/KairoPlayer/RuntimeSceneAudioBridge.cppm) — audio resources, scene components and lifecycle.
12. [RuntimeWorldStreamingBridge.cppm](../Runtime/KairoPlayer/RuntimeWorldStreamingBridge.cppm) and [RuntimeSaveGameBridge.cppm](../Runtime/KairoPlayer/RuntimeSaveGameBridge.cppm) — load/unload state and persistence boundaries; identify durable vs session-only IDs.
13. [RuntimePackaging.cppm](../Runtime/KairoPlayer/RuntimePackaging.cppm) and [RuntimeShippingBridge.cppm](../Runtime/KairoPlayer/RuntimeShippingBridge.cppm) — packaging, validation and distribution paths.

## III. Work backwards from the editor

14. [KairoEditor](https://github.com/swayam8624/KairoEditor) — document schema, commands, undo/redo, authoring state, asset browser, and live scene presentation.
15. [KairoEngineCore](https://github.com/swayam8624/KairoEngineCore) — scene, component, input map, job-system, platform and gameplay runtime interfaces.
16. [KairoRenderer](https://github.com/swayam8624/KairoRenderer) — render graph, renderer abstraction and Vulkan/Metal/OpenGL/Direct3D backends.
17. [Runtime/KairoRenderBridge](../Runtime/KairoRenderBridge/) — offline/material-preview/snapshot adapters. The realtime scene bridge is a different conversion boundary.
18. [KairoHub](https://github.com/swayam8624/KairoHub) — desktop launcher/project and imported-game inspection tools; do not confuse the launcher with the native player.

## IV. Migration, demos, and validation

19. [Migration/KairoBridge](../Migration/KairoBridge/) — source-project descriptors and canonical intermediate representation, with Unity/Godot adapter modules. This is an optional migration target, not necessarily part of default player execution.
20. [Samples/Phase1Game](../Samples/Phase1Game/) and [Samples/KairoRacingGame](../Samples/KairoRacingGame/) — exercise integration and follow expected asset/config paths.
21. [Runtime/KairoPlayer/tests](../Runtime/KairoPlayer/tests/), [Migration/KairoBridge/tests](../Migration/KairoBridge/tests/) — validate behaviour at system boundaries.
22. [STATUS.md](../STATUS.md) and [STATUS.yaml](../STATUS.yaml) — project state is a report, not proof of passing on your machine; rerun CMake, CTest and CI.

## Data and control-flow traces

**Project boot:** CLI `main` → `RuntimeProject` → `Kairo.Assets` manifest → `Kairo.EngineCore` scene/input loader → runtime bridges → game loop.

**Frame:** host input → action map / player control → `RuntimeFixedStepFanout` callbacks → physics accumulator/substeps → scene synchronization → render bridge → `KairoRenderer` → host GPU API.

**Content authoring:** editor document/command → canonical asset/scene identity → imported artifacts → runtime scene bridge → process-local renderer handles. Stable IDs and transient GPU handles must not be mixed.

**Gameplay persistence:** runtime world/scene entities → save-game representation → deserialize/reconcile world identity. Physics contact/body handles are ephemeral; persistence should be expressed through stable entity IDs.

**Migration:** foreign source project → source adapter → canonical IR and migration manifest → native Kairo assets/scenes. An importer that parses a document is not automatically a complete gameplay converter.

## Exercises

1. Trace one key event from player input to fixed-step physics and final visible transform.
2. Identify the exact ownership lifetime of a GPU texture, entity, physics body and imported asset ID.
3. Compare the integration target to a standalone KairoRenderer or KairoPhysicsEngine build: what dependencies are optional?
4. Find the validation performed before opening a project and test a missing/escaping asset path.
5. Follow one save/load cycle and identify which session-only IDs must be regenerated.

## Documentation policy

Source comments should explain intent, inputs/outputs, ownership, errors, API contracts, and **where the next call goes**. Avoid paraphrasing every line. Cover all meaningful authored source/config files in future passes; exclude generated binaries, vendored assets and automatically generated code from mass commentary without provenance review.

**Licensing:** KairoGameEngine already has [LICENSE](../LICENSE); each sibling repository is independently licensed and must be checked before claiming that the *whole workspace* has a single blanket license.
