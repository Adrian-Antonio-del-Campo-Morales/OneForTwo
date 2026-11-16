if(NOT TARGET oneintwo_app_core)
    message(FATAL_ERROR "oneintwo_app_core debe existir antes de la prueba de Application")
endif()

get_filename_component(ONEINTWO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
enable_testing()

# Compila application.cpp en un ejecutable real y enlaza la UI/emparejamiento
# del núcleo. Los dobles de plataforma y avisos están en application_test.cpp.
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
