add_library(KairoBridge)

target_sources(KairoBridge PUBLIC
    FILE_SET CXX_MODULES
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    FILES
        MigrationTypes.cppm
        SourceProject.cppm
        CanonicalIR.cppm
        MigrationManifest.cppm
        UnityAdapter.cppm
        GodotAdapter.cppm
        KairoBridge.cppm)

add_library(Kairo::Bridge ALIAS KairoBridge)

if(BUILD_TESTING AND TARGET Catch2::Catch2WithMain)
    add_executable(KairoBridgeTests
        tests/KairoBridgeTests.cpp
        tests/DurableIdentityTests.cpp
        tests/GodotAdapterTests.cpp)
    target_link_libraries(KairoBridgeTests PRIVATE
        KairoBridge
        Catch2::Catch2WithMain)
    add_test(NAME KairoBridgeTests COMMAND KairoBridgeTests)
endif()
