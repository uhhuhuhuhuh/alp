add_executable(perft main.cpp)

target_link_libraries(perft PRIVATE alp::lib alp_build_options)

if(ALP_ENABLE_SANITIZERS)
    target_link_libraries(perft PRIVATE alp_sanitizers)
endif()
