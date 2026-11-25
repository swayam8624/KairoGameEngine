# Kairo study order

Read this as a course, not a directory listing. Follow one entity from disk
to a rendered frame first, then inspect the exact pinned implementation.
The [annotation standard](docs/ANNOTATION_STYLE_GUIDE.md) explains what
every function's comments must convey. [DEVLOG.md](DEVLOG.md) records
changes and evidence, and [the M50 ledger](docs/MILESTONE_50.md) defines the
first product milestone.

## Pass 1 — understand one complete game frame

1. [Workspace and build](README.md), [workspace.lock](workspace.lock), [CMakeLists.txt](CMakeLists.txt).
2. [Integration walkthrough](docs/ENGINE_INTEGRATION_WALKTHROUGH.md).
3. [Authored entity → Player frame](docs/AUTHORED_ENTITY_TO_PLAYER_FRAME_LAB.md).
4. [Asset → GPU ownership](docs/ASSET_SCENE_TO_GPU_LIFETIME_LAB.md).
5. [RuntimeProject.cppm](Runtime/KairoPlayer/RuntimeProject.cppm) → [RuntimeInputBridge.cppm](Runtime/KairoPlayer/RuntimeInputBridge.cppm) → [RuntimePhysicsBridge.cppm](Runtime/KairoPlayer/RuntimePhysicsBridge.cppm) → [RuntimePlayerControllerBridge.cppm](Runtime/KairoPlayer/RuntimePlayerControllerBridge.cppm).
6. [RuntimeRenderBridge.cppm](Runtime/KairoPlayer/RuntimeRenderBridge.cppm) → [main.cpp](Runtime/KairoPlayer/main.cpp).
7. [Playable sample](Samples/PlayableVerticalSlice) → [Craft playtest](docs/CRAFT_PLAYTEST.md) → [packaging](Runtime/KairoPlayer/RuntimePackaging.cppm).

## Pass 2 — sibling repositories in dependency-first order

These links point to the exact revisions recorded in the workspace lock at
this pass, not drifting main branches. Read each repository's public modules
and implementation before its tests. Optional research/DCC components come
after the game-facing stack. The ordering is pedagogical, not a claim that
every pair of adjacent repositories has a direct dependency.

