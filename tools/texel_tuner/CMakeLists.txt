add_executable(texel_tuner main.cpp)

add_subdirectory(pgntotsv)

target_link_libraries(texel_tuner PRIVATE alp::lib alp_build_options)

if(ALP_ENABLE_SANITIZERS)
    target_link_libraries(texel_tuner PRIVATE alp_sanitizers)
endif()
