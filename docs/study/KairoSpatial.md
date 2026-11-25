# KairoSpatial — pinned file checklist

[Workspace study order](../../STUDY_ORDER.md) · [Locked repository](https://github.com/swayam8624/KairoSpatial/tree/366379c290c79e6fffc6f84e4f4bb6832f9f91b3)

27 source/configuration paths at `366379c290c79e6fffc6f84e4f4bb6832f9f91b3`. Read build contracts, interfaces, implementation, examples, tests, then automation. Inside each group follow actual imports/includes and callers; alphabetical ordering is not a proven topological sort. Inventory coverage is not annotation coverage.

## Build contracts

- [`CMakeLists.txt`](https://github.com/swayam8624/KairoSpatial/blob/366379c290c79e6fffc6f84e4f4bb6832f9f91b3/CMakeLists.txt)

## Module interfaces and headers

- [`AStar.cppm`](https://github.com/swayam8624/KairoSpatial/blob/366379c290c79e6fffc6f84e4f4bb6832f9f91b3/AStar.cppm)
- [`BVH.cppm`](https://github.com/swayam8624/KairoSpatial/blob/366379c290c79e6fffc6f84e4f4bb6832f9f91b3/BVH.cppm)
- [`BVHBuilder.cppm`](https://github.com/swayam8624/KairoSpatial/blob/366379c290c79e6fffc6f84e4f4bb6832f9f91b3/BVHBuilder.cppm)
- [`BVHTraversal.cppm`](https://github.com/swayam8624/KairoSpatial/blob/366379c290c79e6fffc6f84e4f4bb6832f9f91b3/BVHTraversal.cppm)
- [`CrowdAvoidance.cppm`](https://github.com/swayam8624/KairoSpatial/blob/366379c290c79e6fffc6f84e4f4bb6832f9f91b3/CrowdAvoidance.cppm)
- [`DynamicAABBTree.cppm`](https://github.com/swayam8624/KairoSpatial/blob/366379c290c79e6fffc6f84e4f4bb6832f9f91b3/DynamicAABBTree.cppm)
- [`KairoSpatial.cppm`](https://github.com/swayam8624/KairoSpatial/blob/366379c290c79e6fffc6f84e4f4bb6832f9f91b3/KairoSpatial.cppm)
- [`KDTree.cppm`](https://github.com/swayam8624/KairoSpatial/blob/366379c290c79e6fffc6f84e4f4bb6832f9f91b3/KDTree.cppm)
- [`NavigationGraph.cppm`](https://github.com/swayam8624/KairoSpatial/blob/366379c290c79e6fffc6f84e4f4bb6832f9f91b3/NavigationGraph.cppm)
- [`NavMesh.cppm`](https://github.com/swayam8624/KairoSpatial/blob/366379c290c79e6fffc6f84e4f4bb6832f9f91b3/NavMesh.cppm)
- [`Octree.cppm`](https://github.com/swayam8624/KairoSpatial/blob/366379c290c79e6fffc6f84e4f4bb6832f9f91b3/Octree.cppm)
- [`Quadtree.cppm`](https://github.com/swayam8624/KairoSpatial/blob/366379c290c79e6fffc6f84e4f4bb6832f9f91b3/Quadtree.cppm)
- [`SpatialDebug.cppm`](https://github.com/swayam8624/KairoSpatial/blob/366379c290c79e6fffc6f84e4f4bb6832f9f91b3/SpatialDebug.cppm)
- [`SpatialHash.cppm`](https://github.com/swayam8624/KairoSpatial/blob/366379c290c79e6fffc6f84e4f4bb6832f9f91b3/SpatialHash.cppm)
- [`SpatialQuery.cppm`](https://github.com/swayam8624/KairoSpatial/blob/366379c290c79e6fffc6f84e4f4bb6832f9f91b3/SpatialQuery.cppm)
- [`SpatialTypes.cppm`](https://github.com/swayam8624/KairoSpatial/blob/366379c290c79e6fffc6f84e4f4bb6832f9f91b3/SpatialTypes.cppm)
- [`SweepAndPrune.cppm`](https://github.com/swayam8624/KairoSpatial/blob/366379c290c79e6fffc6f84e4f4bb6832f9f91b3/SweepAndPrune.cppm)

## Implementations

- [`visual/server.py`](https://github.com/swayam8624/KairoSpatial/blob/366379c290c79e6fffc6f84e4f4bb6832f9f91b3/visual/server.py)
- [`visual/SpatialVisualizer.cpp`](https://github.com/swayam8624/KairoSpatial/blob/366379c290c79e6fffc6f84e4f4bb6832f9f91b3/visual/SpatialVisualizer.cpp)

## Tests and benchmarks

- [`tests/CrowdAvoidanceTests.cpp`](https://github.com/swayam8624/KairoSpatial/blob/366379c290c79e6fffc6f84e4f4bb6832f9f91b3/tests/CrowdAvoidanceTests.cpp)
- [`tests/NavMeshTests.cpp`](https://github.com/swayam8624/KairoSpatial/blob/366379c290c79e6fffc6f84e4f4bb6832f9f91b3/tests/NavMeshTests.cpp)
- [`tests/ScaleStressTests.cpp`](https://github.com/swayam8624/KairoSpatial/blob/366379c290c79e6fffc6f84e4f4bb6832f9f91b3/tests/ScaleStressTests.cpp)
- [`tests/SpatialTests.cpp`](https://github.com/swayam8624/KairoSpatial/blob/366379c290c79e6fffc6f84e4f4bb6832f9f91b3/tests/SpatialTests.cpp)

## Remaining configuration and tools

- [`.github/workflows/spatial-ci.yml`](https://github.com/swayam8624/KairoSpatial/blob/366379c290c79e6fffc6f84e4f4bb6832f9f91b3/.github/workflows/spatial-ci.yml)
- [`research/adaptive_collision_pipeline.yaml`](https://github.com/swayam8624/KairoSpatial/blob/366379c290c79e6fffc6f84e4f4bb6832f9f91b3/research/adaptive_collision_pipeline.yaml)
- [`STATUS.yaml`](https://github.com/swayam8624/KairoSpatial/blob/366379c290c79e6fffc6f84e4f4bb6832f9f91b3/STATUS.yaml)
