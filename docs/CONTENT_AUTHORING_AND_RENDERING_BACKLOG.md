# Content, Authoring, and Rendering Backlog

Status: tracked delivery record. Checked items are complete only when their exit
gate is evidenced; later-phase descriptions remain requirements, not claims.

This document tracks the content-compatibility and editor-authoring work needed
to turn Kairo's existing runtime foundations into a practical game-production
tool. It exists so asset testing, mesh authoring, scene lighting, code-driven
gameplay, real-time rendering, and offline rendering evolve as one workflow.

## Confirmed Current Boundaries

- KairoEditor uses KairoRenderer for its live viewport. The editor receives the
  renderer's offscreen viewport texture and places Dear ImGui tooling around it.
- KairoRayTracer is currently a separate offline renderer with its own `.kairo`
  scene format. The editor does not currently submit its EngineCore scene to the
  ray tracer.
- EngineCore now owns versioned camera, light, environment, material-slot,
  shadow, and render-layer authoring descriptors. KairoRenderer does not yet
  consume the complete contract, and the editor does not yet expose complete
  creation, inspector, viewport-gizmo, and camera-preview workflows.
- Imported and procedural meshes are runtime/cooked geometry. They are not an
  editable topology model, so vertex, edge, face, sculpt, modifier, and texture
  painting workflows do not exist yet.
- C++ can create entities through `Scene::CreateEntity`, and runtime code can
  queue transactional spawns through `RuntimeWorld`. Gameplay bytecode also has
  a spawn operation. Kairo does not yet provide a polished Unity-style gameplay
  component API, script lifecycle, native hot reload, or editor-exposed C++
  component workflow.

## Track A: Licensed Compatibility Content

Acquire a small, permanent compatibility corpus rather than manually testing
with unrelated files. Every downloaded asset must include:

- original source URL, author, exact license, required attribution, and access date;
- original archive checksum and an unmodified source copy;
- a machine-readable `ASSET_LICENSES` manifest;
- permitted redistribution status before any file is committed;
- expected import result, screenshots, and known unsupported features;
- a bounded size suitable for CI, or a separate opt-in large-content package.

Candidate sources to evaluate include Khronos glTF Sample Assets, Kenney,
Poly Haven, Blender demo assets, and other sources with explicit redistribution
terms. License approval is per asset, not inferred from the hosting website.

The corpus should cover:

1. A simple textured static prop with normals, tangents, UVs, and one PBR material.
2. A multi-material hard-surface object.
3. A hierarchy with transformed child meshes.
4. A small furnished interior or courtyard scene.
5. A larger exterior environment for culling and streaming measurements.
6. Alpha-mask foliage and alpha-blended glass.
7. Emissive, metallic, rough, normal-mapped, and high-dynamic-range materials.
8. A rigged and animated character, retained as a future fixture until animation lands.
9. Deliberately malformed and partially supported files for diagnostic tests.

Each accepted asset must pass this matrix:

| Consumer | Required result |
| --- | --- |
| KairoAssets | Deterministic import, typed artifacts, dependencies, cache hit, and located failures |
| KairoRenderer | Correct geometry, transforms, materials, textures, color space, and bounds |
| KairoEditor | Thumbnail, drag/drop or instantiate, selection, transform, material inspection, and save/reload |
| KairoPlayer | Load and render from a packaged project without source-directory assumptions |
| KairoRayTracer | Shared-scene conversion or an explicitly documented unsupported-feature diagnostic |

## Track B: Scene Objects and Components

EngineCore should own serializable, renderer-neutral authoring descriptors for:

- camera: perspective/orthographic, field of view or size, near/far planes,
  exposure, primary flag, clear mode, and render layers;
- directional, point, spot, and rectangular-area lights: linear color,
  photometric intensity convention, range/cones, shadow policy, and layers;
- environment: background, sky/environment texture, ambient/IBL settings,
  fog, exposure, and tone-mapping profile;
