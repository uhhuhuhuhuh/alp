file(GLOB CAS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_executable(pgntotsv ${CAS_SOURCES})

target_link_libraries(pgntotsv PRIVATE alp::lib alp_build_options)

if(ALP_ENABLE_SANITIZERS)
    target_link_libraries(pgntotsv PRIVATE alp_sanitizers)
endif()
