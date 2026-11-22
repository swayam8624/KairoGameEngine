# Teaching source map — KairoGameEngine

**Cross-repository integration status:** [31 historic documentation PR reconciliation ledger](DOCUMENTATION_MERGE_RECONCILIATION_20261122.md). This distinguishes already-integrated mainline teaching from unresolved protected/conflicting PRs.

**Latest integration teaching pass:** [asset/scene-to-GPU annotations and test evidence ledger](ASSET_SCENE_GPU_TEACHING_PROGRESS.md).

**Cross-repository integration lab:** [Asset/scene-to-GPU handle ownership, glTF animation and test evidence](ASSET_SCENE_TO_GPU_LIFETIME_LAB.md). Read the authored/CPU contracts before following the native GPU backend; source inspection is not a native test result.

**Recovered deep tutorial:** [KairoPlayer launch-to-frame execution](PLAYER_EXECUTION_EXPLAINED.md) — source-linked teaching, not a verified test result.

**Historical full source inventory:** [92 linked first-party source/build paths](SOURCE_CATALOG.md) (November 21, 2026 snapshot; verify changed paths against current main).


> A guided source-reading itinerary. It is not a test result or claim that every file is documented yet.

## How to use this guide

Start with the new [complete Kairo integration walkthrough](ENGINE_INTEGRATION_WALKTHROUGH.md).
It follows one project from portable `.kproject` and `.kscene` files through
the Player's shared realtime render bridge, physics adapter, native frame
loop, and exact-head workspace acceptance.

KairoGameEngine is a sibling-repository CMake superbuild. Read integration contracts first; each component implementation lives in another repository.

Read each phase in order. Every link goes to a source file or directory in this repository. For a directory, inspect its public interface and then its implementation files. After each phase, identify the caller, the state it owns, the inputs it validates, and its observable outputs.

## 1. Trust and bootstrap

1. [`README.md`](../README.md)
2. [`STATUS.md`](../STATUS.md)
3. [`STATUS.yaml`](../STATUS.yaml)
4. [`workspace.lock`](../workspace.lock)
5. [`CMakeLists.txt`](../CMakeLists.txt)
6. [`CMakePresets.json`](../CMakePresets.json)
7. [`scripts/bootstrap_workspace.sh`](../scripts/bootstrap_workspace.sh)

## 2. Architecture and acceptance

1. [`docs/DEVELOPMENT_MAP.md`](../docs/DEVELOPMENT_MAP.md)
2. [`docs/V1_95_COMPLETION.md`](../docs/V1_95_COMPLETION.md)
3. [`docs/status/exact-head-ci-audit.md`](../docs/status/exact-head-ci-audit.md)
4. [`scripts/run_portfolio_acceptance.sh`](../scripts/run_portfolio_acceptance.sh)
5. [`scripts/verify_portfolio_95.sh`](../scripts/verify_portfolio_95.sh)

**Follow one entity all the way through:** [Project bootstrap, persistent IDs, ECS extraction, fixed physics, streaming fragments, glTF skinning and rendered frame](AUTHORED_ENTITY_TO_PLAYER_FRAME_LAB.md). This is a source trace and exercise, not native-host verification.

## 3. Runtime and gameplay

1. [`Runtime`](../Runtime)
2. [`Samples/SharedContentShowcase`](../Samples/SharedContentShowcase)
3. [`Samples/Phase1Game`](../Samples/Phase1Game)
4. [`Samples/PlayableVerticalSlice`](../Samples/PlayableVerticalSlice)
5. [`Samples/KairoRacingGame`](../Samples/KairoRacingGame)

## 4. Integration tests and CI

1. [`tests/SharedContentContractTests.cpp`](../tests/SharedContentContractTests.cpp)
2. [`tests/SceneECSBridgeTests.cpp`](../tests/SceneECSBridgeTests.cpp)
3. [`tests/CompatibilityContentTests.cpp`](../tests/CompatibilityContentTests.cpp)
4. [`tests/ComputeStackIntegration.cpp`](../tests/ComputeStackIntegration.cpp)
5. [`.github/workflows/build-test.yml`](../.github/workflows/build-test.yml)
6. [`.github/workflows/portfolio-ci-audit.yml`](../.github/workflows/portfolio-ci-audit.yml)

