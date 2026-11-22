# Kairo from source: one workspace, one project, one game frame

This guide is an integration companion to
[the linked KairoGameEngine source map](SOURCE_READING_ORDER.md).
The components live in separate sibling repositories, each with its own
version, tests and documentation. The superbuild **composes** them; it does not
make their module boundaries disappear or certify every runtime.

## 1. Why the superbuild is not one giant engine repository

[`CMakeLists.txt`](../CMakeLists.txt) uses a parent workspace containing
sibling `KairoMath`, `KairoGeometry`, `KairoSpatial`, `KairoAssets`,
`KairoEngineCore`, `KairoRenderer`, `KairoPhysicsEngine`, `KairoEditor`
and other repositories.

The build options select optional components such as native Editor, Player,
physics sandbox, offline ray tracer, optional AI/compute stack and migration
tooling. Presence of a target does not mean its host-dependent features were
executed successfully.

[`workspace.lock`](../workspace.lock) lists exact revision SHAs for the
integration snapshot. [`scripts/bootstrap_workspace.sh`](../scripts/bootstrap_workspace.sh)
prepares a sibling layout; [`scripts/verify_workspace_lock.sh`](../scripts/verify_workspace_lock.sh)
compares checked-out heads against the lock. A repository at a later feature
or documentation branch is **not** automatically the locked accepted version.

```text
Kairo/                   (one parent workspace)
  KairoGameEngine/       (CMake superbuild, Player and integration bridges)
  KairoMath/
  KairoAssets/
  KairoEngineCore/
  KairoRenderer/
  KairoPhysicsMath/
  KairoPhysicsEngine/
  KairoEditor/
  ...                    (other sibling repositories)
```

**Exercise:** compare `workspace.lock` SHA values to each sibling
checkout's `git rev-parse HEAD`. Treat mismatches as a reason to pin or
explicitly update the integration baseline, not as proof that one
repository's newer source is wrong.

## 2. Project bootstrap and authored source of truth