- renderable: mesh, material slots, visibility, cast/receive shadows, and layers;
- reflection probe and camera/render target descriptors later.

The editor must provide creation commands, hierarchy icons, reflected inspector
controls, viewport visualization, undo/redo, serialization, duplication, prefab
support, and play-mode cloning for these components. No editor panel should own
a second copy of scene state.

## Track C: Editable Geometry

Do not make the portable runtime `MeshArtifact` mutable. Introduce a separate
authoring representation that can cook deterministically into a runtime mesh.

Required authoring layers:

1. `EditableMesh` with stable vertex, edge, loop, polygon, UV, material-slot,
   crease, normal, and selection identities. A half-edge or loop topology is
   preferable to triangle-only mutation.
2. Object/Edit mode separation with vertex, edge, and face selection.
3. Transform, extrude, inset, bevel, loop cut, knife, merge, dissolve, bridge,
   fill, triangulate, recalculate normals, and duplicate operations.
4. Transactional topology commands with bounded undo/redo and validation after
   every operation.
5. A non-destructive modifier stack before adding destructive convenience tools.
6. Deterministic cook/bake into KairoAssets mesh artifacts with source linkage.
7. Dedicated UV editing, unwrap, seam, packing, and texel-density workflows.

Sculpting is a separate subsystem, not a face-selection mode. Track brush
strokes, falloff, symmetry, masks, multiresolution or dynamic topology,
incremental GPU updates, bounded undo storage, remeshing, and bake/cook behavior
as its own phase after editable polygon modeling is reliable.

## Track D: Textures and Materials

Complete the path from Phase 2 KairoAssets artifacts to visible authored results:

- Vulkan texture/image allocation, upload, mip use, samplers, and lifetime;
- sRGB/linear/normal-map handling preserved from import to shader;
- material instances referencing typed texture assets;
- base color, metallic-roughness, normal, occlusion, and emissive channels;
- material preview sphere/plane and assignment to mesh material slots;
- editor import/reimport settings, thumbnails, dependency graph, and diagnostics;
- UV viewport, texture inspection, channel inspection, and texture painting later.

## Track E: Code-Driven Objects and Gameplay

Kairo should support code-driven creation without copying Unity's weaknesses.
The target is one transactional entity/component API shared by C++, visual
logic, prefabs, editor commands, save/load, replay, and networking adapters.

Required workflow:

- register a native component/system with stable reflected metadata;
- spawn a prefab or entity archetype from C++ and receive a stable runtime handle;
- add, query, modify, and remove supported components through validated commands;
- expose selected fields and callable actions to the editor and visual logic;
- define deterministic `BeginPlay`, fixed update, frame update, event, and teardown phases;
- support safe native-module reload or an explicit restart-required workflow;
- preserve authored data when a code component is temporarily unavailable;
- separate queued structural mutation from iteration to protect runtime storage.

The ergonomic goal is concise code, for example a future shape resembling
`world.Spawn(prefab)` plus typed component access, while retaining Kairo's
transaction, validation, stable-ID, and deterministic execution guarantees.

## Track F: Real-Time and Offline Render Ownership

The intended ownership is:

```text
KairoEditor
    -> edits KairoEngineCore Scene + KairoAssets references
    -> KairoRenderer adapter provides the interactive viewport
    -> KairoRayTracer adapter produces an offline render from a scene snapshot

KairoPlayer
    -> loads the same EngineCore Scene + Assets
    -> KairoRenderer adapter provides the shipped real-time frame
```

The editor's future `Render` action must be an explicit offline-render job:

1. Validate and snapshot the current authored scene.
2. Convert supported camera, light, material, texture, mesh, and environment
   components through a shared adapter owned outside both renderers.
3. Report unsupported features before rendering instead of silently changing them.
4. Run KairoRayTracer asynchronously with cancellation and progress.
5. Store the result and metadata as a project-owned render output.
6. Display the result in a dedicated image/render-results surface.

