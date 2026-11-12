add_executable(search main.cpp)

target_link_libraries(search PRIVATE alp::lib alp_build_options)

if(ALP_ENABLE_SANITIZERS)
    target_link_libraries(search PRIVATE alp_sanitizers)
endif()
