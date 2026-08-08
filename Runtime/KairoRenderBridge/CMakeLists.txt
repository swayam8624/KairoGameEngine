add_library(KairoRenderBridge)
target_sources(KairoRenderBridge PUBLIC FILE_SET CXX_MODULES
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    FILES
        SceneSnapshotBridge.cppm
        OfflineRenderSession.cppm)
target_link_libraries(KairoRenderBridge PUBLIC KairoEngineCore KairoRayTracer)
add_library(Kairo::RenderBridge ALIAS KairoRenderBridge)

if(TARGET KairoEditor)
    add_library(KairoEditorOfflineRenderBridge)
    target_sources(KairoEditorOfflineRenderBridge PUBLIC FILE_SET CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES EditorOfflineRenderService.cppm)
    target_link_libraries(KairoEditorOfflineRenderBridge PUBLIC KairoRenderBridge KairoEditor)
    add_library(Kairo::EditorOfflineRenderBridge ALIAS KairoEditorOfflineRenderBridge)
endif()

if(BUILD_TESTING AND TARGET Catch2::Catch2WithMain)
    add_executable(KairoRenderBridgeTests tests/SceneSnapshotBridgeTests.cpp)
    target_link_libraries(KairoRenderBridgeTests PRIVATE KairoRenderBridge Catch2::Catch2WithMain)
    add_test(NAME KairoRenderBridgeTests COMMAND KairoRenderBridgeTests)

    if(TARGET KairoEditorOfflineRenderBridge)
        add_executable(KairoEditorOfflineRenderBridgeTests tests/EditorOfflineRenderServiceTests.cpp)
        target_link_libraries(KairoEditorOfflineRenderBridgeTests PRIVATE
            KairoEditorOfflineRenderBridge Catch2::Catch2WithMain)
        add_test(NAME KairoEditorOfflineRenderBridgeTests COMMAND KairoEditorOfflineRenderBridgeTests)
    endif()
endif()
