file(GLOB UI_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(csf ${UI_SOURCES})

target_include_directories(csf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(csf PRIVATE alp::lib alp_build_options)

if(ALP_ENABLE_SANITIZERS)
    target_link_libraries(csf PRIVATE alp_sanitizers)
endif()
