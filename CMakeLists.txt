cmake_minimum_required(VERSION 3.28)
project(KairoGameEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

option(KAIRO_GAME_ENGINE_BUILD_EDITOR "Build the native Vulkan editor" ON)
option(KAIRO_GAME_ENGINE_BUILD_RAYTRACER "Build the offline CPU ray tracer" ON)
option(KAIRO_GAME_ENGINE_BUILD_PHYSICS_SANDBOX "Build physics terminal and GLFW sandboxes" ON)
option(KAIRO_GAME_ENGINE_BUILD_COMPUTE_STACK "Build experimental GPU/SIMD/scheduler/ONNX/transformer modules" OFF)
option(KAIRO_GAME_ENGINE_ENABLE_PACKAGING "Enable CPack source-package configuration" ON)

set(KAIRO_WORKSPACE_COMPONENTS
    Foundation/KairoMath
    Foundation/KairoGeometry
    Foundation/Spatial
    Foundation/KairoPhysicsMath
    Foundation/KairoPhysicsEngine
    KairoAssets
    KairoECS
    KairoEngineCore
    KairoRenderer
    KairoEditor
    KairoRayTracer)

foreach(component IN LISTS KAIRO_WORKSPACE_COMPONENTS)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${component}/CMakeLists.txt")
        message(FATAL_ERROR
            "Kairo component '${component}' is not initialized. "
            "Run: git submodule update --init --recursive")
    endif()
endforeach()

# Component projects remain independently buildable. These cache values only
# coordinate their optional surfaces when they participate in the superbuild.
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
set(KAIRO_ECS_BUILD_TESTS ${BUILD_TESTING} CACHE BOOL "" FORCE)
set(KAIRO_ENGINE_CORE_BUILD_TESTS ${BUILD_TESTING} CACHE BOOL "" FORCE)
set(KAIRO_RENDERER_BUILD_TESTS ${BUILD_TESTING} CACHE BOOL "" FORCE)
set(KAIRO_RENDERER_BUILD_PHYSICS_BRIDGE ON CACHE BOOL "" FORCE)
set(KAIRO_EDITOR_BUILD_TESTS ${BUILD_TESTING} CACHE BOOL "" FORCE)
set(KAIRO_EDITOR_BUILD_APP ${KAIRO_GAME_ENGINE_BUILD_EDITOR} CACHE BOOL "" FORCE)
set(KAIRO_RAYTRACER_BUILD_TESTS ${BUILD_TESTING} CACHE BOOL "" FORCE)
set(KAIRO_RAYTRACER_BUILD_PREVIEW ${KAIRO_GAME_ENGINE_BUILD_EDITOR} CACHE BOOL "" FORCE)

# Dependency order matters for C++ module targets. Once a target exists, every
# component's local-first dependency resolver reuses it instead of fetching or
# compiling a second copy.
add_subdirectory(Foundation/KairoPhysicsEngine)
add_subdirectory(KairoECS)
add_subdirectory(KairoEngineCore)
add_subdirectory(KairoRenderer)

if(KAIRO_GAME_ENGINE_BUILD_EDITOR)
    add_subdirectory(KairoEditor)
endif()

if(KAIRO_GAME_ENGINE_BUILD_RAYTRACER)
    add_subdirectory(KairoRayTracer)
endif()

if(KAIRO_GAME_ENGINE_BUILD_COMPUTE_STACK)
    foreach(component IN ITEMS KairoGPU KairoSIMD KairoScheduler KairoONNX KairoTransformers)
        if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${component}/CMakeLists.txt")
            message(FATAL_ERROR
                "Optional component '${component}' is not initialized. "
                "Run: git submodule update --init --recursive")
        endif()
        add_subdirectory(${component})
    endforeach()
endif()

# Aggregate runtime consumption target. Tools such as KairoEditor and
# KairoRayTracer remain separate executables/libraries rather than leaking
# authoring-only dependencies into shipped games.
add_library(KairoGameEngine INTERFACE)
target_link_libraries(KairoGameEngine INTERFACE
    KairoECS
    KairoEngineCore
    KairoRenderer
    KairoPhysicsEngine)
add_library(Kairo::GameEngine ALIAS KairoGameEngine)

# The component repositories do not yet export installable package targets, so
# a binary SDK archive would be misleading. Source packages are still useful:
# they preserve the exact superproject and submodule sources used for an
# integration build, without pretending that the resulting archive is a
# complete redistributable runtime.
if(KAIRO_GAME_ENGINE_ENABLE_PACKAGING)
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
