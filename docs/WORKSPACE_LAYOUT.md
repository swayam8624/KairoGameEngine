# Kairo Sibling Workspace

Kairo uses **one physical checkout per repository**.

The canonical local layout is:

```text
Kairo/
├── KairoGameEngine/
├── KairoMath/
├── KairoGeometry/
├── KairoSpatial/
├── KairoPhysicsMath/
├── KairoPhysicsEngine/
├── KairoAssets/
├── KairoAI/
├── KairoECS/
├── KairoReflection/
├── KairoEngineCore/
├── KairoRenderer/
├── KairoEditor/
├── KairoRayTracer/
├── KairoGPU/
├── KairoSIMD/
├── KairoScheduler/
├── KairoONNX/
├── KairoTransformers/
├── KairoHub/
└── KairoMacPerception/
```

KairoGameEngine is the **integration/superbuild repository**, not a second copy
of the component repositories. Its CMake build consumes the sibling checkouts
through `KAIRO_WORKSPACE_ROOT`, which defaults to the parent directory.

## Why submodules were removed

The historical layout cloned the same component repositories again inside
KairoGameEngine. That produced two editable copies of each project on one
machine and allowed nested component CMake files to create still more fallback
copies under `build/.../_deps`.

The sibling workspace removes both problems:

1. there is one working tree to edit, commit, pull, and inspect for each repo;
2. the GameEngine superbuild adds foundational targets in dependency order, so
   downstream CMake files reuse them instead of FetchContent-ing duplicates.

Standalone component builds remain supported. Their immutable FetchContent
fallbacks are for consumers that do **not** have the sibling workspace.

## Migrating an existing clone

First update KairoGameEngine. Then run the migration in dry-run mode:

```bash
cd /Users/swayamsingal/Desktop/Programming/Kairo/KairoGameEngine
bash scripts/migrate_from_submodules.sh
```

The script refuses to delete a nested repository that contains uncommitted
changes. If every safety check passes:

```bash
bash scripts/migrate_from_submodules.sh --apply
```

The apply phase removes clean legacy nested copies, old local submodule
administration, and the CMake build directory. Deleting the build directory is
required because CMake caches absolute source paths from the former nested
layout.

Then build from a clean cache:

```bash
cmake --preset dev-clang
cmake --build --preset dev-clang --parallel
ctest --preset dev-clang --output-on-failure
```

## Different workspace location

```bash
cmake --preset dev-clang -DKAIRO_WORKSPACE_ROOT=/absolute/path/to/Kairo
```

## Missing repositories

```bash
bash scripts/bootstrap_workspace.sh
```

The bootstrap script clones only missing siblings and never resets or overwrites
an existing checkout.
