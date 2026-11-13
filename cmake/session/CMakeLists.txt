cmake_minimum_required(VERSION 3.24)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(oneintwo_session LANGUAGES CXX)
endif()

get_filename_component(ONEINTWO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
option(ONEINTWO_BUILD_TESTS "Build OneInTwo package tests" OFF)

if(NOT TARGET oneintwo_session)
    add_library(oneintwo_session STATIC
        "${ONEINTWO_ROOT}/src/core/session.cpp"
        "${ONEINTWO_ROOT}/src/features/profiles.cpp"
        "${ONEINTWO_ROOT}/src/net/protocol.cpp"
        "${ONEINTWO_ROOT}/src/net/session_link.cpp"
    )
    add_library(OneInTwo::session ALIAS oneintwo_session)
    target_include_directories(oneintwo_session PUBLIC "${ONEINTWO_ROOT}/src")
    target_compile_features(oneintwo_session PUBLIC cxx_std_17)
    if(MSVC)
        target_compile_options(oneintwo_session PRIVATE /W4)
    else()
        target_compile_options(oneintwo_session PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    if(ONEINTWO_BUILD_TESTS)
        enable_testing()
        add_subdirectory("${ONEINTWO_ROOT}/tests/session"
                         "${CMAKE_CURRENT_BINARY_DIR}/tests")
    endif()
endif()
