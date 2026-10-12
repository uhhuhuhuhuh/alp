file(GLOB ALP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(alp_lib ${ALP_SOURCES})
add_library(alp::lib ALIAS alp_lib)

target_compile_features(alp_lib PUBLIC cxx_std_20)

target_include_directories(alp_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(alp_lib PRIVATE alp_build_options)

if(ALP_ENABLE_SANITIZERS)
    target_link_libraries(alp_lib PRIVATE alp_sanitizers)
endif()
