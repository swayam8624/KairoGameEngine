cmake_minimum_required(VERSION 3.28)
project(KairoGameEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

option(KAIRO_GAME_ENGINE_BUILD_EDITOR "Build the native Vulkan editor" ON)
option(KAIRO_GAME_ENGINE_BUILD_PLAYER "Build the standalone Kairo runtime player" ON)
option(KAIRO_GAME_ENGINE_BUILD_PHASE1_SAMPLE "Build the Phase 1 gameplay vertical slice" ON)
option(KAIRO_GAME_ENGINE_BUILD_RAYTRACER "Build the offline CPU ray tracer" ON)
option(KAIRO_GAME_ENGINE_BUILD_PHYSICS_SANDBOX "Build physics terminal and GLFW sandboxes" ON)
option(KAIRO_GAME_ENGINE_BUILD_AI_CLOUD "Build the optional CPR OpenAI-compatible transport" OFF)
option(KAIRO_GAME_ENGINE_BUILD_COMPUTE_STACK "Build experimental GPU/SIMD/scheduler/ONNX/transformer modules" OFF)
option(KAIRO_GAME_ENGINE_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(KAIRO_GAME_ENGINE_ENABLE_PACKAGING "Enable CPack source-package configuration" OFF)

# KairoGameEngine is an integration repository inside a sibling-repository
# workspace.  The default layout is:
#
#   Kairo/
#     KairoGameEngine/
#     KairoMath/
#     KairoGeometry/
#     KairoSpatial/
#     ...
#
# Component source is never vendored or cloned again inside KairoGameEngine.
set(KAIRO_WORKSPACE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." CACHE PATH
    "Directory containing the sibling Kairo component repositories")

function(kairo_require_workspace_repo out_variable repo_name)
    set(_candidate "${KAIRO_WORKSPACE_ROOT}/${repo_name}")
    if(NOT EXISTS "${_candidate}/CMakeLists.txt")
        message(FATAL_ERROR
            "Missing sibling Kairo repository '${repo_name}' at '${_candidate}'. "
            "Clone https://github.com/swayam8624/${repo_name}.git beside KairoGameEngine "
            "or set KAIRO_WORKSPACE_ROOT to the directory containing the Kairo repos.")
    endif()
    set(${out_variable} "${_candidate}" PARENT_SCOPE)
endfunction()

function(kairo_add_workspace_repo target_name repo_name source_variable)
    if(TARGET ${target_name})
        return()
    endif()
    set(_source "${${source_variable}}")
    add_subdirectory("${_source}" "${CMAKE_BINARY_DIR}/components/${repo_name}")
endfunction()

message(STATUS "Kairo sibling workspace root: ${KAIRO_WORKSPACE_ROOT}")

if(KAIRO_GAME_ENGINE_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    else()
        message(FATAL_ERROR
            "KAIRO_GAME_ENGINE_ENABLE_SANITIZERS currently requires Clang or GCC.")
    endif()
endif()

# Resolve the source trees once.  Required repositories are siblings of this
# integration repo; optional compute repositories are resolved only when enabled.
kairo_require_workspace_repo(KAIRO_MATH_SOURCE KairoMath)
kairo_require_workspace_repo(KAIRO_GEOMETRY_SOURCE KairoGeometry)
kairo_require_workspace_repo(KAIRO_SPATIAL_SOURCE KairoSpatial)
kairo_require_workspace_repo(KAIRO_PHYSICS_MATH_SOURCE KairoPhysicsMath)
kairo_require_workspace_repo(KAIRO_PHYSICS_ENGINE_SOURCE KairoPhysicsEngine)
kairo_require_workspace_repo(KAIRO_ASSETS_SOURCE KairoAssets)
kairo_require_workspace_repo(KAIRO_AI_SOURCE KairoAI)
kairo_require_workspace_repo(KAIRO_ECS_SOURCE KairoECS)
kairo_require_workspace_repo(KAIRO_REFLECTION_SOURCE KairoReflection)
kairo_require_workspace_repo(KAIRO_ENGINE_CORE_SOURCE KairoEngineCore)
kairo_require_workspace_repo(KAIRO_RENDERER_SOURCE KairoRenderer)

if(KAIRO_GAME_ENGINE_BUILD_EDITOR)
    kairo_require_workspace_repo(KAIRO_EDITOR_SOURCE KairoEditor)
endif()
if(KAIRO_GAME_ENGINE_BUILD_RAYTRACER)
    kairo_require_workspace_repo(KAIRO_RAYTRACER_SOURCE KairoRayTracer)
endif()

set(KAIRO_MATH_BUILD_TESTS ${BUILD_TESTING} CACHE BOOL "" FORCE)
set(KAIRO_MATH_BUILD_VISUALIZER OFF CACHE BOOL "" FORCE)
set(KAIRO_GEOMETRY_BUILD_TESTS ${BUILD_TESTING} CACHE BOOL "" FORCE)
set(KAIRO_GEOMETRY_BUILD_VISUALIZER OFF CACHE BOOL "" FORCE)
set(KAIRO_SPATIAL_BUILD_TESTS ${BUILD_TESTING} CACHE BOOL "" FORCE)
set(KAIRO_SPATIAL_BUILD_VISUALIZER OFF CACHE BOOL "" FORCE)
set(KAIRO_PHYSICS_MATH_BUILD_TESTS ${BUILD_TESTING} CACHE BOOL "" FORCE)
set(KAIRO_PHYSICS_ENGINE_BUILD_TESTS ${BUILD_TESTING} CACHE BOOL "" FORCE)
set(KAIRO_PHYSICS_ENGINE_BUILD_SANDBOX ${KAIRO_GAME_ENGINE_BUILD_PHYSICS_SANDBOX} CACHE BOOL "" FORCE)
set(KAIRO_PHYSICS_ENGINE_BUILD_GLFW_SANDBOX ${KAIRO_GAME_ENGINE_BUILD_PHYSICS_SANDBOX} CACHE BOOL "" FORCE)
set(KAIRO_ASSETS_BUILD_TESTS ${BUILD_TESTING} CACHE BOOL "" FORCE)
set(KAIRO_AI_BUILD_TESTS ${BUILD_TESTING} CACHE BOOL "" FORCE)
set(KAIRO_AI_BUILD_CLOUD_PROVIDER ${KAIRO_GAME_ENGINE_BUILD_AI_CLOUD} CACHE BOOL "" FORCE)
set(KAIRO_ECS_BUILD_TESTS ${BUILD_TESTING} CACHE BOOL "" FORCE)
set(KAIRO_REFLECTION_BUILD_TESTS ${BUILD_TESTING} CACHE BOOL "" FORCE)
set(KAIRO_ENGINE_CORE_BUILD_TESTS ${BUILD_TESTING} CACHE BOOL "" FORCE)
set(KAIRO_RENDERER_BUILD_TESTS ${BUILD_TESTING} CACHE BOOL "" FORCE)
set(KAIRO_RENDERER_BUILD_PHYSICS_BRIDGE ON CACHE BOOL "" FORCE)
set(KAIRO_EDITOR_BUILD_TESTS ${BUILD_TESTING} CACHE BOOL "" FORCE)
set(KAIRO_EDITOR_BUILD_APP ${KAIRO_GAME_ENGINE_BUILD_EDITOR} CACHE BOOL "" FORCE)
set(KAIRO_RAYTRACER_BUILD_TESTS ${BUILD_TESTING} CACHE BOOL "" FORCE)
set(KAIRO_RAYTRACER_BUILD_PREVIEW ${KAIRO_GAME_ENGINE_BUILD_EDITOR} CACHE BOOL "" FORCE)
set(KAIRO_EDITOR_SHARED_CONTENT_PROJECT
    "${CMAKE_CURRENT_SOURCE_DIR}/Samples/SharedContentShowcase/Project.kproject"
    CACHE FILEPATH "" FORCE)

# Build foundational dependencies first so component CMake files reuse these
# targets instead of recursively FetchContent-ing duplicate copies.
kairo_add_workspace_repo(KairoMath KairoMath KAIRO_MATH_SOURCE)
kairo_add_workspace_repo(KairoGeometry KairoGeometry KAIRO_GEOMETRY_SOURCE)
kairo_add_workspace_repo(KairoSpatial KairoSpatial KAIRO_SPATIAL_SOURCE)
kairo_add_workspace_repo(KairoPhysicsMath KairoPhysicsMath KAIRO_PHYSICS_MATH_SOURCE)
kairo_add_workspace_repo(KairoPhysicsEngine KairoPhysicsEngine KAIRO_PHYSICS_ENGINE_SOURCE)

kairo_add_workspace_repo(KairoAssets KairoAssets KAIRO_ASSETS_SOURCE)
kairo_add_workspace_repo(KairoAI KairoAI KAIRO_AI_SOURCE)
kairo_add_workspace_repo(KairoECS KairoECS KAIRO_ECS_SOURCE)
kairo_add_workspace_repo(KairoReflection KairoReflection KAIRO_REFLECTION_SOURCE)
kairo_add_workspace_repo(KairoEngineCore KairoEngineCore KAIRO_ENGINE_CORE_SOURCE)
kairo_add_workspace_repo(KairoRenderer KairoRenderer KAIRO_RENDERER_SOURCE)

add_library(KairoSceneECSBridge)
add_library(Kairo::SceneECSBridge ALIAS KairoSceneECSBridge)
target_sources(KairoSceneECSBridge PUBLIC FILE_SET CXX_MODULES
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/Runtime
    FILES Runtime/KairoSceneECSBridge.cppm)
target_link_libraries(KairoSceneECSBridge PUBLIC KairoECS KairoEngineCore)
target_compile_features(KairoSceneECSBridge PUBLIC cxx_std_23)

add_subdirectory(Runtime/KairoRealtimeRenderBridge)

if(KAIRO_GAME_ENGINE_BUILD_EDITOR)
    kairo_add_workspace_repo(KairoEditor KairoEditor KAIRO_EDITOR_SOURCE)
endif()

if(KAIRO_GAME_ENGINE_BUILD_RAYTRACER)
    kairo_add_workspace_repo(KairoRayTracer KairoRayTracer KAIRO_RAYTRACER_SOURCE)
    add_subdirectory(Runtime/KairoRenderBridge)

    # Homebrew LLVM 23.x on arm64 macOS has a 32-bit SourceLocation ceiling.
    # Importing the editor, renderer and full offline-raytracer module graph into
    # the single KairoEditorApp translation unit crosses that ceiling even though
    # the offline bridge itself compiles and tests successfully. Keep the engine
    # bridge built on every platform, but avoid injecting it into this one host TU
    # for the affected compiler generation. Other Clang versions, AppleClang,
    # MSVC and Linux retain the integrated offline-render editor service.
    set(_kairo_editor_offline_host_supported ON)
    if(APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND
       CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 23 AND
       CMAKE_CXX_COMPILER_VERSION VERSION_LESS 24)
        set(_kairo_editor_offline_host_supported OFF)
        message(STATUS
            "KairoEditor offline-render host injection disabled for Homebrew LLVM 23.x on macOS due to SourceLocation exhaustion")
    endif()

    if(TARGET KairoEditorApp AND TARGET KairoEditorOfflineRenderBridge AND
       _kairo_editor_offline_host_supported)
        target_link_libraries(KairoEditorApp PRIVATE KairoEditorOfflineRenderBridge)
        target_compile_definitions(KairoEditorApp PRIVATE KAIRO_EDITOR_HAS_OFFLINE_RENDER=1)
    endif()
endif()

if(KAIRO_GAME_ENGINE_BUILD_COMPUTE_STACK)
    kairo_require_workspace_repo(KAIRO_GPU_SOURCE KairoGPU)
    kairo_require_workspace_repo(KAIRO_SIMD_SOURCE KairoSIMD)
    kairo_require_workspace_repo(KAIRO_SCHEDULER_SOURCE KairoScheduler)
    kairo_require_workspace_repo(KAIRO_ONNX_SOURCE KairoONNX)
    kairo_require_workspace_repo(KAIRO_TRANSFORMERS_SOURCE KairoTransformers)

    kairo_add_workspace_repo(KairoGPU KairoGPU KAIRO_GPU_SOURCE)
    kairo_add_workspace_repo(KairoSIMD KairoSIMD KAIRO_SIMD_SOURCE)
    kairo_add_workspace_repo(KairoScheduler KairoScheduler KAIRO_SCHEDULER_SOURCE)
    kairo_add_workspace_repo(KairoONNX KairoONNX KAIRO_ONNX_SOURCE)
    kairo_add_workspace_repo(KairoTransformers KairoTransformers KAIRO_TRANSFORMERS_SOURCE)

    if(BUILD_TESTING)
        add_executable(KairoComputeStackIntegration
            tests/ComputeStackIntegration.cpp)
        target_link_libraries(KairoComputeStackIntegration PRIVATE
            KairoSIMD KairoScheduler KairoGPU KairoONNX KairoTransformers)
        set_property(TARGET KairoComputeStackIntegration
            PROPERTY COMPILE_WARNING_AS_ERROR ON)
        add_test(NAME KairoComputeStackIntegration
            COMMAND KairoComputeStackIntegration)
    endif()
endif()

add_library(KairoGameEngine INTERFACE)
target_link_libraries(KairoGameEngine INTERFACE
    KairoAssets
    KairoECS
    KairoReflection
    KairoEngineCore
    KairoSceneECSBridge
    KairoRenderer
    KairoPhysicsEngine)
add_library(Kairo::GameEngine ALIAS KairoGameEngine)

if(BUILD_TESTING)
    add_test(NAME KairoCompatibilityContentChecksums
        COMMAND ${CMAKE_COMMAND} -P
            ${CMAKE_CURRENT_SOURCE_DIR}/TestContent/Compatibility/verify_checksums.cmake)
    if(TARGET Catch2::Catch2WithMain)
        add_executable(KairoCompatibilityContentTests tests/CompatibilityContentTests.cpp)
        target_link_libraries(KairoCompatibilityContentTests PRIVATE
            KairoAssets Catch2::Catch2WithMain)
        target_compile_definitions(KairoCompatibilityContentTests PRIVATE
            KAIRO_COMPATIBILITY_CONTENT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/TestContent/Compatibility")
        add_test(NAME KairoCompatibilityContentTests COMMAND KairoCompatibilityContentTests)
    endif()

    add_executable(KairoSceneECSBridgeTests tests/SceneECSBridgeTests.cpp)
    target_link_libraries(KairoSceneECSBridgeTests PRIVATE KairoSceneECSBridge)
    add_test(NAME KairoSceneECSBridgeTests COMMAND KairoSceneECSBridgeTests)
endif()

if(KAIRO_GAME_ENGINE_BUILD_PLAYER)
    add_subdirectory(Runtime/KairoPlayer)
    set(_kairo_shared_content_project
        "${CMAKE_CURRENT_SOURCE_DIR}/Samples/SharedContentShowcase/Project.kproject")
    if(NOT EXISTS "${_kairo_shared_content_project}")
        message(FATAL_ERROR
            "Phase 5 shared-content fixture is missing: ${_kairo_shared_content_project}")
    endif()
    if(BUILD_TESTING)
        add_test(NAME KairoSharedContentChecksums
            COMMAND ${CMAKE_COMMAND} -P
                ${CMAKE_CURRENT_SOURCE_DIR}/Samples/SharedContentShowcase/verify_checksums.cmake)
        add_test(NAME KairoSharedContentProjectValidation
            COMMAND KairoPlayer "${_kairo_shared_content_project}" --validate)

        # The shared-content smoke verifies project/runtime integration, not
        # graphics-backend discovery. Use one backend that is deterministic on
        # each CI host so headless Linux does not accidentally select Vulkan
        # when GLFW cannot create a Vulkan surface under Xvfb.
        if(WIN32)
            set(_kairo_shared_content_smoke_backend d3d12)
        else()
            set(_kairo_shared_content_smoke_backend opengl)
        endif()
        add_test(NAME KairoSharedContentPlayerSmoke
            COMMAND KairoPlayer "${_kairo_shared_content_project}"
                --renderer ${_kairo_shared_content_smoke_backend} --smoke)
        set_tests_properties(KairoSharedContentProjectValidation
            KairoSharedContentPlayerSmoke PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77)
        if(TARGET KairoEditorRendererBridge AND TARGET Catch2::Catch2WithMain)
            add_executable(KairoSharedContentContractTests
                tests/SharedContentContractTests.cpp)
            target_link_libraries(KairoSharedContentContractTests PRIVATE
                KairoPlayerRuntime KairoEditorRendererBridge
                Catch2::Catch2WithMain)
            target_compile_definitions(KairoSharedContentContractTests PRIVATE
                KAIRO_SHARED_CONTENT_PROJECT="${_kairo_shared_content_project}")
            add_test(NAME KairoSharedContentContractTests
                COMMAND KairoSharedContentContractTests)
        endif()
    endif()
endif()

if(KAIRO_GAME_ENGINE_BUILD_PHASE1_SAMPLE)
    if(NOT KAIRO_GAME_ENGINE_BUILD_PLAYER)
        message(FATAL_ERROR
            "KAIRO_GAME_ENGINE_BUILD_PHASE1_SAMPLE requires KAIRO_GAME_ENGINE_BUILD_PLAYER.")
    endif()
    add_subdirectory(Samples/Phase1Game)
endif()

if(KAIRO_GAME_ENGINE_ENABLE_PACKAGING)
    # This package contains the integration repository only.  Component source
    # lives in sibling repositories and is intentionally not duplicated here.
    set(CPACK_PACKAGE_NAME "KairoGameEngine")
    set(CPACK_PACKAGE_VENDOR "Kairo")
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    set(CPACK_SOURCE_GENERATOR "TGZ;ZIP")
    set(CPACK_SOURCE_IGNORE_FILES
        "/\\.git/"
        "/\\.idea/"
        "/\\.DS_Store$"
        "/build/"
        "/build-[^/]+/"
        "/out/"
        "/cmake-build-[^/]+/"
        "/_CPack_Packages/"
        "/KairoGameEngine-.*-Source\\.(tar\\.gz|zip)$")
    include(CPack)
endif()
