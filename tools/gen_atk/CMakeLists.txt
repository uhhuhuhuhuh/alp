add_executable(gen_atk main.cpp)

target_link_libraries(gen_atk PRIVATE alp::lib alp_build_options)

if(ALP_ENABLE_SANITIZERS)
    target_link_libraries(gen_atk PRIVATE alp_sanitizers)
endif()
