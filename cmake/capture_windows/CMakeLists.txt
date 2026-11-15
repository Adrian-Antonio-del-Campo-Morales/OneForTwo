cmake_minimum_required(VERSION 3.24)

# Raíz del repositorio obtenida desde la ruta de este archivo, para funcionar
# como proyecto independiente y como subdirectorio del raíz.
get_filename_component(ONEINTWO_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

set(ONEINTWO_CAPTURE_STANDALONE OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(oneintwo_capture_windows LANGUAGES CXX)
    set(ONEINTWO_CAPTURE_STANDALONE ON)
endif()

# En CI se activa para que la falta de cabeceras WGC falle en compilación y
# para construir un ejecutable que compruebe el enlace de los import libs.
option(ONEINTWO_REQUIRE_WGC
    "Exigir cabeceras WGC y comprobar el enlace del backend" OFF)

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

# Núcleo nativo portable: sesión, frame pool y frames se inyectan, de modo que
# el ciclo de vida WGC puede probarse con dobles sin WinRT ni GPU.
add_library(oneintwo_wgc_capture_core STATIC
    ${ONEINTWO_REPO_ROOT}/src/platform/windows/visual/wgc_capture_core.cpp
)
target_include_directories(oneintwo_wgc_capture_core PUBLIC
    $<BUILD_INTERFACE:${ONEINTWO_REPO_ROOT}/src>)
target_compile_features(oneintwo_wgc_capture_core PUBLIC cxx_std_17)
set_target_properties(oneintwo_wgc_capture_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

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
    # Línea estable para que CI pueda publicar y validar el SDK realmente
    # seleccionado por el generador Visual Studio.
    message(STATUS
        "OneInTwo selected Windows SDK: '${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}'")

    # IsCursorCaptureEnabled llegó con el SDK 10.0.19041. El backend lo usa
    # siempre; el QI de runtime conserva Windows 1903/1909. Fallar aquí evita
    # una build que compile el fallback silenciosamente.
    set(ONEINTWO_WINDOWS_SDK_MIN "10.0.19041")
    set(_oneintwo_sdk_version "${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
    if(NOT _oneintwo_sdk_version AND DEFINED ENV{WindowsSDKVersion})
        string(REGEX REPLACE "[\\\\/]+$" "" _oneintwo_sdk_version "$ENV{WindowsSDKVersion}")
    endif()
    if(_oneintwo_sdk_version AND _oneintwo_sdk_version VERSION_LESS ONEINTWO_WINDOWS_SDK_MIN)
        message(FATAL_ERROR
            "OneInTwo requiere Windows SDK ${ONEINTWO_WINDOWS_SDK_MIN} o posterior "
            "para IsCursorCaptureEnabled; el SDK seleccionado es '${_oneintwo_sdk_version}'.")
    endif()

    include(CheckCXXSourceCompiles)
    set(_oneintwo_try_compile_type "${CMAKE_TRY_COMPILE_TARGET_TYPE}")
    set(_oneintwo_required_flags "${CMAKE_REQUIRED_FLAGS}")
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
    if(MSVC)
        string(APPEND CMAKE_REQUIRED_FLAGS
            " /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_WIN32_WINNT=0x0A00 /DWINVER=0x0A00")
    else()
        string(APPEND CMAKE_REQUIRED_FLAGS
            " -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00")
    endif()
    unset(ONEINTWO_HAS_WGC_CURSOR_API CACHE)
    check_cxx_source_compiles([=[
        #include <windows.h>
        #include <windows.graphics.capture.h>
        void checkCursor(ABI::Windows::Graphics::Capture::IGraphicsCaptureSession2& session)
        {
            session.put_IsCursorCaptureEnabled(false);
        }
        int main() { return 0; }
    ]=] ONEINTWO_HAS_WGC_CURSOR_API)
    set(CMAKE_TRY_COMPILE_TARGET_TYPE "${_oneintwo_try_compile_type}")
    set(CMAKE_REQUIRED_FLAGS "${_oneintwo_required_flags}")
    if(NOT ONEINTWO_HAS_WGC_CURSOR_API)
        message(FATAL_ERROR
            "OneInTwo requiere Windows SDK ${ONEINTWO_WINDOWS_SDK_MIN} o posterior "
            "(IGraphicsCaptureSession2/IsCursorCaptureEnabled); no se encontró la API "
            "en las cabeceras seleccionadas.")
    endif()

    add_library(oneintwo_capture_windows STATIC
        ${ONEINTWO_REPO_ROOT}/src/platform/windows/visual_capture.cpp
        ${ONEINTWO_REPO_ROOT}/src/platform/windows/visual/win32_selector.cpp
        ${ONEINTWO_REPO_ROOT}/src/platform/windows/visual/wgc_capture.cpp
        ${ONEINTWO_CAPTURE_PARENT_SOURCES}
    )
    target_include_directories(oneintwo_capture_windows PUBLIC
        $<BUILD_INTERFACE:${ONEINTWO_REPO_ROOT}/src>)
    target_compile_features(oneintwo_capture_windows PUBLIC cxx_std_17)
    # IsCursorCaptureEnabled exige el SDK 10.0.19041; el QI de runtime conserva
    # Windows 1903/1909 sin intentar llamar a la propiedad.
    target_compile_definitions(oneintwo_capture_windows PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
        _WIN32_WINNT=0x0A00 WINVER=0x0A00)
    target_link_libraries(oneintwo_capture_windows
        PUBLIC oneintwo_capture_windows_model
        PRIVATE oneintwo_wgc_capture_core d3d11 dxgi dwmapi user32 gdi32 ole32 runtimeobject
                windowsapp)
    if(ONEINTWO_CAPTURE_PARENT_LIBRARIES)
        target_link_libraries(oneintwo_capture_windows PRIVATE
            ${ONEINTWO_CAPTURE_PARENT_LIBRARIES})
    endif()
    if(ONEINTWO_REQUIRE_WGC)
        target_compile_definitions(oneintwo_capture_windows PRIVATE
            ONEINTWO_REQUIRE_WGC=1)
        target_compile_definitions(oneintwo_capture_windows_model PRIVATE
            ONEINTWO_REQUIRE_WGC=1)
        add_executable(oneintwo_capture_windows_link_check
            ${CMAKE_CURRENT_LIST_DIR}/../../tests/capture_windows/wgc_link_check.cpp)
        target_link_libraries(oneintwo_capture_windows_link_check
            PRIVATE oneintwo_capture_windows)
        add_executable(oneintwo_wgc_probe
            ${CMAKE_CURRENT_LIST_DIR}/../../tests/capture_windows/wgc_runtime_probe.cpp)
        target_link_libraries(oneintwo_wgc_probe
            PRIVATE oneintwo_capture_windows)
        target_compile_definitions(oneintwo_wgc_probe PRIVATE
            UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
            _WIN32_WINNT=0x0A00 WINVER=0x0A00)
        add_executable(oneintwo_wgc_smoke
            ${CMAKE_CURRENT_LIST_DIR}/../../tests/capture_windows/wgc_smoke.cpp)
        target_link_libraries(oneintwo_wgc_smoke
            PRIVATE oneintwo_capture_windows user32 gdi32 dwmapi)
        target_compile_definitions(oneintwo_wgc_smoke PRIVATE
            UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
            _WIN32_WINNT=0x0A00 WINVER=0x0A00)
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

    add_executable(oneintwo_wgc_capture_tests
        ${CMAKE_CURRENT_LIST_DIR}/../../tests/capture_windows/test_wgc_capture.cpp
    )
    target_include_directories(oneintwo_wgc_capture_tests PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../../tests/capture_windows)
    target_link_libraries(oneintwo_wgc_capture_tests
        PRIVATE oneintwo_wgc_capture_core)
    add_test(NAME capture_windows_wgc COMMAND oneintwo_wgc_capture_tests)
    set_tests_properties(capture_windows_wgc PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        TIMEOUT 120)

    add_executable(oneintwo_wgc_probe_tests
        ${CMAKE_CURRENT_LIST_DIR}/../../tests/capture_windows/test_wgc_probe.cpp
    )
    target_include_directories(oneintwo_wgc_probe_tests PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../../tests/capture_windows
        ${ONEINTWO_REPO_ROOT}/src)
    target_compile_features(oneintwo_wgc_probe_tests PRIVATE cxx_std_17)
    add_test(NAME capture_windows_wgc_probe COMMAND oneintwo_wgc_probe_tests)
    set_tests_properties(capture_windows_wgc_probe PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        TIMEOUT 120)

    add_executable(oneintwo_wgc_probe_fake
        ${CMAKE_CURRENT_LIST_DIR}/../../tests/capture_windows/wgc_probe_fake_main.cpp
    )
    target_include_directories(oneintwo_wgc_probe_fake PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../../tests/capture_windows)
    target_compile_features(oneintwo_wgc_probe_fake PRIVATE cxx_std_17)
    add_test(NAME capture_windows_wgc_probe_process
        COMMAND ${CMAKE_COMMAND}
            -DPROBE=$<TARGET_FILE:oneintwo_wgc_probe_fake>
            -P ${CMAKE_CURRENT_LIST_DIR}/../../tests/capture_windows/test_wgc_probe_process.cmake)
    set_tests_properties(capture_windows_wgc_probe_process PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        TIMEOUT 120)

    if(TARGET oneintwo_wgc_smoke)
        add_test(NAME capture_windows_wgc_smoke COMMAND oneintwo_wgc_smoke)
        set_tests_properties(capture_windows_wgc_smoke PROPERTIES
            SKIP_RETURN_CODE 77
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            TIMEOUT 30)
    endif()
endif()
