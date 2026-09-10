set(KAIRO_PLAYER_NATIVE_GAMEPLAY_TARGETS "" CACHE STRING
    "Semicolon-separated CMake targets that register project native gameplay types with KairoPlayer")

add_library(KairoPlayerRuntime)
target_sources(KairoPlayerRuntime PUBLIC FILE_SET CXX_MODULES
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    FILES RuntimeProject.cppm RuntimeRenderBridge.cppm RuntimeAudioBridge.cppm RuntimeSceneAudioBridge.cppm RuntimePhysicsBridge.cppm
        RuntimeCharacterMotorBridge.cppm RuntimeNavigationAgentBridge.cppm RuntimeCrowdNavigationBridge.cppm RuntimeLocomotionAnimationBridge.cppm RuntimeSaveGameBridge.cppm RuntimeInputBridge.cppm RuntimeLogicBridge.cppm RuntimeNativeGameplayBridge.cppm RuntimeProductionSystemsBridge.cppm RuntimeShippingBridge.cppm RuntimePackaging.cppm
        RuntimeWorldStreamingBridge.cppm)
target_link_libraries(KairoPlayerRuntime PUBLIC Kairo::GameEngine KairoRealtimeRenderBridge Kairo::AI)
target_compile_features(KairoPlayerRuntime PUBLIC cxx_std_23)

add_executable(KairoPlayer main.cpp)
target_link_libraries(KairoPlayer PRIVATE KairoPlayerRuntime)
foreach(native_target IN LISTS KAIRO_PLAYER_NATIVE_GAMEPLAY_TARGETS)
    if(NOT TARGET ${native_target})
        message(FATAL_ERROR "KAIRO_PLAYER_NATIVE_GAMEPLAY_TARGETS names missing target '${native_target}'.")
    endif()
    target_link_libraries(KairoPlayer PRIVATE ${native_target})
endforeach()

if(BUILD_TESTING)
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

    add_executable(KairoPlayerNavigationAgentBridgeTests tests/RuntimeNavigationAgentBridgeTests.cpp)
    target_link_libraries(KairoPlayerNavigationAgentBridgeTests PRIVATE KairoPlayerRuntime)
    add_test(NAME KairoPlayerNavigationAgentBridgeTests COMMAND KairoPlayerNavigationAgentBridgeTests)

    add_executable(KairoPlayerCrowdNavigationBridgeTests tests/RuntimeCrowdNavigationBridgeTests.cpp)
    target_link_libraries(KairoPlayerCrowdNavigationBridgeTests PRIVATE KairoPlayerRuntime)
    add_test(NAME KairoPlayerCrowdNavigationBridgeTests COMMAND KairoPlayerCrowdNavigationBridgeTests)

    add_executable(KairoPlayerLocomotionAnimationBridgeTests tests/RuntimeLocomotionAnimationBridgeTests.cpp)
    target_link_libraries(KairoPlayerLocomotionAnimationBridgeTests PRIVATE KairoPlayerRuntime)
    add_test(NAME KairoPlayerLocomotionAnimationBridgeTests COMMAND KairoPlayerLocomotionAnimationBridgeTests)

    add_executable(KairoPlayerSaveGameBridgeTests tests/RuntimeSaveGameBridgeTests.cpp)
    target_link_libraries(KairoPlayerSaveGameBridgeTests PRIVATE KairoPlayerRuntime)
    add_test(NAME KairoPlayerSaveGameBridgeTests COMMAND KairoPlayerSaveGameBridgeTests)

    add_executable(KairoPlayerNativeGameplayTests tests/RuntimeNativeGameplayTests.cpp)
    target_link_libraries(KairoPlayerNativeGameplayTests PRIVATE KairoPlayerRuntime)
    add_test(NAME KairoPlayerNativeGameplayTests COMMAND KairoPlayerNativeGameplayTests)

    add_executable(KairoPlayerProductionSystemsTests tests/RuntimeProductionSystemsTests.cpp)
    target_link_libraries(KairoPlayerProductionSystemsTests PRIVATE KairoPlayerRuntime)
    add_test(NAME KairoPlayerProductionSystemsTests COMMAND KairoPlayerProductionSystemsTests)

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
    if(WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/Samples/SharedContentShowcase/Project.kproject")
        add_test(NAME KairoPlayerDirect3D12SharedContentSmoke COMMAND KairoPlayer
            ${CMAKE_SOURCE_DIR}/Samples/SharedContentShowcase/Project.kproject
            --renderer d3d12 --smoke)
        set_tests_properties(KairoPlayerDirect3D12SharedContentSmoke PROPERTIES
            TIMEOUT 30 SKIP_RETURN_CODE 77)
    endif()
endif()
