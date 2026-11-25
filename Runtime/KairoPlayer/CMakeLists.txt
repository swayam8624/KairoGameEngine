set(KAIRO_PLAYER_NATIVE_GAMEPLAY_TARGETS "" CACHE STRING
    "Semicolon-separated CMake targets that register project native gameplay types with KairoPlayer")

add_library(KairoPlayerRuntime)
target_sources(KairoPlayerRuntime PUBLIC FILE_SET CXX_MODULES
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    FILES RuntimeProject.cppm RuntimeRenderBridge.cppm RuntimeAudioBridge.cppm RuntimeSceneAudioBridge.cppm RuntimePhysicsBridge.cppm
        RuntimeCharacterMotorBridge.cppm RuntimePlayerControllerBridge.cppm RuntimeNavigationAgentBridge.cppm RuntimeCrowdNavigationBridge.cppm RuntimeLocomotionAnimationBridge.cppm RuntimeEntityLifecycleBridge.cppm RuntimeSaveGameBridge.cppm RuntimeWorldSaveBridge.cppm RuntimeInputBridge.cppm RuntimeLogicBridge.cppm RuntimeNativeGameplayBridge.cppm RuntimeProductionSystemsBridge.cppm RuntimeShippingBridge.cppm RuntimePackaging.cppm
        RuntimeWorldStreamingBridge.cppm)
# Prefer the small, deterministic gameplay module. Existing sibling checkouts
# predating the split still provide that module through Kairo::AI.
if(TARGET Kairo::GameplayAI)
    set(_kairo_player_gameplay_ai Kairo::GameplayAI)
elseif(KAIRO_GAME_ENGINE_REQUIRE_GAMEPLAY_AI_SPLIT)
    message(FATAL_ERROR "Gameplay-only AI target missing; use KairoAI refactor/gameplay-runtime-split or a newer compatible revision.")
else()
    message(STATUS "KairoPlayer: legacy KairoAI target; update sibling for gameplay-only linkage")
    set(_kairo_player_gameplay_ai Kairo::AI)
endif()
target_link_libraries(KairoPlayerRuntime PUBLIC
    Kairo::GameEngine KairoRealtimeRenderBridge ${_kairo_player_gameplay_ai})
target_compile_features(KairoPlayerRuntime PUBLIC cxx_std_23)

# Opt-in until the reviewed EngineCore procedural stack is accepted and pinned.
option(KAIRO_PLAYER_BUILD_PROCEDURAL_TERRAIN "Build the procedural terrain viewport demo" OFF)
if(KAIRO_PLAYER_BUILD_PROCEDURAL_TERRAIN)
    find_package(Threads REQUIRED)
    target_link_libraries(KairoPlayerRuntime PRIVATE Threads::Threads)
    target_sources(KairoPlayerRuntime PUBLIC FILE_SET CXX_MODULES
        FILES RuntimeProceduralTerrainBridge.cppm)
    add_executable(KairoTerrainDemo TerrainDemo.cpp)
    target_link_libraries(KairoTerrainDemo PRIVATE KairoPlayerRuntime)
    set_property(TARGET KairoTerrainDemo PROPERTY COMPILE_WARNING_AS_ERROR ON)
    add_executable(KairoTerrainExplorer TerrainExplorer.cpp)
    target_link_libraries(KairoTerrainExplorer PRIVATE KairoPlayerRuntime)
    set_property(TARGET KairoTerrainExplorer PROPERTY COMPILE_WARNING_AS_ERROR ON)
    add_executable(KairoCraft CraftDemo.cpp)
    target_link_libraries(KairoCraft PRIVATE KairoPlayerRuntime)
    set_property(TARGET KairoCraft PROPERTY COMPILE_WARNING_AS_ERROR ON)
    add_executable(KairoCraftStream CraftStreamDemo.cpp)
    target_link_libraries(KairoCraftStream PRIVATE KairoPlayerRuntime)
    set_property(TARGET KairoCraftStream PROPERTY COMPILE_WARNING_AS_ERROR ON)
    add_executable(KairoCraftCaves CraftCaveDemo.cpp)
    target_link_libraries(KairoCraftCaves PRIVATE KairoPlayerRuntime Threads::Threads)
    set_property(TARGET KairoCraftCaves PROPERTY COMPILE_WARNING_AS_ERROR ON)
