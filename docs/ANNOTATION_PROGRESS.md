# Teaching overhaul — progress and acceptance ledger

**Snapshot:** 21 November 2026  
**Working branch across all repositories:** `docs/teaching-walkthrough-20261121`  
**Original main/master branches:** intentionally unchanged

## Goal

Transform the Kairo engine ecosystem, NES emulator, Maveb, Vulkax and MLLibrary into inspectable, teachable open-source projects. The target is **every first-party code file**, with function/API comments that explain algorithms, invariants, ownership, data flow, callers, consumers, limitations and verification—not simply a comment at the beginning of each file.

## Completed in the working branches

- Linked source-reading guides exist in all **31** repositories (27 Kairo, NES, MAVEB, VULKAX, MLLibrary).
- All **31** README files link to their source maps and licensing/status constraints.
- Guides have curated conceptual paths followed by linked **complete first-party code/build/test file inventories**: **1,787** indexed files at the branch snapshot.
- Curated links checked against all 31 GitHub trees: **0 missing**. Full inventories were generated directly from tracked tree paths.
- Missing root permissive licenses were added while retaining existing Apache/MIT terms. The NES grant covers original contributions only and a separate third-party provenance audit is mandatory.
- Portfolio entry: [PORTFOLIO_READING_ORDER.md](PORTFOLIO_READING_ORDER.md).
- KairoSpatial extension: **24/24** first-party files have file-level teaching annotations; selected spatial search algorithms have targeted API comments and a linked narrative walkthrough. No new local CTest/visualizer run has been performed.
- Reflection + Scheduler extension: **9/9** first-party source/build/test files now have first-pass teaching notes and new walkthroughs; compiler and CI acceptance remain pending.
- Connected foundation course: [FOUNDATIONS_STUDY_PATH.md](FOUNDATIONS_STUDY_PATH.md); all **65/65** indexed first-party files in KairoMath, KairoGeometry and KairoPhysicsMath now have introductory teaching comments (not complete individual API coverage).

## Detailed source annotation milestones

