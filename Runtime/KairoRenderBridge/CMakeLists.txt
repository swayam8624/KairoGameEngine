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
        FILES EditorOfflineRenderService.cppm MaterialPreviewBridge.cppm)
    target_link_libraries(KairoEditorOfflineRenderBridge PUBLIC KairoRenderBridge KairoEditor KairoRenderer)

    # Homebrew LLVM 23.1.0 on arm64 macOS currently crashes in CodeGen while
    # emitting libc++ allocator/new machinery for this C++ module at -O2. The
    # same source is clean on Windows MSVC, Linux Clang, and sanitizer builds.
    # Keep the workaround source-local so the rest of the Release build remains
    # fully optimized; a later LLVM can remove this once the frontend ICE is gone.
    if(APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND
       CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 23 AND
       CMAKE_CXX_COMPILER_VERSION VERSION_LESS 24)
        set_source_files_properties(MaterialPreviewBridge.cppm PROPERTIES
            COMPILE_OPTIONS "-O1")
    endif()

    add_library(Kairo::EditorOfflineRenderBridge ALIAS KairoEditorOfflineRenderBridge)
endif()

if(BUILD_TESTING AND TARGET Catch2::Catch2WithMain)
    add_executable(KairoRenderBridgeTests tests/SceneSnapshotBridgeTests.cpp)
    target_link_libraries(KairoRenderBridgeTests PRIVATE KairoRenderBridge Catch2::Catch2WithMain)
    add_test(NAME KairoRenderBridgeTests COMMAND KairoRenderBridgeTests)

    if(TARGET KairoEditorOfflineRenderBridge)
        add_executable(KairoEditorOfflineRenderBridgeTests
            tests/EditorOfflineRenderServiceTests.cpp
            tests/MaterialPreviewBridgeTests.cpp)
        target_link_libraries(KairoEditorOfflineRenderBridgeTests PRIVATE
            KairoEditorOfflineRenderBridge Catch2::Catch2WithMain)
        add_test(NAME KairoEditorOfflineRenderBridgeTests COMMAND KairoEditorOfflineRenderBridgeTests)
    endif()
endif()
