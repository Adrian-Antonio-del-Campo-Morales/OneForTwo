# Responsabilidad: compilación independiente de experiments/input.
# No depende del CMake raíz ni del código de producto.
cmake_minimum_required(VERSION 3.16)
project(OneInTwoInputExperiment CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(ONEINTWO_BACKEND "native" CACHE STRING "Backend: native o none (solo pruebas)")
set_property(CACHE ONEINTWO_BACKEND PROPERTY STRINGS native none)
option(ONEINTWO_WITH_TLS "Compilar el canal TLS con OpenSSL" ON)
option(ONEINTWO_BUILD_APP "Compilar el ejecutable principal" ON)
set(ONEINTWO_X11 "AUTO" CACHE STRING "Backend Linux X11: AUTO, ON u OFF")
set_property(CACHE ONEINTWO_X11 PROPERTY STRINGS AUTO ON OFF)
set(ONEINTWO_WAYLAND "AUTO" CACHE STRING "Backend Linux Wayland: AUTO, ON u OFF")
set_property(CACHE ONEINTWO_WAYLAND PROPERTY STRINGS AUTO ON OFF)

add_library(oit_core STATIC
  src/core/types.cpp
  src/core/keymap.cpp
  src/core/protocol.cpp
  src/core/geometry.cpp
  src/core/input_state.cpp
  src/core/source_controller.cpp
  src/core/target_controller.cpp
  src/platform/wayland_flow.cpp
)
target_include_directories(oit_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(oit_self_test STATIC tests/self_test.cpp)
target_include_directories(oit_self_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/tests)
target_link_libraries(oit_self_test PUBLIC oit_core)

add_executable(input_self_test tests/self_test_main.cpp)
target_link_libraries(input_self_test PRIVATE oit_self_test)

if(NOT ONEINTWO_BUILD_APP OR ONEINTWO_BACKEND STREQUAL "none")
  return()
endif()

set(OIT_SOURCES
  src/main.cpp
  src/net/tls_link.cpp
  tests/self_test.cpp
)

if(WIN32)
  list(APPEND OIT_SOURCES src/platform/windows_backend.cpp)
elseif(UNIX)
  list(APPEND OIT_SOURCES src/platform/linux_factory.cpp)

  # X11 es opcional: se usa cuando existen libX11, libXi y libXtst.
  set(OIT_X11_OK OFF)
  find_package(X11 QUIET)
  find_library(OIT_XTST_LIBRARY NAMES Xtst)
  find_library(OIT_XI_LIBRARY NAMES Xi)
  if(X11_FOUND AND OIT_XTST_LIBRARY AND OIT_XI_LIBRARY)
    set(OIT_X11_OK ON)
  endif()

  # Wayland requiere GDBus (gio-2.0) y libei (libei-1.0). No se instala nada.
  set(OIT_WAYLAND_DEPS OFF)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(OIT_GIO QUIET gio-2.0)
    pkg_check_modules(OIT_LIBEI QUIET libei-1.0)
    if(OIT_GIO_FOUND AND OIT_LIBEI_FOUND)
      set(OIT_WAYLAND_DEPS ON)
    endif()
  endif()

  if(ONEINTWO_X11 STREQUAL "ON" AND NOT OIT_X11_OK)
    message(FATAL_ERROR "ONEINTWO_X11=ON requiere libX11, libXi y libXtst de desarrollo")
  endif()
  if(ONEINTWO_WAYLAND STREQUAL "ON" AND NOT OIT_WAYLAND_DEPS)
    message(FATAL_ERROR
      "ONEINTWO_WAYLAND=ON requiere gio-2.0 y libei-1.0 (pkg-config). "
      "Instale libglib2.0-dev, libei-dev y pkg-config, o use AUTO.")
  endif()

  set(OIT_USE_X11 OFF)
  if(OIT_X11_OK AND NOT ONEINTWO_X11 STREQUAL "OFF")
    set(OIT_USE_X11 ON)
    list(APPEND OIT_SOURCES src/platform/linux_backend.cpp)
  endif()

  set(OIT_USE_WAYLAND OFF)
  if(OIT_WAYLAND_DEPS AND NOT ONEINTWO_WAYLAND STREQUAL "OFF")
    set(OIT_USE_WAYLAND ON)
    list(APPEND OIT_SOURCES src/platform/wayland_backend.cpp)
  endif()

  if(NOT OIT_USE_X11 AND NOT OIT_USE_WAYLAND)
    message(WARNING
      "Sin backend Linux: faltan X11 o gio-2.0+libei-1.0. "
      "El binario compilará pero solo informará el error en ejecución.")
  endif()
else()
  message(FATAL_ERROR "Plataforma no soportada por este experimento")
endif()

add_executable(input_experiment ${OIT_SOURCES})
target_include_directories(input_experiment PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
target_link_libraries(input_experiment PRIVATE oit_core)

if(WIN32)
  target_link_libraries(input_experiment PRIVATE user32 ws2_32)
  target_compile_definitions(input_experiment PRIVATE _WIN32_WINNT=0x0601 NOMINMAX WIN32_LEAN_AND_MEAN)
elseif(UNIX)
  if(OIT_USE_X11)
    target_compile_definitions(input_experiment PRIVATE OIT_WITH_X11=1)
    target_include_directories(input_experiment PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(input_experiment PRIVATE ${X11_LIBRARIES} ${OIT_XTST_LIBRARY} ${OIT_XI_LIBRARY})
  endif()
  if(OIT_USE_WAYLAND)
    target_compile_definitions(input_experiment PRIVATE OIT_WITH_WAYLAND=1)
    target_include_directories(input_experiment PRIVATE
      ${OIT_GIO_INCLUDE_DIRS} ${OIT_LIBEI_INCLUDE_DIRS})
    target_link_libraries(input_experiment PRIVATE
      ${OIT_GIO_LIBRARIES} ${OIT_LIBEI_LIBRARIES})
    target_compile_options(input_experiment PRIVATE
      ${OIT_GIO_CFLAGS_OTHER} ${OIT_LIBEI_CFLAGS_OTHER})
  endif()
endif()

if(ONEINTWO_WITH_TLS)
  if(WIN32 AND NOT DEFINED OPENSSL_ROOT_DIR AND EXISTS "C:/Program Files/OpenSSL-Win64")
    set(OPENSSL_ROOT_DIR "C:/Program Files/OpenSSL-Win64")
  endif()
  find_package(OpenSSL REQUIRED)
  target_link_libraries(input_experiment PRIVATE OpenSSL::SSL OpenSSL::Crypto)
  target_compile_definitions(input_experiment PRIVATE ONEINTWO_WITH_TLS=1)
else()
  target_compile_definitions(input_experiment PRIVATE ONEINTWO_WITH_TLS=0)
endif()

# Las comprobaciones no arrancan el escritorio ni abren red.
enable_testing()
add_test(NAME input_self_test COMMAND input_self_test)
