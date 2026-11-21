# KairoGameEngine: complete linked source catalog

Read [READING_ORDER.md](../READING_ORDER.md) for the conceptual sequence, then use this catalog as a complete file index for the integration repository. This covers tracked source and build/scripts as of generation; sibling Kairo modules are separate repositories. Paths in a group are alphabetic; read the public module interface, implementation, and corresponding tests as one unit. Comments are being added in staged passes.

## Build & acceptance (17)

- [`scripts/audit_ecosystem_ci.py`](../scripts/audit_ecosystem_ci.py)
- [`scripts/bootstrap_workspace.sh`](../scripts/bootstrap_workspace.sh)
- [`scripts/build_and_test.sh`](../scripts/build_and_test.sh)
- [`scripts/materialize_workspace_lock.sh`](../scripts/materialize_workspace_lock.sh)
- [`scripts/migrate_from_submodules.sh`](../scripts/migrate_from_submodules.sh)
- [`scripts/repair_legacy_generated_artifacts.sh`](../scripts/repair_legacy_generated_artifacts.sh)
- [`scripts/run_authoring_workflow.sh`](../scripts/run_authoring_workflow.sh)
- [`scripts/run_compute_stack_campaign.sh`](../scripts/run_compute_stack_campaign.sh)
- [`scripts/run_flagship_campaign.sh`](../scripts/run_flagship_campaign.sh)
- [`scripts/run_kairo_racing_local.sh`](../scripts/run_kairo_racing_local.sh)
- [`scripts/run_portfolio_acceptance.sh`](../scripts/run_portfolio_acceptance.sh)
- [`scripts/run_wave_c_scale_campaign.sh`](../scripts/run_wave_c_scale_campaign.sh)
- [`scripts/sync_workspace.sh`](../scripts/sync_workspace.sh)
- [`scripts/tests/test_audit_ecosystem_ci.py`](../scripts/tests/test_audit_ecosystem_ci.py)
- [`scripts/validate_and_run_kairo_project.sh`](../scripts/validate_and_run_kairo_project.sh)
- [`scripts/verify_portfolio_95.sh`](../scripts/verify_portfolio_95.sh)
- [`scripts/verify_workspace_lock.sh`](../scripts/verify_workspace_lock.sh)

## Portable scene-to-render bridge (2)

- [`Runtime/KairoRealtimeRenderBridge/RealtimeSceneBridge.cppm`](../Runtime/KairoRealtimeRenderBridge/RealtimeSceneBridge.cppm)
- [`Runtime/KairoRealtimeRenderBridge/tests/RealtimeSceneBridgeTests.cpp`](../Runtime/KairoRealtimeRenderBridge/tests/RealtimeSceneBridgeTests.cpp)

## Player implementation and tests (37)

