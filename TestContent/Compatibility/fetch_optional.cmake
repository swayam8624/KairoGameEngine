cmake_minimum_required(VERSION 3.28)

set(output_dir "${CMAKE_CURRENT_LIST_DIR}/optional")
file(MAKE_DIRECTORY "${output_dir}")

function(fetch_reviewed_asset name url sha256)
    set(destination "${output_dir}/${name}")
    message(STATUS "Fetching reviewed compatibility asset: ${name}")
    file(DOWNLOAD "${url}" "${destination}"
        EXPECTED_HASH "SHA256=${sha256}"
        TLS_VERIFY ON
        SHOW_PROGRESS
        STATUS download_status)
    list(GET download_status 0 status_code)
    list(GET download_status 1 status_message)
    if(NOT status_code EQUAL 0)
        file(REMOVE "${destination}")
        message(FATAL_ERROR "Failed to fetch ${name}: ${status_message}")
    endif()
endfunction()

fetch_reviewed_asset(
    "ABeautifulGame.glb"
    "https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/2bac6f8c57bf471df0d2a1e8a8ec023c7801dddf/Models/ABeautifulGame/glTF-Binary/ABeautifulGame.glb"
    "bd7133b4b322aae97c589b8839dae8155ad2546acb35ae32a127e722a959d007")
fetch_reviewed_asset(
    "kenney_nature-kit.zip"
    "https://kenney.nl/media/pages/assets/nature-kit/37ac38a37b-1677698939/kenney_nature-kit.zip"
    "fa7974a0d342bfe63c38664ba9f8ec1a4aab8ea25f099bdc56870e33588c4d9d")
