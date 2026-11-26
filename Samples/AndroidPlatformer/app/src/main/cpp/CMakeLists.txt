cmake_minimum_required(VERSION 3.22)
project(KairoAndroidPlatformer LANGUAGES CXX)
# The project directory is nested under Samples/AndroidPlatformer/app/src/main/cpp.
# Compile the exact shared C ABI used by the WASM deployment, not a fork.
get_filename_component(KAIRO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../.." ABSOLUTE)
add_library(kairo_mobile SHARED PlatformerJni.cpp "${KAIRO_ROOT}/Samples/WebPlatformer/PlatformerWasm.cpp")
target_include_directories(kairo_mobile PRIVATE "${KAIRO_ROOT}/Runtime/KairoPlayer")
target_compile_features(kairo_mobile PRIVATE cxx_std_23)
target_compile_options(kairo_mobile PRIVATE -Wall -Wextra -Werror)
