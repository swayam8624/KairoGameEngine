# KairoGameEngine

`KairoGameEngine` is the integration repository for the Kairo engine workspace.
It provides the CMake superbuild and end-to-end runtime/editor fixtures while
consuming the independently versioned Kairo repositories as **sibling
checkouts**.

There is one physical working tree per component repository. KairoGameEngine no
longer clones another KairoMath/KairoRenderer/etc. tree inside itself; standalone
component builds remain supported, while the workspace superbuild reuses the
same source trees you edit and commit directly.

## Architecture

```text
KairoMath --------------------------------------------------------+
    +-> KairoGeometry -> KairoSpatial -> KairoRayTracer           |
    +-> KairoPhysicsMath -> KairoPhysicsEngine -------------------+
                                                                 |
KairoAssets -> KairoEngineCore -----------------------------------+
    +--------> KairoRenderer -------------------------------------+
    +--------> KairoRayTracer                                     |
KairoECS ----------------------------------------------------------+
KairoReflection ---------------------------------------------------+
KairoAI -----------------------------> editor assistance / optional game services
                                                                 v
                           KairoRenderer -> KairoEditor -> game tools
                                         -> KairoPlayer -> shipped runtime

Optional compute stack:
KairoSIMD + KairoScheduler + KairoGPU -> KairoONNX -> KairoTransformers
```

The aggregate CMake target `Kairo::GameEngine` exposes the runtime-facing
`KairoAssets`, `KairoEngineCore`, `KairoRenderer`, and `KairoPhysicsEngine`
targets. KairoAssets owns the portable `kairo.mesh.v1` contract consumed by
both renderers, including strict OBJ import, while each renderer owns only its
backend conversion. Authoring tools and the offline ray tracer remain separate
targets so their dependencies do not leak into a shipped game runtime.
`KairoRealtimeRenderBridge` is the single Editor/Player conversion boundary for
builtin and imported meshes, textures, PBR materials, scene instances, lights,
environments, layers, object IDs, and shadow policy.

The complete KAIRO v1 portfolio is frozen at **95% source-complete across
27 repositories**. Source completion and exact-head native verification are
tracked separately; no platform-only result is inferred from source state.
See [`docs/V1_95_COMPLETION.md`](docs/V1_95_COMPLETION.md),
[`docs/DEVELOPMENT_MAP.md`](docs/DEVELOPMENT_MAP.md), and the machine-readable
`STATUS.yaml` files. Research claims remain separate in
[`docs/RESEARCH_TRACKS.md`](docs/RESEARCH_TRACKS.md).

## Workspace

Use one parent directory for all Kairo repositories:

```text
Kairo/
├── KairoGameEngine/
├── KairoMath/
├── KairoGeometry/
├── KairoSpatial/
└── ...
```

For a fresh workspace:

```bash
mkdir -p Kairo
cd Kairo
git clone https://github.com/swayam8624/KairoGameEngine.git
cd KairoGameEngine
bash scripts/bootstrap_workspace.sh
```

For a clone created with the historical nested-submodule layout:

```bash
bash scripts/migrate_from_submodules.sh
bash scripts/migrate_from_submodules.sh --apply
```

The first invocation is a safety dry-run and refuses migration if a nested copy
contains uncommitted work. See [docs/WORKSPACE_LAYOUT.md](docs/WORKSPACE_LAYOUT.md).

Synchronize clean sibling repositories with their current upstream branches:

```bash
bash scripts/sync_workspace.sh
```

`workspace.lock` records the exact integration snapshot without creating
duplicate worktrees. Check it with `bash scripts/verify_workspace_lock.sh`.

## Prerequisites

The checked-in developer preset targets the current macOS/Homebrew toolchain:

- CMake 3.28 or newer
- Ninja
- Homebrew LLVM with C++23 module support
- GLFW, OpenGL, Vulkan headers/loader, MoltenVK, and `glslangValidator`

```bash
brew install cmake ninja llvm glfw vulkan-headers vulkan-loader molten-vk shaderc glslang
```

## Build And Test

For normal development:

```bash
bash scripts/build_and_test.sh
```

Validate the full 27-repository source milestone:

```bash
bash scripts/verify_portfolio_95.sh
```

Run every acceptance gate available on the current host:

```bash
bash scripts/run_portfolio_acceptance.sh
```

After changing workspace layout or compiler/toolchain state, force a clean
configure and verify the exact recorded sibling revisions first:

```bash
bash scripts/build_and_test.sh --clean --verify-lock
```