Viewport shading controls are not offline rendering. `Lit`, `Unlit`, `Normals`,
and `Lighting` remain real-time KairoRenderer diagnostic modes.

## Delivery Order

- [ ] Restore green Assets, EngineCore, Editor, and umbrella builds first. Local macOS
  superbuild restored; Windows CI and committed umbrella pins remain the exit gate.
- [x] Approve and acquire the licensed compatibility corpus. Committed fixtures and
  hash-locked optional scenery are recorded under `TestContent/Compatibility`.
- [x] Add scene-authored camera, light, environment, and material descriptors.
- [ ] Complete texture/material upload in KairoRenderer.
- [ ] Import and instantiate one shared glTF scene in Assets, Editor, Player, and Renderer.
- [ ] Add EngineCore-scene to KairoRayTracer conversion and an editor render job.
- [ ] Deliver the reflected C++ entity/component authoring workflow.
- [ ] Deliver editable polygon topology and deterministic mesh cooking.
- [ ] Deliver UV/material authoring and texture inspection.
- [ ] Deliver sculpting as a separately tested authoring subsystem.
- [ ] Add animation, terrain/foliage, particles, cloth, fluids, and large-world content later.

## Phase Distribution

Each phase has an acceptance gate. Work may proceed in parallel only where the
table explicitly says it is independent; a phase is not complete because its UI
exists while its serialization, runtime behavior, or tests remain missing.

| Phase | Primary repositories | Deliverable | Exit gate |
| --- | --- | --- | --- |
| 0. Baseline health | KairoGameEngine, KairoAssets, KairoEngineCore, KairoRenderer, KairoEditor, KairoPlayer, KairoRayTracer | Reproducible dependency graph, clean builds, tests, CI, and documented run commands | macOS and Windows CI are green; umbrella pins verified; no unexplained generated or dirty files |
| 1. Compatibility corpus | KairoAssets, KairoGameEngine | Licensed compact models, textures, HDR environment, interior scene, exterior scene, malformed fixtures, checksums, and machine-readable license manifest | Every committed asset has redistribution approval and a deterministic import expectation |
| 2. Scene authoring contract | KairoEngineCore | Renderer-neutral camera, light, environment, renderable, material-slot, layer, and shadow descriptors with versioned serialization | Create, edit, duplicate, undo, save, reload, and play-mode clone tests preserve every field |
| 3. Real-time material path | KairoAssets, KairoRenderer | Texture upload, mip/sampler handling, color-space correctness, PBR channels, multiple material slots, and scene-authored lighting | Reference textured model matches controlled expected captures; no renderer-owned hardcoded scene light remains |
| 4. Editor scene tools | KairoEditor, KairoEngineCore, KairoRenderer | Camera/light creation, hierarchy icons, complete inspectors, viewport manipulators, camera preview, material assignment, and environment controls | A user can author and save a lit scene without editing source files |
| 5. Shared content end-to-end | KairoAssets, KairoEngineCore, KairoRenderer, KairoEditor, KairoPlayer | Import and instantiate the same furnished scene using stable asset identities across editor and packaged runtime | Editor and Player load the same scene with matching transforms, materials, lights, cameras, and bounds |
| 6. Offline render bridge | shared adapter package, KairoEngineCore, KairoRayTracer, KairoEditor | Scene snapshot conversion, capability diagnostics, asynchronous render job, progress, cancellation, and render-results view | Editor camera/light/material changes produce corresponding offline output without a second scene file |
| 7. Code-driven gameplay API | KairoEngineCore, KairoEditor, KairoPlayer | Reflected native components/systems, transactional spawn and mutation, lifecycle phases, prefab spawning, field exposure, and reload policy | One sample behavior works identically when authored in C++, instantiated in editor, saved, and run in Player |
| 8. Editable mesh kernel | KairoAssets or a dedicated KairoModeling repo, KairoMath, KairoGeometry | Stable half-edge/loop topology, validation, selection identities, transactional operations, and deterministic cook to `MeshArtifact` | Topology property tests and round-trip cook tests pass for valid and deliberately degenerate meshes |
| 9. Polygon modeling UX | KairoEditor, editable-mesh owner | Object/Edit modes; vertex/edge/face picking; transform, extrude, inset, bevel, loop cut, knife, merge, dissolve, bridge, fill, normals, duplicate; modifier stack | A modeled prop survives undo/redo, save/reload, cooking, and rendering without topology corruption |
| 10. UV and material authoring | KairoEditor, KairoAssets, KairoRenderer | UV editor, seams, unwrap, packing, texel-density tools, material previews, texture/channel inspection, and reimport controls | A user can unwrap, texture, assign, save, reopen, and package a multi-material prop |
| 11. Sculpting | dedicated sculpt subsystem, KairoEditor, KairoRenderer, KairoAssets | Brush engine, falloff, symmetry, masks, bounded stroke undo, multiresolution or dynamic topology, remesh, incremental viewport updates, and cook/bake | Sustained sculpt session meets correctness, memory, latency, save/reload, and final-cook budgets |
| 12. Advanced production systems | separate subsystem repos plus EngineCore/Editor adapters | Animation, terrain, foliage, particles, cloth, fluids, and large-world streaming, each with its own proposal and performance budget | Each subsystem ships through Assets, Editor, Player, serialization, profiling, and automated tests rather than as an editor-only demo |

