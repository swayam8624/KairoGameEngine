# Understanding KairoPlayer: one complete launch and frame

This walkthrough is a teaching companion to [SOURCE_READING_ORDER.md](SOURCE_READING_ORDER.md). It follows the **actual native Player source**, starting at [main.cpp](../Runtime/KairoPlayer/main.cpp). It is deliberately concrete: the same words, `Entity`, `AssetID`, `BodyID`, and `TextureHandle`, refer to **different identity domains** and must never be treated as interchangeable integers.

The integration repository is a CMake superbuild of sibling Kairo libraries. For a reproducible experiment, record the commit of each sibling checkout, compiler version, target platform, and CMake options: the umbrella module's imports are not proof that optional implementations have been built.

## 1. Validate before doing expensive or unsafe work

`main` begins by parsing `Arguments`. The `--validate`, `--smoke`, and `--package` operations are mutually exclusive; `--replace` only applies to packaging. The graphics backend may be selected explicitly or taken from the authored descriptor.

`RuntimeProject` then performs two related but distinct security steps. [ResolveProjectDescriptorPath](../Runtime/KairoPlayer/RuntimeProject.cppm) canonicalizes the input and requires an existing regular `.kproject` file. Its parent is the canonical project root. For each referenced asset manifest, startup scene, and input map, `RequireProjectFile` normalizes the portable relative path and weakly canonicalizes the resulting filesystem path. It compares **path components** against the project root and rejects missing files, directories, and symlink escapes.

