add_library(KairoRealtimeRenderBridge)
target_sources(KairoRealtimeRenderBridge PUBLIC FILE_SET CXX_MODULES
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    FILES RealtimeSceneBridge.cppm)
target_link_libraries(KairoRealtimeRenderBridge PUBLIC
    KairoEngineCore KairoAssets KairoRenderer)
add_library(Kairo::RealtimeRenderBridge ALIAS KairoRealtimeRenderBridge)

if(BUILD_TESTING AND TARGET Catch2::Catch2WithMain)
    add_executable(KairoRealtimeRenderBridgeTests tests/RealtimeSceneBridgeTests.cpp)
    target_link_libraries(KairoRealtimeRenderBridgeTests PRIVATE
        KairoRealtimeRenderBridge Catch2::Catch2WithMain)
    add_test(NAME KairoRealtimeRenderBridgeTests COMMAND KairoRealtimeRenderBridgeTests)
endif()
