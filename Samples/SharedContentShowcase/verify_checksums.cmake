cmake_minimum_required(VERSION 3.28)

get_filename_component(showcase_root "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)

function(require_sha256 relative expected)
    set(path "${showcase_root}/${relative}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Shared-content fixture is missing: ${relative}")
    endif()
    file(SHA256 "${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR
            "Shared-content checksum mismatch for ${relative}: expected ${expected}, got ${actual}")
    endif()
endfunction()

require_sha256("Content/ToyCar/ToyCar.glb"
    "01a60862de55cd4b9f3acfab0b0def86451800f9c42467fcd61052c16cb9838c")
require_sha256("Content/TexturedPanel/checker.png"
    "1cdc534b7837acb5af7941863873d03bbd8b27f43b13df03821142ab79bbbc66")
require_sha256("docs/shared-content-reference.png"
    "11ed7c801b20eb2a4580c6f22174f5ae50cda6a685c66c32e823107990afbb99")
