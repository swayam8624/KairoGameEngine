# KairoRayTracer — pinned file checklist

[Workspace study order](../../STUDY_ORDER.md) · [Locked repository](https://github.com/swayam8624/KairoRayTracer/tree/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e)

36 source/configuration paths at `99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e`. Read build contracts, interfaces, implementation, examples, tests, then automation. Inside each group follow actual imports/includes and callers; alphabetical ordering is not a proven topological sort. Inventory coverage is not annotation coverage.

## Build contracts

- [`CMakeLists.txt`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/CMakeLists.txt)

## Module interfaces and headers

- [`Camera.cppm`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/Camera.cppm)
- [`Color.cppm`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/Color.cppm)
- [`DepthIntegrator.cppm`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/DepthIntegrator.cppm)
- [`Environment.cppm`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/Environment.cppm)
- [`Film.cppm`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/Film.cppm)
- [`ImageIO.cppm`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/ImageIO.cppm)
- [`Integrator.cppm`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/Integrator.cppm)
- [`KairoRayTracer.cppm`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/KairoRayTracer.cppm)
- [`Light.cppm`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/Light.cppm)
- [`Material.cppm`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/Material.cppm)
- [`Mesh.cppm`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/Mesh.cppm)
- [`NormalIntegrator.cppm`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/NormalIntegrator.cppm)
- [`OBJLoader.cppm`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/OBJLoader.cppm)
- [`OfflineRenderJob.cppm`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/OfflineRenderJob.cppm)
- [`PathIntegrator.cppm`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/PathIntegrator.cppm)
- [`PBRIntegrator.cppm`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/PBRIntegrator.cppm)
- [`PreviewWindow.cppm`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/PreviewWindow.cppm)
- [`Primitive.cppm`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/Primitive.cppm)
- [`ProgressiveRenderer.cppm`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/ProgressiveRenderer.cppm)
- [`RayTracerTypes.cppm`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/RayTracerTypes.cppm)
- [`Renderer.cppm`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/Renderer.cppm)
- [`Sampler.cppm`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/Sampler.cppm)
- [`Scene.cppm`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/Scene.cppm)
- [`SceneParser.cppm`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/SceneParser.cppm)
- [`Shading.cppm`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/Shading.cppm)
- [`Texture.cppm`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/Texture.cppm)
- [`WhittedIntegrator.cppm`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/WhittedIntegrator.cppm)

## Examples and samples

- [`examples/cli.cpp`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/examples/cli.cpp)
- [`examples/preview.cpp`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/examples/preview.cpp)

## Tests and benchmarks

- [`tests/OfflineRenderJobTests.cpp`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/tests/OfflineRenderJobTests.cpp)
- [`tests/RayTracerTests.cpp`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/tests/RayTracerTests.cpp)
- [`tests/SharedLightSemanticsTests.cpp`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/tests/SharedLightSemanticsTests.cpp)

## Remaining configuration and tools

- [`.github/workflows/raytracer-ci.yml`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/.github/workflows/raytracer-ci.yml)
- [`research/cross_backend_differential_rendering.yaml`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/research/cross_backend_differential_rendering.yaml)
- [`STATUS.yaml`](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/STATUS.yaml)