The script is fail-fast: CTest is never run after a failed build, so one linker
failure cannot turn into dozens of misleading "Not Run" test failures. A
successful compile/link must also pass the zero-warning log gate before tests
start. See [docs/BUILD_HYGIENE.md](docs/BUILD_HYGIENE.md).

The equivalent manual commands are:

```bash
cmake --preset dev-clang
cmake --build --preset dev-clang --parallel
ctest --preset dev-clang --output-on-failure
```

`dev-clang` uses the repository-owned portable Clang toolchain. On macOS it
prefers the current Homebrew LLVM installation; on other platforms it resolves
`clang++` from `PATH`. Set `KAIRO_CXX_COMPILER` to an absolute compiler path to
select a specific Clang installation. The plain `dev` preset deliberately uses
the compiler selected by the host environment.

Both developer presets build the real-time editor, CPU ray tracer, physics
sandboxes, standalone player, and all registered tests. The optimized preset is:

```bash
cmake --preset release
cmake --build --preset release
```

The KairoMath and KairoSpatial visual laboratories remain available from their
standalone repositories. The umbrella build excludes them because they are
interactive developer tools, not runtime or integration-test artifacts.

The superbuild fails during configuration with the exact missing sibling path
and repository clone URL when a required workspace repository is absent.
`KAIRO_WORKSPACE_ROOT` may point at a different sibling-repository directory.

## Source Package

Component source lives in sibling repositories, so KairoGameEngine no longer
pretends that one source archive contains the entire engine workspace.
`KAIRO_GAME_ENGINE_ENABLE_PACKAGING` is off by default. Runtime game packaging
remains available through KairoPlayer's project packaging flow.

## Continuous Integration

Workspace CI checks out KairoGameEngine together with its required sibling
repositories, then runs the same CMake presets and tests used locally. Component
repositories continue to run their own standalone CI independently.

## Run

Compile every logic document attached to the startup scene. Saving incomplete
graphs remains allowed; this explicit build gate rejects graph diagnostics and
publishes source-bound runtime artifacts:

```bash
./build/dev-clang/KairoEditor/KairoProjectCompiler \
  KairoEditor/examples/StarterProject/Project.kproject
```

Validate a project without opening a native window. This is the stable contract
used by launchers, CI, recovery tools, and future packaging profiles:

```bash
./build/dev-clang/Runtime/KairoPlayer/KairoPlayer \
  KairoEditor/examples/StarterProject/Project.kproject --validate
```

Run the same startup scene in the standalone player. The project descriptor's
`graphics-backend` policy is used unless `--renderer` overrides it. `auto`
selects the native-first compiled backend; explicit unavailable APIs fail before
native window/device creation:

```bash
./build/dev-clang/Runtime/KairoPlayer/KairoPlayer \
  KairoEditor/examples/StarterProject/Project.kproject \
  --renderer auto

./build/dev-clang/Runtime/KairoPlayer/KairoPlayer \
  KairoEditor/examples/StarterProject/Project.kproject \
  --renderer vulkan

./build/dev-clang/Runtime/KairoPlayer/KairoPlayer \
  Samples/SharedContentShowcase/Project.kproject \
  --renderer opengl --smoke
```

Build one of the exact profiles authored in `Project.kproject` after publishing
current logic artifacts:

```bash
./build/dev-clang/KairoEditor/KairoProjectCompiler \
  KairoEditor/examples/StarterProject/Project.kproject

./build/dev-clang/Runtime/KairoPlayer/KairoPlayer \
  KairoEditor/examples/StarterProject/Project.kproject \
  --package Release
```

The profile's output directory receives a staged runtime bundle and is
published by one filesystem rename only after the relocated project validates.
An existing output is preserved unless replacement is explicit:

```bash
./build/dev-clang/Runtime/KairoPlayer/KairoPlayer \
  KairoEditor/examples/StarterProject/Project.kproject \
  --package Release --replace
```

The bundle contains `bin/KairoPlayer`, `project/Project.kproject`, project
payload files, `package.kmanifest`, and `run.sh` (`run.cmd` on Windows). It keeps
compiled logic but excludes every configured build output, Git metadata,
recovery journals, and derived editor caches. Symbolic links are rejected so a
package cannot copy data outside its project boundary. The current artifact is
a host runtime bundle: GLFW, Vulkan loader, graphics driver, and other shared
system dependencies must still be installed on the target machine. Platform
dependency deployment and signing remain separate release-engineering gates.

Validate or run the relocated project through its generated launcher:

```bash
KairoEditor/examples/StarterProject/Build/Release/run.sh --validate
KairoEditor/examples/StarterProject/Build/Release/run.sh
```