## End-to-end interaction map

`workspace.lock` selects exact component SHAs; CMake wires sibling targets. Assets/Scene descriptors are read by EngineCore and adapted to KairoRenderer and KairoPhysicsEngine. KairoEditor authors the same contract the Player runs. KairoHub launches validated projects. Cross-repository changes must be tested at their locked heads, not merely at independent main branches.

## APIs and code-reading questions

For each function, distinguish **contract** (preconditions, postconditions, failure modes), **implementation** (state/algorithms), **collaborators** (imports, calls and consumers), **lifetime/ownership**, and **verification** (test or evidence that checks it). For C++ modules, `export module` names the public compiled interface and `import` expresses module dependencies; an implementation unit's `module ...;` contributes definitions without exporting them.

## Boundaries and honesty

The 95% figure is an **unverified target**, not completion. Weekly CI audit without `--strict` does not certify the portfolio. Native host and packaging gates remain separate.

## Documentation rules

- Teaching comments describe *why*, invariants, edge cases, API ownership and cross-file effects; avoid narrating obvious assignments.
- Preserve behavior during documentation-only edits; keep correctness changes in separate reviewed commits.
- Link function names and tests where possible; mark claims as source-present, CI-observed, host-verified or research-evidenced.
- License grants apply only to material for which the repository owner holds rights; third-party source, models, media and datasets keep their own notices.


## Complete source-file inventory

After the curated teaching path above, use this ordered checklist to reach every first-party source, script, shader and test file indexed in this branch. Group order follows concepts before operations, tooling and evidence; follow module imports when a different within-group order is more helpful. Vendored/dependency code, binaries, data and generated artifacts are excluded, not relicensed.

### Entry points and build (10)

1. [`CMakeLists.txt`](../CMakeLists.txt)
2. [`Migration/KairoBridge/CMakeLists.txt`](../Migration/KairoBridge/CMakeLists.txt)
3. [`Runtime/KairoPlayer/CMakeLists.txt`](../Runtime/KairoPlayer/CMakeLists.txt)
4. [`Runtime/KairoPlayer/main.cpp`](../Runtime/KairoPlayer/main.cpp)
5. [`Runtime/KairoRealtimeRenderBridge/CMakeLists.txt`](../Runtime/KairoRealtimeRenderBridge/CMakeLists.txt)
6. [`Runtime/KairoRenderBridge/CMakeLists.txt`](../Runtime/KairoRenderBridge/CMakeLists.txt)
7. [`Samples/KairoRacingGame/CMakeLists.txt`](../Samples/KairoRacingGame/CMakeLists.txt)
8. [`Samples/KairoRacingGame/main.cpp`](../Samples/KairoRacingGame/main.cpp)
9. [`Samples/Phase1Game/CMakeLists.txt`](../Samples/Phase1Game/CMakeLists.txt)
10. [`Samples/Phase1Game/main.cpp`](../Samples/Phase1Game/main.cpp)

### Interfaces and modules (34)

