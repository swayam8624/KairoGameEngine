set(KAIRO_PLAYER_NATIVE_GAMEPLAY_TARGETS "" CACHE STRING
    "Semicolon-separated CMake targets that register project native gameplay types with KairoPlayer")

add_library(KairoPlayerRuntime)
target_sources(KairoPlayerRuntime PUBLIC FILE_SET CXX_MODULES
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    FILES RuntimeProject.cppm RuntimeRenderBridge.cppm RuntimePhysicsBridge.cppm
        RuntimeInputBridge.cppm RuntimeLogicBridge.cppm RuntimeNativeGameplayBridge.cppm RuntimeProductionSystemsBridge.cppm RuntimeShippingBridge.cppm RuntimePackaging.cppm)
target_link_libraries(KairoPlayerRuntime PUBLIC Kairo::GameEngine KairoRealtimeRenderBridge)
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
