cmake_minimum_required(VERSION 3.24)

# Derive the repository root from this file so the package can build
# independently or as a subdirectory of the root project.
get_filename_component(ONEINTWO_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

set(ONEINTWO_CAPTURE_STANDALONE OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(oneintwo_capture_wayland LANGUAGES CXX)
    set(ONEINTWO_CAPTURE_STANDALONE ON)
endif()

if(NOT UNIX OR APPLE)
    message(STATUS "oneintwo_capture_wayland: solo se construye en Linux")
    return()
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(GIO REQUIRED IMPORTED_TARGET gio-2.0 gio-unix-2.0)
pkg_check_modules(PIPEWIRE REQUIRED IMPORTED_TARGET libpipewire-0.3)
find_package(Threads REQUIRED)

set(ONEINTWO_CAPTURE_PARENT_SOURCES
    ${ONEINTWO_REPO_ROOT}/src/platform/linux/visual/parent_window_default.cpp)
set(ONEINTWO_CAPTURE_PARENT_LIBRARIES "")
set(ONEINTWO_CAPTURE_QT ON CACHE BOOL
    "Use Qt for the picker parent window when available")
if(ONEINTWO_CAPTURE_QT)
    find_package(Qt6 QUIET COMPONENTS Widgets)
    if(Qt6Widgets_FOUND)
        set(ONEINTWO_CAPTURE_PARENT_SOURCES
            ${ONEINTWO_REPO_ROOT}/src/platform/linux/visual/parent_window_qt.cpp)
        set(ONEINTWO_CAPTURE_PARENT_LIBRARIES Qt6::Widgets)
    endif()
endif()

# The core covers the real D-Bus portal, protocol, and orchestration and is tested with doubles.
add_library(oneintwo_capture_wayland_core STATIC
    ${ONEINTWO_REPO_ROOT}/src/platform/linux/visual/portal_screencast.cpp
    ${ONEINTWO_REPO_ROOT}/src/platform/linux/visual/video_frame_convert.cpp
    ${ONEINTWO_REPO_ROOT}/src/platform/linux/visual/wayland_visual_capture.cpp
    ${ONEINTWO_CAPTURE_PARENT_SOURCES}
)
target_include_directories(oneintwo_capture_wayland_core PUBLIC
    $<BUILD_INTERFACE:${ONEINTWO_REPO_ROOT}/src>)
target_compile_features(oneintwo_capture_wayland_core PUBLIC cxx_std_17)
target_link_libraries(oneintwo_capture_wayland_core PUBLIC PkgConfig::GIO Threads::Threads)
if(ONEINTWO_CAPTURE_PARENT_LIBRARIES)
    target_link_libraries(oneintwo_capture_wayland_core PRIVATE
        ${ONEINTWO_CAPTURE_PARENT_LIBRARIES})
endif()
set_target_properties(oneintwo_capture_wayland_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Public backend with a platform-selected factory and a PipeWire consumer.
add_library(oneintwo_capture_wayland STATIC
    ${ONEINTWO_REPO_ROOT}/src/platform/linux/visual_capture.cpp
    ${ONEINTWO_REPO_ROOT}/src/platform/linux/visual/pipewire_source.cpp
)
target_include_directories(oneintwo_capture_wayland PUBLIC
    $<BUILD_INTERFACE:${ONEINTWO_REPO_ROOT}/src>)
target_compile_features(oneintwo_capture_wayland PUBLIC cxx_std_17)
target_link_libraries(oneintwo_capture_wayland PUBLIC oneintwo_capture_wayland_core)
target_link_libraries(oneintwo_capture_wayland PRIVATE PkgConfig::PIPEWIRE)
set_target_properties(oneintwo_capture_wayland PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(NOT DEFINED ONEINTWO_BUILD_TESTS)
    set(ONEINTWO_BUILD_TESTS ${ONEINTWO_CAPTURE_STANDALONE} CACHE BOOL
        "Build the package tests")
endif()

if(ONEINTWO_BUILD_TESTS)
    enable_testing()
    add_executable(oneintwo_capture_wayland_tests
        ${CMAKE_CURRENT_LIST_DIR}/../../tests/capture_wayland/test_main.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../../tests/capture_wayland/mock_portal.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../../tests/capture_wayland/test_presentation.cpp
    )
    target_include_directories(oneintwo_capture_wayland_tests PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../../tests/capture_wayland)
    target_link_libraries(oneintwo_capture_wayland_tests
        PRIVATE oneintwo_capture_wayland_core)
    add_test(NAME capture_wayland COMMAND oneintwo_capture_wayland_tests)
    set_tests_properties(capture_wayland PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        TIMEOUT 120)
endif()
