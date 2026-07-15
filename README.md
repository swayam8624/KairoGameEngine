# KairoGameEngine

`KairoGameEngine` is the integration repository for the Kairo engine workspace.
It pins each independently versioned Kairo library and tool as a Git submodule,
provides one CMake superbuild, and verifies that the complete stack compiles and
tests together.

The component repositories remain usable on their own. This repository does not
copy their source or merge their histories; each submodule commit is an explicit,
reproducible engine dependency version.

## Architecture

```text
KairoMath ------------------------------------------------------+
    +-> KairoGeometry -> KairoSpatial --------------------------+
    |                         +-> KairoRayTracer                 |
    +-> KairoPhysicsMath -> KairoPhysicsEngine -----------------+
                                                               |
KairoAssets -> KairoEngineCore ---------------------------------+
                                                               v
                         KairoRenderer -> KairoEditor -> game tools

Optional compute stack:
KairoSIMD + KairoScheduler + KairoGPU -> KairoONNX -> KairoTransformers
```

The aggregate CMake target `Kairo::GameEngine` exposes the runtime-facing
`KairoEngineCore`, `KairoRenderer`, and `KairoPhysicsEngine` targets. Authoring
tools and the offline ray tracer remain separate targets so their dependencies
do not leak into a shipped game runtime.

## Clone

Clone recursively so every pinned component is checked out:

```bash
git clone --recurse-submodules https://github.com/swayam8624/KairoGameEngine.git
cd KairoGameEngine
```

For an existing non-recursive clone:

```bash
git submodule update --init --recursive
```

## Prerequisites

The checked-in developer preset targets the current macOS/Homebrew toolchain:

- CMake 3.28 or newer
- Ninja
- Homebrew LLVM with C++23 module support
- GLFW, Vulkan headers/loader, MoltenVK, and `glslangValidator`

```bash
brew install cmake ninja llvm glfw vulkan-headers vulkan-loader molten-vk shaderc glslang
```

## Build And Test

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

The `dev` preset builds the real-time editor, CPU ray tracer, physics sandboxes,
and all registered tests. The optimized preset is:

```bash
cmake --preset release
cmake --build --preset release
```

The superbuild fails during configuration with the repair command when a
required submodule has not been initialized.

## Run

Launch the native editor with its starter project:

```bash
./build/dev/KairoEditor/KairoEditorApp \
  --project KairoEditor/examples/StarterProject/Project.kproject
```

Launch the interactive physics sandbox:

```bash
./build/dev/Foundation/KairoPhysicsEngine/KairoPhysicsGlfwSandbox
```

Render and preview a CPU ray-traced scene:

```bash
./build/dev/KairoRayTracer/KairoRayTracerPreview \
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
| `KairoAssets` | [KairoAssets](https://github.com/swayam8624/KairoAssets) | Asset identity, metadata, and project manifests | `main` |
| `KairoEngineCore` | [KairoEngineCore](https://github.com/swayam8624/KairoEngineCore) | Scene/runtime services and application contracts | `main` |
| `KairoRenderer` | [KairoRenderer](https://github.com/swayam8624/KairoRenderer) | Real-time Vulkan renderer and debug drawing | `main` |
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
| `KAIRO_GAME_ENGINE_BUILD_RAYTRACER` | `ON` | Build the offline CPU ray tracer |
| `KAIRO_GAME_ENGINE_BUILD_PHYSICS_SANDBOX` | `ON` | Build terminal and GLFW physics sandboxes |
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

The initial umbrella baseline configures and builds all runtime, editor,
ray-tracing, and sandbox targets in one 582-step Ninja build. Its 18 registered
cross-repository test targets pass from the root `dev` preset. This is an
integration result for the pinned commits, not a substitute for component-level
tests and platform coverage.

## License

See [LICENSE](LICENSE).
