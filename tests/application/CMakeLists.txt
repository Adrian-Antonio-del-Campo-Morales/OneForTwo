if(NOT TARGET oneintwo_app_core)
    message(FATAL_ERROR "The Application test requires the oneintwo_app_core target")
endif()

get_filename_component(ONEINTWO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
enable_testing()

# Build application.cpp into the test executable and link the core UI and pairing code.
# application_test.cpp supplies the platform and notification doubles.
add_executable(oneintwo_application_tests
    application_test.cpp
    "${ONEINTWO_ROOT}/src/app/application.cpp")
target_link_libraries(oneintwo_application_tests
    PRIVATE oneintwo_app_core Qt6::Widgets)
target_compile_features(oneintwo_application_tests PRIVATE cxx_std_17)
target_include_directories(oneintwo_application_tests
    PRIVATE "${ONEINTWO_ROOT}/src" "${ONEINTWO_ROOT}/tests/session")
add_test(NAME oneintwo_application COMMAND oneintwo_application_tests)
set_tests_properties(oneintwo_application PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
