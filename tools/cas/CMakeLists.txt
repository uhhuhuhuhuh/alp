file(GLOB CAS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_executable(cas_helper ${CAS_SOURCES})

target_link_libraries(cas_helper PRIVATE alp::lib alp_build_options)

if(ALP_ENABLE_SANITIZERS)
    target_link_libraries(cas_helper PRIVATE alp_sanitizers)
endif()