1. [`Migration/KairoBridge/CanonicalIR.cppm`](../Migration/KairoBridge/CanonicalIR.cppm)
2. [`Migration/KairoBridge/GodotAdapter.cppm`](../Migration/KairoBridge/GodotAdapter.cppm)
3. [`Migration/KairoBridge/KairoBridge.cppm`](../Migration/KairoBridge/KairoBridge.cppm)
4. [`Migration/KairoBridge/MigrationManifest.cppm`](../Migration/KairoBridge/MigrationManifest.cppm)
5. [`Migration/KairoBridge/MigrationTypes.cppm`](../Migration/KairoBridge/MigrationTypes.cppm)
6. [`Migration/KairoBridge/SourceProject.cppm`](../Migration/KairoBridge/SourceProject.cppm)
7. [`Migration/KairoBridge/UnityAdapter.cppm`](../Migration/KairoBridge/UnityAdapter.cppm)
8. [`Runtime/KairoPlayer/RuntimeAudioBridge.cppm`](../Runtime/KairoPlayer/RuntimeAudioBridge.cppm)
9. [`Runtime/KairoPlayer/RuntimeCharacterMotorBridge.cppm`](../Runtime/KairoPlayer/RuntimeCharacterMotorBridge.cppm)
10. [`Runtime/KairoPlayer/RuntimeCrowdNavigationBridge.cppm`](../Runtime/KairoPlayer/RuntimeCrowdNavigationBridge.cppm)
11. [`Runtime/KairoPlayer/RuntimeEntityLifecycleBridge.cppm`](../Runtime/KairoPlayer/RuntimeEntityLifecycleBridge.cppm)
12. [`Runtime/KairoPlayer/RuntimeInputBridge.cppm`](../Runtime/KairoPlayer/RuntimeInputBridge.cppm)
13. [`Runtime/KairoPlayer/RuntimeLocomotionAnimationBridge.cppm`](../Runtime/KairoPlayer/RuntimeLocomotionAnimationBridge.cppm)
14. [`Runtime/KairoPlayer/RuntimeLogicBridge.cppm`](../Runtime/KairoPlayer/RuntimeLogicBridge.cppm)
15. [`Runtime/KairoPlayer/RuntimeNativeGameplayBridge.cppm`](../Runtime/KairoPlayer/RuntimeNativeGameplayBridge.cppm)
16. [`Runtime/KairoPlayer/RuntimeNavigationAgentBridge.cppm`](../Runtime/KairoPlayer/RuntimeNavigationAgentBridge.cppm)
17. [`Runtime/KairoPlayer/RuntimePackaging.cppm`](../Runtime/KairoPlayer/RuntimePackaging.cppm)
18. [`Runtime/KairoPlayer/RuntimePhysicsBridge.cppm`](../Runtime/KairoPlayer/RuntimePhysicsBridge.cppm)
19. [`Runtime/KairoPlayer/RuntimePlayerControllerBridge.cppm`](../Runtime/KairoPlayer/RuntimePlayerControllerBridge.cppm)
20. [`Runtime/KairoPlayer/RuntimeProductionSystemsBridge.cppm`](../Runtime/KairoPlayer/RuntimeProductionSystemsBridge.cppm)
21. [`Runtime/KairoPlayer/RuntimeProject.cppm`](../Runtime/KairoPlayer/RuntimeProject.cppm)
22. [`Runtime/KairoPlayer/RuntimeRenderBridge.cppm`](../Runtime/KairoPlayer/RuntimeRenderBridge.cppm)
23. [`Runtime/KairoPlayer/RuntimeSaveGameBridge.cppm`](../Runtime/KairoPlayer/RuntimeSaveGameBridge.cppm)
24. [`Runtime/KairoPlayer/RuntimeSceneAudioBridge.cppm`](../Runtime/KairoPlayer/RuntimeSceneAudioBridge.cppm)
25. [`Runtime/KairoPlayer/RuntimeShippingBridge.cppm`](../Runtime/KairoPlayer/RuntimeShippingBridge.cppm)
26. [`Runtime/KairoPlayer/RuntimeWorldSaveBridge.cppm`](../Runtime/KairoPlayer/RuntimeWorldSaveBridge.cppm)
27. [`Runtime/KairoPlayer/RuntimeWorldStreamingBridge.cppm`](../Runtime/KairoPlayer/RuntimeWorldStreamingBridge.cppm)
28. [`Runtime/KairoRealtimeRenderBridge/RealtimeSceneBridge.cppm`](../Runtime/KairoRealtimeRenderBridge/RealtimeSceneBridge.cppm)
29. [`Runtime/KairoRenderBridge/EditorOfflineRenderService.cppm`](../Runtime/KairoRenderBridge/EditorOfflineRenderService.cppm)
30. [`Runtime/KairoRenderBridge/MaterialPreviewBridge.cppm`](../Runtime/KairoRenderBridge/MaterialPreviewBridge.cppm)
31. [`Runtime/KairoRenderBridge/OfflineRenderSession.cppm`](../Runtime/KairoRenderBridge/OfflineRenderSession.cppm)
32. [`Runtime/KairoRenderBridge/SceneSnapshotBridge.cppm`](../Runtime/KairoRenderBridge/SceneSnapshotBridge.cppm)
33. [`Runtime/KairoSceneECSBridge.cppm`](../Runtime/KairoSceneECSBridge.cppm)
34. [`Samples/Phase1Game/Phase1GameState.cppm`](../Samples/Phase1Game/Phase1GameState.cppm)