| Repository | Current depth | Not yet completed |
|---|---|---|
| [NESEmulator](https://github.com/swayam8624/NESEmulator/tree/docs/teaching-walkthrough-20261121) | 186 `TEACHING API` comments across Bus, Cartridge, CPU, PPU, APU, host application and seven mapper files; 14 more diagnostic-demo method comments; teaching build commentary and [hardware walkthrough](https://github.com/swayam8624/NESEmulator/blob/docs/teaching-walkthrough-20261121/docs/NES_ARCHITECTURE_GUIDE.md) | Upstream PixelGameEngine attribution, tests/CI, correctness fixes and any remaining internal helper explanations |
| KairoMath | **32/32** indexed source/build/test files carry an introductory teaching annotation; selected vector, matrix, quaternion, TRS, solve, Eigen/SVD, optimization, Tensor, autodiff and GPU methods have deeper explanations; [full walkthrough](https://github.com/swayam8624/KairoMath/blob/docs/teaching-walkthrough-20261121/docs/MATH_FOUNDATIONS_WALKTHROUGH.md) | Review the remaining individual algorithms/functions, run scalar and optional Scheduler/SIMD/Metal tests, certify numerical limits and release inverse behavior |
| KairoGeometry | **20/20** indexed source/build/test files annotated; selected ray, line, segment, plane, triangle, sphere, capsule, AABB, OBB, frustum and SAT APIs explained; [full walkthrough](https://github.com/swayam8624/KairoGeometry/blob/docs/teaching-walkthrough-20261121/docs/GEOMETRY_WALKTHROUGH.md) | Deeper function-by-function review of intersection edge cases, native tests and numerical/visual cross-validation |
| KairoPhysicsMath | **13/13** indexed source/build/test files annotated, with formula-specific comments on mass, inertia, impulse, constraints, integrators and Catch2 cases; [full walkthrough](https://github.com/swayam8624/KairoPhysicsMath/blob/docs/teaching-walkthrough-20261121/docs/PHYSICS_MATH_WALKTHROUGH.md) | Full function review, CMake/CTest on supported hosts and wider compound inertia/solver oracle validation |
| KairoReflection | **5/5** first-party code/build/test files annotated; metadata kinds, type registry read/write, member adapters and six Catch2 cases explained; [walkthrough](https://github.com/swayam8624/KairoReflection/blob/docs/teaching-walkthrough-20261121/docs/REFLECTION_WALKTHROUGH.md) | Execute CMake/CTest; review remaining private validation helpers and mutation rollback behavior |
| KairoScheduler | **4/4** first-party code/build/test files annotated; range arithmetic, thread pool, TaskGroup, topological TaskGraph and cooperative stop behavior explained; [walkthrough](https://github.com/swayam8624/KairoScheduler/blob/docs/teaching-walkthrough-20261121/docs/SCHEDULER_WALKTHROUGH.md) | Run smoke/benchmark, inspect cancellation/oversubscription edge cases on supported hosts |
| KairoSpatial | **24/24** first-party source/build/test/visual files have introductory annotations; selected BVH, dynamic AABB tree, SAP, A*, NavMesh corridor/funnel APIs deeply explained; [walkthrough](https://github.com/swayam8624/KairoSpatial/blob/docs/teaching-walkthrough-20261121/docs/SPATIAL_WALKTHROUGH.md) | Complete private algorithm/function reviews, test native build and compare accelerated queries with independent brute-force oracles |
| KairoPhysicsEngine | Broadphase indexing, pair/filter/query method contracts | PhysicsWorld, narrowphase, solver, joints, controllers, engine/world tests |
| KairoEditor | EditorState methods including state transitions and scene lifetime | Document compiler, undo/redo transaction core, EditorShell, runtime UI tests |
| KairoRenderer | Backend-neutral RendererRuntime API and resource boundaries | Native GPU backends, synchronization, render graph, shaders, graphics tests |
| KairoAssets | AssetRegistry functions, typed handles, transactional manifest semantics | Importers, cache, scene/texture/mesh artifacts, source watcher |
| KairoEngineCore | ProjectDescriptor parsing, serialization, validation and atomic persistence | Scene model, gameplay runtime, jobs, reflection, animation, streaming |
| KairoGameEngine | Player entry-point/composition explanation | Runtime bridges, samples, tests, packaging and exact-head integration |
| Maveb | CapturedWorldRevisionGraph CBRC evidence adapter | Hundreds of other implementation, test, shader and research automation files |
| Vulkax | Result certificate state machine | Counterfactual pipeline, solvers, models, experiments and wider tests |
| KairoECS | All 7 current first-party code/build/test files contain teaching annotations; 26 Registry and 15 SparseSet method notes, all 8 Catch2 test cases, benchmark and build guidance; [full walkthrough](https://github.com/swayam8624/KairoECS/blob/docs/teaching-walkthrough-20261121/docs/ECS_WALKTHROUGH.md) | Run CMake/CTest/benchmark, complete native integration and independent accuracy review before declaring accepted |
| Other Kairo components + MLLibrary | Initial source-file ownership and dependency maps | Most public and internal functions require detailed method annotations |

**Progress semantics:** source inventory completed is not source annotation completed. Comment counts are not tests or correctness evidence. A comment describing an assumption does not make the assumption true.

## Active technical caveats

1. **NES input safety:** Cartridge currently trusts incomplete header/read assumptions and may report unsupported mappers valid; Bus/Cartridge subsequently dereference mapper state. Separate correction and adversarial fixture tests are needed.
2. **NES legacy diagnostics:** `testing6502.cpp` does not attach a cartridge before bus writes; `testing2C02.cpp` has an absolute developer ROM path. Neither source is an active CMake test target.
3. **NES mapper ownership:** base Mapper destructor is not virtual, so raw deletion through a base pointer would be unsafe; current `make_shared<Derived>` to `shared_ptr<Mapper>` use retains its derived control block.
4. **NES provenance:** third-party/adapted PixelGameEngine and potentially other files require upstream license/credit verification before redistribution.
5. **Kairo acceptance:** 95% remains an unverified target. All 27 locked repository SHAs and native host smoke tests require a complete acceptance pass.
6. **MAVEB:** frozen calibrated-work reductions are not equivalent to measured full-runtime speedup. Nonzero-residual claims use separate evidence.
7. **VULKAX:** preserve both successful pendulum and failed free-fall validation; do not silently broaden trust-state claims.
8. **KairoMath fixed inverse:** singular fixed-size matrices can hit an assert and fall back to identity when assertions are disabled; identity is not a valid inverse. Transform inverse also requires supported uniform nonzero scale.
9. **Foundation native execution:** KairoMath's GPU/Scheduler bridges are optional; their source existence does not prove native performance or cross-platform GPU layout compatibility.

## Remaining stages, in execution order

1. Finish NES internal helper clarity, identify upstream attribution, create bounded parser/mapper/CPU/PPU tests in behavior-focused commits.
2. Review all remaining internal/math/geometry/physics methods beyond the now-complete **file-level** introductory teaching coverage, continue exhaustive function review and native acceptance in Math/Geometry/PhysicsMath/Reflection/Scheduler/Spatial; all now have file-level teaching coverage but no new test acceptance. KairoECS also has file-level coverage, but its CMake/CTest acceptance remains open.
3. Complete KairoAssets and EngineCore schemas, importers, persistence, state and asset-lifetime contracts.
4. Complete KairoRenderer and RayTracer render-graph, shaders, native backend interfaces, lighting, sampling and test explanations.
5. Complete KairoPhysicsEngine and KairoEditor execution flows; connect Editor/Player/Hub/samples to first-party engine contracts.
6. Complete Kairo ML/compute and production DCC bridges, then MLLibrary.
7. Complete MAVEB and VULKAX source annotations by experimental layer while keeping frozen evidence/manuscript boundaries immutable.
8. Run exact-head build, format, test, native smoke and README-link validation on supported hosts; audit third-party licenses, then review before any merge to main.

## Gate before declaring a repository complete

- Every first-party source file in the inventory reviewed and annotated at meaningful APIs and nontrivial algorithms.
- Public API contract, caller/callee and data ownership correctly explained, with at least one linked test/evidence path where possible.
- No undocumented behavior changes mixed with comment-only edits.
- README build/run steps reproduce on declared hosts; skipped native gates remain explicitly unsupported.
- All reading links valid on the branch's exact Git tree.
- Licenses, notices and asset rights reviewed rather than inferred.
- No research claim upgraded without associated, reproducible evidence.

**CI/build status:** no local compilation or native test run has been performed in this teaching pass. GitHub Actions shows no runs for the teaching branch at the last check. These are committed reviewable changes, not build acceptance.

## Policy for future passes

Follow [ANNOTATION_STYLE_GUIDE.md](ANNOTATION_STYLE_GUIDE.md). Work on the same named teaching branches and update this ledger as each repository gains complete coverage. Avoid auto-generated boilerplate; explanations should be written after reading implementations and tested against the actual code.
