# Full repository portfolio — a learning and evidence roadmap

This is the *across-repository* course plan for the source-first teaching branches. Each linked project contains a more detailed, file-by-file `docs/SOURCE_READING_ORDER.md`; open that local guide after reading its conceptual companion. Follow dependencies before jumping to performance optimizations or complex UI code.

The list is not a CMake build order. The Kairo superbuild's actual pinned revision set is recorded in [workspace.lock](../workspace.lock). Source comments and teaching guides are on documentation branches; they have not automatically been merged into the default branches or accepted as a new locked production build.

**How to study:** for every file, explain (1) the problem it solves, (2) inputs and outputs, (3) invariants and failure modes, (4) ownership and lifetime, (5) callers and downstream consumers, and (6) a test that would falsify your understanding. Keep an experiment notebook of exact SHA, host, compiler and command.

## Foundation: represent values before computing worlds

1. **[KairoMath](https://github.com/swayam8624/KairoMath/blob/docs/teaching-walkthrough-20261121/docs/MATH_FOUNDATIONS_WALKTHROUGH.md)** — Numerical invariants, C++23 module semantics, transforms, decompositions, tensor training. [File-by-file map](https://github.com/swayam8624/KairoMath/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md).
2. **[KairoGeometry](https://github.com/swayam8624/KairoGeometry/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md)** — Parametric primitives, intersection contracts, box and triangle geometry. [File-by-file map](https://github.com/swayam8624/KairoGeometry/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md).
3. **[KairoSpatial](https://github.com/swayam8624/KairoSpatial/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md)** — Static BVH, dynamic fat AABB trees, candidate generation, pathfinding. [File-by-file map](https://github.com/swayam8624/KairoSpatial/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md).
4. **[KairoPhysicsMath](https://github.com/swayam8624/KairoPhysicsMath/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md)** — Inertia, force, impulse, integrators and Jacobian constraints. [File-by-file map](https://github.com/swayam8624/KairoPhysicsMath/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md).
5. **[KairoPhysicsEngine](https://github.com/swayam8624/KairoPhysicsEngine/blob/docs/teaching-walkthrough-20261121/docs/COLLISION_PIPELINE_WALKTHROUGH.md)** — World stepping, candidate-to-contact pipeline, island solver and CCD. [File-by-file map](https://github.com/swayam8624/KairoPhysicsEngine/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md).
6. **[KairoECS](https://github.com/swayam8624/KairoECS/blob/docs/teaching-walkthrough-20261121/docs/ECS_IDENTITY_AND_ITERATION.md)** — Generational handles, sparse sets and safe iteration. [File-by-file map](https://github.com/swayam8624/KairoECS/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md).

## Asset and scene runtime: authoritative state versus native resources

7. **[KairoAssets](https://github.com/swayam8624/KairoAssets/blob/docs/teaching-walkthrough-20261121/docs/ASSET_PIPELINE_WALKTHROUGH.md)** — Stable ID, importer, manifest, content cache, renderer upload. [File-by-file map](https://github.com/swayam8624/KairoAssets/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md).
8. **[KairoEngineCore](https://github.com/swayam8624/KairoEngineCore/blob/docs/teaching-walkthrough-20261121/docs/SCENE_RUNTIME_WALKTHROUGH.md)** — Project schema, scene hierarchy, deferred mutation and persisted state. [File-by-file map](https://github.com/swayam8624/KairoEngineCore/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md).
9. **[KairoRenderer](https://github.com/swayam8624/KairoRenderer/blob/docs/teaching-walkthrough-20261121/docs/FRAME_LIFECYCLE_WALKTHROUGH.md)** — Render graph, native backend ownership, scene and GPU lifetimes. [File-by-file map](https://github.com/swayam8624/KairoRenderer/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md).
10. **[KairoRayTracer](https://github.com/swayam8624/KairoRayTracer/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md)** — CPU camera rays, BVH intersection callbacks, light transport and film. [File-by-file map](https://github.com/swayam8624/KairoRayTracer/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md).
11. **[KairoGameEngine](https://github.com/swayam8624/KairoGameEngine/blob/docs/teaching-walkthrough-20261121/docs/ENGINE_INTEGRATION_WALKTHROUGH.md)** — Superbuild, exact revisions, Player/ECS/physics/render bridges. [File-by-file map](https://github.com/swayam8624/KairoGameEngine/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md).
12. **[KairoEditor](https://github.com/swayam8624/KairoEditor/blob/docs/teaching-walkthrough-20261121/docs/AUTHORING_TRANSACTION_WALKTHROUGH.md)** — Undoable authoring edits, inspector, project lifecycle and play separation. [File-by-file map](https://github.com/swayam8624/KairoEditor/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md).
13. **[KairoHub](https://github.com/swayam8624/KairoHub/blob/docs/teaching-walkthrough-20261121/docs/TAURI_PROJECT_TRUST_BOUNDARY.md)** — Trusted Tauri commands, project validation, external game inspection. [File-by-file map](https://github.com/swayam8624/KairoHub/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md).

## CPU/GPU and AI systems: capability boundaries

14. **[KairoScheduler](https://github.com/swayam8624/KairoScheduler/blob/docs/teaching-walkthrough-20261121/docs/TASK_SCHEDULING_WALKTHROUGH.md)** — CPU pool lifetime, exceptions, cancellation and DAG scheduling. [File-by-file map](https://github.com/swayam8624/KairoScheduler/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md).
15. **[KairoSIMD](https://github.com/swayam8624/KairoSIMD/blob/docs/teaching-walkthrough-20261121/docs/SIMD_SCALAR_PARITY_GUIDE.md)** — Native instruction dispatch, numerical parity and memory assumptions. [File-by-file map](https://github.com/swayam8624/KairoSIMD/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md).
16. **[KairoGPU](https://github.com/swayam8624/KairoGPU/blob/docs/teaching-walkthrough-20261121/docs/GPU_DEVICE_LIFECYCLE.md)** — Bounded Metal compute, buffers, transfers and completion. [File-by-file map](https://github.com/swayam8624/KairoGPU/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md).
17. **[KairoONNX](https://github.com/swayam8624/KairoONNX/blob/docs/teaching-walkthrough-20261121/docs/ONNX_MODEL_TO_TENSOR_EXECUTION.md)** — Strict model import, shape rewriting and bounded CPU operator support. [File-by-file map](https://github.com/swayam8624/KairoONNX/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md).
18. **[KairoTransformers](https://github.com/swayam8624/KairoTransformers/blob/docs/teaching-walkthrough-20261121/docs/TRANSFORMER_FROM_TOKENS_TO_TRAINING.md)** — Attention, generation, KV caches, LoRA and optimizer checkpoints. [File-by-file map](https://github.com/swayam8624/KairoTransformers/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md).
19. **[KairoReflection](https://github.com/swayam8624/KairoReflection/blob/docs/teaching-walkthrough-20261121/docs/REFLECTION_METADATA_TO_SAFE_EDITOR.md)** — Stable property schemas and checked authoring accessors. [File-by-file map](https://github.com/swayam8624/KairoReflection/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md).
20. **[KairoAI](https://github.com/swayam8624/KairoAI/blob/docs/teaching-walkthrough-20261121/docs/AI_PROPOSAL_TO_APPROVED_ACTION.md)** — Provider transport, tool-call proposals, exact host authorization. [File-by-file map](https://github.com/swayam8624/KairoAI/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md).
21. **[KairoMacPerception](https://github.com/swayam8624/KairoMacPerception/blob/docs/teaching-walkthrough-20261121/docs/PERCEPTION_TO_APPROVED_CONTROL.md)** — Screen/Vision data, preview approvals, control-channel boundary. [File-by-file map](https://github.com/swayam8624/KairoMacPerception/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md).

## Digital-content-creation: author, preflight, publish, consume

22. **[KairoPipelineCore](https://github.com/swayam8624/KairoPipelineCore/blob/docs/teaching-walkthrough-20261121/docs/CROSS_DCC_PUBLICATION_WALKTHROUGH.md)** — Portable manifest, safe publication and consumer integrity. [File-by-file map](https://github.com/swayam8624/KairoPipelineCore/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md).
23. **[KairoBlender](https://github.com/swayam8624/KairoBlender/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md)** — Blender snapshot, validation and glTF publication. [File-by-file map](https://github.com/swayam8624/KairoBlender/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md).
24. **[KairoMaya](https://github.com/swayam8624/KairoMaya/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md)** — Maya SceneDoctor preflight and portable publication. [File-by-file map](https://github.com/swayam8624/KairoMaya/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md).
25. **[KairoHoudini](https://github.com/swayam8624/KairoHoudini/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md)** — Temporal cache preflight and immutable publish. [File-by-file map](https://github.com/swayam8624/KairoHoudini/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md).
26. **[KairoNuke](https://github.com/swayam8624/KairoNuke/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md)** — Read/Write shot validation and verified ingest. [File-by-file map](https://github.com/swayam8624/KairoNuke/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md).
27. **[KairoProductionTools](https://github.com/swayam8624/KairoProductionTools/blob/docs/teaching-walkthrough-20261121/docs/PORTFOLIO_EVIDENCE_CHAIN.md)** — Technical portfolio evidence versus native DCC results. [File-by-file map](https://github.com/swayam8624/KairoProductionTools/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md).

## Standalone systems and research: validate before claiming

28. **[NESEmulator](https://github.com/swayam8624/NESEmulator/blob/docs/teaching-walkthrough-20261121/docs/NES_ARCHITECTURE_GUIDE.md)** — 6502/PPU/APU/bus, cartridge mapper and timing. [File-by-file map](https://github.com/swayam8624/NESEmulator/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md).
29. **[Maveb](https://github.com/swayam8624/Maveb/blob/docs/teaching-walkthrough-20261121/docs/CBRC_FROM_EDIT_TO_CERTIFICATE.md)** — Locality/evidence graph, full-rebuild fallback and render correctness. [File-by-file map](https://github.com/swayam8624/Maveb/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md).
30. **[Vulkax](https://github.com/swayam8624/Vulkax/blob/docs/teaching-walkthrough-20261121/docs/VERIFIED_REWRITE_LEARNING_PATH.md)** — WorldIR fitting, verified counterfactuals, rollback and scientific limits. [File-by-file map](https://github.com/swayam8624/Vulkax/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md).
31. **[desktop-pets](https://github.com/swayam8624/desktop-pets/blob/docs/teaching-guided-source-20261121/docs/ITCH_RELEASE_LEARNING_PATH.md)** — Deterministic Rust pet, optional OS awareness, unfinished publishing path. [File-by-file map](https://github.com/swayam8624/desktop-pets/blob/docs/teaching-guided-source-20261121/docs/SOURCE_READING_ORDER.md).

## One integrated path through the engine

Start with a vector in KairoMath, then watch it become a triangle/ray in KairoGeometry and a BVH candidate in KairoSpatial. Use KairoPhysicsMath to derive mass, inertia, force and constraints; see KairoPhysicsEngine turn candidates into contacts, resolve islands and advance a timestep. Then track a separate KairoAssets `AssetID` through a validated import into KairoEngineCore's authored Scene. The shared GameEngine bridge converts that Scene to KairoRenderer resource handles; Player, Editor and Hub each own a different lifecycle and trust boundary.

This path explains why `AssetID`, EngineCore Entity ID, ECS entity (slot/generation), PhysicsEngine BodyID and renderer MeshHandle are all **different types of identity**. Refactoring them into one integer without a clear adapter would damage persistence, stale-handle safety or native resource lifetimes.

## One integrated path through the production pipeline

Blender or Maya produces an authored mesh; Houdini can produce a cache; Nuke can consume rendered or cached outputs. Each host's native adapter extracts a scene snapshot. Host-neutral preflight produces typed diagnostics. KairoPipelineCore normalizes filenames, fingerprints bytes, writes a versioned manifest, stages and publishes, and Nuke/KairoAssets checks those bytes again on the *consumer's* host. A screenshot or successful report generator proves neither native DCC compatibility nor independent data integrity.

## Four evidence levels

| Label | Meaning | What it does **not** mean |
| --- | --- | --- |
| Source-present | The function/schema exists at a pinned source SHA | It compiled or ran on a target host |
| Unit-test observed | A stated suite produced pass output at that SHA | All edge cases or real native hardware passed |
| Native-host verified | A relevant OS/GPU/DCC integration test completed | Scientific generalization or performance superiority |
| Research/benchmark evidenced | A reproducible protocol, data and repeated metrics support a bounded claim | The result applies to arbitrary workloads or models |

In the documentation passes so far, source lines were compared to the earlier teaching branch for selected files; **that is not a build or test run**. Any test or measurement not actually executed for the current head remains unverified.

## Known follow-up work

The learning maps and initial comments are broad, but **function-by-function coverage is not yet certified complete** in every file. The following discoveries should be handled in separate functional branches with tests, not quietly inside teaching-only commits:

- KairoEditor subtree undo snapshots currently omit EngineCore audio emitter/listener components.
- KairoECS `SparseSet::Remove` is `noexcept` while moving arbitrary component types, creating an exception-policy constraint.
- NES Cartridge iNES input checks and unsupported mapper handling need hardening; controller debugger reads may shift latched input.
- KairoPhysicsEngine friction uses a reduced tangent model, and KairoSpatial's dynamic tree handles require care around rebuilds.
- Vulkax's frozen free-fall negative result and MAVEB's distinction between modeled work savings and observed speedup must stay visible.
- Desktop Pets still draws its pet procedurally; source-linked sprite atlas/Godot/Pomodoro integration and platform packaging require distinct work and validation.
- Third-party models, sprites, fonts, ROMs, DCC plugins and output media keep upstream rights even where the original code/documentation uses MIT.

## Editing rules for future passes

Keep comments explanatory and accurate to actual control flow. Avoid altering program statements during documentation edits. Compare non-comment source with the prior commit, run language syntax checks and appropriate tests when possible, and keep native host verification separate. Prefer committing focused files in the existing teaching branch, recording the commit SHA and source scope. After source maps and comments stabilize, merge only through ordinary review/test gates.

The main source links here intentionally open the active documentation branches. When merged, convert them to default-branch-relative links or a stable release tag so readers do not end up in an abandoned teaching snapshot.
