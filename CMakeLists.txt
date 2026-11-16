cmake_minimum_required(VERSION 3.24)

project(OneInTwo VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ONEINTWO_BUILD_TESTS "Build the package test suites" ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets)

if(ONEINTWO_BUILD_TESTS)
    enable_testing()
endif()

# Session, TLS, and visual packages. Each keeps its standalone project and
# registers its tests once when configured from the repository root.
add_subdirectory(cmake/session)
add_subdirectory(cmake/tls)
add_subdirectory(cmake/visual)

# The composition core can build without the capture and notification packages.
# The composition harness links it separately to keep test doubles out of the product.
add_library(oneintwo_app_core STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/app/coordinator.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/app/main_window.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/app/qt_runtime.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/storage/local_store.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/storage/pairing_store.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/main_window.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/pairing_dialog.cpp"
)
add_library(OneInTwo::app_core ALIAS oneintwo_app_core)
target_include_directories(oneintwo_app_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(oneintwo_app_core PUBLIC cxx_std_17)
target_link_libraries(oneintwo_app_core PUBLIC oneintwo_visual oneintwo_tls Qt6::Widgets)
if(MSVC)
    target_compile_options(oneintwo_app_core PRIVATE /W4)
else()
    target_compile_options(oneintwo_app_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(ONEINTWO_BUILD_TESTS)
    add_subdirectory(tests/pairing_ui)
    add_subdirectory(tests/application)
endif()

# Native capture is platform-specific. The build skips the backend if the
# corresponding capture package is absent.
set(ONEINTWO_CAPTURE_TARGET "")
if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/capture_windows/CMakeLists.txt")
    add_subdirectory(cmake/capture_windows)
    set(ONEINTWO_CAPTURE_TARGET oneintwo_capture_windows)
elseif(UNIX AND NOT APPLE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/capture_wayland/CMakeLists.txt")
    add_subdirectory(cmake/capture_wayland)
    set(ONEINTWO_CAPTURE_TARGET oneintwo_capture_wayland)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/notifications/CMakeLists.txt")
    add_subdirectory(cmake/notifications)
endif()

# The executable requires the native notification and capture backends.
# The harness continues to test composition with test doubles.
if(TARGET oneintwo_notifications AND NOT ONEINTWO_CAPTURE_TARGET STREQUAL "")
    add_executable(oneintwo
        "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/app/application.cpp"
    )
    target_compile_features(oneintwo PRIVATE cxx_std_17)
    target_link_libraries(oneintwo PRIVATE
        oneintwo_app_core
        oneintwo_notifications
        ${ONEINTWO_CAPTURE_TARGET}
        Qt6::Widgets
    )
    if(MSVC)
        target_compile_options(oneintwo PRIVATE /W4)
    else()
        target_compile_options(oneintwo PRIVATE -Wall -Wextra -Wpedantic)
    endif()
else()
    message(STATUS
        "oneintwo: executable skipped because oneintwo_notifications, native capture, or both are missing")
endif()
