# Portable Clang toolchain selection for local development and CI.
#
# Set KAIRO_CXX_COMPILER to an absolute clang++ path when multiple Clang
# installations exist. Otherwise CMake resolves clang++ from PATH. This keeps
# machine-local package-manager paths out of versioned presets while still
# allowing a project to require a modern compiler for C++ module support.

if(DEFINED ENV{KAIRO_CXX_COMPILER} AND NOT "$ENV{KAIRO_CXX_COMPILER}" STREQUAL "")
    set(CMAKE_CXX_COMPILER "$ENV{KAIRO_CXX_COMPILER}" CACHE FILEPATH
        "C++ compiler selected by KAIRO_CXX_COMPILER" FORCE)
else()
    # Homebrew deliberately keeps LLVM outside the default PATH. Prefer its
    # current installation when present, rather than encoding either ARM or
    # Intel Homebrew's installation prefix in this repository.
    if(APPLE)
        find_program(KAIRO_BREW_EXECUTABLE NAMES brew)
        if(KAIRO_BREW_EXECUTABLE)
            execute_process(
                COMMAND "${KAIRO_BREW_EXECUTABLE}" --prefix llvm
                OUTPUT_VARIABLE KAIRO_BREW_LLVM_PREFIX
                OUTPUT_STRIP_TRAILING_WHITESPACE
                RESULT_VARIABLE KAIRO_BREW_LLVM_RESULT
                ERROR_QUIET)
            if(KAIRO_BREW_LLVM_RESULT EQUAL 0
                AND EXISTS "${KAIRO_BREW_LLVM_PREFIX}/bin/clang++")
                set(KAIRO_CLANG_CXX_COMPILER
                    "${KAIRO_BREW_LLVM_PREFIX}/bin/clang++")
            endif()
        endif()
    endif()

    if(NOT KAIRO_CLANG_CXX_COMPILER)
        find_program(KAIRO_CLANG_CXX_COMPILER NAMES clang++ clang++-22 clang++-21 REQUIRED)
    endif()
    set(CMAKE_CXX_COMPILER "${KAIRO_CLANG_CXX_COMPILER}" CACHE FILEPATH
        "C++ compiler selected by Kairo's Clang toolchain" FORCE)
endif()

# Keep C and Objective-C++ on the same LLVM installation as C++. Mixing
# upstream Homebrew Clang objects with a different driver makes Darwin
# diagnostics and ABI/toolchain behavior unnecessarily unpredictable.
get_filename_component(_kairo_clang_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
if(EXISTS "${_kairo_clang_bin_dir}/clang")
    set(CMAKE_C_COMPILER "${_kairo_clang_bin_dir}/clang" CACHE FILEPATH
        "C compiler paired with Kairo's Clang toolchain" FORCE)
endif()
if(EXISTS "${_kairo_clang_bin_dir}/clang++")
    set(CMAKE_OBJCXX_COMPILER "${_kairo_clang_bin_dir}/clang++" CACHE FILEPATH
        "Objective-C++ compiler paired with Kairo's Clang toolchain" FORCE)
endif()

if(APPLE)
    # Upstream LLVM 23 targets current macOS with newer DWARF by default, while
    # the Apple ld shipped with the selected Xcode can emit thousands of
    # 'can't parse dwarf compilation unit info' warnings for those objects.
    # DWARF4 remains fully adequate for LLDB source debugging and is understood
    # consistently by Apple's linker across supported Kairo macOS hosts.
    set(CMAKE_C_FLAGS_DEBUG "-g -gdwarf-4 -gstrict-dwarf" CACHE STRING
        "Kairo Debug C flags compatible with Apple's linker" FORCE)
    set(CMAKE_CXX_FLAGS_DEBUG "-g -gdwarf-4 -gstrict-dwarf" CACHE STRING
        "Kairo Debug C++ flags compatible with Apple's linker" FORCE)
    set(CMAKE_OBJCXX_FLAGS_DEBUG "-g -gdwarf-4 -gstrict-dwarf" CACHE STRING
        "Kairo Debug Objective-C++ flags compatible with Apple's linker" FORCE)
endif()
