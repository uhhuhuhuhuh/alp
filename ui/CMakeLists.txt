file(GLOB UI_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(alp ${UI_SOURCES})

target_include_directories(alp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(alp PRIVATE alp::lib alp_build_options)

if(ALP_ENABLE_SANITIZERS)
    target_link_libraries(alp PRIVATE alp_sanitizers)
endif()

find_package(wxWidgets QUIET COMPONENTS core base)

if(wxWidgets_FOUND)
    include(${wxWidgets_USE_FILE})
    target_link_libraries(alp PRIVATE ${wxWidgets_LIBRARIES})
else()
    include(FetchContent)

    FetchContent_Declare(
        wxwidgets
        GIT_REPOSITORY https://github.com/wxWidgets/wxWidgets.git
        GIT_TAG v3.2.8
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(wxwidgets)

    target_link_libraries(alp PRIVATE wx::core wx::base)
endif()

file(COPY "${PROJECT_SOURCE_DIR}/resources"
     DESTINATION "${CMAKE_BINARY_DIR}")
