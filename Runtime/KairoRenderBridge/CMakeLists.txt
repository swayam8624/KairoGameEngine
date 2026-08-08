add_library(KairoRenderBridge)
target_sources(KairoRenderBridge PUBLIC FILE_SET CXX_MODULES
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    FILES
        SceneSnapshotBridge.cppm
        OfflineRenderSession.cppm)
target_link_libraries(KairoRenderBridge PUBLIC KairoEngineCore KairoRayTracer)
add_library(Kairo::RenderBridge ALIAS KairoRenderBridge)

if(BUILD_TESTING AND TARGET Catch2::Catch2WithMain)
    add_executable(KairoRenderBridgeTests tests/SceneSnapshotBridgeTests.cpp)
    target_link_libraries(KairoRenderBridgeTests PRIVATE KairoRenderBridge Catch2::Catch2WithMain)
    add_test(NAME KairoRenderBridgeTests COMMAND KairoRenderBridgeTests)
endif()
