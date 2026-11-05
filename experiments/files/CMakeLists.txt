# Responsabilidad: compilación independiente del experimento de archivos.
# No depende del CMake raíz ni del código del producto.
cmake_minimum_required(VERSION 3.16)
project(oneintwo_files_experiment LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(O2F_ENABLE_TLS "Compilar transporte TLS con OpenSSL" ON)
option(O2F_ENABLE_GTK "Compilar publicación de portapapeles con GTK3 en Linux" ON)

function(o2f_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

find_package(Threads REQUIRED)

add_library(o2f_core STATIC
    src/core/model.cpp
    src/core/manifest.cpp
    src/core/store.cpp
    src/core/sha256.cpp
    src/core/stream.cpp
    src/core/wire.cpp
    src/core/transfer.cpp
    src/core/cache.cpp
    src/core/net.cpp
    src/core/remote.cpp
)
target_include_directories(o2f_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(o2f_core PUBLIC Threads::Threads)
o2f_set_warnings(o2f_core)

if(WIN32)
    target_link_libraries(o2f_core PUBLIC bcrypt ws2_32)
    # fopen de la API C de OpenSSL: se usa por interoperabilidad con PEM_read_X509.
    target_compile_definitions(o2f_core PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if(O2F_ENABLE_TLS)
    find_package(OpenSSL QUIET)
    if(OpenSSL_FOUND)
        target_compile_definitions(o2f_core PUBLIC O2F_WITH_TLS=1)
        target_link_libraries(o2f_core PUBLIC OpenSSL::SSL OpenSSL::Crypto)
        message(STATUS "TLS: OpenSSL ${OPENSSL_VERSION}")
    else()
        message(WARNING "OpenSSL no encontrado: se compila sin transporte TLS; "
                        "los modos --share y --receive-offer quedan bloqueados")
    endif()
endif()

if(WIN32)
    set(O2F_PLATFORM_SOURCE src/platform/win/clipboard_win.cpp)
elseif(UNIX)
    find_package(PkgConfig QUIET)
    if(O2F_ENABLE_GTK AND PkgConfig_FOUND)
        pkg_check_modules(GTK3 IMPORTED_TARGET gtk+-3.0)
    endif()
    if(GTK3_FOUND)
        set(O2F_PLATFORM_SOURCE src/platform/linux/clipboard_linux.cpp)
    else()
        message(WARNING "GTK3 no encontrado: el portapapeles de archivos en Linux no se compila")
        set(O2F_PLATFORM_SOURCE src/platform/clipboard_unavailable.cpp)
    endif()
else()
    set(O2F_PLATFORM_SOURCE src/platform/clipboard_unavailable.cpp)
endif()

add_library(o2f_platform STATIC ${O2F_PLATFORM_SOURCE})
target_link_libraries(o2f_platform PUBLIC o2f_core)
o2f_set_warnings(o2f_platform)

if(WIN32)
    target_link_libraries(o2f_platform PUBLIC ole32 oleaut32 user32 shell32)
elseif(GTK3_FOUND)
    target_compile_definitions(o2f_platform PRIVATE O2F_WITH_GTK=1)
    target_link_libraries(o2f_platform PUBLIC PkgConfig::GTK3)
endif()

add_executable(oneintwo-files src/main.cpp tests/self_test.cpp)
target_include_directories(oneintwo-files PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(oneintwo-files PRIVATE o2f_platform)
o2f_set_warnings(oneintwo-files)

enable_testing()
add_test(NAME o2f_self_test COMMAND oneintwo-files --self-test)
