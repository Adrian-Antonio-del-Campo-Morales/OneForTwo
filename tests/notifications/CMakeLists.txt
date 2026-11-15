get_filename_component(ONEINTWO_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

add_executable(oneintwo_notifications_tests
    test_main.cpp
    test_notifications.cpp
    test_support_helpers.cpp
    test_windows_helpers.cpp
    support/test_harness.cpp)
target_link_libraries(oneintwo_notifications_tests PRIVATE oneintwo_notifications)
target_include_directories(oneintwo_notifications_tests PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_compile_features(oneintwo_notifications_tests PRIVATE cxx_std_17)

# El adaptador Linux solo existe en su plataforma.
if(UNIX AND NOT APPLE)
    target_sources(oneintwo_notifications_tests PRIVATE test_linux_adapter.cpp)
endif()

# Los ayudantes de texto XML y línea base son independientes de WinRT y se
# comprueban también fuera de Windows.
if(NOT WIN32)
    target_sources(oneintwo_notifications_tests PRIVATE
        ${ONEINTWO_REPO_ROOT}/src/platform/windows/notification_support/toast_text.cpp)
endif()

add_test(NAME notifications-core COMMAND oneintwo_notifications_tests)

if(ONEINTWO_BUILD_NATIVE_NOTIFICATION_TESTS)
    if(WIN32)
        add_executable(oneintwo_notifications_native_windows native/windows_presentation.cpp)
        target_link_libraries(oneintwo_notifications_native_windows
                              PRIVATE oneintwo_notifications)
        target_compile_features(oneintwo_notifications_native_windows PRIVATE cxx_std_17)
        add_test(NAME notifications-native-windows
                 COMMAND oneintwo_notifications_native_windows)

        # Sin MSIX, el descubrimiento debe fallar cerrado antes de solicitar
        # consentimiento. Se ejecuta en el job MSVC, que no empaqueta el proceso.
        add_executable(oneintwo_notifications_native_windows_discovery
                       native/windows_discovery.cpp)
        target_link_libraries(oneintwo_notifications_native_windows_discovery
                              PRIVATE oneintwo_notifications)
        target_compile_features(oneintwo_notifications_native_windows_discovery
                                PRIVATE cxx_std_17)
        add_test(NAME notifications-windows-discovery-no-package
                 COMMAND oneintwo_notifications_native_windows_discovery)
        # 77: el proceso sí tiene identidad de paquete; la prueba no aplica.
        set_tests_properties(notifications-windows-discovery-no-package
                             PROPERTIES SKIP_RETURN_CODE 77)
    elseif(UNIX)
        add_executable(oneintwo_notifications_native_linux native/linux_presentation.cpp)
        target_link_libraries(oneintwo_notifications_native_linux
                              PRIVATE oneintwo_notifications)
        target_compile_features(oneintwo_notifications_native_linux PRIVATE cxx_std_17)
        add_test(NAME notifications-native-linux
                 COMMAND oneintwo_notifications_native_linux)
        # 77: no hay bus de sesión o servicio de notificaciones disponible.
        set_tests_properties(notifications-native-linux PROPERTIES SKIP_RETURN_CODE 77)

        # Servicio mínimo para el recorrido con un bus de sesión privado.
        add_executable(oneintwo_notifications_dbus_stub native/dbus_notification_stub.cpp)
        target_compile_features(oneintwo_notifications_dbus_stub PRIVATE cxx_std_17)
        if(TARGET PkgConfig::DBUS)
            target_link_libraries(oneintwo_notifications_dbus_stub PRIVATE PkgConfig::DBUS)
        else()
            target_include_directories(oneintwo_notifications_dbus_stub PRIVATE
                ${DBUS_INCLUDE_DIR} ${DBUS_ARCH_INCLUDE_DIR})
            target_link_libraries(oneintwo_notifications_dbus_stub PRIVATE ${DBUS_LIBRARY})
        endif()
    endif()
endif()
