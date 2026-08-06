add_library(KairoPhase1GameState)
target_sources(KairoPhase1GameState PUBLIC FILE_SET CXX_MODULES
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    FILES Phase1GameState.cppm)
target_link_libraries(KairoPhase1GameState PUBLIC KairoEngineCore KairoMath)
target_compile_features(KairoPhase1GameState PUBLIC cxx_std_23)

add_executable(KairoPhase1Game main.cpp)
target_link_libraries(KairoPhase1Game PRIVATE KairoPhase1GameState KairoPlayerRuntime)
target_compile_features(KairoPhase1Game PRIVATE cxx_std_23)
target_compile_definitions(KairoPhase1Game PRIVATE
    KAIRO_PHASE1_PROJECT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/Project/Phase1.kproject")

if(BUILD_TESTING)
    add_executable(KairoPhase1GameStateTests tests/Phase1GameStateTests.cpp)
    target_link_libraries(KairoPhase1GameStateTests PRIVATE KairoPhase1GameState)
    add_test(NAME KairoPhase1.GameState COMMAND KairoPhase1GameStateTests)

    add_test(NAME KairoPhase1.ProjectValidation
        COMMAND $<TARGET_FILE:KairoPlayer>
            "${CMAKE_CURRENT_SOURCE_DIR}/Project/Phase1.kproject" --validate)

    add_test(NAME KairoPhase1.NativeSmoke COMMAND $<TARGET_FILE:KairoPhase1Game> --smoke)
    set_tests_properties(KairoPhase1.NativeSmoke PROPERTIES
        RUN_SERIAL TRUE TIMEOUT 30 SKIP_RETURN_CODE 77)
endif()
