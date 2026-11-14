if(NOT TARGET oneintwo_tls)
    message(FATAL_ERROR "oneintwo_tls debe existir antes de la prueba de UI de emparejamiento")
endif()

get_filename_component(ONEINTWO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
enable_testing()

add_executable(oneintwo_pairing_ui_tests
    pairing_ui_test.cpp
    "${ONEINTWO_ROOT}/src/ui/pairing_dialog.cpp"
    "${ONEINTWO_ROOT}/src/storage/pairing_store.cpp")
target_link_libraries(oneintwo_pairing_ui_tests
    PRIVATE oneintwo_tls oneintwo_session Qt6::Widgets)
target_compile_features(oneintwo_pairing_ui_tests PRIVATE cxx_std_17)
target_include_directories(oneintwo_pairing_ui_tests
    PRIVATE "${ONEINTWO_ROOT}/src" "${ONEINTWO_ROOT}/tests/session")
add_test(NAME oneintwo_pairing_ui COMMAND oneintwo_pairing_ui_tests)
set_tests_properties(oneintwo_pairing_ui PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