[`Runtime/KairoPlayer/RuntimeProject.cppm`](../Runtime/KairoPlayer/RuntimeProject.cppm)
begins with a canonical `.kproject` path and shared
[KairoEngineCore ProjectDescriptor](https://github.com/swayam8624/KairoEngineCore/blob/docs/teaching-walkthrough-20261121/ProjectDescriptor.cppm).

The loaded portable project contains a scene and AssetRegistry. A scene
Entity ID is persistent across save/load; an AssetID identifies a mesh,
material, texture or document independently from its path. Neither value is
a physics BodyID, a KairoECS generation/index, or a Vulkan/Metal GPU handle.

Core identity and authored hierarchy are described in
[KairoEngineCore's scene lifecycle](https://github.com/swayam8624/KairoEngineCore/blob/docs/teaching-walkthrough-20261121/docs/SCENE_RUNTIME_WALKTHROUGH.md).

## 3. Two adapters, two different runtime lifetimes

### Render conversion

[`Runtime/KairoRealtimeRenderBridge/RealtimeSceneBridge.cppm`](../Runtime/KairoRealtimeRenderBridge/RealtimeSceneBridge.cppm)
is shared by Editor and Player. `RenderAssetBindings` maps stable KairoAssets
IDs to current renderer mesh/texture handles and material data, but does
**not** own GPU allocations.

`BuildRenderScene` traverses visible EngineCore scene objects, checks
render layers, composes hierarchy transforms, maps Entity ID to renderer
ObjectID for picking, converts authored lights and optionally evaluates glTF
animation/skin palettes. Skinned model transforms require care to avoid
applying node transforms twice.

The chosen KairoRenderer backend owns the real GPU buffers, textures,
command queues, viewport readback and presentation. For the full path,
read [the renderer frame-lifecycle guide](https://github.com/swayam8624/KairoRenderer/blob/docs/teaching-walkthrough-20261121/docs/FRAME_LIFECYCLE_WALKTHROUGH.md).

### Physics conversion

[`Runtime/KairoPlayer/RuntimePhysicsBridge.cppm`](../Runtime/KairoPlayer/RuntimePhysicsBridge.cppm)
converts scene collider/body descriptors to a live PhysicsWorld and keeps
bidirectional authored Entity <-> runtime BodyID mappings. It owns
previous/current pose history for rendering interpolation and converts
contact events back to authored entity identities.

`Advance(elapsedSeconds)` clamps very long elapsed times, performs bounded
fixed-delta steps, reports discarded excess time and publishes interpolated
poses. The interpolation result makes visuals smoother; it is not an
extra physics solve. Snapshot restore verifies body topology and resets
interpolation history to prevent blending from stale pre-load positions.

See [the PhysicsEngine collision walkthrough](https://github.com/swayam8624/KairoPhysicsEngine/blob/docs/teaching-walkthrough-20261121/docs/COLLISION_PIPELINE_WALKTHROUGH.md)
for broadphase -> narrowphase -> solver details.

## 4. Authored Scene versus KairoECS runtime layout

[`Runtime/KairoSceneECSBridge.cppm`](../Runtime/KairoSceneECSBridge.cppm)
builds a process-local ECS mirror. It allocates runtime entities before
resolving hierarchy references, allowing a child to refer to a parent that
appears later in the authored entity order.

The mirror keeps `AuthoredEntityIdentity` next to ECS components.
Saving an ECS slot index/generation instead of the authored Entity ID would
make saved content depend on transient allocation history. Extraction is
one-way; other code must explicitly reconcile when the authoritative
EngineCore Scene changes.

## 5. The Player frame in execution order

[`Runtime/KairoPlayer/main.cpp`](../Runtime/KairoPlayer/main.cpp)
is the composition root. It creates portable RuntimeProject first, then
physics, audio, input, character motor, logic, gameplay/production and
shipping adapters, finally selecting the native RendererRuntime.

A typical running frame follows the actual loop:

```text
Poll window events and raw input
       |
       v
Capture action mapping (Move / Jump / named logic actions)
       |
       v
RuntimePhysicsBridge::Advance(elapsed, fixed-step listener)
       |    before each physics step: controls / logic / gameplay hooks
       |    PhysicsWorld::Step(fixed dt)
       |    interpolate poses
       v
Dispatch contacts and update non-fixed gameplay systems
       |
       v
Advance animations and scene audio
       |
       v
BuildRenderScene, submit camera, RendererRuntime::DrawFrame
       |
       v
Present / optionally inspect captured viewport
```

`--validate-only` exits after portable project checking without creating
a renderer window. A smoke run requests a viewport capture, then verifies
the returned image is nonuniform within a bounded number of frames. That
demonstrates some native execution but does not certify shading
correctness, network timing, all backend types or numerical physics.

## 6. How the Editor shares the same schema

[KairoEditor](https://github.com/swayam8624/KairoEditor) authors EngineCore
scenes and document data, using reversible commands and project recovery.
It re-exports the shared real-time scene bridge via
`SceneRenderBridge.cppm` for compatibility, instead of keeping a separate
mesh/light mapping implementation.

The distinction between **Edit** state and runtime **Play** state is
essential: a physics preview should not silently overwrite authored scene
transforms or write transient solver caches into a `.kscene` file.

Read [the Editor authoring-transaction guide](https://github.com/swayam8624/KairoEditor/blob/docs/teaching-walkthrough-20261121/docs/AUTHORING_TRANSACTION_WALKTHROUGH.md).
Its source review also documents a current undo-snapshot gap for audio
emitter/listener components, which requires separate functional repair.

## 7. Offline rendering, migration and optional research stack

[`Runtime/KairoRenderBridge`](../Runtime/KairoRenderBridge) exposes the
separate offline CPU ray-tracing bridge; it is not identical to the
interactive KairoRenderer GPU backend. Read
[KairoRayTracer's source guide](https://github.com/swayam8624/KairoRayTracer/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md).

[`Migration/KairoBridge`](../Migration/KairoBridge) hosts source-project
adapters and canonical intermediate representations for migration.
Preserving content means mapping semantics, identities, materials and
transforms, not just copying file names.

Optional SIMD/Scheduler/GPU/ONNX/Transformers components must be tested
at their own selected toolchains and revision heads. Source presence does
not establish that an optional GPU kernel runs on every device.

## 8. Exact-head acceptance and evidence hygiene

[`scripts/run_portfolio_acceptance.sh`](../scripts/run_portfolio_acceptance.sh)
is an acceptance driver, not a promise of future background tests.
It clears an old accepted stamp before running so a stale success cannot
be cited for changed code. [`scripts/verify_portfolio_95.sh`](../scripts/verify_portfolio_95.sh)
and the status documentation define the higher-level evidence gate.

A rigorous acceptance record needs the locked component SHAs, compiler,
CMake configuration, test selection, native GPU/OS gates, exact outputs,
and any unsupported/skipped targets. A cross-platform policy unit test
is not a substitute for Metal/D3D12 native smoke execution.

**As of this documentation pass, 95% is an unverified *target*, not a
measured completion score.** This guide is based on reading source on
the teaching branch; it does not assert that the acceptance campaign
has been run for these new comments.
