if(NOT DEFINED KAIRO_PHASE1_EXECUTABLE OR NOT EXISTS "${KAIRO_PHASE1_EXECUTABLE}")
    message(FATAL_ERROR "KAIRO_PHASE1_EXECUTABLE must identify the built Phase 1 executable.")
endif()
if(NOT DEFINED KAIRO_PHASE1_PROJECT_SOURCE OR
   NOT EXISTS "${KAIRO_PHASE1_PROJECT_SOURCE}/Phase1.kproject")
    message(FATAL_ERROR "KAIRO_PHASE1_PROJECT_SOURCE must identify the checked-in project directory.")
endif()
if(NOT DEFINED KAIRO_PHASE1_PACKAGE_ROOT OR KAIRO_PHASE1_PACKAGE_ROOT STREQUAL "")
    message(FATAL_ERROR "KAIRO_PHASE1_PACKAGE_ROOT is required.")
endif()

file(REMOVE_RECURSE "${KAIRO_PHASE1_PACKAGE_ROOT}")
file(MAKE_DIRECTORY "${KAIRO_PHASE1_PACKAGE_ROOT}/Project")
file(COPY "${KAIRO_PHASE1_PROJECT_SOURCE}/"
    DESTINATION "${KAIRO_PHASE1_PACKAGE_ROOT}/Project")

execute_process(
    COMMAND "${KAIRO_PHASE1_EXECUTABLE}"
        "${KAIRO_PHASE1_PACKAGE_ROOT}/Project/Phase1.kproject"
        --package Development --replace
    RESULT_VARIABLE package_result
    OUTPUT_VARIABLE package_output
    ERROR_VARIABLE package_error
    TIMEOUT 25)
if(NOT package_result EQUAL 0)
    message(FATAL_ERROR
        "Phase 1 packaging failed (${package_result}).\nstdout:\n${package_output}\nstderr:\n${package_error}")
endif()

set(package_directory "${KAIRO_PHASE1_PACKAGE_ROOT}/Project/Build/Development")
set(package_manifest "${package_directory}/package.kmanifest")
if(NOT EXISTS "${package_manifest}")
    message(FATAL_ERROR "Phase 1 package manifest was not created: ${package_manifest}")
endif()
file(READ "${package_manifest}" manifest_text)
if(NOT manifest_text MATCHES "kairo-package 1")
    message(FATAL_ERROR "Phase 1 package manifest has an invalid header.")
endif()
if(NOT manifest_text MATCHES "profile \"Development\"")
    message(FATAL_ERROR "Phase 1 package manifest does not identify the Development profile.")
endif()
if(NOT manifest_text MATCHES "project \"project/Project.kproject\"")
    message(FATAL_ERROR "Phase 1 package manifest does not reference the relocated project.")
endif()
if(NOT manifest_text MATCHES "player \"bin/KairoPhase1Game(\\.exe)?\"")
    message(FATAL_ERROR "Phase 1 package manifest does not reference the sample game executable.")
endif()

file(GLOB package_executables
    "${package_directory}/bin/KairoPhase1Game"
    "${package_directory}/bin/KairoPhase1Game.exe")
list(LENGTH package_executables executable_count)
if(NOT executable_count EQUAL 1)
    message(FATAL_ERROR "Phase 1 package must contain exactly one game executable.")
endif()
if(NOT EXISTS "${package_directory}/run.sh" AND NOT EXISTS "${package_directory}/run.cmd")
    message(FATAL_ERROR "Phase 1 package must contain a platform launcher.")
endif()

message(STATUS "Phase 1 package acceptance test passed: ${package_directory}")
