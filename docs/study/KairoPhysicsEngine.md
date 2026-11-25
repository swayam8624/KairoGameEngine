# KairoPhysicsEngine — pinned file checklist

[Workspace study order](../../STUDY_ORDER.md) · [Locked repository](https://github.com/swayam8624/KairoPhysicsEngine/tree/b256df7327cfa0e4ae393e811cc3a661a17559e6)

35 source/configuration paths at `b256df7327cfa0e4ae393e811cc3a661a17559e6`. Read build contracts, interfaces, implementation, examples, tests, then automation. Inside each group follow actual imports/includes and callers; alphabetical ordering is not a proven topological sort. Inventory coverage is not annotation coverage.

## Build contracts

- [`CMakeLists.txt`](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/CMakeLists.txt)

## Module interfaces and headers

- [`Broadphase.cppm`](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/Broadphase.cppm)
- [`Buoyancy.cppm`](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/Buoyancy.cppm)
- [`CharacterController.cppm`](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/CharacterController.cppm)
- [`Collider.cppm`](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/Collider.cppm)
- [`ContactSolver.cppm`](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/ContactSolver.cppm)
- [`ConvexCollision.cppm`](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/ConvexCollision.cppm)
- [`Joint.cppm`](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/Joint.cppm)
- [`KairoPhysicsEngine.cppm`](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/KairoPhysicsEngine.cppm)
- [`Narrowphase.cppm`](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/Narrowphase.cppm)
- [`PhysicsDebug.cppm`](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/PhysicsDebug.cppm)
- [`PhysicsEngineTypes.cppm`](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/PhysicsEngineTypes.cppm)
- [`PhysicsMaterial.cppm`](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/PhysicsMaterial.cppm)
- [`PhysicsReplay.cppm`](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/PhysicsReplay.cppm)
- [`PhysicsSerialization.cppm`](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/PhysicsSerialization.cppm)
- [`PhysicsWorld.cppm`](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/PhysicsWorld.cppm)
- [`ProjectileSystem.cppm`](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/ProjectileSystem.cppm)
- [`RigidBody.cppm`](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/RigidBody.cppm)
- [`sandbox/DemoScene.cppm`](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/sandbox/DemoScene.cppm)
- [`sandbox/KairoPhysicsSandbox.cppm`](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/sandbox/KairoPhysicsSandbox.cppm)
- [`sandbox/PhysicsDebugRenderer.cppm`](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/sandbox/PhysicsDebugRenderer.cppm)
- [`sandbox/SandboxApp.cppm`](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/sandbox/SandboxApp.cppm)
- [`sandbox/SandboxTypes.cppm`](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/sandbox/SandboxTypes.cppm)
- [`sandbox/TerminalRenderer.cppm`](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/sandbox/TerminalRenderer.cppm)
- [`SolverIsland.cppm`](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/SolverIsland.cppm)
- [`VehicleSystem.cppm`](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/VehicleSystem.cppm)

## Implementations

- [`sandbox/KairoPhysicsGlfwSandbox.cpp`](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/sandbox/KairoPhysicsGlfwSandbox.cpp)

## Examples and samples

- [`sandbox/examples/main.cpp`](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/sandbox/examples/main.cpp)

## Tests and benchmarks

- [`tests/CharacterControllerTests.cpp`](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/tests/CharacterControllerTests.cpp)
- [`tests/PhysicsEngineTests.cpp`](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/tests/PhysicsEngineTests.cpp)
- [`tests/PhysicsStressTests.cpp`](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/tests/PhysicsStressTests.cpp)
- [`tests/VehicleSystemTests.cpp`](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/tests/VehicleSystemTests.cpp)

## Remaining configuration and tools

- [`.github/workflows/physics-ci.yml`](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/.github/workflows/physics-ci.yml)
- [`research/adaptive_collision_pipeline.yaml`](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/research/adaptive_collision_pipeline.yaml)
- [`STATUS.yaml`](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/STATUS.yaml)