The descriptor parser itself lives in [KairoEngineCore/ProjectDescriptor.cppm](https://github.com/swayam8624/KairoEngineCore/blob/main/ProjectDescriptor.cppm). The asset manifest parser is in [KairoAssets/AssetManifest.cppm](https://github.com/swayam8624/KairoAssets/blob/main/AssetManifest.cppm). The scene and input-map loaders also belong to EngineCore. That separation is intentional: KairoHub, Editor and Player must agree about the on-disk project format instead of writing independent, incompatible parsers.

After `RuntimeProject` loads the manifest, scene, and input map, `main` constructs physics, audio, controller, logic, native-gameplay, production, and shipping bridges. A `--validate` run returns **after this setup**, before a renderer or native window is constructed; it is not merely a syntax-only parsing command.

### Trace exercise

Take a deliberately invalid manifest path. Identify whether failure occurs in `ProjectDescriptor` validation, `RuntimeProject::RequireProjectFile`, or `KairoAssets::LoadAssetManifest`. A relative path with `..` and a symlink pointing outside the project root exercise different checks.

## 2. Identify what each system actually owns

| Identity or resource | Persistent? | Owner / consumer | Key rule |
|---|---|---|---|
| `kairo::assets::AssetID` | Intended for persistence | `KairoAssets` manifest and registry | Represents an asset even if its relative path changes |
| `kairo::engine::Entity` | Scene-local authored identity | `KairoEngineCore::Scene` | Used by scene serialization and player-facing events |
| `BodyID`, `ColliderID` | Session-local | `PhysicsWorld` | Map to scene entities in [RuntimePhysicsBridge](../Runtime/KairoPlayer/RuntimePhysicsBridge.cppm) |
| `MeshHandle`, `TextureHandle` | Session-local | Native renderer / [RuntimeRenderBridge](../Runtime/KairoPlayer/RuntimeRenderBridge.cppm) | Never serialize directly into an asset manifest |
| Animation playback clock | Runtime session state | `RuntimeRenderBridge` | Samples authored clips; requires explicit persistence if a game needs it |
| Audio playback/voice object | Runtime session state | Runtime audio bridges | Clip identity is not the same as a live voice handle |

The **entity-to-body** mapping deserves particular attention. `RuntimePhysicsBridge::AddEntityToWorld` examines authored rigid-body and collider components, converts them into physics descriptors, creates a body and collider, then records both forward and reverse mappings. On an exception, the bridge destroys the partly created physics body and erases its maps. A collider without an authored rigid-body component is treated as a static body; a rigid-body component without a collider is rejected.

The **asset-to-GPU** mapping is separate. `RuntimeRenderBridge::EnsureTexture` imports a texture using the project's validated metadata/cache and asks `RendererRuntime` for a GPU texture handle. It remembers the bound handle and verifies that later uses of the same asset do not reinterpret its color space or normal-map semantics incompatibly. Its `ReleaseResources` method destroys owned meshes/textures through the renderer. In `main`, the renderer is constructed before its bridge, giving the bridge a shorter lifetime.

### Trace exercise

Place an entity with an imported mesh and a dynamic rigid body in a scene. Draw **two diagrams**, one mapping `AssetID → MeshHandle` and the other mapping `Entity → BodyID`. Ask what must be reconstructed after reloading a save file.

## 3. One displayed frame contains zero or more physics steps

The native loop in [main.cpp](../Runtime/KairoPlayer/main.cpp) proceeds in this approximate order (look at the code to distinguish conditional paths):

1. Poll host window events and update `RuntimeInputBridge`.
2. Apply optional Quit/Move/Jump actions; send all named input actions to gameplay logic.
3. Measure the elapsed wall-clock time since the last displayed frame.
4. Call `RuntimePhysicsBridge::Advance(elapsedSeconds, &fixedSteps)`.
5. Dispatch contact events and invoke native gameplay, production, shipping, and animation advancement.
6. Advance scene audio, rebuild the renderer-facing scene, select the camera pose, and draw.
7. If `--smoke` was selected, inspect the captured native viewport and fail on a blank/uniform image or capture timeout.

The fourth stage is not a single physics step. `RuntimePhysicsBridge` has a fixed timestep (default 1/60 s) and an accumulator. It clamps unusually large frame deltas, repeatedly executes solver steps while there is enough accumulated time, and stops at `MaximumSubsteps` (default 8). Each substep invokes `RuntimeFixedStepFanout::BeforePhysicsStep` **before** calling `PhysicsWorld::Step`. The fan-out passes the callback to the player controller, authored logic, and native gameplay. If an extreme delay leaves too much backlog, the bridge drops whole excess steps and records `DroppedExcessTime` rather than attempting an unbounded catch-up loop.

After stepping, current and previous simulated poses are interpolated using the accumulator fraction. A parent-first scene traversal is necessary because writing local child transforms depends on the updated world transform of each parent. The bridge then publishes entity-facing contact events: gameplay should not store a raw solver `BodyID` in a persistent scene file.

**Why this separation matters:** display refresh rates vary, while stable simulation/constraint behavior benefits from a fixed `dt`. Interpolation reduces visual judder without pretending that the GPU render loop and physics solver run at the same frequency. This does **not** establish perfect cross-platform determinism; floating-point, scheduling, input ordering, solver details and build configuration must still be tested.

### Trace exercise

Assume a 60 Hz fixed step and a one-time 45 ms display-frame stall. Sketch how the accumulator may execute multiple physics steps in a single displayed frame. Then compare that to a stall long enough to reach the configured maximum-substep cap.

## 4. Rendering and animation are deliberate conversion boundaries

[RealtimeSceneBridge](../Runtime/KairoRealtimeRenderBridge/RealtimeSceneBridge.cppm) is shared between editor and player paths. `RuntimeRenderBridge::BuildScene` uses it to convert the current authored scene, bound imported materials/meshes, and sampled animation poses into `kairo::renderer::RenderScene`. The [renderer package](https://github.com/swayam8624/KairoRenderer) chooses one compiled backend and owns GPU allocations/command submission. A compiled backend is not inferred merely from the existence of a platform enum.

`RuntimeRenderBridge::StepAnimations` advances explicit gameplay clip clocks or compatibility defaults for imported scene instances, then invokes the shared animation evaluator. It discards playback records for scene instances that disappear or no longer have playable clips. The bridge therefore has both **durable source data** and **ephemeral sampling state**; their lifetimes differ.

The primary camera may optionally follow a player controller, but `main.cpp` only enables that policy when the authored camera has the `kairo.player-follow-camera` tag. The bridge otherwise respects the selected authored camera pose.

## 5. Saving, restoring, and streaming require topology care

[RuntimeSaveGameBridge](../Runtime/KairoPlayer/RuntimeSaveGameBridge.cppm), [RuntimeWorldStreamingBridge](../Runtime/KairoPlayer/RuntimeWorldStreamingBridge.cppm), and [RuntimeWorldSaveBridge](../Runtime/KairoPlayer/RuntimeWorldSaveBridge.cppm) cover distinct persistence and lifecycle concerns. Do not assume their state is just a memory dump of the current runtime.

`RuntimePhysicsBridge::RestoreSnapshot` validates that a candidate PhysicsWorld snapshot has the **same active body topology** as the currently running entity/body mapping before replacing solver state. It also resets its interpolation/contact history so a restored pose does not interpolate from the pre-load position. Streaming can instead activate or remove entities, potentially changing the runtime topology; its transaction/order matters to any save/restore protocol.

The high-level distinction is that saving a game should preserve a **meaningful authored/runtime state** whose external session resources can be re-created. It should not blindly write C++ pointers, GPU handles, native audio objects or transient solver index values to disk.

## 6. Where to prove each property

- [Runtime/KairoPlayer/tests](../Runtime/KairoPlayer/tests/) — player-facing loading, audio, physics/controller, streaming and save-game boundaries.
- [Runtime/KairoRealtimeRenderBridge/tests](../Runtime/KairoRealtimeRenderBridge/tests/) — authored scene → render scene conversion.
- [KairoPhysicsEngine tests](https://github.com/swayam8624/KairoPhysicsEngine/tree/main/tests) — solver and snapshot mathematics, independent of the player.
- [KairoAssets tests](https://github.com/swayam8624/KairoAssets/tree/main/tests) — asset/manifest/import validation, independent of GPU ownership.
- [KairoEngineCore tests](https://github.com/swayam8624/KairoEngineCore/tree/main/tests) — scene, input-map, project-descriptor and save-game contracts.

A source comment is a **hypothesis about executable behavior**. Confirm it with the relevant test, a reproducible build configuration and—where GPU execution matters—the actual hardware backend. This documentation is educational and does not replace those checks.