Render, read back, and verify a nonblank native frame without leaving the
window open. CI runners with Vulkan presentation support use this mode as
visual acceptance evidence:

```bash
./build/dev-clang/Runtime/KairoPlayer/KairoPlayer \
  KairoEditor/examples/StarterProject/Project.kproject --smoke
```

`KairoPlayer` resolves all authored paths relative to the descriptor, rejects
missing or escaping manifest/scene paths, imports source meshes through
KairoAssets' content-addressed OBJ pipeline, and keeps GPU handles private to
the runtime render bridge.

Authored Scene V2 rigid-body and collider descriptors are instantiated in an
isolated `KairoPhysicsEngine` world when the player starts. Rendering may run at
any frame rate: elapsed time feeds a bounded 60 Hz fixed-step accumulator, body
poses retain previous/current snapshots, and the scene receives interpolated
hierarchy-local transforms before draw extraction. Collision category/mask and
trigger settings are preserved, contact callbacks are translated back to stable
scene entities, and player-side raycasts return both entity and physics hit
data. Excess backlog beyond the configured substep budget is reported and
dropped instead of producing an unbounded spiral after a debugger or window
stall.

Gameplay input is loaded from the project's versioned `.kinput` map and polled
through GLFW's keyboard, mouse, and standardized gamepad paths. Scene-attached
logic runs as bounded deterministic EngineCore bytecode without linking editor
graph code into the player. Begin Play and input transitions dispatch once;
Tick dispatches on the fixed physics clock; collision begin/end events map back
to scene entities. Host calls currently cover terminal output, world-position
updates, and center-of-mass impulses. `--validate` fingerprints every source
document and rejects missing, foreign, stale, or unknown-action artifacts before
opening a Vulkan window.

Launch the native editor with its starter project:

```bash
./build/dev-clang/KairoEditor/KairoEditorApp \
  --project KairoEditor/examples/StarterProject/Project.kproject \
  --renderer auto
```

The `Kairo AI` dock is always present, while cloud transport remains opt-in:

```bash
cmake --preset dev-clang -DKAIRO_GAME_ENGINE_BUILD_AI_CLOUD=ON
cmake --build --preset dev-clang

KAIRO_AI_API_KEY='...' \
KAIRO_AI_MODEL='your-openai-compatible-model' \
./build/dev-clang/KairoEditor/KairoEditorApp \
  --project KairoEditor/examples/StarterProject/Project.kproject
```

Ask and Plan cannot mutate the project. Agent changes appear as validated
command previews and require exact approval before entering the manual undo
history. `KAIRO_AI_ENDPOINT` may select another HTTPS-compatible endpoint;
credentials are never written to project or recovery data.

Launch the interactive physics sandbox:

```bash
./build/dev-clang/Foundation/KairoPhysicsEngine/KairoPhysicsGlfwSandbox
```

Render and preview a CPU ray-traced scene:

```bash
./build/dev-clang/KairoRayTracer/KairoRayTracerPreview \
  KairoRayTracer/scenes/cornell.kairo --mode whitted
```

Each component README documents its narrower examples, controls, formats, and
standalone build path.

## Components