endif()

add_executable(KairoPlayer main.cpp)
target_link_libraries(KairoPlayer PRIVATE KairoPlayerRuntime)
foreach(native_target IN LISTS KAIRO_PLAYER_NATIVE_GAMEPLAY_TARGETS)
    if(NOT TARGET ${native_target})
        message(FATAL_ERROR "KAIRO_PLAYER_NATIVE_GAMEPLAY_TARGETS names missing target '${native_target}'.")
    endif()
    target_link_libraries(KairoPlayer PRIVATE ${native_target})
endforeach()

if(BUILD_TESTING)
    find_package(Threads REQUIRED)
    add_executable(KairoCraftEquipmentEnvelopeTests tests/CraftEquipmentEnvelopeTests.cpp)
    target_compile_features(KairoCraftEquipmentEnvelopeTests PRIVATE cxx_std_23)
    set_property(TARGET KairoCraftEquipmentEnvelopeTests PROPERTY COMPILE_WARNING_AS_ERROR ON)
    add_test(NAME KairoCraftEquipmentEnvelopeTests COMMAND KairoCraftEquipmentEnvelopeTests)
    add_executable(KairoCraftEquipmentTests tests/CraftEquipmentTests.cpp)
    target_compile_features(KairoCraftEquipmentTests PRIVATE cxx_std_23)
    set_property(TARGET KairoCraftEquipmentTests PROPERTY COMPILE_WARNING_AS_ERROR ON)
    add_test(NAME KairoCraftEquipmentTests COMMAND KairoCraftEquipmentTests)
    add_executable(KairoCraftMiningTests tests/CraftMiningTests.cpp)
    target_compile_features(KairoCraftMiningTests PRIVATE cxx_std_23)
    set_property(TARGET KairoCraftMiningTests PROPERTY COMPILE_WARNING_AS_ERROR ON)
    add_test(NAME KairoCraftMiningTests COMMAND KairoCraftMiningTests)
    add_executable(KairoCraftVitalsTests tests/CraftVitalsTests.cpp)
    target_compile_features(KairoCraftVitalsTests PRIVATE cxx_std_23)
    set_property(TARGET KairoCraftVitalsTests PROPERTY COMPILE_WARNING_AS_ERROR ON)
    add_test(NAME KairoCraftVitalsTests COMMAND KairoCraftVitalsTests)
    add_executable(KairoCraftHudPublicationTests tests/CraftHudPublicationTests.cpp)
    target_compile_features(KairoCraftHudPublicationTests PRIVATE cxx_std_23)
    set_property(TARGET KairoCraftHudPublicationTests PROPERTY COMPILE_WARNING_AS_ERROR ON)
    add_test(NAME KairoCraftHudPublicationTests COMMAND KairoCraftHudPublicationTests)
    add_executable(KairoCraftHudProjectionTests tests/CraftHudProjectionTests.cpp)
    target_compile_features(KairoCraftHudProjectionTests PRIVATE cxx_std_23)
    set_property(TARGET KairoCraftHudProjectionTests PROPERTY COMPILE_WARNING_AS_ERROR ON)
    add_test(NAME KairoCraftHudProjectionTests COMMAND KairoCraftHudProjectionTests)
    add_executable(KairoCraftRecipesHudTests tests/CraftRecipesHudTests.cpp)
    target_compile_features(KairoCraftRecipesHudTests PRIVATE cxx_std_23)
    set_property(TARGET KairoCraftRecipesHudTests PROPERTY COMPILE_WARNING_AS_ERROR ON)
    add_test(NAME KairoCraftRecipesHudTests COMMAND KairoCraftRecipesHudTests)
    add_executable(KairoCraftCaveWorldTests tests/CraftCaveWorldTests.cpp)
    target_compile_features(KairoCraftCaveWorldTests PRIVATE cxx_std_23)
    target_link_libraries(KairoCraftCaveWorldTests PRIVATE Threads::Threads)
    set_property(TARGET KairoCraftCaveWorldTests PROPERTY COMPILE_WARNING_AS_ERROR ON)
    add_test(NAME KairoCraftCaveWorldTests COMMAND KairoCraftCaveWorldTests)
    add_executable(KairoCraftInventoryTests tests/CraftInventoryTests.cpp)
    target_compile_features(KairoCraftInventoryTests PRIVATE cxx_std_23)
    set_property(TARGET KairoCraftInventoryTests PROPERTY COMPILE_WARNING_AS_ERROR ON)
    add_test(NAME KairoCraftInventoryTests COMMAND KairoCraftInventoryTests)
    add_executable(KairoCraftMeshBatchTests tests/CraftMeshBatchTests.cpp)
    target_compile_features(KairoCraftMeshBatchTests PRIVATE cxx_std_23)
    target_link_libraries(KairoCraftMeshBatchTests PRIVATE Threads::Threads)
    set_property(TARGET KairoCraftMeshBatchTests PROPERTY COMPILE_WARNING_AS_ERROR ON)
    add_test(NAME KairoCraftMeshBatchTests COMMAND KairoCraftMeshBatchTests)
    add_executable(KairoCraftStreamSaveTests tests/CraftStreamSaveTests.cpp)
    target_compile_features(KairoCraftStreamSaveTests PRIVATE cxx_std_23)
    set_property(TARGET KairoCraftStreamSaveTests PROPERTY COMPILE_WARNING_AS_ERROR ON)
    add_test(NAME KairoCraftStreamSaveTests COMMAND KairoCraftStreamSaveTests)
    add_executable(KairoCraftStreamingWindowTests tests/CraftStreamingWindowTests.cpp)
    target_compile_features(KairoCraftStreamingWindowTests PRIVATE cxx_std_23)
    set_property(TARGET KairoCraftStreamingWindowTests PROPERTY COMPILE_WARNING_AS_ERROR ON)
    add_test(NAME KairoCraftStreamingWindowTests COMMAND KairoCraftStreamingWindowTests)
    add_executable(KairoVoxelPlayerTests tests/VoxelPlayerTests.cpp)
    target_compile_features(KairoVoxelPlayerTests PRIVATE cxx_std_23)
    set_property(TARGET KairoVoxelPlayerTests PROPERTY COMPILE_WARNING_AS_ERROR ON)
    add_test(NAME KairoVoxelPlayerTests COMMAND KairoVoxelPlayerTests)
    add_executable(KairoCraftSaveTests tests/CraftSaveTests.cpp)
    target_compile_features(KairoCraftSaveTests PRIVATE cxx_std_23)
    set_property(TARGET KairoCraftSaveTests PROPERTY COMPILE_WARNING_AS_ERROR ON)
    add_test(NAME KairoCraftSaveTests COMMAND KairoCraftSaveTests)
    add_executable(KairoVoxelMeshPublicationTests tests/VoxelMeshPublicationTests.cpp)
    target_compile_features(KairoVoxelMeshPublicationTests PRIVATE cxx_std_23)
    set_property(TARGET KairoVoxelMeshPublicationTests PROPERTY COMPILE_WARNING_AS_ERROR ON)
    add_test(NAME KairoVoxelMeshPublicationTests COMMAND KairoVoxelMeshPublicationTests)
    add_executable(KairoCraftWorldTests tests/CraftWorldTests.cpp)
    target_compile_features(KairoCraftWorldTests PRIVATE cxx_std_23)
    set_property(TARGET KairoCraftWorldTests PROPERTY COMPILE_WARNING_AS_ERROR ON)
    add_test(NAME KairoCraftWorldTests COMMAND KairoCraftWorldTests)
    add_executable(KairoVoxelChunkSurfaceTests tests/VoxelChunkSurfaceTests.cpp)
    target_compile_features(KairoVoxelChunkSurfaceTests PRIVATE cxx_std_23)
    set_property(TARGET KairoVoxelChunkSurfaceTests PROPERTY COMPILE_WARNING_AS_ERROR ON)
    add_test(NAME KairoVoxelChunkSurfaceTests COMMAND KairoVoxelChunkSurfaceTests)
    add_executable(KairoVoxelInteractionTests tests/VoxelInteractionTests.cpp)
    target_compile_features(KairoVoxelInteractionTests PRIVATE cxx_std_23)
    set_property(TARGET KairoVoxelInteractionTests PROPERTY COMPILE_WARNING_AS_ERROR ON)
    add_test(NAME KairoVoxelInteractionTests COMMAND KairoVoxelInteractionTests)
    add_executable(KairoTerrainWalkerTests tests/TerrainWalkerTests.cpp)
    target_compile_features(KairoTerrainWalkerTests PRIVATE cxx_std_23)
    set_property(TARGET KairoTerrainWalkerTests PROPERTY COMPILE_WARNING_AS_ERROR ON)
    add_test(NAME KairoTerrainWalkerTests COMMAND KairoTerrainWalkerTests)
    add_executable(KairoTerrainScreenErrorTests tests/TerrainScreenErrorTests.cpp)
    target_compile_features(KairoTerrainScreenErrorTests PRIVATE cxx_std_23)
    set_property(TARGET KairoTerrainScreenErrorTests PROPERTY COMPILE_WARNING_AS_ERROR ON)
    add_test(NAME KairoTerrainScreenErrorTests COMMAND KairoTerrainScreenErrorTests)
    add_executable(KairoTerrainErrorProfileTests tests/TerrainErrorProfileTests.cpp)
    target_compile_features(KairoTerrainErrorProfileTests PRIVATE cxx_std_23)
    set_property(TARGET KairoTerrainErrorProfileTests PROPERTY COMPILE_WARNING_AS_ERROR ON)
    add_test(NAME KairoTerrainErrorProfileTests COMMAND KairoTerrainErrorProfileTests)
    add_executable(KairoTerrainRenderOriginTests tests/TerrainRenderOriginTests.cpp)
    target_compile_features(KairoTerrainRenderOriginTests PRIVATE cxx_std_23)
    set_property(TARGET KairoTerrainRenderOriginTests PROPERTY COMPILE_WARNING_AS_ERROR ON)
    add_test(NAME KairoTerrainRenderOriginTests COMMAND KairoTerrainRenderOriginTests)
    add_executable(KairoTerrainGenerationQueueTests tests/TerrainGenerationQueueTests.cpp)
    target_compile_features(KairoTerrainGenerationQueueTests PRIVATE cxx_std_23)
    target_link_libraries(KairoTerrainGenerationQueueTests PRIVATE Threads::Threads)
    add_test(NAME KairoTerrainGenerationQueueTests COMMAND KairoTerrainGenerationQueueTests)
    add_executable(KairoTerrainNativeStoreTests tests/TerrainNativeStoreTests.cpp)
    target_compile_features(KairoTerrainNativeStoreTests PRIVATE cxx_std_23)
    add_test(NAME KairoTerrainNativeStoreTests COMMAND KairoTerrainNativeStoreTests)
    # These callbacks exercise the SAME handle-adoption/cleanup helper used in
    # RuntimeRenderBridge.cppm. A native GPU/window is not required.
    add_executable(KairoPlayerRenderResourceTrackingTests
        tests/RuntimeRenderResourceTrackingTests.cpp)
    target_compile_features(KairoPlayerRenderResourceTrackingTests PRIVATE cxx_std_23)
    add_test(NAME KairoPlayerRenderResourceTrackingTests
        COMMAND KairoPlayerRenderResourceTrackingTests)

    add_executable(KairoPlayerRuntimeTests tests/RuntimeProjectTests.cpp)
    target_link_libraries(KairoPlayerRuntimeTests PRIVATE KairoPlayerRuntime)
    add_test(NAME KairoPlayerRuntimeTests COMMAND KairoPlayerRuntimeTests)

    add_executable(KairoPlayerAudioBridgeTests tests/RuntimeAudioBridgeTests.cpp)
    target_link_libraries(KairoPlayerAudioBridgeTests PRIVATE KairoPlayerRuntime)
    add_test(NAME KairoPlayerAudioBridgeTests COMMAND KairoPlayerAudioBridgeTests)

    add_executable(KairoPlayerSceneAudioBridgeTests tests/RuntimeSceneAudioBridgeTests.cpp)
    target_link_libraries(KairoPlayerSceneAudioBridgeTests PRIVATE KairoPlayerRuntime)
    add_test(NAME KairoPlayerSceneAudioBridgeTests COMMAND KairoPlayerSceneAudioBridgeTests)

    add_executable(KairoPlayerWorldStreamingBridgeTests tests/RuntimeWorldStreamingBridgeTests.cpp)
    target_link_libraries(KairoPlayerWorldStreamingBridgeTests PRIVATE KairoPlayerRuntime)
    add_test(NAME KairoPlayerWorldStreamingBridgeTests COMMAND KairoPlayerWorldStreamingBridgeTests)

    add_executable(KairoPlayerPhysicsTopologyTests tests/RuntimePhysicsTopologyTests.cpp)
    target_link_libraries(KairoPlayerPhysicsTopologyTests PRIVATE KairoPlayerRuntime)
    add_test(NAME KairoPlayerPhysicsTopologyTests COMMAND KairoPlayerPhysicsTopologyTests)

    add_executable(KairoPlayerWorldStreamingPhysicsTests tests/RuntimeWorldStreamingPhysicsTests.cpp)
    target_link_libraries(KairoPlayerWorldStreamingPhysicsTests PRIVATE KairoPlayerRuntime)
    add_test(NAME KairoPlayerWorldStreamingPhysicsTests COMMAND KairoPlayerWorldStreamingPhysicsTests)

    add_executable(KairoPlayerCharacterMotorBridgeTests tests/RuntimeCharacterMotorBridgeTests.cpp)
    target_link_libraries(KairoPlayerCharacterMotorBridgeTests PRIVATE KairoPlayerRuntime)
    add_test(NAME KairoPlayerCharacterMotorBridgeTests COMMAND KairoPlayerCharacterMotorBridgeTests)

    add_executable(KairoPlayerControllerBridgeTests tests/RuntimePlayerControllerBridgeTests.cpp)
    target_link_libraries(KairoPlayerControllerBridgeTests PRIVATE KairoPlayerRuntime)
    add_test(NAME KairoPlayerControllerBridgeTests COMMAND KairoPlayerControllerBridgeTests)

    add_executable(KairoPlayerNavigationAgentBridgeTests tests/RuntimeNavigationAgentBridgeTests.cpp)
    target_link_libraries(KairoPlayerNavigationAgentBridgeTests PRIVATE KairoPlayerRuntime)
    add_test(NAME KairoPlayerNavigationAgentBridgeTests COMMAND KairoPlayerNavigationAgentBridgeTests)

    add_executable(KairoPlayerCrowdNavigationBridgeTests tests/RuntimeCrowdNavigationBridgeTests.cpp)
    target_link_libraries(KairoPlayerCrowdNavigationBridgeTests PRIVATE KairoPlayerRuntime)
    add_test(NAME KairoPlayerCrowdNavigationBridgeTests COMMAND KairoPlayerCrowdNavigationBridgeTests)

    add_executable(KairoPlayerLocomotionAnimationBridgeTests tests/RuntimeLocomotionAnimationBridgeTests.cpp)
    target_link_libraries(KairoPlayerLocomotionAnimationBridgeTests PRIVATE KairoPlayerRuntime)
    add_test(NAME KairoPlayerLocomotionAnimationBridgeTests COMMAND KairoPlayerLocomotionAnimationBridgeTests)

    add_executable(KairoPlayerEntityLifecycleTests tests/RuntimeEntityLifecycleBridgeTests.cpp)
    target_link_libraries(KairoPlayerEntityLifecycleTests PRIVATE KairoPlayerRuntime)
    add_test(NAME KairoPlayerEntityLifecycleTests COMMAND KairoPlayerEntityLifecycleTests)

    add_executable(KairoPlayerSaveGameBridgeTests tests/RuntimeSaveGameBridgeTests.cpp)
    target_link_libraries(KairoPlayerSaveGameBridgeTests PRIVATE KairoPlayerRuntime)
    add_test(NAME KairoPlayerSaveGameBridgeTests COMMAND KairoPlayerSaveGameBridgeTests)

    add_executable(KairoPlayerWorldSaveBridgeTests tests/RuntimeWorldSaveBridgeTests.cpp)
    target_link_libraries(KairoPlayerWorldSaveBridgeTests PRIVATE KairoPlayerRuntime)
    add_test(NAME KairoPlayerWorldSaveBridgeTests COMMAND KairoPlayerWorldSaveBridgeTests)

    add_executable(KairoPlayerNativeGameplayTests tests/RuntimeNativeGameplayTests.cpp)
    target_link_libraries(KairoPlayerNativeGameplayTests PRIVATE KairoPlayerRuntime)
    add_test(NAME KairoPlayerNativeGameplayTests COMMAND KairoPlayerNativeGameplayTests)

    add_executable(KairoPlayerProductionSystemsTests tests/RuntimeProductionSystemsTests.cpp)
    target_link_libraries(KairoPlayerProductionSystemsTests PRIVATE KairoPlayerRuntime)
    add_test(NAME KairoPlayerProductionSystemsTests COMMAND KairoPlayerProductionSystemsTests)

    if(EXISTS "${CMAKE_SOURCE_DIR}/Samples/PlayableVerticalSlice/Project.kproject")
        add_test(NAME KairoPlayerPlayableVerticalSliceValidate COMMAND KairoPlayer
            ${CMAKE_SOURCE_DIR}/Samples/PlayableVerticalSlice/Project.kproject --validate)
        set_tests_properties(KairoPlayerPlayableVerticalSliceValidate PROPERTIES TIMEOUT 20)
    endif()

    if(NOT WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/Samples/SharedContentShowcase/Project.kproject")
        add_test(NAME KairoPlayerOpenGLSharedContentSmoke COMMAND KairoPlayer
            ${CMAKE_SOURCE_DIR}/Samples/SharedContentShowcase/Project.kproject
            --renderer opengl --smoke)
        set_tests_properties(KairoPlayerOpenGLSharedContentSmoke PROPERTIES
            TIMEOUT 20 SKIP_RETURN_CODE 77)
        if(APPLE)
            add_test(NAME KairoPlayerMetalSharedContentSmoke COMMAND KairoPlayer
                ${CMAKE_SOURCE_DIR}/Samples/SharedContentShowcase/Project.kproject
                --renderer metal --smoke)
            set_tests_properties(KairoPlayerMetalSharedContentSmoke PROPERTIES
                TIMEOUT 20 SKIP_RETURN_CODE 77)
        endif()
    endif()
    if(NOT WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/Samples/PlayableVerticalSlice/Project.kproject")
        # Headless macOS CI cannot reliably create an OpenGL GLFW window even
        # when Metal windows work. Keep OpenGL coverage on Linux/Xvfb, Metal
        # coverage on native macOS, and D3D12 coverage on Windows. Do not turn
        # an actual GLFW error into a passing smoke test.
        if(NOT APPLE)
            add_test(NAME KairoPlayerOpenGLPlayableVerticalSliceSmoke COMMAND KairoPlayer
                ${CMAKE_SOURCE_DIR}/Samples/PlayableVerticalSlice/Project.kproject
                --renderer opengl --smoke)
            set_tests_properties(KairoPlayerOpenGLPlayableVerticalSliceSmoke PROPERTIES
                TIMEOUT 20 SKIP_RETURN_CODE 77)
        endif()
        if(APPLE)
            add_test(NAME KairoPlayerMetalPlayableVerticalSliceSmoke COMMAND KairoPlayer
                ${CMAKE_SOURCE_DIR}/Samples/PlayableVerticalSlice/Project.kproject
                --renderer metal --smoke)
            set_tests_properties(KairoPlayerMetalPlayableVerticalSliceSmoke PROPERTIES
                TIMEOUT 20 SKIP_RETURN_CODE 77)
        endif()
    endif()
    if(WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/Samples/SharedContentShowcase/Project.kproject")
        add_test(NAME KairoPlayerDirect3D12SharedContentSmoke COMMAND KairoPlayer
            ${CMAKE_SOURCE_DIR}/Samples/SharedContentShowcase/Project.kproject
            --renderer d3d12 --smoke)
        set_tests_properties(KairoPlayerDirect3D12SharedContentSmoke PROPERTIES
            TIMEOUT 30 SKIP_RETURN_CODE 77)
    endif()
    if(WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/Samples/PlayableVerticalSlice/Project.kproject")
        add_test(NAME KairoPlayerDirect3D12PlayableVerticalSliceSmoke COMMAND KairoPlayer
            ${CMAKE_SOURCE_DIR}/Samples/PlayableVerticalSlice/Project.kproject
            --renderer d3d12 --smoke)
        set_tests_properties(KairoPlayerDirect3D12PlayableVerticalSliceSmoke PROPERTIES
            TIMEOUT 30 SKIP_RETURN_CODE 77)
    endif()
endif()
