cmake_minimum_required(VERSION 3.5.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-Wall -Werror -pedantic")

project(alp)

file(GLOB_RECURSE CORE_SRC_FILES alp_core/src/*.cpp)
add_library(core ${CORE_SRC_FILES})
target_include_directories(core PUBLIC alp_core/include/)
target_compile_options(core PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-march=native>
)

file(GLOB_RECURSE UI_SRC_FILES alp_ui/src/*.cpp)
add_executable(alp ${UI_SRC_FILES})
target_link_libraries(alp PRIVATE core)
target_compile_options(alp PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-march=native>
)

option(BUILD_BBGEN "Build the constbbgen tool" OFF)
if (BUILD_BBGEN)
    add_executable(constbbgen
        tools/constbbgen/constbbgen.cpp
    )
    target_link_libraries(constbbgen PRIVATE core)
    target_compile_options(constbbgen PRIVATE
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-march=native>
    )
endif()