| Path | Repository | Responsibility | Tracked branch |
| --- | --- | --- | --- |
| `Foundation/KairoMath` | [KairoMath](https://github.com/swayam8624/KairoMath) | Vectors, matrices, transforms, numerical algorithms | `master` |
| `Foundation/KairoGeometry` | [KairoGeometry](https://github.com/swayam8624/KairoGeometry) | Geometry value types and intersection primitives | `master` |
| `Foundation/Spatial` | [KairoSpatial](https://github.com/swayam8624/KairoSpatial) | BVH, broadphase, partitioning, and spatial queries | `main` |
| `Foundation/KairoPhysicsMath` | [KairoPhysicsMath](https://github.com/swayam8624/KairoPhysicsMath) | Reusable rigid-body formulas and integration math | `main` |
| `Foundation/KairoPhysicsEngine` | [KairoPhysicsEngine](https://github.com/swayam8624/KairoPhysicsEngine) | Rigid-body world, collision, solver, and sandboxes | `main` |
| `KairoAssets` | [KairoAssets](https://github.com/swayam8624/KairoAssets) | Identity, manifests, derived cache, importer registry, strict OBJ import, and portable mesh artifacts | `main` |
| `KairoAI` | [KairoAI](https://github.com/swayam8624/KairoAI) | Bounded provider contracts, streaming, cancellation, structured tool calls, and deterministic mock inference | `main` |
| `KairoECS` | [KairoECS](https://github.com/swayam8624/KairoECS) | Generational entities, sparse-set component storage, and runtime iteration | `main` |
| `KairoReflection` | [KairoReflection](https://github.com/swayam8624/KairoReflection) | Stable type/property metadata and inspector-ready access adapters | `main` |
| `KairoEngineCore` | [KairoEngineCore](https://github.com/swayam8624/KairoEngineCore) | Scene/runtime services and application contracts | `main` |
| `KairoRenderer` | [KairoRenderer](https://github.com/swayam8624/KairoRenderer) | Multi-backend Vulkan/Metal/D3D12/OpenGL renderer, portable scene adaptation, and debug drawing | `main` |
| `KairoEditor` | [KairoEditor](https://github.com/swayam8624/KairoEditor) | Native docked authoring application | `main` |
| `KairoRayTracer` | [KairoRayTracer](https://github.com/swayam8624/KairoRayTracer) | Offline CPU rendering and visual diagnostics | `main` |
| `KairoGPU` | [KairoGPU](https://github.com/swayam8624/KairoGPU) | Compute-backend abstraction | `main` |
| `KairoSIMD` | [KairoSIMD](https://github.com/swayam8624/KairoSIMD) | CPU vector kernels | `main` |
| `KairoScheduler` | [KairoScheduler](https://github.com/swayam8624/KairoScheduler) | Deterministic task execution | `main` |
| `KairoONNX` | [KairoONNX](https://github.com/swayam8624/KairoONNX) | Model import and graph IR | `main` |
| `KairoTransformers` | [KairoTransformers](https://github.com/swayam8624/KairoTransformers) | Transformer model planning and runtime work | `main` |

## Build Options

| CMake option | Default | Purpose |
| --- | --- | --- |
| `KAIRO_GAME_ENGINE_BUILD_EDITOR` | `ON` | Build the native editor application |
| `KAIRO_GAME_ENGINE_BUILD_PLAYER` | `ON` | Build the standalone project validator and Vulkan player |
| `KAIRO_GAME_ENGINE_BUILD_RAYTRACER` | `ON` | Build the offline CPU ray tracer |
| `KAIRO_GAME_ENGINE_BUILD_PHYSICS_SANDBOX` | `ON` | Build terminal and GLFW physics sandboxes |
| `KAIRO_GAME_ENGINE_BUILD_AI_CLOUD` | `OFF` | Build KairoAI's pinned CPR OpenAI-compatible transport |
| `KAIRO_GAME_ENGINE_BUILD_COMPUTE_STACK` | `OFF` | Include the experimental ML/compute repositories |

Override an option during configuration when a narrower build is useful:

```bash
cmake --preset dev -DKAIRO_GAME_ENGINE_BUILD_EDITOR=OFF
```

## Submodule Development Workflow

Make and publish component changes inside the owning repository first:

```bash
cd KairoRenderer
git switch main
git add <files>
git commit -m "Describe renderer change"
git push origin main
cd ..
```

Then advance the reproducible component pin in this umbrella repository:

```bash
git add KairoRenderer
git commit -m "Update KairoRenderer integration"
git push origin main
```

Pull an umbrella update and synchronize all pinned components with:

```bash
git pull --ff-only
git submodule update --init --recursive
```

Do not use `git submodule update --remote` as a routine pull command. It moves
components to branch heads that the umbrella repository has not yet integrated
or verified.

## Integration Status

The current umbrella baseline configures and builds all runtime, editor,
ray-tracing, and sandbox targets in one Ninja superbuild. Its 21 registered
cross-repository test targets pass from the root `dev-clang` preset. GitHub
Actions separately validates the pinned submodule graph on Ubuntu/macOS Clang
and Windows MSVC, then creates a source-package smoke artifact. This is an
integration result for the pinned commits, not a substitute for component-level
tests and platform coverage.

## License

See [LICENSE](LICENSE).


## Wave A + Wave B Completion

The first completion campaign is locked. Foundation certification (Math,
Geometry, PhysicsMath, PipelineCore, Blender) is at the frozen 95% scope;
runtime infrastructure (ECS, Reflection, Scheduler, bounded Metal KairoGPU v1)
is at the frozen 80% scope.

The engine now has an explicit `Kairo.SceneECSBridge`: EngineCore Scene remains
the persistent/editor source of truth, while KairoECS is a process-local
execution layout with stable authored identity retained as data rather than
serializing ECS handles.

See [the Wave A/B completion ledger](docs/WAVE_A_B_COMPLETION.md) for exact
component pins, evidence, research tracks, limitations and verification
commands.
