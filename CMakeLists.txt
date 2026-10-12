cmake_minimum_required(VERSION 3.20)

project(alp)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

add_library(alp_build_options INTERFACE)

option(ALP_NATIVE_OPTIMIZATIONS "Optimize for the build machine's CPU" ON)
if(ALP_NATIVE_OPTIMIZATIONS)
    target_compile_options(alp_build_options INTERFACE
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-march=native>
    )
endif()

option(ALP_ENABLE_SANITIZERS "Enable AddressSanitizer and UBSan" OFF)
if(ALP_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_library(alp_sanitizers INTERFACE)

        target_compile_options(alp_sanitizers INTERFACE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )

        target_link_options(alp_sanitizers INTERFACE
            -fsanitize=address,undefined
        )
    else()
        message(FATAL_ERROR
            "ALP_ENABLE_SANITIZERS requires GCC or Clang"
        )
    endif()
endif()

add_subdirectory(alp)

option(ALP_BUILD_GUI "Build the alp gui" ON)
if(ALP_BUILD_GUI)
    add_subdirectory(ui)
endif()


option(ALP_BUILD_CAS "Build the Compare Against Stockfish tool" OFF)
if(ALP_BUILD_CAS)
    add_subdirectory(tools/cas)
endif()


option(ALP_BUILD_GAMES "Build the Games tool" OFF)
if(ALP_BUILD_GAMES)
    add_subdirectory(tools/games)
endif()

option(ALP_BUILD_CSF "Build the CSF tool" OFF)
if(ALP_BUILD_CSF)
    add_subdirectory(tools/csf)
endif()

option(ALP_BUILD_PERFT "Build the PERFT tool" OFF)
if(ALP_BUILD_PERFT)
    add_subdirectory(tools/perft)
endif()
