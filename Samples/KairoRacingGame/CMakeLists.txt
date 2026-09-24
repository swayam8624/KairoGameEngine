add_executable(KairoRacingGame main.cpp)
target_link_libraries(KairoRacingGame PRIVATE
    KairoPlayerRuntime
    KairoEngineCore
    KairoMath
    KairoPhysicsEngine
    KairoRenderer)
target_compile_features(KairoRacingGame PRIVATE cxx_std_23)
target_compile_definitions(KairoRacingGame PRIVATE
    KAIRO_RACING_PROJECT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/Project/Racing.kproject")

set(_kairo_racing_track "${CMAKE_CURRENT_SOURCE_DIR}/Project/Content/Racing/track.glb")
set(_kairo_racing_chassis "${CMAKE_CURRENT_SOURCE_DIR}/Project/Content/Racing/chassis.glb")
set(_kairo_racing_wheel "${CMAKE_CURRENT_SOURCE_DIR}/Project/Content/Racing/wheel.glb")
set(_kairo_racing_assets_ready TRUE)
foreach(_asset IN ITEMS
    "${_kairo_racing_track}"
    "${_kairo_racing_chassis}"
    "${_kairo_racing_wheel}")
    if(NOT EXISTS "${_asset}")
        set(_kairo_racing_assets_ready FALSE)
    endif()
endforeach()

if(BUILD_TESTING AND _kairo_racing_assets_ready)
    add_test(NAME KairoRacing.ProjectValidation
        COMMAND $<TARGET_FILE:KairoPlayer>
            "${CMAKE_CURRENT_SOURCE_DIR}/Project/Racing.kproject" --validate)
    add_test(NAME KairoRacing.NativeSmoke
        COMMAND $<TARGET_FILE:KairoRacingGame>
            "${CMAKE_CURRENT_SOURCE_DIR}/Project/Racing.kproject"
            --smoke)
    set_tests_properties(KairoRacing.ProjectValidation KairoRacing.NativeSmoke
        PROPERTIES TIMEOUT 60 SKIP_RETURN_CODE 77)
elseif(BUILD_TESTING)
    message(STATUS
        "KairoRacing runtime tests disabled until third-party GLBs are prepared. "
        "Run: bash Samples/KairoRacingGame/tools/prepare_assets.sh")
endif()
