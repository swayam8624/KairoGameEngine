cmake_minimum_required(VERSION 3.28)

function(require_sha256 relative_path expected)
    set(path "${CMAKE_CURRENT_LIST_DIR}/${relative_path}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Compatibility asset is missing: ${relative_path}")
    endif()
    file(SHA256 "${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR
            "Compatibility asset checksum mismatch: ${relative_path}\n"
            "expected ${expected}\nactual   ${actual}")
    endif()
endfunction()

require_sha256("committed/ToyCar/ToyCar.glb"
    "01a60862de55cd4b9f3acfab0b0def86451800f9c42467fcd61052c16cb9838c")
require_sha256("committed/MetalRoughSpheresNoTextures/MetalRoughSpheresNoTextures.glb"
    "5e677f260ec0f366967a6e9545de2f3dab2a160e307b9bcb9d050cd2028f63f8")
require_sha256("committed/LightVisibility/LightVisibility.glb"
    "0295dadb5467f4b24d2ebe255c2957446e83c1eea5510e23f22fcedc96330267")
require_sha256("committed/KloppenheimSky/kloppenheim_06_puresky_1k.hdr"
    "206c67e3a1b992282821cf06662bdd69bbb4915c1c4444a66338a40d6a7d4e34")
require_sha256("committed/Malformed/invalid_json.gltf"
    "61ee795d49d073555f923b5423cc0578b2e75d4e3f44bdf690069b79cb857878")

foreach(optional_asset IN ITEMS ABeautifulGame.glb kenney_nature-kit.zip)
    if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/optional/${optional_asset}")
        if(optional_asset STREQUAL "ABeautifulGame.glb")
            require_sha256("optional/${optional_asset}"
                "bd7133b4b322aae97c589b8839dae8155ad2546acb35ae32a127e722a959d007")
        else()
            require_sha256("optional/${optional_asset}"
                "fa7974a0d342bfe63c38664ba9f8ec1a4aab8ea25f099bdc56870e33588c4d9d")
        endif()
    endif()
endforeach()
