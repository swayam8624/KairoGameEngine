# Kairo Development Map

Status date: 2026-08-09. This map is an execution contract, not a marketing
feature list. A capability is `verified` only when its owning component tests
and the relevant cross-repository/native acceptance path have run.

## Evidence Levels

| State | Meaning |
| --- | --- |
| Verified | Implemented and executed on a supported host in the current workspace |
| Platform-gated | Implemented and registered for its native host, but not executable on this machine |
| Planned | Design direction only; users must not depend on it |

## Stabilization Priorities

| Priority | State | Exit evidence |
| --- | --- | --- |
| P0 green integration baseline | Verified locally | Complete superbuild and CTest suite pass; Windows remains CI-authoritative |
| P1 shared scene extraction | Verified | Editor and Player import `KairoRealtimeRenderBridge` rather than maintaining separate scene semantics |
| P2 representative vertical slice | Verified | `Samples/SharedContentShowcase` contains hierarchy, glTF, textures, materials, lights, camera and environment and renders in both hosts |
| P3 repository-boundary contracts | Verified | Serialized project imports through Assets/EngineCore and produces equal Editor/Player draws, materials, lights, environment and camera |
| P4 renderer/physics foundation | In progress | Multi-backend runtime, render graph, Metal lifetime hardening and convex GJK/EPA are present; native render-pass migration and mesh collision remain |

## Track A: Product Integration

Current: one project descriptor selects `auto`, Vulkan, Metal, D3D12, or
OpenGL; CLI selection overrides it. Editor and Player share camera selection,
asset binding, scene extraction, and authored render semantics.

Next gates:

1. Run Windows MSVC CI with the D3D12 Editor/Player native smoke tests.
2. Add packaged-executable parity for the shared showcase on each native API.
3. Compare deterministic extracted scene fingerprints before GPU submission.
4. Keep leaf repositories green before advancing umbrella submodule pins.

## Track B: Real-Time Rendering

Verified locally on macOS: Metal, Vulkan/MoltenVK, and OpenGL Editor/Player
frames; PBR textures, authored lights/environment, directional shadows,
transparency, debug drawing, object picking, capture, and tooling overlays.

Platform-gated: Direct3D 12 uses DXGI/D3D12 resources, PBR/shadow pipelines,
readback, picking, presentation, and Dear ImGui integration. Windows execution
is required before release status.

Next gates:

1. Migrate shadow, opaque, transparent, debug, tooling, readback, and present
   work from the backend-frame envelope into logical render-graph passes.
2. Translate graph transitions and transient alias plans into native barriers
   and physical resource allocation per backend.
3. Add GPU timestamp queries, upload budgets, deferred destruction, and frame
   latency/resource-pressure telemetry.
4. Add cascaded directional shadows, point-light cube shadows, and a shadow
   atlas only after graph/resource scheduling is stable.

## Track C: Physics

Verified: persistent dynamic-AABB broadphase; sphere, capsule, plane, AABB,
oriented-box and validated convex-hull colliders; SAT and GJK/EPA narrowphase;
sequential impulses, friction/restitution, sleeping, filters, triggers/events,
rays, overlap queries, sphere sweeps, projectiles, buoyancy, and debug snapshots.

Next gates:

1. Add static triangle-mesh collider artifacts with a per-mesh BVH and exact
   convex-versus-triangle candidate contacts.
2. Add persistent clipped multi-point manifolds before raising stack/joint
   stability targets.
3. Add conservative advancement/general rigid-body CCD with deterministic
   time-of-impact limits.
4. Add islands, parallel solving, joints, articulated constraints, and replay
   fixtures in that order.
5. Start particles, cloth, fluids, vehicles, and destruction as separate
   modules only after shared collision/query/event contracts are stable.

## Track D: Content And Authoring

Current: licensed compatibility fixtures, deterministic Assets import/cache,
editable mesh kernel, scene components, inspector workflows, glTF hierarchy,
materials/textures, native gameplay attachments, visual logic, and offline
ray-tracer jobs exist.

Next gates:

1. Complete compatibility coverage for alpha foliage/glass, large scenes,
   malformed assets, animation fixtures, and embedded GLB images.
2. Finish production edit-mode topology UX, UV authoring, modifiers, sculpting,
   texture painting, and deterministic cook/reimport workflows.
3. Add animation/skeleton import and runtime playback before animation graphs.
4. Prove save/reload, undo/redo, Play mode, Player, and package parity for each
   new authoring component.

## Track E: Runtime And Release

Current: fixed-step physics, mapped input, deterministic gameplay logic, native
gameplay registration, project validation, packaging, source archives, and Hub
project management are integrated.

Next gates:

1. Binary install/package-config targets and relocatable dependency deployment.
2. Platform signing, notarization, installer generation, crash reporting, and
   symbol handling.
3. Save-game/replay schemas, deterministic runtime snapshots, networking
   boundaries, and automated packaged-game acceptance.
4. Performance budgets for startup, frame time, memory, asset streaming, and
   representative scene scale.

## Release Rule

No track advances the umbrella pin based only on a component build. Release
evidence requires the owning tests, shared project contract, relevant native
smoke, package/relocation check, and a clean diff/status audit. Unsupported work
stays explicitly `planned`; platform-only work stays `platform-gated` until its
native CI evidence is available.
