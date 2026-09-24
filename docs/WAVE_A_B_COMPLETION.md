# KAIRO Wave A + Wave B Completion Ledger

Status date: 2026-09-24.

This ledger closes the first two execution waves of the KAIRO completion
program. Completion score and exact-head execution evidence are intentionally
separate: the score measures the frozen source/product scope, while a release is
called verified only when its stated tests run on that exact commit.

## Wave A — 95% foundation certification

| Repository | Frozen score | Certified snapshot SHA | Closing evidence |
| --- | ---: | --- | --- |
| KairoMath | 95 | `878187604d8d7ecc1af59f0c0e3988d80cccff0c` | deterministic numerical certification, portable optional-dependency discovery, parent-target reuse |
| KairoGeometry | 95 | `ddbdf3747d470890fc20d870528cb86d41be128f` | 25k seeded property certification and final certified Math pin |
| KairoPhysicsMath | 95 | `4f9f9e07079490f43a252e7a36d752a0c457a8ab` | invariant certification, non-shallow exact dependency fetch, final Math pin |
| KairoPipelineCore | 95 | `ad9f836dd0fefd009cfeb8292338612c3c6792a2` | non-mutating plan test, tamper protection, injected final-rename failure and rollback restoration |
| KairoBlender | 95 | `166fd07b2000efca12670801308d1ed5f6bbaf06` | native lifecycle suite extended to immutable publish, explicit replacement and dirty-scene provenance rejection |

Wave-A repositories are now feature-frozen inside their documented v1 scope.
Further work should be bug repair, portability, optimization evidence, or a
deliberate v2 proposal rather than feature-count expansion.

## Wave B — 80% runtime infrastructure

| Repository | Frozen score | Certified snapshot SHA | Closing evidence |
| --- | ---: | --- | --- |
| KairoECS | 80 | `f5af896ad6552350f2a43d9529be1f36d51403e9` | capacity control, smallest-pool 2/3-component joins, 100k runtime benchmark, Scene→ECS extraction boundary |
| KairoReflection | 80 | `1b6494a17146fc2d9404353bffb78efa2ef58c62` | vectors/quaternions/enums/references plus bounded homogeneous V3 arrays and collection validation |
| KairoScheduler | 80 | `c1e39bfbce9196617c02ebf7d780610676a15aa1` | cancellation-aware range execution, worker/task telemetry, deterministic benchmark |
| KairoGPU | 80 | `cfda41f18232b175fa8a89c2b923c134fa211cef` | frozen Metal-v1 scope, device-owned resource identity/lifetime, add/multiply/matmul, transfer/dispatch telemetry, regression benchmark |

KairoGPU's 80 score applies to the explicitly frozen **Metal compute v1** scope.
Vulkan/CUDA/WebGPU, generic resource binding, asynchronous queues and hardware
timestamp queries are v2 and therefore do not silently depress or inflate the
v1 score.

## Engine integration

The umbrella integrates `Kairo.SceneECSBridge`. EngineCore Scene stays the
durable authoring/persistence representation; KairoECS is process-local runtime
storage. Extraction retains stable authored IDs explicitly and never serializes
ECS index/generation pairs. A bridge test proves forward parent resolution,
stable ID mapping and explicit authoring-state refresh.

The final Wave-A/B umbrella integration commit is:

`1979fc9f4bde3f0fbc6b02e872d903aea5e8bc0b`

KairoGameEngine now consumes sibling repositories rather than Git submodules.
The SHAs above are certification snapshots, and `workspace.lock` records the
current exact integration snapshot without creating duplicate worktrees.

## Research tracks created during Wave B

**R2 MORPH-ECS** lives under `KairoECS/research/`. It has a locked problem,
adaptive-layout hypothesis, falsification criteria, fixed baselines, normal /
structural / mixed / adversarial workloads, scale matrix, metrics and required
figures.

**R7 Frame-Budget-Aware Heterogeneous Inference** lives under
`KairoGPU/research/`, with KairoScheduler as a supporting repository. It is
explicitly at the engineering-prerequisite stage until later ONNX/Transformer
waves are frozen.

Neither research track is marketed as proven. The production v1 contracts stay
stable while the experiments earn or reject later architectural changes.

## Exact-head verification gates

On a machine with the KAIRO toolchain:

```bash
# Umbrella: validates the integrated Foundation/ECS/Reflection bridge.
cmake --preset dev-clang
cmake --build --preset dev-clang --parallel
ctest --preset dev-clang --output-on-failure

# Optional Wave-B compute stack.
cmake --preset dev-clang -DKAIRO_GAME_ENGINE_BUILD_COMPUTE_STACK=ON
cmake --build --preset dev-clang --parallel
ctest --preset dev-clang --output-on-failure
```

Standalone repository certification commands are recorded in each
`STATUS.md` and `STATUS.yaml`. KairoBlender additionally requires its Blender
5.2 LTS headless native suite, while KairoGPU's native Metal smoke/benchmark
requires an Apple Metal host.

No current GitHub push-run status was exposed through the connected GitHub API
for these direct-main commits, so this ledger does not fabricate a green
exact-head CI run.


## Workspace layout migration

As of 2026-09-24 the integration repository no longer contains component
submodules. The canonical layout is one sibling checkout per repository under
the Kairo workspace. `scripts/migrate_from_submodules.sh` safely removes clean
legacy nested copies, and `scripts/verify_workspace_lock.sh` provides exact-SHA
integration verification without source duplication.
