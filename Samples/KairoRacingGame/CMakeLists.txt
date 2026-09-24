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

if(BUILD_TESTING)
    add_test(NAME KairoRacing.ProjectValidation
        COMMAND $<TARGET_FILE:KairoPlayer>
            "${CMAKE_CURRENT_SOURCE_DIR}/Project/Racing.kproject" --validate)
    add_test(NAME KairoRacing.NativeSmoke
        COMMAND $<TARGET_FILE:KairoRacingGame>
            "${CMAKE_CURRENT_SOURCE_DIR}/Project/Racing.kproject"
            --smoke)
    set_tests_properties(KairoRacing.ProjectValidation KairoRacing.NativeSmoke
        PROPERTIES TIMEOUT 60 SKIP_RETURN_CODE 77)
endif()
