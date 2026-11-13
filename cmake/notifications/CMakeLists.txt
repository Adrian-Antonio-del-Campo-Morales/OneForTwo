cmake_minimum_required(VERSION 3.24)

get_filename_component(ONEINTWO_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(oneintwo_notifications LANGUAGES CXX)
endif()

option(ONEINTWO_BUILD_TESTS "Construir las pruebas del paquete de notificaciones" OFF)
option(ONEINTWO_BUILD_NATIVE_NOTIFICATION_TESTS
       "Construir el recorrido nativo de presentación del paquete" OFF)

set(ONEINTWO_NOTIFICATION_SOURCES
    ${ONEINTWO_REPO_ROOT}/src/features/notifications.cpp
    ${ONEINTWO_REPO_ROOT}/src/features/notification_support/identity.cpp
    ${ONEINTWO_REPO_ROOT}/src/features/notification_support/text_validation.cpp)

add_library(oneintwo_notifications ${ONEINTWO_NOTIFICATION_SOURCES})
target_include_directories(oneintwo_notifications PUBLIC ${ONEINTWO_REPO_ROOT}/src)
target_compile_features(oneintwo_notifications PUBLIC cxx_std_17)

if(WIN32)
    target_sources(oneintwo_notifications PRIVATE
        ${ONEINTWO_REPO_ROOT}/src/platform/windows/notifications.cpp
        ${ONEINTWO_REPO_ROOT}/src/platform/windows/notification_support/toast_text.cpp)
    target_compile_definitions(oneintwo_notifications PUBLIC
        UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
        WINVER=0x0A00 _WIN32_WINNT=0x0A00)
    # C++/WinRT se activa desde windowsapp; la identidad de proceso y los
    # accesos directos del menú Inicio usan shell32.
    target_link_libraries(oneintwo_notifications PUBLIC windowsapp shell32)
elseif(UNIX AND NOT APPLE)
    target_sources(oneintwo_notifications PRIVATE
        ${ONEINTWO_REPO_ROOT}/src/platform/linux/notifications.cpp
        ${ONEINTWO_REPO_ROOT}/src/platform/linux/notification_support/dbus_notifications.cpp
        ${ONEINTWO_REPO_ROOT}/src/platform/linux/notification_support/linux_notifications.cpp)

    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(DBUS QUIET IMPORTED_TARGET dbus-1)
    endif()
    if(TARGET PkgConfig::DBUS)
        target_link_libraries(oneintwo_notifications PUBLIC PkgConfig::DBUS)
    else()
        find_path(DBUS_INCLUDE_DIR NAMES dbus/dbus.h PATH_SUFFIXES dbus-1.0)
        find_path(DBUS_ARCH_INCLUDE_DIR NAMES dbus/dbus-arch-deps.h
                  PATH_SUFFIXES dbus-1.0/include)
        find_library(DBUS_LIBRARY NAMES dbus-1)
        if(NOT DBUS_INCLUDE_DIR OR NOT DBUS_ARCH_INCLUDE_DIR OR NOT DBUS_LIBRARY)
            message(FATAL_ERROR
                "No se encontró libdbus-1. Instalar libdbus-1-dev o indicar "
                "DBUS_INCLUDE_DIR, DBUS_ARCH_INCLUDE_DIR y DBUS_LIBRARY.")
        endif()
        target_include_directories(oneintwo_notifications PRIVATE
            ${DBUS_INCLUDE_DIR} ${DBUS_ARCH_INCLUDE_DIR})
        target_link_libraries(oneintwo_notifications PUBLIC ${DBUS_LIBRARY})
    endif()
else()
    message(FATAL_ERROR
        "oneintwo_notifications solo admite los backends Windows y Linux.")
endif()

if(ONEINTWO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(${ONEINTWO_REPO_ROOT}/tests/notifications
                     ${CMAKE_CURRENT_BINARY_DIR}/tests/notifications)
endif()