### Implementation and other source (1)

1. [`Runtime/KairoRenderBridge/MaterialPreviewBridge.cpp`](../Runtime/KairoRenderBridge/MaterialPreviewBridge.cpp)

### Automation and tooling (18)

1. [`Samples/KairoRacingGame/tools/export_glb.py`](../Samples/KairoRacingGame/tools/export_glb.py)
2. [`Samples/KairoRacingGame/tools/prepare_assets.sh`](../Samples/KairoRacingGame/tools/prepare_assets.sh)
3. [`scripts/audit_ecosystem_ci.py`](../scripts/audit_ecosystem_ci.py)
4. [`scripts/bootstrap_workspace.sh`](../scripts/bootstrap_workspace.sh)
5. [`scripts/build_and_test.sh`](../scripts/build_and_test.sh)
6. [`scripts/materialize_workspace_lock.sh`](../scripts/materialize_workspace_lock.sh)
7. [`scripts/migrate_from_submodules.sh`](../scripts/migrate_from_submodules.sh)
8. [`scripts/repair_legacy_generated_artifacts.sh`](../scripts/repair_legacy_generated_artifacts.sh)
9. [`scripts/run_authoring_workflow.sh`](../scripts/run_authoring_workflow.sh)
10. [`scripts/run_compute_stack_campaign.sh`](../scripts/run_compute_stack_campaign.sh)
11. [`scripts/run_flagship_campaign.sh`](../scripts/run_flagship_campaign.sh)
12. [`scripts/run_kairo_racing_local.sh`](../scripts/run_kairo_racing_local.sh)
13. [`scripts/run_portfolio_acceptance.sh`](../scripts/run_portfolio_acceptance.sh)
14. [`scripts/run_wave_c_scale_campaign.sh`](../scripts/run_wave_c_scale_campaign.sh)
15. [`scripts/sync_workspace.sh`](../scripts/sync_workspace.sh)
16. [`scripts/validate_and_run_kairo_project.sh`](../scripts/validate_and_run_kairo_project.sh)
17. [`scripts/verify_portfolio_95.sh`](../scripts/verify_portfolio_95.sh)
18. [`scripts/verify_workspace_lock.sh`](../scripts/verify_workspace_lock.sh)

### Verification and tests (30)

