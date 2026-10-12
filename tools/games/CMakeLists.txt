file(GLOB GAMES_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_executable(games_helper ${GAMES_SOURCES})

target_link_libraries(games_helper PRIVATE alp::lib alp_build_options)

if(ALP_ENABLE_SANITIZERS)
    target_link_libraries(games_helper PRIVATE alp_sanitizers)
endif()