### Safe Parallel Work

- During Phase 0, document current import/render behavior and prepare asset
  licensing review, but do not commit third-party content before approval.
- Phase 1 fixture preparation and Phase 2 scene-schema design may run in
  parallel once the baseline is reproducible.
- Phase 3 renderer implementation and Phase 4 editor interaction design may
  overlap after Phase 2 types are frozen; Editor must consume those types rather
  than invent panel-owned state.
- Phase 6 adapter design and Phase 7 gameplay API design may run in parallel
  after Phase 5 proves stable scene and asset identities.
- Phase 8 is independent of the offline render bridge, but Phases 9 and 10 must
  wait for the editable-mesh invariants and deterministic cook contract.
- Phase 11 must not begin until polygon editing, UV ownership, mesh cooking, and
  renderer update paths are measured and reliable.

### Phase Work Packages

Every phase should be delivered as reviewable vertical work packages rather
than one repository-wide commit:

1. Contract and public types, including versioning and invalid-input behavior.
2. Core implementation with focused unit tests.
3. Serialization, migration, and asset dependency behavior.
4. Renderer or runtime integration with deterministic fixtures.
5. Editor interaction, undo/redo, diagnostics, and accessibility/keymap work.
6. End-to-end test, documentation, screenshots or captures, profiling, and CI.

Child repositories must be committed and pushed before the umbrella repository
updates their pins. Generated files, downloaded archives, local build trees, and
unapproved third-party content must never be swept into those commits.

### Immediate Queue

Only these items enter the next execution queue:

1. Audit current build/test/CI health and repository pins without changing features.
2. Produce a candidate asset register with licenses and sizes; approve each asset before download or commit.
3. Define the EngineCore scene-component contract and compatibility/versioning rules.
4. Write the first cross-consumer acceptance test specification for one textured prop and one furnished scene.

Editable modeling, sculpting, and advanced simulation remain tracked, but they
do not begin before shared assets, scene semantics, and renderer ownership are
proven end to end.

## Acceptance Milestone

The first milestone is complete only when one legally redistributable furnished
scene can be imported once, instantiated and edited in KairoEditor, rendered in
the live KairoRenderer viewport, packaged and run by KairoPlayer, and rendered
offline by KairoRayTracer from the same authored scene and asset identities.
Camera, light, transform, and material changes must survive save/reload and
produce corresponding changes in both renderers.
