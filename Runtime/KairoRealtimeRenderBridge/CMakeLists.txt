add_library(KairoRealtimeRenderBridge)
target_sources(KairoRealtimeRenderBridge PUBLIC FILE_SET CXX_MODULES
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    FILES RealtimeSceneBridge.cppm)
target_link_libraries(KairoRealtimeRenderBridge PUBLIC
    KairoEngineCore KairoAssets KairoRenderer)
add_library(Kairo::RealtimeRenderBridge ALIAS KairoRealtimeRenderBridge)

if(BUILD_TESTING)
    add_executable(KairoAuthoredClipTests AuthoredClipTests.cpp)
    target_compile_features(KairoAuthoredClipTests PRIVATE cxx_std_23)
    set_property(TARGET KairoAuthoredClipTests PROPERTY COMPILE_WARNING_AS_ERROR ON)
    add_test(NAME KairoAuthoredClipTests COMMAND KairoAuthoredClipTests)
    add_executable(KairoFrameAssetCacheTests tests/FrameAssetCacheTests.cpp)
    target_compile_features(KairoFrameAssetCacheTests PRIVATE cxx_std_23)
    add_test(NAME KairoFrameAssetCacheTests COMMAND KairoFrameAssetCacheTests)
    add_executable(KairoFrameHierarchyCacheTests tests/FrameHierarchyCacheTests.cpp)
    target_compile_features(KairoFrameHierarchyCacheTests PRIVATE cxx_std_23)
    add_test(NAME KairoFrameHierarchyCacheTests COMMAND KairoFrameHierarchyCacheTests)
    # Compile the pure C++ file-size gate independently from Catch2/GPU.
    add_executable(KairoMaterialFileSizeBudgetTests
        tests/MaterialFileSizeBudgetTests.cpp)
    target_compile_features(KairoMaterialFileSizeBudgetTests PRIVATE cxx_std_23)
    add_test(NAME KairoMaterialFileSizeBudgetTests
        COMMAND KairoMaterialFileSizeBudgetTests)
endif()

if(BUILD_TESTING AND TARGET Catch2::Catch2WithMain)
    add_executable(KairoRealtimeRenderBridgeTests tests/RealtimeSceneBridgeTests.cpp)
    target_link_libraries(KairoRealtimeRenderBridgeTests PRIVATE
        KairoRealtimeRenderBridge Catch2::Catch2WithMain)
    add_test(NAME KairoRealtimeRenderBridgeTests COMMAND KairoRealtimeRenderBridgeTests)
endif()
