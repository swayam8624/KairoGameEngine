add_library(KairoRenderBridge)
target_sources(KairoRenderBridge PUBLIC FILE_SET CXX_MODULES
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    FILES
        SceneSnapshotBridge.cppm
        OfflineRenderSession.cppm)
target_link_libraries(KairoRenderBridge PUBLIC KairoEngineCore KairoRayTracer)
add_library(Kairo::RenderBridge ALIAS KairoRenderBridge)

# Homebrew LLVM 23.1.0 on arm64 macOS crashes in frontend CodeGen while
# emitting std::__libcpp_allocate for the material-preview implementation. The
# same source passes Linux Clang, MSVC, and sanitizer builds, and the crash is
# independent of optimization level and of whether the implementation lives in
# a module interface or a normal .cpp translation unit. Keep the exception
# limited to this optional editor helper and this compiler generation; all
# realtime animation/render bridges and the offline-render service remain built
# and tested on macOS.
set(KAIRO_MATERIAL_PREVIEW_BRIDGE_SUPPORTED ON)
if(APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND
   CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 23 AND
   CMAKE_CXX_COMPILER_VERSION VERSION_LESS 24)
    set(KAIRO_MATERIAL_PREVIEW_BRIDGE_SUPPORTED OFF)
    message(STATUS
        "Kairo material preview bridge disabled for Homebrew LLVM 23.x on macOS due to compiler ICE")
endif()

if(TARGET KairoEditor)
    add_library(KairoEditorOfflineRenderBridge)
    target_sources(KairoEditorOfflineRenderBridge PUBLIC FILE_SET CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES EditorOfflineRenderService.cppm)

    if(KAIRO_MATERIAL_PREVIEW_BRIDGE_SUPPORTED)
        target_sources(KairoEditorOfflineRenderBridge PUBLIC FILE_SET CXX_MODULES
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
            FILES MaterialPreviewBridge.cppm)
        target_sources(KairoEditorOfflineRenderBridge PRIVATE MaterialPreviewBridge.cpp)
    endif()

    target_link_libraries(KairoEditorOfflineRenderBridge PUBLIC KairoRenderBridge KairoEditor KairoRenderer)
    add_library(Kairo::EditorOfflineRenderBridge ALIAS KairoEditorOfflineRenderBridge)
endif()

if(BUILD_TESTING AND TARGET Catch2::Catch2WithMain)
    add_executable(KairoRenderBridgeTests tests/SceneSnapshotBridgeTests.cpp)
    target_link_libraries(KairoRenderBridgeTests PRIVATE KairoRenderBridge Catch2::Catch2WithMain)
    add_test(NAME KairoRenderBridgeTests COMMAND KairoRenderBridgeTests)

    if(TARGET KairoEditorOfflineRenderBridge)
        add_executable(KairoEditorOfflineRenderBridgeTests
            tests/EditorOfflineRenderServiceTests.cpp)
        if(KAIRO_MATERIAL_PREVIEW_BRIDGE_SUPPORTED)
            target_sources(KairoEditorOfflineRenderBridgeTests PRIVATE
                tests/MaterialPreviewBridgeTests.cpp)
        endif()
        target_link_libraries(KairoEditorOfflineRenderBridgeTests PRIVATE
            KairoEditorOfflineRenderBridge Catch2::Catch2WithMain)
        add_test(NAME KairoEditorOfflineRenderBridgeTests COMMAND KairoEditorOfflineRenderBridgeTests)
    endif()
endif()
