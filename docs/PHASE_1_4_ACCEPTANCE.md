# Phase 1–4 Integration Acceptance

This integration snapshot pins the component commits implementing Kairo's first four practical engine phases.

## Phase 1 — Complete gameplay vertical slice

The existing `Samples/Phase1Game` remains the executable acceptance project. It validates project loading, action-mapped movement, fixed-step physics, collision/trigger gameplay, collectible and win/loss state, pause/reset, atomic save/load, Vulkan presentation and relocatable packaging.

## Phase 2 — Production asset pipeline

Pinned `KairoAssets` adds:

- versioned RGBA8/RGBA32F texture artifacts;
- explicit linear/sRGB and normal-map semantics;
- deterministic mip generation;
- stb-backed image importing;
- metallic-roughness PBR material artifacts;
- cgltf-backed glTF 2.0 / GLB static scene importing;
- hierarchy, transforms, normals, UVs, tangents, indices and material metadata;
- malformed-source and typed-artifact tests.

Acceptance requires `KairoAssetsPhase2Tests` to pass inside the superbuild.

## Phase 3 — Gameplay runtime

Pinned `KairoEngineCore` adds:

- transactional queued scene mutations with rollback on failure;
- deterministic spawn tickets, entity/tag queries and revision tracking;
- a persistent typed gameplay VM with arithmetic, comparisons, branches, events and bounded execution;
- host calls for tags, positions, enabled state, spawning and destruction;
- explicit cross-compiler decoding for both legacy and gameplay bytecode.

Acceptance requires `KairoEngineCorePhase3Tests`, existing EngineCore tests and player-runtime logic tests to pass.

## Phase 4 — Production editor workflows

Pinned `KairoEditor` adds headless services for:

- asset browsing, source-state evaluation, dependency inspection and safe deletion;
- single, range, toggle and marquee scene selection;
- transactional scene fragments and basic prefab overrides;
- atomic recent-project persistence and project cloning;
- producer-owned diagnostics with navigable project, asset, entity, graph and source targets.

Acceptance requires `KairoEditorPhase4Tests` and the existing editor suite to pass. The native editor application remains compiled on Clang platforms; Windows continues to use the runtime-focused gate while the upstream MSVC editor-module ICE remains isolated.

## Required integration matrix

The umbrella PR is not ready until all of the following succeed from a recursive checkout of the exact gitlinks in this commit:

1. Ubuntu Clang full superbuild and complete CTest suite.
2. Ubuntu AddressSanitizer + UndefinedBehaviorSanitizer runtime suite.
3. macOS Homebrew LLVM full superbuild and complete CTest suite.
4. Windows MSVC runtime build and complete supported CTest suite.
5. Phase 1 native smoke and package relocation tests.
6. Source-package generation.

## Deliberate boundary

These phases establish a content pipeline, reusable gameplay runtime and production editor service layer. Textured PBR GPU consumption, animation, runtime audio/UI and advanced rendering remain later phases; they are not represented by placeholders in this integration claim.
