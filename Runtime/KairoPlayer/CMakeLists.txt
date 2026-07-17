add_library(KairoPlayerRuntime)
target_sources(KairoPlayerRuntime PUBLIC FILE_SET CXX_MODULES
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    FILES RuntimeProject.cppm RuntimeRenderBridge.cppm RuntimePhysicsBridge.cppm
        RuntimeInputBridge.cppm RuntimeLogicBridge.cppm)
target_link_libraries(KairoPlayerRuntime PUBLIC Kairo::GameEngine)
target_compile_features(KairoPlayerRuntime PUBLIC cxx_std_23)

add_executable(KairoPlayer main.cpp)
target_link_libraries(KairoPlayer PRIVATE KairoPlayerRuntime)

if(BUILD_TESTING)
    add_executable(KairoPlayerRuntimeTests tests/RuntimeProjectTests.cpp)
    target_link_libraries(KairoPlayerRuntimeTests PRIVATE KairoPlayerRuntime)
    add_test(NAME KairoPlayerRuntimeTests COMMAND KairoPlayerRuntimeTests)
endif()
