cmake_minimum_required(VERSION 3.5.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-Wall -Werror -pedantic")

project(alp)

file(GLOB_RECURSE SRC_FILES src/*.cpp)
list(FILTER SRC_FILES EXCLUDE REGEX ".*/main\\.cpp$")
add_library(core ${SRC_FILES})
target_include_directories(core PUBLIC include/)
target_compile_options(core PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-march=native>
)

add_executable(alp src/main.cpp)
target_link_libraries(alp PRIVATE core)
target_compile_options(alp PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-march=native>
)

option(BUILD_GMAGIC "Build the gmagicattacks tool" OFF)
if (BUILD_GMAGIC)
    add_executable(gmagicattacks
        tools/gmagicattacks/gmagicattacks.cpp
    )
    target_link_libraries(gmagicattacks PRIVATE core)
    target_compile_options(gmagicattacks PRIVATE
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-march=native>
    )
endif()
