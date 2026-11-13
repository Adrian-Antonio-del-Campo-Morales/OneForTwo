if(NOT TARGET oneintwo_tls)
    message(FATAL_ERROR "oneintwo_tls debe existir antes de las pruebas TLS")
endif()

enable_testing()

add_executable(oneintwo_tls_transport_tests tls_transport_test.cpp)
target_link_libraries(oneintwo_tls_transport_tests
    PRIVATE oneintwo_tls oneintwo_session Qt6::Core Qt6::Network)
target_compile_features(oneintwo_tls_transport_tests PRIVATE cxx_std_17)
target_include_directories(oneintwo_tls_transport_tests
    PRIVATE "${ONEINTWO_ROOT}/tests/session")
add_test(NAME oneintwo_tls_transport COMMAND oneintwo_tls_transport_tests)
