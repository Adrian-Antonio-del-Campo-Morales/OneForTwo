# Compilación independiente del experimento de portapapeles.
# No depende del CMake raíz, del código de producto ni de otros experimentos.
# Responsabilidad: construir el prototipo, sus adaptadores y el ejecutable de
# auto-pruebas aisladas. El transporte TLS sólo se incluye si OpenSSL está
# disponible; en caso contrario se informa y se compila sin ese modo.

cmake_minimum_required(VERSION 3.16)
project(oneintwo_clipboard_experiment LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(OIT_ENABLE_PEER "Compilar el transporte TLS entre dos equipos" ON)
option(OIT_ENABLE_PORTAL "Compilar el adaptador Wayland con xdg-desktop-portal" ON)

find_package(Threads REQUIRED)

# Núcleo sin dependencias de plataforma: texto, orden lógico y protocolo.
add_library(clipboard_core STATIC
  src/model.cpp
  src/engine.cpp
  src/protocol.cpp
)
target_include_directories(clipboard_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Adaptador nativo del portapapeles. Sólo se compila el backend de la
# plataforma actual; el ajeno no se incluye ni se simula.
add_library(clipboard_platform STATIC)
if(WIN32)
  target_sources(clipboard_platform PRIVATE src/platform/windows_clipboard.cpp)
  target_link_libraries(clipboard_platform PUBLIC user32)
else()
  target_sources(clipboard_platform PRIVATE src/platform/linux_clipboard.cpp)
  if(OIT_ENABLE_PORTAL)
    # libdbus necesita dbus.h y, en Linux multiarch, dbus-arch-deps.h desde
    # otro directorio. pkg-config devuelve ambos; el respaldo permite indicar
    # DBUS_INCLUDE_DIR y DBUS_ARCH_INCLUDE_DIR manualmente.
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
      pkg_check_modules(DBUS QUIET dbus-1)
    endif()
    set(OIT_DBUS_INCLUDE_DIRS "")
    set(OIT_DBUS_LIBRARIES "")
    if(DBUS_FOUND)
      set(OIT_DBUS_INCLUDE_DIRS ${DBUS_INCLUDE_DIRS})
      set(OIT_DBUS_LIBRARIES ${DBUS_LINK_LIBRARIES})
      if(NOT OIT_DBUS_LIBRARIES)
        set(OIT_DBUS_LIBRARIES ${DBUS_LIBRARIES})
      endif()
    else()
      find_path(DBUS_INCLUDE_DIR NAMES dbus/dbus.h PATH_SUFFIXES dbus-1.0)
      find_path(DBUS_ARCH_INCLUDE_DIR NAMES dbus/dbus-arch-deps.h
                PATH_SUFFIXES dbus-1.0/include
                HINTS /usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/dbus-1.0/include)
      find_library(DBUS_LIBRARY NAMES dbus-1)
      if(DBUS_INCLUDE_DIR AND DBUS_ARCH_INCLUDE_DIR AND DBUS_LIBRARY)
        set(OIT_DBUS_INCLUDE_DIRS ${DBUS_INCLUDE_DIR} ${DBUS_ARCH_INCLUDE_DIR})
        set(OIT_DBUS_LIBRARIES ${DBUS_LIBRARY})
      endif()
    endif()
    if(OIT_DBUS_INCLUDE_DIRS AND OIT_DBUS_LIBRARIES)
      target_sources(clipboard_platform PRIVATE src/platform/portal_clipboard.cpp)
      target_include_directories(clipboard_platform PRIVATE
                                 ${OIT_DBUS_INCLUDE_DIRS})
      target_link_libraries(clipboard_platform PRIVATE ${OIT_DBUS_LIBRARIES})
      target_compile_definitions(clipboard_platform PUBLIC OIT_HAVE_PORTAL=1)
      message(STATUS "xdg-desktop-portal backend enabled (libdbus: ${OIT_DBUS_LIBRARIES})")
    else()
      message(WARNING
        "libdbus-1 no encontrado: el adaptador Wayland se compila como "
        "limitación explícita. Instale libdbus-1-dev y vuelva a configurar.")
    endif()
  endif()
endif()
target_link_libraries(clipboard_platform PUBLIC clipboard_core)

if(OIT_ENABLE_PEER)
  find_package(OpenSSL QUIET)
endif()

if(OpenSSL_FOUND)
  add_library(clipboard_net STATIC
    src/net/tls_channel.cpp
    src/net/peer.cpp
  )
  target_link_libraries(clipboard_net
    PUBLIC clipboard_core clipboard_platform Threads::Threads
           OpenSSL::SSL OpenSSL::Crypto)
  if(WIN32)
    # Sockets de Winsock usados por el transporte TLS.
    target_link_libraries(clipboard_net PRIVATE ws2_32)
  endif()
  target_compile_definitions(clipboard_net PUBLIC OIT_HAVE_OPENSSL=1)
  # Regresión aislada de PEM/DLL; no crea sockets ni inicializa el escritorio.
  add_executable(clipboard_tls_certificate_test tests/tls_certificate_test.cpp)
  target_link_libraries(clipboard_tls_certificate_test PRIVATE clipboard_net)
else()
  message(WARNING
    "OpenSSL no encontrado: se compila sin el modo entre equipos. "
    "Consulte MANUAL.md para instalar las dependencias de desarrollo.")
  add_library(clipboard_net STATIC src/net/peer_unavailable.cpp)
  target_link_libraries(clipboard_net PUBLIC clipboard_core clipboard_platform)
endif()

add_executable(oneintwo-clipboard
  src/main.cpp
  src/cli.cpp
  tests/selftest.cpp
)
target_link_libraries(oneintwo-clipboard PRIVATE
  clipboard_core clipboard_platform clipboard_net Threads::Threads)

if(WIN32)
  # CommandLineToArgvW: argumentos Unicode en Windows.
  target_link_libraries(oneintwo-clipboard PRIVATE shell32)
endif()

if(MSVC)
  # Los literales de los datos sintéticos están en UTF-8.
  target_compile_options(oneintwo-clipboard PRIVATE /utf-8 /W4)
  target_compile_options(clipboard_core PRIVATE /utf-8 /W4)
  target_compile_options(clipboard_platform PRIVATE /utf-8 /W4)
  if(TARGET clipboard_net)
    target_compile_options(clipboard_net PRIVATE /utf-8 /W4)
  endif()
else()
  target_compile_options(oneintwo-clipboard PRIVATE -Wall -Wextra)
  target_compile_options(clipboard_core PRIVATE -Wall -Wextra)
  target_compile_options(clipboard_platform PRIVATE -Wall -Wextra)
  if(TARGET clipboard_net)
    target_compile_options(clipboard_net PRIVATE -Wall -Wextra)
  endif()
endif()