1. [KairoMath](https://github.com/swayam8624/KairoMath/tree/f31f5a2feac8ebb958fef5bfc81a1fca50578e85) · [File checklist (34)](docs/study/KairoMath.md) — [README](https://github.com/swayam8624/KairoMath/blob/f31f5a2feac8ebb958fef5bfc81a1fca50578e85/README.md)
2. [KairoGeometry](https://github.com/swayam8624/KairoGeometry/tree/b5808b99faed80fe330f889e9374822deaf64dbd) · [File checklist (22)](docs/study/KairoGeometry.md) — [README](https://github.com/swayam8624/KairoGeometry/blob/b5808b99faed80fe330f889e9374822deaf64dbd/README.md)
3. [KairoSpatial](https://github.com/swayam8624/KairoSpatial/tree/366379c290c79e6fffc6f84e4f4bb6832f9f91b3) · [File checklist (27)](docs/study/KairoSpatial.md) — [README](https://github.com/swayam8624/KairoSpatial/blob/366379c290c79e6fffc6f84e4f4bb6832f9f91b3/README.md)
4. [KairoECS](https://github.com/swayam8624/KairoECS/tree/82d7a5e3c2e445d369e830508fa82e23aacca25a) · [File checklist (11)](docs/study/KairoECS.md) — [README](https://github.com/swayam8624/KairoECS/blob/82d7a5e3c2e445d369e830508fa82e23aacca25a/README.md)
5. [KairoReflection](https://github.com/swayam8624/KairoReflection/tree/b048dd7fc96f19c0a39fd2076625561e2dae6db5) · [File checklist (7)](docs/study/KairoReflection.md) — [README](https://github.com/swayam8624/KairoReflection/blob/b048dd7fc96f19c0a39fd2076625561e2dae6db5/README.md)
6. [KairoPhysicsMath](https://github.com/swayam8624/KairoPhysicsMath/tree/a257c6d93296306e797d685028f0bf7866340465) · [File checklist (15)](docs/study/KairoPhysicsMath.md) — [README](https://github.com/swayam8624/KairoPhysicsMath/blob/a257c6d93296306e797d685028f0bf7866340465/README.md)
7. [KairoPhysicsEngine](https://github.com/swayam8624/KairoPhysicsEngine/tree/b256df7327cfa0e4ae393e811cc3a661a17559e6) · [File checklist (35)](docs/study/KairoPhysicsEngine.md) — [README](https://github.com/swayam8624/KairoPhysicsEngine/blob/b256df7327cfa0e4ae393e811cc3a661a17559e6/README.md)
8. [KairoAssets](https://github.com/swayam8624/KairoAssets/tree/a0a6c81b2526bcb1531dfa4094ccfae4aa568f19) · [File checklist (54)](docs/study/KairoAssets.md) — [README](https://github.com/swayam8624/KairoAssets/blob/a0a6c81b2526bcb1531dfa4094ccfae4aa568f19/README.md)
9. [KairoEngineCore](https://github.com/swayam8624/KairoEngineCore/tree/523ed41da74ee117256281a3ebc45cde91368bc6) · [File checklist (97)](docs/study/KairoEngineCore.md) — [README](https://github.com/swayam8624/KairoEngineCore/blob/523ed41da74ee117256281a3ebc45cde91368bc6/README.md)
10. [KairoAI](https://github.com/swayam8624/KairoAI/tree/5f09694af688a34be157fe3a00171fe7be8bcd00) · [File checklist (19)](docs/study/KairoAI.md) — [README](https://github.com/swayam8624/KairoAI/blob/5f09694af688a34be157fe3a00171fe7be8bcd00/README.md)
11. [KairoGPU](https://github.com/swayam8624/KairoGPU/tree/befe484db34dab841af2c4e54ce9f224ae8391dd) · [File checklist (8)](docs/study/KairoGPU.md) — [README](https://github.com/swayam8624/KairoGPU/blob/befe484db34dab841af2c4e54ce9f224ae8391dd/README.md)
12. [KairoRenderer](https://github.com/swayam8624/KairoRenderer/tree/8bed00179bf6712a69efb6a102df7caa1ef81b45) · [File checklist (109)](docs/study/KairoRenderer.md) — [README](https://github.com/swayam8624/KairoRenderer/blob/8bed00179bf6712a69efb6a102df7caa1ef81b45/README.md)
13. [KairoEditor](https://github.com/swayam8624/KairoEditor/tree/da2369262e666b8769778ae35681f247573dbec0) · [File checklist (89)](docs/study/KairoEditor.md) — [README](https://github.com/swayam8624/KairoEditor/blob/da2369262e666b8769778ae35681f247573dbec0/README.md)
14. [KairoHub](https://github.com/swayam8624/KairoHub/tree/48ed09b7f291d8d8cb9b51ad2e993bf4f16930c7) · [File checklist (17)](docs/study/KairoHub.md) — [README](https://github.com/swayam8624/KairoHub/blob/48ed09b7f291d8d8cb9b51ad2e993bf4f16930c7/README.md)
15. [KairoRayTracer](https://github.com/swayam8624/KairoRayTracer/tree/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e) · [File checklist (36)](docs/study/KairoRayTracer.md) — [README](https://github.com/swayam8624/KairoRayTracer/blob/99c45dfdaa67c2dc9e2d9846fc1dc21a0ba5782e/README.md)
16. [KairoSIMD](https://github.com/swayam8624/KairoSIMD/tree/5d65e950ec64e5ae1021cc4b58d374e6a3b5cde6) · [File checklist (8)](docs/study/KairoSIMD.md) — [README](https://github.com/swayam8624/KairoSIMD/blob/5d65e950ec64e5ae1021cc4b58d374e6a3b5cde6/README.md)
17. [KairoScheduler](https://github.com/swayam8624/KairoScheduler/tree/e9304fcb4a47cb66a32a3426e83669c88468d9ef) · [File checklist (7)](docs/study/KairoScheduler.md) — [README](https://github.com/swayam8624/KairoScheduler/blob/e9304fcb4a47cb66a32a3426e83669c88468d9ef/README.md)
18. [KairoONNX](https://github.com/swayam8624/KairoONNX/tree/572a0b49177584b85d726fe4cfc72a861f6ddd1e) · [File checklist (9)](docs/study/KairoONNX.md) — [README](https://github.com/swayam8624/KairoONNX/blob/572a0b49177584b85d726fe4cfc72a861f6ddd1e/README.md)
19. [KairoTransformers](https://github.com/swayam8624/KairoTransformers/tree/de2c6d1fcfda279e286f63dc49fba29cb27b5c99) · [File checklist (12)](docs/study/KairoTransformers.md) — [README](https://github.com/swayam8624/KairoTransformers/blob/de2c6d1fcfda279e286f63dc49fba29cb27b5c99/README.md)
20. [KairoMacPerception](https://github.com/swayam8624/KairoMacPerception/tree/b0545b494f753b30146880e1c2e906f2277594d9) · [File checklist (11)](docs/study/KairoMacPerception.md) — [README](https://github.com/swayam8624/KairoMacPerception/blob/b0545b494f753b30146880e1c2e906f2277594d9/README.md)
21. [KairoPipelineCore](https://github.com/swayam8624/KairoPipelineCore/tree/ff0ecd431bbf6e826014ea2ccbc21055884b26f0) · [File checklist (18)](docs/study/KairoPipelineCore.md) — [README](https://github.com/swayam8624/KairoPipelineCore/blob/ff0ecd431bbf6e826014ea2ccbc21055884b26f0/README.md)
22. [KairoProductionTools](https://github.com/swayam8624/KairoProductionTools/tree/95708af537b312a9a5d56c45e46f419abe858378) · [File checklist (5)](docs/study/KairoProductionTools.md) — [README](https://github.com/swayam8624/KairoProductionTools/blob/95708af537b312a9a5d56c45e46f419abe858378/README.md)
23. [KairoBlender](https://github.com/swayam8624/KairoBlender/tree/ba0ed9a731b45b91a0991161e4b6b333d828687b) · [File checklist (15)](docs/study/KairoBlender.md) — [README](https://github.com/swayam8624/KairoBlender/blob/ba0ed9a731b45b91a0991161e4b6b333d828687b/README.md)
24. [KairoHoudini](https://github.com/swayam8624/KairoHoudini/tree/0ef547ba3a47911851a01b3cd8ddac062a80caf4) · [File checklist (12)](docs/study/KairoHoudini.md) — [README](https://github.com/swayam8624/KairoHoudini/blob/0ef547ba3a47911851a01b3cd8ddac062a80caf4/README.md)
25. [KairoMaya](https://github.com/swayam8624/KairoMaya/tree/2750133c3abf334789df7703ba2e8ba8a60fa6a7) · [File checklist (11)](docs/study/KairoMaya.md) — [README](https://github.com/swayam8624/KairoMaya/blob/2750133c3abf334789df7703ba2e8ba8a60fa6a7/README.md)
26. [KairoNuke](https://github.com/swayam8624/KairoNuke/tree/d1f66798ec047a015a981d1429893d8b3af201a6) · [File checklist (12)](docs/study/KairoNuke.md) — [README](https://github.com/swayam8624/KairoNuke/blob/d1f66798ec047a015a981d1429893d8b3af201a6/README.md)

## Pass 3 — exhaustive integration source inventory

254 tracked source/configuration/sample/test paths from
`c489bf16f1aa4f697d20a9c160da287d85bb3b0f` (first-party path filter; generated binaries, external source and
media excluded). New source paths must be added here as part of their PR.
Within a group, follow the imports/calls and the linked tests, not merely
alphabetical order. The curated paths in Pass 1 take priority.


### Bootstrap and build (8)

- [`CMakeLists.txt`](CMakeLists.txt)
- [`CMakePresets.json`](CMakePresets.json)
- [`cmake/toolchains/clang.cmake`](cmake/toolchains/clang.cmake)
- [`scripts/bootstrap_workspace.sh`](scripts/bootstrap_workspace.sh)
- [`scripts/build_and_test.sh`](scripts/build_and_test.sh)
- [`scripts/materialize_workspace_lock.sh`](scripts/materialize_workspace_lock.sh)
- [`scripts/sync_workspace.sh`](scripts/sync_workspace.sh)
- [`scripts/verify_workspace_lock.sh`](scripts/verify_workspace_lock.sh)

### Player data contracts and frame primitives (8)

- [`Runtime/KairoPlayer/RuntimeFrameClock.hpp`](Runtime/KairoPlayer/RuntimeFrameClock.hpp)
- [`Runtime/KairoPlayer/RuntimeInputGate.hpp`](Runtime/KairoPlayer/RuntimeInputGate.hpp)
- [`Runtime/KairoPlayer/RuntimePcm16.hpp`](Runtime/KairoPlayer/RuntimePcm16.hpp)
- [`Runtime/KairoPlayer/RuntimePcmPump.hpp`](Runtime/KairoPlayer/RuntimePcmPump.hpp)
- [`Runtime/KairoPlayer/RuntimePcmQueue.hpp`](Runtime/KairoPlayer/RuntimePcmQueue.hpp)
- [`Runtime/KairoPlayer/RuntimePickupPool.hpp`](Runtime/KairoPlayer/RuntimePickupPool.hpp)
- [`Runtime/KairoPlayer/RuntimeProject.cppm`](Runtime/KairoPlayer/RuntimeProject.cppm)
- [`Runtime/KairoPlayer/RuntimeRenderResourceTracking.hpp`](Runtime/KairoPlayer/RuntimeRenderResourceTracking.hpp)

### Shared render extraction (5)

- [`Runtime/KairoRealtimeRenderBridge/CMakeLists.txt`](Runtime/KairoRealtimeRenderBridge/CMakeLists.txt)
- [`Runtime/KairoRealtimeRenderBridge/FrameAssetCache.hpp`](Runtime/KairoRealtimeRenderBridge/FrameAssetCache.hpp)
- [`Runtime/KairoRealtimeRenderBridge/FrameHierarchyCache.hpp`](Runtime/KairoRealtimeRenderBridge/FrameHierarchyCache.hpp)
- [`Runtime/KairoRealtimeRenderBridge/MaterialFileSizeBudget.hpp`](Runtime/KairoRealtimeRenderBridge/MaterialFileSizeBudget.hpp)
- [`Runtime/KairoRealtimeRenderBridge/RealtimeSceneBridge.cppm`](Runtime/KairoRealtimeRenderBridge/RealtimeSceneBridge.cppm)

### Runtime adapters (23)

- [`Runtime/KairoPlayer/RuntimeAlsaAudioOutput.hpp`](Runtime/KairoPlayer/RuntimeAlsaAudioOutput.hpp)
- [`Runtime/KairoPlayer/RuntimeAudioBridge.cppm`](Runtime/KairoPlayer/RuntimeAudioBridge.cppm)
- [`Runtime/KairoPlayer/RuntimeCharacterMotorBridge.cppm`](Runtime/KairoPlayer/RuntimeCharacterMotorBridge.cppm)
- [`Runtime/KairoPlayer/RuntimeCrowdNavigationBridge.cppm`](Runtime/KairoPlayer/RuntimeCrowdNavigationBridge.cppm)
- [`Runtime/KairoPlayer/RuntimeEntityLifecycleBridge.cppm`](Runtime/KairoPlayer/RuntimeEntityLifecycleBridge.cppm)
- [`Runtime/KairoPlayer/RuntimeInputBridge.cppm`](Runtime/KairoPlayer/RuntimeInputBridge.cppm)
- [`Runtime/KairoPlayer/RuntimeLocomotionAnimationBridge.cppm`](Runtime/KairoPlayer/RuntimeLocomotionAnimationBridge.cppm)
- [`Runtime/KairoPlayer/RuntimeLogicBridge.cppm`](Runtime/KairoPlayer/RuntimeLogicBridge.cppm)
- [`Runtime/KairoPlayer/RuntimeNativeAudioOutput.hpp`](Runtime/KairoPlayer/RuntimeNativeAudioOutput.hpp)
- [`Runtime/KairoPlayer/RuntimeNativeGameplayBridge.cppm`](Runtime/KairoPlayer/RuntimeNativeGameplayBridge.cppm)
- [`Runtime/KairoPlayer/RuntimeNavigationAgentBridge.cppm`](Runtime/KairoPlayer/RuntimeNavigationAgentBridge.cppm)
- [`Runtime/KairoPlayer/RuntimePackaging.cppm`](Runtime/KairoPlayer/RuntimePackaging.cppm)
- [`Runtime/KairoPlayer/RuntimePhysicsBridge.cppm`](Runtime/KairoPlayer/RuntimePhysicsBridge.cppm)
- [`Runtime/KairoPlayer/RuntimePlayerControllerBridge.cppm`](Runtime/KairoPlayer/RuntimePlayerControllerBridge.cppm)
- [`Runtime/KairoPlayer/RuntimeProceduralTerrainBridge.cppm`](Runtime/KairoPlayer/RuntimeProceduralTerrainBridge.cppm)
- [`Runtime/KairoPlayer/RuntimeProductionSystemsBridge.cppm`](Runtime/KairoPlayer/RuntimeProductionSystemsBridge.cppm)
- [`Runtime/KairoPlayer/RuntimeRenderBridge.cppm`](Runtime/KairoPlayer/RuntimeRenderBridge.cppm)
- [`Runtime/KairoPlayer/RuntimeSaveGameBridge.cppm`](Runtime/KairoPlayer/RuntimeSaveGameBridge.cppm)
- [`Runtime/KairoPlayer/RuntimeSceneAudioBridge.cppm`](Runtime/KairoPlayer/RuntimeSceneAudioBridge.cppm)
- [`Runtime/KairoPlayer/RuntimeShippingBridge.cppm`](Runtime/KairoPlayer/RuntimeShippingBridge.cppm)
- [`Runtime/KairoPlayer/RuntimeWindowsAudioOutput.hpp`](Runtime/KairoPlayer/RuntimeWindowsAudioOutput.hpp)
- [`Runtime/KairoPlayer/RuntimeWorldSaveBridge.cppm`](Runtime/KairoPlayer/RuntimeWorldSaveBridge.cppm)
- [`Runtime/KairoPlayer/RuntimeWorldStreamingBridge.cppm`](Runtime/KairoPlayer/RuntimeWorldStreamingBridge.cppm)

### Craft, terrain and voxel gameplay (47)

- [`Runtime/KairoPlayer/CMakeLists.txt`](Runtime/KairoPlayer/CMakeLists.txt)
- [`Runtime/KairoPlayer/CraftActionLatch.hpp`](Runtime/KairoPlayer/CraftActionLatch.hpp)
- [`Runtime/KairoPlayer/CraftAutosave.hpp`](Runtime/KairoPlayer/CraftAutosave.hpp)
- [`Runtime/KairoPlayer/CraftCaveDemo.cpp`](Runtime/KairoPlayer/CraftCaveDemo.cpp)
- [`Runtime/KairoPlayer/CraftCaveSession.hpp`](Runtime/KairoPlayer/CraftCaveSession.hpp)
- [`Runtime/KairoPlayer/CraftCaveSpawn.hpp`](Runtime/KairoPlayer/CraftCaveSpawn.hpp)
- [`Runtime/KairoPlayer/CraftCaveWorld.hpp`](Runtime/KairoPlayer/CraftCaveWorld.hpp)
- [`Runtime/KairoPlayer/CraftCheckpoint.hpp`](Runtime/KairoPlayer/CraftCheckpoint.hpp)
- [`Runtime/KairoPlayer/CraftCombatHud.hpp`](Runtime/KairoPlayer/CraftCombatHud.hpp)
- [`Runtime/KairoPlayer/CraftCombatReward.hpp`](Runtime/KairoPlayer/CraftCombatReward.hpp)
- [`Runtime/KairoPlayer/CraftCreature.hpp`](Runtime/KairoPlayer/CraftCreature.hpp)
- [`Runtime/KairoPlayer/CraftCreatureNavigation.hpp`](Runtime/KairoPlayer/CraftCreatureNavigation.hpp)
- [`Runtime/KairoPlayer/CraftCreatureVisual.hpp`](Runtime/KairoPlayer/CraftCreatureVisual.hpp)
- [`Runtime/KairoPlayer/CraftDaylight.hpp`](Runtime/KairoPlayer/CraftDaylight.hpp)
- [`Runtime/KairoPlayer/CraftDemo.cpp`](Runtime/KairoPlayer/CraftDemo.cpp)
- [`Runtime/KairoPlayer/CraftEquipment.hpp`](Runtime/KairoPlayer/CraftEquipment.hpp)
- [`Runtime/KairoPlayer/CraftEquipmentSave.hpp`](Runtime/KairoPlayer/CraftEquipmentSave.hpp)
- [`Runtime/KairoPlayer/CraftHudProjection.hpp`](Runtime/KairoPlayer/CraftHudProjection.hpp)
- [`Runtime/KairoPlayer/CraftHudPublication.hpp`](Runtime/KairoPlayer/CraftHudPublication.hpp)
- [`Runtime/KairoPlayer/CraftInventory.hpp`](Runtime/KairoPlayer/CraftInventory.hpp)
- [`Runtime/KairoPlayer/CraftMeshBatch.hpp`](Runtime/KairoPlayer/CraftMeshBatch.hpp)
- [`Runtime/KairoPlayer/CraftMining.hpp`](Runtime/KairoPlayer/CraftMining.hpp)
- [`Runtime/KairoPlayer/CraftMouseLook.hpp`](Runtime/KairoPlayer/CraftMouseLook.hpp)
- [`Runtime/KairoPlayer/CraftNutrition.hpp`](Runtime/KairoPlayer/CraftNutrition.hpp)
- [`Runtime/KairoPlayer/CraftRecipesHud.hpp`](Runtime/KairoPlayer/CraftRecipesHud.hpp)
- [`Runtime/KairoPlayer/CraftSave.hpp`](Runtime/KairoPlayer/CraftSave.hpp)
- [`Runtime/KairoPlayer/CraftSaveFile.hpp`](Runtime/KairoPlayer/CraftSaveFile.hpp)
- [`Runtime/KairoPlayer/CraftStreamDemo.cpp`](Runtime/KairoPlayer/CraftStreamDemo.cpp)
- [`Runtime/KairoPlayer/CraftStreamSave.hpp`](Runtime/KairoPlayer/CraftStreamSave.hpp)
- [`Runtime/KairoPlayer/CraftStreamingWindow.hpp`](Runtime/KairoPlayer/CraftStreamingWindow.hpp)
- [`Runtime/KairoPlayer/CraftSurfaceSpawn.hpp`](Runtime/KairoPlayer/CraftSurfaceSpawn.hpp)
- [`Runtime/KairoPlayer/CraftTargetFeedback.hpp`](Runtime/KairoPlayer/CraftTargetFeedback.hpp)
- [`Runtime/KairoPlayer/CraftVitals.hpp`](Runtime/KairoPlayer/CraftVitals.hpp)
- [`Runtime/KairoPlayer/CraftWorld.hpp`](Runtime/KairoPlayer/CraftWorld.hpp)
- [`Runtime/KairoPlayer/TerrainDemo.cpp`](Runtime/KairoPlayer/TerrainDemo.cpp)
- [`Runtime/KairoPlayer/TerrainErrorProfile.hpp`](Runtime/KairoPlayer/TerrainErrorProfile.hpp)
- [`Runtime/KairoPlayer/TerrainExplorer.cpp`](Runtime/KairoPlayer/TerrainExplorer.cpp)
- [`Runtime/KairoPlayer/TerrainGenerationQueue.hpp`](Runtime/KairoPlayer/TerrainGenerationQueue.hpp)
- [`Runtime/KairoPlayer/TerrainNativeStore.hpp`](Runtime/KairoPlayer/TerrainNativeStore.hpp)
- [`Runtime/KairoPlayer/TerrainRenderOrigin.hpp`](Runtime/KairoPlayer/TerrainRenderOrigin.hpp)
- [`Runtime/KairoPlayer/TerrainScreenError.hpp`](Runtime/KairoPlayer/TerrainScreenError.hpp)
- [`Runtime/KairoPlayer/TerrainWalker.hpp`](Runtime/KairoPlayer/TerrainWalker.hpp)
- [`Runtime/KairoPlayer/VoxelChunkSurface.hpp`](Runtime/KairoPlayer/VoxelChunkSurface.hpp)
- [`Runtime/KairoPlayer/VoxelInteraction.hpp`](Runtime/KairoPlayer/VoxelInteraction.hpp)
- [`Runtime/KairoPlayer/VoxelMeshPublication.hpp`](Runtime/KairoPlayer/VoxelMeshPublication.hpp)
- [`Runtime/KairoPlayer/VoxelPlayer.hpp`](Runtime/KairoPlayer/VoxelPlayer.hpp)
- [`Runtime/KairoPlayer/main.cpp`](Runtime/KairoPlayer/main.cpp)

### Offline renderer and migration (14)

- [`Migration/KairoBridge/CMakeLists.txt`](Migration/KairoBridge/CMakeLists.txt)
- [`Migration/KairoBridge/CanonicalIR.cppm`](Migration/KairoBridge/CanonicalIR.cppm)
- [`Migration/KairoBridge/GodotAdapter.cppm`](Migration/KairoBridge/GodotAdapter.cppm)
- [`Migration/KairoBridge/KairoBridge.cppm`](Migration/KairoBridge/KairoBridge.cppm)
- [`Migration/KairoBridge/MigrationManifest.cppm`](Migration/KairoBridge/MigrationManifest.cppm)
- [`Migration/KairoBridge/MigrationTypes.cppm`](Migration/KairoBridge/MigrationTypes.cppm)
- [`Migration/KairoBridge/SourceProject.cppm`](Migration/KairoBridge/SourceProject.cppm)
- [`Migration/KairoBridge/UnityAdapter.cppm`](Migration/KairoBridge/UnityAdapter.cppm)
- [`Runtime/KairoRenderBridge/CMakeLists.txt`](Runtime/KairoRenderBridge/CMakeLists.txt)
- [`Runtime/KairoRenderBridge/EditorOfflineRenderService.cppm`](Runtime/KairoRenderBridge/EditorOfflineRenderService.cppm)
- [`Runtime/KairoRenderBridge/MaterialPreviewBridge.cpp`](Runtime/KairoRenderBridge/MaterialPreviewBridge.cpp)
- [`Runtime/KairoRenderBridge/MaterialPreviewBridge.cppm`](Runtime/KairoRenderBridge/MaterialPreviewBridge.cppm)
- [`Runtime/KairoRenderBridge/OfflineRenderSession.cppm`](Runtime/KairoRenderBridge/OfflineRenderSession.cppm)
- [`Runtime/KairoRenderBridge/SceneSnapshotBridge.cppm`](Runtime/KairoRenderBridge/SceneSnapshotBridge.cppm)

### Authored samples and fixtures (30)

- [`Samples/KairoRacingGame/CMakeLists.txt`](Samples/KairoRacingGame/CMakeLists.txt)
- [`Samples/KairoRacingGame/Project/Assets.kassets`](Samples/KairoRacingGame/Project/Assets.kassets)
- [`Samples/KairoRacingGame/Project/Config/Input.kinput`](Samples/KairoRacingGame/Project/Config/Input.kinput)
- [`Samples/KairoRacingGame/Project/Racing.kproject`](Samples/KairoRacingGame/Project/Racing.kproject)
- [`Samples/KairoRacingGame/Project/Scenes/Racing.kscene`](Samples/KairoRacingGame/Project/Scenes/Racing.kscene)
- [`Samples/KairoRacingGame/main.cpp`](Samples/KairoRacingGame/main.cpp)
- [`Samples/KairoRacingGame/tools/export_glb.py`](Samples/KairoRacingGame/tools/export_glb.py)
- [`Samples/KairoRacingGame/tools/prepare_assets.sh`](Samples/KairoRacingGame/tools/prepare_assets.sh)
- [`Samples/Phase1Game/CMakeLists.txt`](Samples/Phase1Game/CMakeLists.txt)
- [`Samples/Phase1Game/Phase1GameState.cppm`](Samples/Phase1Game/Phase1GameState.cppm)
- [`Samples/Phase1Game/Project/Assets.kassets`](Samples/Phase1Game/Project/Assets.kassets)
- [`Samples/Phase1Game/Project/Config/Input.kinput`](Samples/Phase1Game/Project/Config/Input.kinput)
- [`Samples/Phase1Game/Project/Phase1.kproject`](Samples/Phase1Game/Project/Phase1.kproject)
- [`Samples/Phase1Game/Project/Scenes/Arena.kscene`](Samples/Phase1Game/Project/Scenes/Arena.kscene)
- [`Samples/Phase1Game/main.cpp`](Samples/Phase1Game/main.cpp)
- [`Samples/PlayableVerticalSlice/Assets.kassets`](Samples/PlayableVerticalSlice/Assets.kassets)
- [`Samples/PlayableVerticalSlice/Config/Input.kinput`](Samples/PlayableVerticalSlice/Config/Input.kinput)
- [`Samples/PlayableVerticalSlice/Project.kproject`](Samples/PlayableVerticalSlice/Project.kproject)
- [`Samples/PlayableVerticalSlice/Scenes/PlayableArena.kscene`](Samples/PlayableVerticalSlice/Scenes/PlayableArena.kscene)
- [`Samples/SharedContentShowcase/Assets.kassets`](Samples/SharedContentShowcase/Assets.kassets)
- [`Samples/SharedContentShowcase/Config/Input.kinput`](Samples/SharedContentShowcase/Config/Input.kinput)
- [`Samples/SharedContentShowcase/Project.kproject`](Samples/SharedContentShowcase/Project.kproject)
- [`Samples/SharedContentShowcase/Scenes/Showcase.kscene`](Samples/SharedContentShowcase/Scenes/Showcase.kscene)
- [`Samples/SharedContentShowcase/verify_checksums.cmake`](Samples/SharedContentShowcase/verify_checksums.cmake)
- [`TestContent/Compatibility/ASSET_LICENSES.json`](TestContent/Compatibility/ASSET_LICENSES.json)
- [`TestContent/Compatibility/committed/LightVisibility/metadata.json`](TestContent/Compatibility/committed/LightVisibility/metadata.json)
- [`TestContent/Compatibility/committed/MetalRoughSpheresNoTextures/metadata.json`](TestContent/Compatibility/committed/MetalRoughSpheresNoTextures/metadata.json)
- [`TestContent/Compatibility/committed/ToyCar/metadata.json`](TestContent/Compatibility/committed/ToyCar/metadata.json)
- [`TestContent/Compatibility/fetch_optional.cmake`](TestContent/Compatibility/fetch_optional.cmake)
- [`TestContent/Compatibility/verify_checksums.cmake`](TestContent/Compatibility/verify_checksums.cmake)

### Integration tests (6)

- [`tests/CompatibilityContentTests.cpp`](tests/CompatibilityContentTests.cpp)
- [`tests/ComputeStackIntegration.cpp`](tests/ComputeStackIntegration.cpp)
- [`tests/SceneECSBridgeTests.cpp`](tests/SceneECSBridgeTests.cpp)
- [`tests/SharedContentContractTests.cpp`](tests/SharedContentContractTests.cpp)
- [`tests/WaveCScaleBenchmark.cpp`](tests/WaveCScaleBenchmark.cpp)
- [`tests/test_audit_teaching_links.py`](tests/test_audit_teaching_links.py)

### Tooling and CI (39)

- [`.github/workflows/bridge-native-tests.yml`](.github/workflows/bridge-native-tests.yml)
- [`.github/workflows/build-test.yml`](.github/workflows/build-test.yml)
- [`.github/workflows/cpu-player-resource-guards.yml`](.github/workflows/cpu-player-resource-guards.yml)
- [`.github/workflows/frame-hierarchy-cache.yml`](.github/workflows/frame-hierarchy-cache.yml)
- [`.github/workflows/game-first-scope.yml`](.github/workflows/game-first-scope.yml)
- [`.github/workflows/phases6-12-stacked-integration.yml`](.github/workflows/phases6-12-stacked-integration.yml)
- [`.github/workflows/portfolio-ci-audit.yml`](.github/workflows/portfolio-ci-audit.yml)
- [`.github/workflows/procedural-terrain-viewport.yml`](.github/workflows/procedural-terrain-viewport.yml)
- [`.github/workflows/racing-port-compile.yml`](.github/workflows/racing-port-compile.yml)
- [`.github/workflows/racing-port-macos-integration.yml`](.github/workflows/racing-port-macos-integration.yml)
- [`.github/workflows/runtime-resource-journal.yml`](.github/workflows/runtime-resource-journal.yml)
- [`.github/workflows/wave-evidence.yml`](.github/workflows/wave-evidence.yml)
- [`Runtime/KairoSceneECSBridge.cppm`](Runtime/KairoSceneECSBridge.cppm)
- [`STATUS.yaml`](STATUS.yaml)
- [`docs/status/certified-mac-95-2026-09-24.yaml`](docs/status/certified-mac-95-2026-09-24.yaml)
- [`docs/status/portfolio-95.yaml`](docs/status/portfolio-95.yaml)
- [`docs/status/research-portfolio.yaml`](docs/status/research-portfolio.yaml)
- [`docs/status/wave-a-b.yaml`](docs/status/wave-a-b.yaml)
- [`docs/status/wave-c-f.yaml`](docs/status/wave-c-f.yaml)
- [`research/transactional_heterogeneous_worlds.yaml`](research/transactional_heterogeneous_worlds.yaml)
- [`research/workloads.yaml`](research/workloads.yaml)
- [`scripts/StageCraftPlaytest.cmake`](scripts/StageCraftPlaytest.cmake)
- [`scripts/audit_ecosystem_ci.py`](scripts/audit_ecosystem_ci.py)
- [`scripts/audit_teaching_links.py`](scripts/audit_teaching_links.py)
- [`scripts/migrate_from_submodules.sh`](scripts/migrate_from_submodules.sh)
- [`scripts/repair_legacy_generated_artifacts.sh`](scripts/repair_legacy_generated_artifacts.sh)
- [`scripts/run_authoring_workflow.sh`](scripts/run_authoring_workflow.sh)
- [`scripts/run_compute_stack_campaign.sh`](scripts/run_compute_stack_campaign.sh)
- [`scripts/run_craft_cpu_contracts.sh`](scripts/run_craft_cpu_contracts.sh)
- [`scripts/run_flagship_campaign.sh`](scripts/run_flagship_campaign.sh)
- [`scripts/run_kairo_racing_local.sh`](scripts/run_kairo_racing_local.sh)
- [`scripts/run_native_metal_gameplay_acceptance.sh`](scripts/run_native_metal_gameplay_acceptance.sh)
- [`scripts/run_portfolio_acceptance.sh`](scripts/run_portfolio_acceptance.sh)
- [`scripts/run_renderer_resource_lifetime_regressions.sh`](scripts/run_renderer_resource_lifetime_regressions.sh)
- [`scripts/run_wave_c_scale_campaign.sh`](scripts/run_wave_c_scale_campaign.sh)
- [`scripts/test_game_first_scope.py`](scripts/test_game_first_scope.py)
- [`scripts/validate_and_run_kairo_project.sh`](scripts/validate_and_run_kairo_project.sh)
- [`scripts/verify_portfolio_95.sh`](scripts/verify_portfolio_95.sh)
- [`scripts/workspace_profiles.sh`](scripts/workspace_profiles.sh)

### Component-local tests (74)

- [`Migration/KairoBridge/tests/DurableIdentityTests.cpp`](Migration/KairoBridge/tests/DurableIdentityTests.cpp)
- [`Migration/KairoBridge/tests/GodotAdapterTests.cpp`](Migration/KairoBridge/tests/GodotAdapterTests.cpp)
- [`Migration/KairoBridge/tests/KairoBridgeTests.cpp`](Migration/KairoBridge/tests/KairoBridgeTests.cpp)
- [`Runtime/KairoPlayer/tests/CraftActionLatchTests.cpp`](Runtime/KairoPlayer/tests/CraftActionLatchTests.cpp)
- [`Runtime/KairoPlayer/tests/CraftAutosaveTests.cpp`](Runtime/KairoPlayer/tests/CraftAutosaveTests.cpp)
- [`Runtime/KairoPlayer/tests/CraftCaveWorldTests.cpp`](Runtime/KairoPlayer/tests/CraftCaveWorldTests.cpp)
- [`Runtime/KairoPlayer/tests/CraftCheckpointTests.cpp`](Runtime/KairoPlayer/tests/CraftCheckpointTests.cpp)
- [`Runtime/KairoPlayer/tests/CraftCombatHudTests.cpp`](Runtime/KairoPlayer/tests/CraftCombatHudTests.cpp)
- [`Runtime/KairoPlayer/tests/CraftCombatRewardTests.cpp`](Runtime/KairoPlayer/tests/CraftCombatRewardTests.cpp)
- [`Runtime/KairoPlayer/tests/CraftCreatureNavigationTests.cpp`](Runtime/KairoPlayer/tests/CraftCreatureNavigationTests.cpp)
- [`Runtime/KairoPlayer/tests/CraftCreatureTests.cpp`](Runtime/KairoPlayer/tests/CraftCreatureTests.cpp)
- [`Runtime/KairoPlayer/tests/CraftDaylightTests.cpp`](Runtime/KairoPlayer/tests/CraftDaylightTests.cpp)
- [`Runtime/KairoPlayer/tests/CraftEquipmentEnvelopeTests.cpp`](Runtime/KairoPlayer/tests/CraftEquipmentEnvelopeTests.cpp)
- [`Runtime/KairoPlayer/tests/CraftEquipmentTests.cpp`](Runtime/KairoPlayer/tests/CraftEquipmentTests.cpp)
- [`Runtime/KairoPlayer/tests/CraftHudProjectionTests.cpp`](Runtime/KairoPlayer/tests/CraftHudProjectionTests.cpp)
- [`Runtime/KairoPlayer/tests/CraftHudPublicationTests.cpp`](Runtime/KairoPlayer/tests/CraftHudPublicationTests.cpp)
- [`Runtime/KairoPlayer/tests/CraftInventoryTests.cpp`](Runtime/KairoPlayer/tests/CraftInventoryTests.cpp)
- [`Runtime/KairoPlayer/tests/CraftMeshBatchTests.cpp`](Runtime/KairoPlayer/tests/CraftMeshBatchTests.cpp)
- [`Runtime/KairoPlayer/tests/CraftMiningTests.cpp`](Runtime/KairoPlayer/tests/CraftMiningTests.cpp)
- [`Runtime/KairoPlayer/tests/CraftMouseLookTests.cpp`](Runtime/KairoPlayer/tests/CraftMouseLookTests.cpp)
- [`Runtime/KairoPlayer/tests/CraftNutritionTests.cpp`](Runtime/KairoPlayer/tests/CraftNutritionTests.cpp)
- [`Runtime/KairoPlayer/tests/CraftRecipesHudTests.cpp`](Runtime/KairoPlayer/tests/CraftRecipesHudTests.cpp)
- [`Runtime/KairoPlayer/tests/CraftSaveFileTests.cpp`](Runtime/KairoPlayer/tests/CraftSaveFileTests.cpp)
- [`Runtime/KairoPlayer/tests/CraftSaveTests.cpp`](Runtime/KairoPlayer/tests/CraftSaveTests.cpp)
- [`Runtime/KairoPlayer/tests/CraftStreamSaveTests.cpp`](Runtime/KairoPlayer/tests/CraftStreamSaveTests.cpp)
- [`Runtime/KairoPlayer/tests/CraftStreamingWindowTests.cpp`](Runtime/KairoPlayer/tests/CraftStreamingWindowTests.cpp)
- [`Runtime/KairoPlayer/tests/CraftSurfaceSpawnTests.cpp`](Runtime/KairoPlayer/tests/CraftSurfaceSpawnTests.cpp)
- [`Runtime/KairoPlayer/tests/CraftSurvivalJourneyTests.cpp`](Runtime/KairoPlayer/tests/CraftSurvivalJourneyTests.cpp)
- [`Runtime/KairoPlayer/tests/CraftTargetFeedbackTests.cpp`](Runtime/KairoPlayer/tests/CraftTargetFeedbackTests.cpp)
- [`Runtime/KairoPlayer/tests/CraftVitalsTests.cpp`](Runtime/KairoPlayer/tests/CraftVitalsTests.cpp)
- [`Runtime/KairoPlayer/tests/CraftWorldTests.cpp`](Runtime/KairoPlayer/tests/CraftWorldTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimeAlsaNullTests.cpp`](Runtime/KairoPlayer/tests/RuntimeAlsaNullTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimeAudioBridgeTests.cpp`](Runtime/KairoPlayer/tests/RuntimeAudioBridgeTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimeCharacterMotorBridgeTests.cpp`](Runtime/KairoPlayer/tests/RuntimeCharacterMotorBridgeTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimeCrowdNavigationBridgeTests.cpp`](Runtime/KairoPlayer/tests/RuntimeCrowdNavigationBridgeTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimeEntityLifecycleBridgeTests.cpp`](Runtime/KairoPlayer/tests/RuntimeEntityLifecycleBridgeTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimeFrameClockTests.cpp`](Runtime/KairoPlayer/tests/RuntimeFrameClockTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimeInputGateTests.cpp`](Runtime/KairoPlayer/tests/RuntimeInputGateTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimeLocomotionAnimationBridgeTests.cpp`](Runtime/KairoPlayer/tests/RuntimeLocomotionAnimationBridgeTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimeNativeGameplayTests.cpp`](Runtime/KairoPlayer/tests/RuntimeNativeGameplayTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimeNavigationAgentBridgeTests.cpp`](Runtime/KairoPlayer/tests/RuntimeNavigationAgentBridgeTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimePcm16Tests.cpp`](Runtime/KairoPlayer/tests/RuntimePcm16Tests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimePcmPumpTests.cpp`](Runtime/KairoPlayer/tests/RuntimePcmPumpTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimePcmQueueTests.cpp`](Runtime/KairoPlayer/tests/RuntimePcmQueueTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimePhysicsTopologyTests.cpp`](Runtime/KairoPlayer/tests/RuntimePhysicsTopologyTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimePlayerControllerBridgeTests.cpp`](Runtime/KairoPlayer/tests/RuntimePlayerControllerBridgeTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimeProductionSystemsTests.cpp`](Runtime/KairoPlayer/tests/RuntimeProductionSystemsTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimeProjectTests.cpp`](Runtime/KairoPlayer/tests/RuntimeProjectTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimeRenderResourceTrackingTests.cpp`](Runtime/KairoPlayer/tests/RuntimeRenderResourceTrackingTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimeSaveGameBridgeTests.cpp`](Runtime/KairoPlayer/tests/RuntimeSaveGameBridgeTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimeSceneAudioBridgeTests.cpp`](Runtime/KairoPlayer/tests/RuntimeSceneAudioBridgeTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimeWorldSaveBridgeTests.cpp`](Runtime/KairoPlayer/tests/RuntimeWorldSaveBridgeTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimeWorldStreamingBridgeTests.cpp`](Runtime/KairoPlayer/tests/RuntimeWorldStreamingBridgeTests.cpp)
- [`Runtime/KairoPlayer/tests/RuntimeWorldStreamingPhysicsTests.cpp`](Runtime/KairoPlayer/tests/RuntimeWorldStreamingPhysicsTests.cpp)
- [`Runtime/KairoPlayer/tests/TerrainErrorProfileTests.cpp`](Runtime/KairoPlayer/tests/TerrainErrorProfileTests.cpp)
- [`Runtime/KairoPlayer/tests/TerrainGenerationQueueTests.cpp`](Runtime/KairoPlayer/tests/TerrainGenerationQueueTests.cpp)
- [`Runtime/KairoPlayer/tests/TerrainNativeStoreTests.cpp`](Runtime/KairoPlayer/tests/TerrainNativeStoreTests.cpp)
- [`Runtime/KairoPlayer/tests/TerrainRenderOriginTests.cpp`](Runtime/KairoPlayer/tests/TerrainRenderOriginTests.cpp)
- [`Runtime/KairoPlayer/tests/TerrainScreenErrorTests.cpp`](Runtime/KairoPlayer/tests/TerrainScreenErrorTests.cpp)
- [`Runtime/KairoPlayer/tests/TerrainWalkerTests.cpp`](Runtime/KairoPlayer/tests/TerrainWalkerTests.cpp)
- [`Runtime/KairoPlayer/tests/VoxelChunkSurfaceTests.cpp`](Runtime/KairoPlayer/tests/VoxelChunkSurfaceTests.cpp)
- [`Runtime/KairoPlayer/tests/VoxelInteractionTests.cpp`](Runtime/KairoPlayer/tests/VoxelInteractionTests.cpp)
- [`Runtime/KairoPlayer/tests/VoxelMeshPublicationTests.cpp`](Runtime/KairoPlayer/tests/VoxelMeshPublicationTests.cpp)
- [`Runtime/KairoPlayer/tests/VoxelPlayerTests.cpp`](Runtime/KairoPlayer/tests/VoxelPlayerTests.cpp)
- [`Runtime/KairoRealtimeRenderBridge/tests/FrameAssetCacheTests.cpp`](Runtime/KairoRealtimeRenderBridge/tests/FrameAssetCacheTests.cpp)
- [`Runtime/KairoRealtimeRenderBridge/tests/FrameHierarchyCacheTests.cpp`](Runtime/KairoRealtimeRenderBridge/tests/FrameHierarchyCacheTests.cpp)
- [`Runtime/KairoRealtimeRenderBridge/tests/MaterialFileSizeBudgetTests.cpp`](Runtime/KairoRealtimeRenderBridge/tests/MaterialFileSizeBudgetTests.cpp)
- [`Runtime/KairoRealtimeRenderBridge/tests/RealtimeSceneBridgeTests.cpp`](Runtime/KairoRealtimeRenderBridge/tests/RealtimeSceneBridgeTests.cpp)
- [`Runtime/KairoRenderBridge/tests/EditorOfflineRenderServiceTests.cpp`](Runtime/KairoRenderBridge/tests/EditorOfflineRenderServiceTests.cpp)
- [`Runtime/KairoRenderBridge/tests/MaterialPreviewBridgeTests.cpp`](Runtime/KairoRenderBridge/tests/MaterialPreviewBridgeTests.cpp)
- [`Runtime/KairoRenderBridge/tests/SceneSnapshotBridgeTests.cpp`](Runtime/KairoRenderBridge/tests/SceneSnapshotBridgeTests.cpp)
- [`Samples/Phase1Game/tests/PackagePhase1.cmake`](Samples/Phase1Game/tests/PackagePhase1.cmake)
- [`Samples/Phase1Game/tests/Phase1GameStateTests.cpp`](Samples/Phase1Game/tests/Phase1GameStateTests.cpp)
- [`scripts/tests/test_audit_ecosystem_ci.py`](scripts/tests/test_audit_ecosystem_ci.py)

## Study method

For each file, identify inputs, output, ownership, lifetime, units, state
transitions, callers, downstream consumers, exceptions and the test oracle.
Read the relevant tests immediately after each function or subsystem.
Reimplement a small example without copying, then compare its behavior.
A link in this inventory does not certify that every function is annotated.

## New M50 gameplay reading path

1. [Quest and dialogue runtime](Runtime/KairoPlayer/RuntimeQuestDialogue.hpp) — read objective invariants, completion, restore, then dialogue choice routing.
2. [Regression tests](Runtime/KairoPlayer/tests/RuntimeQuestDialogueTests.cpp) — trace invalid restore and randomized prerequisites.
3. [Terminal RPG sample](Samples/QuestDialogue/QuestDialogueDemo.cpp) — trace a player choice to a winning state.
4. [Build and play instructions](Samples/QuestDialogue/README.md).

## Inventory coverage at M50 baseline

All 26 sibling trees from the current workspace.lock have individual file checklists: 700 filtered sibling paths, plus 254 baseline integration paths and the new quest/dialogue reading path. Generated artifacts, vendored trees, binaries and media are outside the first-party teaching inventory. Update checklists when the lock changes; file-level dependency traces and substantive comments still require review.

## M50 animation lab

1. [Analytic IK kernel](Runtime/KairoPlayer/RuntimeTwoBoneIK.hpp): vector projection, triangle geometry, clamping and pole singularities.
2. [IK regression](Runtime/KairoPlayer/tests/RuntimeTwoBoneIKTests.cpp): bone length and target invariants.
3. [Interactive CPU lab](Samples/AnimationLab/TwoBoneIKDemo.cpp) and [instructions](Samples/AnimationLab/README.md).
4. [Pinned animation runtime and skin palette](docs/study/KairoEngineCore.md): read AnimationRuntime.cppm and GltfSkinningRuntime.cppm before the future joint rotation adapter.

## Quest persistence follow-up

Read [CraftSaveFile.hpp](Runtime/KairoPlayer/CraftSaveFile.hpp) first (bounded I/O and publication), then [RuntimeQuestSave.hpp](Runtime/KairoPlayer/RuntimeQuestSave.hpp) (versioned format and two-object transaction), [save tests](Runtime/KairoPlayer/tests/RuntimeQuestSaveTests.cpp), and [the demo](Samples/QuestDialogue/QuestDialogueDemo.cpp).

## M50 animation retarget follow-up

1. [Retarget kernel](Runtime/KairoPlayer/RuntimeAnimationRetarget.hpp): quaternion normalization, Hamilton product, inverse, name and parent mapping, root motion scaling.
2. [Tests](Runtime/KairoPlayer/tests/RuntimeAnimationRetargetTests.cpp): compare rest-relative rotations, proportions, noncommuting order and rejection.
3. [Synthetic clip sample](Samples/AnimationLab/RetargetDemo.cpp): follow each pose into the mapped skeleton.
4. [EngineCore animation and skinning inventory](docs/study/KairoEngineCore.md) for future native adapter integration.

## M50 networking lab

1. [Wire protocol and authority](Runtime/KairoPlayer/RuntimeNetworkProtocol.hpp) — fixed frame schema, network byte order, ticks, validation and authoritative state.
2. [Protocol oracle](Runtime/KairoPlayer/tests/RuntimeNetworkProtocolTests.cpp).
3. [TCP transport](Runtime/KairoPlayer/RuntimeLoopbackTcp.hpp) — RAII, socket setup, listen/accept/connect, partial sends/receives and timeouts.
4. [Native pair](Samples/NetworkLab/NetworkLab.cpp), [Python process harness](Samples/NetworkLab/run_pair.py), [instructions](Samples/NetworkLab/README.md).

## M50 network synchronization follow-up

After the protocol/authority and socket lab, read [prediction](Runtime/KairoPlayer/RuntimeNetworkPrediction.hpp), [impairment and correction tests](Runtime/KairoPlayer/tests/RuntimeNetworkPredictionTests.cpp), and the [integrated client](Samples/NetworkLab/NetworkLab.cpp). Trace the difference between authoritative and predicted state, acknowledgment removal, replay order and backpressure.

## Teaching pass: audio, GPU and persistence

Read in this order:

1. [RuntimePcm16.hpp](Runtime/KairoPlayer/RuntimePcm16.hpp) — sample ranges and device conversion.
2. [RuntimePcmQueue.hpp](Runtime/KairoPlayer/RuntimePcmQueue.hpp) — SPSC ring storage and acquire/release publication.
3. [RuntimePcmPump.hpp](Runtime/KairoPlayer/RuntimePcmPump.hpp) — device partial writes and underflow.
4. [RuntimeRenderResourceTracking.hpp](Runtime/KairoPlayer/RuntimeRenderResourceTracking.hpp) — native handle ownership and rollback.
5. [CraftSaveFile.hpp](Runtime/KairoPlayer/CraftSaveFile.hpp) — bounded byte I/O and file publication.
6. [CraftAutosave.hpp](Runtime/KairoPlayer/CraftAutosave.hpp) — dirty state, active-time deadlines and retry.

Follow their matching tests from the integration inventory. These six paths have additional function-level annotations; this is not an assertion of full workspace coverage.

## M50 weather

Read [CraftDaylight](Runtime/KairoPlayer/CraftDaylight.hpp), [weather timeline](Runtime/KairoPlayer/RuntimeWeather.hpp), [weather tests](Runtime/KairoPlayer/tests/RuntimeWeatherTests.cpp), then the applyDaylight and active loop in [CraftStreamDemo](Runtime/KairoPlayer/CraftStreamDemo.cpp). Follow the quantized simulation key to RenderEnvironment and the scene submission.

After the weather timeline, read [rain geometry sampling](Runtime/KairoPlayer/RuntimeRainStreaks.hpp), [rain tests](Runtime/KairoPlayer/tests/RuntimeRainStreaksTests.cpp), then the rain draw construction and native capture gate in [CraftStreamDemo](Runtime/KairoPlayer/CraftStreamDemo.cpp). Trace absolute positions into render-relative float transforms and shared GPU handle lifetime.

## M50 terrain water

1. [Water grid](Runtime/KairoPlayer/RuntimeWaterGrid.hpp): bed/depth representation, free-surface gradients, pairwise flux, donor bounds and transactional publication.
2. [Conservation tests](Runtime/KairoPlayer/tests/RuntimeWaterGridTests.cpp): lake-at-rest, dry ridge and random beds.
3. [Basin lab](Samples/WaterLab/WaterLab.cpp) and [run guide](Samples/WaterLab/README.md).
4. Compare the narrower density diffusion in [EngineCore ProductionSystems.cppm](docs/study/KairoEngineCore.md); neither model is a full fluid solver.

## M50 soft-body dynamics

1. [Soft body solver](Runtime/KairoPlayer/RuntimeSoftBody.hpp): position/velocity state, signed tetra volume, inverse mass, compliance, gradient projection and fixed-step reconstruction.
2. [Constraint and ground tests](Runtime/KairoPlayer/tests/RuntimeSoftBodyTests.cpp): compare a rest pose, pinned node, seeded drops and replay.
3. Compare the existing cloth solver and PhysicsEngine contact contracts through [EngineCore](docs/study/KairoEngineCore.md) and [PhysicsEngine](docs/study/KairoPhysicsEngine.md). Follow the future mesh presentation adapter only after understanding signed volume derivatives.

## Native simulation lab presentation

After the water and soft-body kernels, read [SimulationLab.cpp](Runtime/KairoPlayer/SimulationLab.cpp): UnitCube -> Box/WaterScene -> TetraMesh -> publish -> fixed-step advance -> Capture -> resource teardown. Compare the pinned Renderer Mesh, RenderScene and RendererRuntime contracts in [KairoRenderer inventory](docs/study/KairoRenderer.md). [Native run instructions](Samples/WaterLab/NATIVE.md).

## M50 destruction

Read [RuntimeBreakableCube.hpp](Runtime/KairoPlayer/RuntimeBreakableCube.hpp), then [conservation/ground tests](Runtime/KairoPlayer/tests/RuntimeBreakableCubeTests.cpp), then the destruction branch of [SimulationLab.cpp](Runtime/KairoPlayer/SimulationLab.cpp). Trace impact impulse, mass-weighted velocities, fixed-step motion, ground response and mesh instance ownership. Compare PhysicsEngine's actual rigid contact contracts before integrating arbitrary collision-driven fracture.

## Teaching pass: terrain and voxel mesh architecture

1. [TerrainRenderOrigin.hpp](Runtime/KairoPlayer/TerrainRenderOrigin.hpp) — double-to-float rendering boundary, signed cell indices.
2. [CraftWorld.hpp](Runtime/KairoPlayer/CraftWorld.hpp) — pure finite occupancy, material and chunk identity. Read its mesh adapter after the greedy mesher.
3. [CraftStreamingWindow.hpp](Runtime/KairoPlayer/CraftStreamingWindow.hpp) — negative floor division, stable residency plans, dirty slots.
4. [VoxelChunkSurface.hpp](Runtime/KairoPlayer/VoxelChunkSurface.hpp) — interior/halo sampling, face ownership, greedy merge and winding; then return to BuildCraftMesh.
5. [CraftSurfaceSpawn.hpp](Runtime/KairoPlayer/CraftSurfaceSpawn.hpp) — support and body-clearance query, deterministic search.
6. Trace [CraftStreamDemo](Runtime/KairoPlayer/CraftStreamDemo.cpp) and [SimulationLab](Runtime/KairoPlayer/SimulationLab.cpp) from occupancy through GPU submission. Follow the matching tests from the integration inventory.

## M50 menu authoring

1. [RuntimeMenuDocument.hpp](Runtime/KairoPlayer/RuntimeMenuDocument.hpp): normalized integer geometry, strict parsing, canonical save and validation.
2. [Parser tests](Runtime/KairoPlayer/tests/RuntimeMenuDocumentTests.cpp).
3. [Authored sample](Samples/AuthoredMenu/Config/Menu.kmenu) and [project](Samples/AuthoredMenu/Project.kproject).
4. [RuntimeShippingBridge.cppm](Runtime/KairoPlayer/RuntimeShippingBridge.cppm) maps the document into existing EngineCore RuntimeUIScene and LocalizationCatalog; read the pinned ShippingRuntime.cppm through [EngineCore study catalog](docs/study/KairoEngineCore.md).

After the authored menu/ShippingBridge, read [RuntimeMenuFocus](Runtime/KairoPlayer/RuntimeMenuFocus.hpp), [focus tests](Runtime/KairoPlayer/tests/RuntimeMenuFocusTests.cpp), [RuntimeMenuHost](Runtime/KairoPlayer/RuntimeMenuHost.cppm) and the menu-frame branch of [main.cpp](Runtime/KairoPlayer/main.cpp). Follow GLFW logical pixels to physical framebuffer layout, release edges, capture, glyph rectangles and gameplay pause.
