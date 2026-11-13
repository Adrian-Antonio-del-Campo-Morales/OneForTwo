cmake_minimum_required(VERSION 3.24)

# Raíz del repositorio obtenida desde la ruta de este archivo, para funcionar
# como proyecto independiente y como subdirectorio del raíz.
get_filename_component(ONEINTWO_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

set(ONEINTWO_CAPTURE_STANDALONE OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(oneintwo_capture_windows LANGUAGES CXX)
    set(ONEINTWO_CAPTURE_STANDALONE ON)
endif()

# Modelo portable: selección, identidad de la fuente, orden de callbacks y
# contrato BGRA. Se prueba sin Windows con un puerto falso.
add_library(oneintwo_capture_windows_model STATIC
    ${ONEINTWO_REPO_ROOT}/src/platform/windows/visual/capture_model.cpp
    ${ONEINTWO_REPO_ROOT}/src/platform/windows/visual/unavailable_port.cpp
    ${ONEINTWO_REPO_ROOT}/src/platform/windows/visual/windows_capture_factory.cpp
)
target_include_directories(oneintwo_capture_windows_model PUBLIC
    $<BUILD_INTERFACE:${ONEINTWO_REPO_ROOT}/src>)
target_compile_features(oneintwo_capture_windows_model PUBLIC cxx_std_17)
set_target_properties(oneintwo_capture_windows_model PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(ONEINTWO_CAPTURE_PARENT_SOURCES
    ${ONEINTWO_REPO_ROOT}/src/platform/windows/visual/win32_parent_default.cpp)
set(ONEINTWO_CAPTURE_PARENT_LIBRARIES "")
set(ONEINTWO_CAPTURE_QT ON CACHE BOOL
    "Usar Qt para el padre del selector cuando esté disponible")
if(ONEINTWO_CAPTURE_QT)
    find_package(Qt6 QUIET COMPONENTS Widgets)
    if(Qt6Widgets_FOUND)
        set(ONEINTWO_CAPTURE_PARENT_SOURCES
            ${ONEINTWO_REPO_ROOT}/src/platform/windows/visual/win32_parent_qt.cpp)
        set(ONEINTWO_CAPTURE_PARENT_LIBRARIES Qt6::Widgets)
    endif()
endif()

if(WIN32)
    add_library(oneintwo_capture_windows STATIC
        ${ONEINTWO_REPO_ROOT}/src/platform/windows/visual_capture.cpp
        ${ONEINTWO_REPO_ROOT}/src/platform/windows/visual/win32_selector.cpp
        ${ONEINTWO_REPO_ROOT}/src/platform/windows/visual/wgc_capture.cpp
        ${ONEINTWO_CAPTURE_PARENT_SOURCES}
    )
    target_include_directories(oneintwo_capture_windows PUBLIC
        $<BUILD_INTERFACE:${ONEINTWO_REPO_ROOT}/src>)
    target_compile_features(oneintwo_capture_windows PUBLIC cxx_std_17)
    # WGC exige el SDK 10.0.18362: IGraphicsCaptureItemInterop no existe antes.
    target_compile_definitions(oneintwo_capture_windows PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
        _WIN32_WINNT=0x0A00 WINVER=0x0A00)
    target_link_libraries(oneintwo_capture_windows
        PUBLIC oneintwo_capture_windows_model
        PRIVATE d3d11 dxgi dwmapi user32 gdi32 ole32 runtimeobject windowsapp)
    if(ONEINTWO_CAPTURE_PARENT_LIBRARIES)
        target_link_libraries(oneintwo_capture_windows PRIVATE
            ${ONEINTWO_CAPTURE_PARENT_LIBRARIES})
    endif()
    if(MSVC)
        target_compile_options(oneintwo_capture_windows PRIVATE /W4 /permissive-)
        target_compile_options(oneintwo_capture_windows_model PRIVATE /W4 /permissive-)
    endif()
else()
    message(STATUS
        "oneintwo_capture_windows: el backend nativo solo se construye en Windows; "
        "el modelo portable queda disponible para las pruebas")
endif()

if(NOT DEFINED ONEINTWO_BUILD_TESTS)
    set(ONEINTWO_BUILD_TESTS ${ONEINTWO_CAPTURE_STANDALONE} CACHE BOOL
        "Construir las pruebas del paquete")
endif()

if(ONEINTWO_BUILD_TESTS)
    enable_testing()
    add_executable(oneintwo_capture_windows_tests
        ${CMAKE_CURRENT_LIST_DIR}/../../tests/capture_windows/test_main.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../../tests/capture_windows/test_presentation.cpp
    )
    target_include_directories(oneintwo_capture_windows_tests PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../../tests/capture_windows)
    target_link_libraries(oneintwo_capture_windows_tests
        PRIVATE oneintwo_capture_windows_model)
    if(ONEINTWO_CAPTURE_PARENT_LIBRARIES)
        target_link_libraries(oneintwo_capture_windows_tests
            PRIVATE ${ONEINTWO_CAPTURE_PARENT_LIBRARIES})
    endif()
    add_test(NAME capture_windows COMMAND oneintwo_capture_windows_tests)
    set_tests_properties(capture_windows PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        TIMEOUT 120)
endif()
