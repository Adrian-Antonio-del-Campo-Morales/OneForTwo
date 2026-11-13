cmake_minimum_required(VERSION 3.24)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(oneintwo_visual LANGUAGES CXX)
endif()

get_filename_component(ONEINTWO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
option(ONEINTWO_BUILD_TESTS "Build OneInTwo package tests" OFF)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets)

if(NOT TARGET oneintwo_session)
    add_subdirectory("${ONEINTWO_ROOT}/cmake/session" "${CMAKE_CURRENT_BINARY_DIR}/session")
endif()

if(NOT TARGET oneintwo_visual)
    add_library(oneintwo_visual STATIC
        "${ONEINTWO_ROOT}/src/features/visual_share.cpp"
        "${ONEINTWO_ROOT}/src/features/visual/visual_codec.cpp"
        "${ONEINTWO_ROOT}/src/ui/visual_viewer.cpp"
    )
    add_library(OneInTwo::visual ALIAS oneintwo_visual)
    target_include_directories(oneintwo_visual PUBLIC "${ONEINTWO_ROOT}/src")
    target_compile_features(oneintwo_visual PUBLIC cxx_std_17)
    target_link_libraries(oneintwo_visual PUBLIC oneintwo_session Qt6::Core Qt6::Gui Qt6::Widgets)
    if(MSVC)
        target_compile_options(oneintwo_visual PRIVATE /W4)
    else()
        target_compile_options(oneintwo_visual PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    if(ONEINTWO_BUILD_TESTS)
        enable_testing()
        add_subdirectory("${ONEINTWO_ROOT}/tests/visual"
                         "${CMAKE_CURRENT_BINARY_DIR}/tests")
    endif()
endif()
