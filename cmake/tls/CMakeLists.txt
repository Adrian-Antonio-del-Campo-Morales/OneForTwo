cmake_minimum_required(VERSION 3.24)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(oneintwo_tls LANGUAGES CXX)
endif()

get_filename_component(ONEINTWO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
option(ONEINTWO_BUILD_TESTS "Build OneInTwo package tests" OFF)

# QSslServer se incorporó en Qt 6.4; la interfaz TLS usa el backend SSL de Qt.
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Network)

if(NOT TARGET oneintwo_session)
    add_subdirectory("${ONEINTWO_ROOT}/cmake/session" "${CMAKE_CURRENT_BINARY_DIR}/session")
endif()

if(NOT TARGET oneintwo_tls)
    add_library(oneintwo_tls STATIC
        "${ONEINTWO_ROOT}/src/net/tls_transport.cpp"
    )
    add_library(OneInTwo::tls ALIAS oneintwo_tls)
    target_include_directories(oneintwo_tls PUBLIC "${ONEINTWO_ROOT}/src")
    target_compile_features(oneintwo_tls PUBLIC cxx_std_17)
    target_link_libraries(oneintwo_tls PUBLIC oneintwo_session Qt6::Core Qt6::Network)
    if(MSVC)
        target_compile_options(oneintwo_tls PRIVATE /W4)
    else()
        target_compile_options(oneintwo_tls PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    if(ONEINTWO_BUILD_TESTS)
        enable_testing()
        add_subdirectory("${ONEINTWO_ROOT}/tests/tls"
                         "${CMAKE_CURRENT_BINARY_DIR}/tests")
    endif()
endif()