- [`Runtime/KairoPlayer/RuntimeAudioBridge.cppm`](../Runtime/KairoPlayer/RuntimeAudioBridge.cppm)
- [`Runtime/KairoPlayer/RuntimeCharacterMotorBridge.cppm`](../Runtime/KairoPlayer/RuntimeCharacterMotorBridge.cppm)
- [`Runtime/KairoPlayer/RuntimeCrowdNavigationBridge.cppm`](../Runtime/KairoPlayer/RuntimeCrowdNavigationBridge.cppm)
- [`Runtime/KairoPlayer/RuntimeEntityLifecycleBridge.cppm`](../Runtime/KairoPlayer/RuntimeEntityLifecycleBridge.cppm)
- [`Runtime/KairoPlayer/RuntimeInputBridge.cppm`](../Runtime/KairoPlayer/RuntimeInputBridge.cppm)
- [`Runtime/KairoPlayer/RuntimeLocomotionAnimationBridge.cppm`](../Runtime/KairoPlayer/RuntimeLocomotionAnimationBridge.cppm)
- [`Runtime/KairoPlayer/RuntimeLogicBridge.cppm`](../Runtime/KairoPlayer/RuntimeLogicBridge.cppm)
- [`Runtime/KairoPlayer/RuntimeNativeGameplayBridge.cppm`](../Runtime/KairoPlayer/RuntimeNativeGameplayBridge.cppm)
- [`Runtime/KairoPlayer/RuntimeNavigationAgentBridge.cppm`](../Runtime/KairoPlayer/RuntimeNavigationAgentBridge.cppm)
- [`Runtime/KairoPlayer/RuntimePackaging.cppm`](../Runtime/KairoPlayer/RuntimePackaging.cppm)
- [`Runtime/KairoPlayer/RuntimePhysicsBridge.cppm`](../Runtime/KairoPlayer/RuntimePhysicsBridge.cppm)
- [`Runtime/KairoPlayer/RuntimePlayerControllerBridge.cppm`](../Runtime/KairoPlayer/RuntimePlayerControllerBridge.cppm)
- [`Runtime/KairoPlayer/RuntimeProductionSystemsBridge.cppm`](../Runtime/KairoPlayer/RuntimeProductionSystemsBridge.cppm)
- [`Runtime/KairoPlayer/RuntimeProject.cppm`](../Runtime/KairoPlayer/RuntimeProject.cppm)
- [`Runtime/KairoPlayer/RuntimeRenderBridge.cppm`](../Runtime/KairoPlayer/RuntimeRenderBridge.cppm)
- [`Runtime/KairoPlayer/RuntimeSaveGameBridge.cppm`](../Runtime/KairoPlayer/RuntimeSaveGameBridge.cppm)
- [`Runtime/KairoPlayer/RuntimeSceneAudioBridge.cppm`](../Runtime/KairoPlayer/RuntimeSceneAudioBridge.cppm)
- [`Runtime/KairoPlayer/RuntimeShippingBridge.cppm`](../Runtime/KairoPlayer/RuntimeShippingBridge.cppm)
- [`Runtime/KairoPlayer/RuntimeWorldSaveBridge.cppm`](../Runtime/KairoPlayer/RuntimeWorldSaveBridge.cppm)
- [`Runtime/KairoPlayer/RuntimeWorldStreamingBridge.cppm`](../Runtime/KairoPlayer/RuntimeWorldStreamingBridge.cppm)
- [`Runtime/KairoPlayer/main.cpp`](../Runtime/KairoPlayer/main.cpp)
- [`Runtime/KairoPlayer/tests/RuntimeAudioBridgeTests.cpp`](../Runtime/KairoPlayer/tests/RuntimeAudioBridgeTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimeCharacterMotorBridgeTests.cpp`](../Runtime/KairoPlayer/tests/RuntimeCharacterMotorBridgeTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimeCrowdNavigationBridgeTests.cpp`](../Runtime/KairoPlayer/tests/RuntimeCrowdNavigationBridgeTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimeEntityLifecycleBridgeTests.cpp`](../Runtime/KairoPlayer/tests/RuntimeEntityLifecycleBridgeTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimeLocomotionAnimationBridgeTests.cpp`](../Runtime/KairoPlayer/tests/RuntimeLocomotionAnimationBridgeTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimeNativeGameplayTests.cpp`](../Runtime/KairoPlayer/tests/RuntimeNativeGameplayTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimeNavigationAgentBridgeTests.cpp`](../Runtime/KairoPlayer/tests/RuntimeNavigationAgentBridgeTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimePhysicsTopologyTests.cpp`](../Runtime/KairoPlayer/tests/RuntimePhysicsTopologyTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimePlayerControllerBridgeTests.cpp`](../Runtime/KairoPlayer/tests/RuntimePlayerControllerBridgeTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimeProductionSystemsTests.cpp`](../Runtime/KairoPlayer/tests/RuntimeProductionSystemsTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimeProjectTests.cpp`](../Runtime/KairoPlayer/tests/RuntimeProjectTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimeSaveGameBridgeTests.cpp`](../Runtime/KairoPlayer/tests/RuntimeSaveGameBridgeTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimeSceneAudioBridgeTests.cpp`](../Runtime/KairoPlayer/tests/RuntimeSceneAudioBridgeTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimeWorldSaveBridgeTests.cpp`](../Runtime/KairoPlayer/tests/RuntimeWorldSaveBridgeTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimeWorldStreamingBridgeTests.cpp`](../Runtime/KairoPlayer/tests/RuntimeWorldStreamingBridgeTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimeWorldStreamingPhysicsTests.cpp`](../Runtime/KairoPlayer/tests/RuntimeWorldStreamingPhysicsTests.cpp)

## Offline render bridges (8)

- [`Runtime/KairoRenderBridge/EditorOfflineRenderService.cppm`](../Runtime/KairoRenderBridge/EditorOfflineRenderService.cppm)
- [`Runtime/KairoRenderBridge/MaterialPreviewBridge.cpp`](../Runtime/KairoRenderBridge/MaterialPreviewBridge.cpp)
- [`Runtime/KairoRenderBridge/MaterialPreviewBridge.cppm`](../Runtime/KairoRenderBridge/MaterialPreviewBridge.cppm)
- [`Runtime/KairoRenderBridge/OfflineRenderSession.cppm`](../Runtime/KairoRenderBridge/OfflineRenderSession.cppm)
- [`Runtime/KairoRenderBridge/SceneSnapshotBridge.cppm`](../Runtime/KairoRenderBridge/SceneSnapshotBridge.cppm)
- [`Runtime/KairoRenderBridge/tests/EditorOfflineRenderServiceTests.cpp`](../Runtime/KairoRenderBridge/tests/EditorOfflineRenderServiceTests.cpp)
- [`Runtime/KairoRenderBridge/tests/MaterialPreviewBridgeTests.cpp`](../Runtime/KairoRenderBridge/tests/MaterialPreviewBridgeTests.cpp)
- [`Runtime/KairoRenderBridge/tests/SceneSnapshotBridgeTests.cpp`](../Runtime/KairoRenderBridge/tests/SceneSnapshotBridgeTests.cpp)

## Scene/ECS bridge (1)

- [`Runtime/KairoSceneECSBridge.cppm`](../Runtime/KairoSceneECSBridge.cppm)

## Game samples (8)

- [`Samples/KairoRacingGame/main.cpp`](../Samples/KairoRacingGame/main.cpp)
- [`Samples/KairoRacingGame/tools/export_glb.py`](../Samples/KairoRacingGame/tools/export_glb.py)
- [`Samples/KairoRacingGame/tools/prepare_assets.sh`](../Samples/KairoRacingGame/tools/prepare_assets.sh)
- [`Samples/Phase1Game/Phase1GameState.cppm`](../Samples/Phase1Game/Phase1GameState.cppm)
- [`Samples/Phase1Game/main.cpp`](../Samples/Phase1Game/main.cpp)
- [`Samples/Phase1Game/tests/PackagePhase1.cmake`](../Samples/Phase1Game/tests/PackagePhase1.cmake)
- [`Samples/Phase1Game/tests/Phase1GameStateTests.cpp`](../Samples/Phase1Game/tests/Phase1GameStateTests.cpp)
- [`Samples/SharedContentShowcase/verify_checksums.cmake`](../Samples/SharedContentShowcase/verify_checksums.cmake)

## Migration and external project adapters (10)

- [`Migration/KairoBridge/CanonicalIR.cppm`](../Migration/KairoBridge/CanonicalIR.cppm)
- [`Migration/KairoBridge/GodotAdapter.cppm`](../Migration/KairoBridge/GodotAdapter.cppm)
- [`Migration/KairoBridge/KairoBridge.cppm`](../Migration/KairoBridge/KairoBridge.cppm)
- [`Migration/KairoBridge/MigrationManifest.cppm`](../Migration/KairoBridge/MigrationManifest.cppm)
- [`Migration/KairoBridge/MigrationTypes.cppm`](../Migration/KairoBridge/MigrationTypes.cppm)
- [`Migration/KairoBridge/SourceProject.cppm`](../Migration/KairoBridge/SourceProject.cppm)
- [`Migration/KairoBridge/UnityAdapter.cppm`](../Migration/KairoBridge/UnityAdapter.cppm)
- [`Migration/KairoBridge/tests/DurableIdentityTests.cpp`](../Migration/KairoBridge/tests/DurableIdentityTests.cpp)
- [`Migration/KairoBridge/tests/GodotAdapterTests.cpp`](../Migration/KairoBridge/tests/GodotAdapterTests.cpp)
- [`Migration/KairoBridge/tests/KairoBridgeTests.cpp`](../Migration/KairoBridge/tests/KairoBridgeTests.cpp)

## Integration tests (5)

- [`tests/CompatibilityContentTests.cpp`](../tests/CompatibilityContentTests.cpp)
- [`tests/ComputeStackIntegration.cpp`](../tests/ComputeStackIntegration.cpp)
- [`tests/SceneECSBridgeTests.cpp`](../tests/SceneECSBridgeTests.cpp)
- [`tests/SharedContentContractTests.cpp`](../tests/SharedContentContractTests.cpp)
- [`tests/WaveCScaleBenchmark.cpp`](../tests/WaveCScaleBenchmark.cpp)

## Build config (1)

- [`CMakeLists.txt`](../CMakeLists.txt)

## Other files (3)

- [`TestContent/Compatibility/fetch_optional.cmake`](../TestContent/Compatibility/fetch_optional.cmake)
- [`TestContent/Compatibility/verify_checksums.cmake`](../TestContent/Compatibility/verify_checksums.cmake)
- [`cmake/toolchains/clang.cmake`](../cmake/toolchains/clang.cmake)

**Indexed files:** 92. Source presence is not evidence of successful exact-head acceptance; see [STATUS.md](../STATUS.md).
