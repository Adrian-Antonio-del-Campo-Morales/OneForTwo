cmake_minimum_required(VERSION 3.24)

project(OneInTwo VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ONEINTWO_BUILD_TESTS "Build the package test suites" ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets)

if(ONEINTWO_BUILD_TESTS)
    enable_testing()
endif()

# Paquetes de sesión y visual. Cada uno conserva su proyecto independiente y
# añade sus pruebas una sola vez cuando se configura desde la raíz.
add_subdirectory(cmake/session)
add_subdirectory(cmake/visual)

# Núcleo de composición disponible aunque los paquetes de captura y avisos
# todavía no estén en esta rama. El arnés de composición lo enlaza aparte para
# mantener los dobles fuera del producto.
add_library(oneintwo_app_core STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/app/coordinator.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/app/main_window.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/app/qt_runtime.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/storage/local_store.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/main_window.cpp"
)
add_library(OneInTwo::app_core ALIAS oneintwo_app_core)
target_include_directories(oneintwo_app_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(oneintwo_app_core PUBLIC cxx_std_17)
target_link_libraries(oneintwo_app_core PUBLIC oneintwo_visual Qt6::Widgets)
if(MSVC)
    target_compile_options(oneintwo_app_core PRIVATE /W4)
else()
    target_compile_options(oneintwo_app_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Captura nativa por plataforma. Los paquetes V pueden no estar presentes en
# esta rama; en ese caso no se inventa un backend ni se enlaza un doble.
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

# El ejecutable solo existe cuando están las implementaciones reales de
# notificaciones y captura. El arnés sigue verificando la composición con dobles.
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
        "oneintwo: ejecutable omitido; faltan oneintwo_notifications y/o la captura nativa de la plataforma")
endif()
