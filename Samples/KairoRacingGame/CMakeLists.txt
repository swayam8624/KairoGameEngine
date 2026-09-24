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

# The Editor's Play contract resolves project-owned runtimes from the
# .kproject. Publish the just-built native game into that project-local
# Development output so clicking Play launches this executable rather than
# falling back to the generic KairoPlayer.
set(_kairo_racing_development_dir
    "${CMAKE_CURRENT_SOURCE_DIR}/Project/Build/Development")
add_custom_command(TARGET KairoRacingGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${_kairo_racing_development_dir}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:KairoRacingGame>"
        "${_kairo_racing_development_dir}/$<TARGET_FILE_NAME:KairoRacingGame>"
    COMMENT "Publishing KairoRacingGame for Editor Play")

set(_kairo_racing_play_dir "${CMAKE_CURRENT_SOURCE_DIR}/Project/Build/Development")
set(_kairo_racing_play_executable "${_kairo_racing_play_dir}/KairoRacingGame")
add_custom_command(TARGET KairoRacingGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_kairo_racing_play_dir}"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:KairoRacingGame>"
        "${_kairo_racing_play_executable}"
    COMMENT "Publishing KairoRacingGame for Editor/Hub Play")

set(_kairo_racing_dev_runtime
    "${CMAKE_CURRENT_SOURCE_DIR}/Project/Build/Development/KairoRacingGame")
add_custom_command(TARGET KairoRacingGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Project/Build/Development"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:KairoRacingGame>"
        "${_kairo_racing_dev_runtime}"
    COMMENT "Publishing KAIRO Racing development Play executable")

add_custom_command(TARGET KairoRacingGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Project/Build/Development"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:KairoRacingGame>"
        "${CMAKE_CURRENT_SOURCE_DIR}/Project/Build/Development/$<TARGET_FILE_NAME:KairoRacingGame>"
    COMMENT "Publishing KairoRacingGame for Editor Play")

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
