# Phases 6-12 completion record

This document records the implementation ownership and acceptance contracts for the second half of the content-authoring/rendering roadmap. The component repositories remain the source of truth for their code; the umbrella repository owns cross-component integration and end-to-end acceptance.

## Phase 6 — offline ray-tracer bridge

The authored `Kairo.EngineCore.Scene` is the single scene source. `Runtime/KairoRenderBridge` converts one scene snapshot into KairoRayTracer data through an injected asset resolver rather than guessing project/cache locations. Camera, environment, PBR materials, mesh and scene-instance geometry, point/directional/spot/rectangle-area lights, and linear/exponential fog preserve their authored semantics. The Editor exposes renderer-neutral render-job and material-preview requests. The umbrella provides the concrete RayTracer job service and the real-time KairoRenderer material-preview adapter, so standalone Editor does not acquire an offline-renderer dependency.

Offline jobs expose queued/running/completed/cancelled/failed state, progressive pass counts, cancellation, diagnostics, and project-owned image plus `.kairo-render` metadata output. Beauty rendering uses the shared fog contract; debug visualization modes remain intentionally diagnostic.

## Phase 7 — code-driven gameplay

`Kairo.EngineCore.NativeGameplay` is the reflected native C++ gameplay contract. User/game modules register factories and reflected type/property metadata rather than relying on type-name construction. Project attachments persist in `Config/NativeGameplay.knative` using stable scene entity IDs and typed property overrides.

Editor authoring, validation, and Player execution consume the same manifest and reflection metadata. Player exposes `KAIRO_PLAYER_NATIVE_GAMEPLAY_TARGETS` for linked game modules, rejects unregistered manifest types before play, and dispatches BeginPlay, FixedUpdate, Update, Event, and EndPlay through the live scene/runtime-world synchronization boundary. Reload policy remains explicit in the Core runtime.

## Phase 8 — editable mesh kernel

`Kairo.Assets.EditableMesh` remains the only editable topology model. It owns stable vertex/face IDs, reconstructed half-edge adjacency, topology validation, transactions, and deterministic cooking. `kairo.editable-mesh.v1` persists topology together with the authoring domains that depend on it instead of creating a second Editor-only mesh format.

## Phase 9 — polygon modeling UX

The shared modeling layer provides transform, extrude, inset, edge split, knife, bevel, propagated quad-strip loop cut, merge, dissolve, triangulate, duplicate, bridge, fill, face-normal flip, smooth-normal recalculation, and a bounded non-destructive modifier stack. Editor exposes these operations through one document workspace with vertex/edge/face selection and full-document undo/redo.

Loop cuts intentionally stop at boundaries, branches, or non-quad topology rather than guessing a continuation that could corrupt topology.

## Phase 10 — UV and material authoring

UV data is stored per face corner with explicit seams. The shared Assets layer owns planar unwrap, seam-derived islands, deterministic packing, texel-density estimation, and cooked UV transfer. Material authoring owns face material slots, PBR channel inspection, and texture semantic/color-space/mipmap/reimport settings. The editable mesh document round-trips all of these settings.

Material preview uses the production KairoRenderer PBR material conversion on a sphere or plane; it is not a separate preview shader/material model.

## Phase 11 — sculpting

The sculpt stack includes Inflate, Smooth, and Grab brushes, symmetry, masks, stroke undo/redo, production memory/stroke/affected-vertex budgets, incremental dirty-vertex viewport updates, deterministic remeshing under a hard topology budget, and bounded multiresolution levels.

The current remesher uses deterministic uniform subdivision. This satisfies the persisted/budgeted production workflow and multires contract but is not Blender-style adaptive dynamic topology. A future adaptive backend can replace the remeshing strategy behind the same budget/interface without changing project documents.

## Phase 12 — advanced production systems

Animation, terrain/foliage, particles, cloth, fluid, and world streaming persist in the versioned `Config/Production.kproduction` manifest. `ProductionPerformanceBudget` and deterministic workload estimation reject configurations that exceed configured resource/work ceilings before runtime construction. `ProductionRuntime` orchestrates the enabled systems and exposes profiling counters used by tests, Editor preview, Player runtime, and future external profilers.

Editor authors and previews the same manifest; Player loads and executes it, using the primary camera position as the current streaming origin. The current implementations are modular CPU kernels and stable project/runtime contracts, not a claim of final GPU-scale AAA simulation backends. Individual specialized/GPU backends can replace those kernels later without changing the persisted authoring contract.

## Acceptance ownership

- **KairoAssets:** topology/modeling/UV/material/sculpt unit and save/reopen tests on Ubuntu, macOS, and Windows.
- **KairoEngineCore:** native gameplay manifests/lifecycle and production manifest/runtime/budget tests on Ubuntu, macOS, and Windows.
- **KairoEditor:** document workspace, reflected native inspector, production authoring, offline-render controller, and material-preview request tests on its standalone matrix.
- **KairoRayTracer:** shared directional/spot/fog semantics and offline-lighting tests on its standalone matrix.
- **KairoGameEngine:** stacked cross-repository build/tests plus Player native/production runtime and Editor-to-RayTracer/material-preview acceptance.

During stacked review the umbrella CI checks out the component review heads explicitly. Before final merge preparation those review refs are replaced by immutable reviewed submodule commit SHAs and the ordinary umbrella `Build, Test, and Package` workflow is the release gate.