1. [`Migration/KairoBridge/tests/DurableIdentityTests.cpp`](../Migration/KairoBridge/tests/DurableIdentityTests.cpp)
2. [`Migration/KairoBridge/tests/GodotAdapterTests.cpp`](../Migration/KairoBridge/tests/GodotAdapterTests.cpp)
3. [`Migration/KairoBridge/tests/KairoBridgeTests.cpp`](../Migration/KairoBridge/tests/KairoBridgeTests.cpp)
4. [`Runtime/KairoPlayer/tests/RuntimeAudioBridgeTests.cpp`](../Runtime/KairoPlayer/tests/RuntimeAudioBridgeTests.cpp)
5. [`Runtime/KairoPlayer/tests/RuntimeCharacterMotorBridgeTests.cpp`](../Runtime/KairoPlayer/tests/RuntimeCharacterMotorBridgeTests.cpp)
6. [`Runtime/KairoPlayer/tests/RuntimeCrowdNavigationBridgeTests.cpp`](../Runtime/KairoPlayer/tests/RuntimeCrowdNavigationBridgeTests.cpp)
7. [`Runtime/KairoPlayer/tests/RuntimeEntityLifecycleBridgeTests.cpp`](../Runtime/KairoPlayer/tests/RuntimeEntityLifecycleBridgeTests.cpp)
8. [`Runtime/KairoPlayer/tests/RuntimeLocomotionAnimationBridgeTests.cpp`](../Runtime/KairoPlayer/tests/RuntimeLocomotionAnimationBridgeTests.cpp)
9. [`Runtime/KairoPlayer/tests/RuntimeNativeGameplayTests.cpp`](../Runtime/KairoPlayer/tests/RuntimeNativeGameplayTests.cpp)
10. [`Runtime/KairoPlayer/tests/RuntimeNavigationAgentBridgeTests.cpp`](../Runtime/KairoPlayer/tests/RuntimeNavigationAgentBridgeTests.cpp)
11. [`Runtime/KairoPlayer/tests/RuntimePhysicsTopologyTests.cpp`](../Runtime/KairoPlayer/tests/RuntimePhysicsTopologyTests.cpp)
12. [`Runtime/KairoPlayer/tests/RuntimePlayerControllerBridgeTests.cpp`](../Runtime/KairoPlayer/tests/RuntimePlayerControllerBridgeTests.cpp)
13. [`Runtime/KairoPlayer/tests/RuntimeProductionSystemsTests.cpp`](../Runtime/KairoPlayer/tests/RuntimeProductionSystemsTests.cpp)
14. [`Runtime/KairoPlayer/tests/RuntimeProjectTests.cpp`](../Runtime/KairoPlayer/tests/RuntimeProjectTests.cpp)
15. [`Runtime/KairoPlayer/tests/RuntimeSaveGameBridgeTests.cpp`](../Runtime/KairoPlayer/tests/RuntimeSaveGameBridgeTests.cpp)
16. [`Runtime/KairoPlayer/tests/RuntimeSceneAudioBridgeTests.cpp`](../Runtime/KairoPlayer/tests/RuntimeSceneAudioBridgeTests.cpp)
17. [`Runtime/KairoPlayer/tests/RuntimeWorldSaveBridgeTests.cpp`](../Runtime/KairoPlayer/tests/RuntimeWorldSaveBridgeTests.cpp)
18. [`Runtime/KairoPlayer/tests/RuntimeWorldStreamingBridgeTests.cpp`](../Runtime/KairoPlayer/tests/RuntimeWorldStreamingBridgeTests.cpp)
19. [`Runtime/KairoPlayer/tests/RuntimeWorldStreamingPhysicsTests.cpp`](../Runtime/KairoPlayer/tests/RuntimeWorldStreamingPhysicsTests.cpp)
20. [`Runtime/KairoRealtimeRenderBridge/tests/RealtimeSceneBridgeTests.cpp`](../Runtime/KairoRealtimeRenderBridge/tests/RealtimeSceneBridgeTests.cpp)
21. [`Runtime/KairoRenderBridge/tests/EditorOfflineRenderServiceTests.cpp`](../Runtime/KairoRenderBridge/tests/EditorOfflineRenderServiceTests.cpp)
22. [`Runtime/KairoRenderBridge/tests/MaterialPreviewBridgeTests.cpp`](../Runtime/KairoRenderBridge/tests/MaterialPreviewBridgeTests.cpp)
23. [`Runtime/KairoRenderBridge/tests/SceneSnapshotBridgeTests.cpp`](../Runtime/KairoRenderBridge/tests/SceneSnapshotBridgeTests.cpp)
24. [`Samples/Phase1Game/tests/Phase1GameStateTests.cpp`](../Samples/Phase1Game/tests/Phase1GameStateTests.cpp)
25. [`scripts/tests/test_audit_ecosystem_ci.py`](../scripts/tests/test_audit_ecosystem_ci.py)
26. [`tests/CompatibilityContentTests.cpp`](../tests/CompatibilityContentTests.cpp)
27. [`tests/ComputeStackIntegration.cpp`](../tests/ComputeStackIntegration.cpp)
28. [`tests/SceneECSBridgeTests.cpp`](../tests/SceneECSBridgeTests.cpp)
29. [`tests/SharedContentContractTests.cpp`](../tests/SharedContentContractTests.cpp)
30. [`tests/WaveCScaleBenchmark.cpp`](../tests/WaveCScaleBenchmark.cpp)

Index snapshot: 93 first-party source/build/test files among 186 tracked files. The curated list remains the conceptual starting point. This inventory alone does not mean those functions have already been annotated or tested.
