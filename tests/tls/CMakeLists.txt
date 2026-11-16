if(NOT TARGET oneintwo_tls)
    message(FATAL_ERROR "TLS tests require the oneintwo_tls target")
endif()

enable_testing()

add_executable(oneintwo_tls_transport_tests tls_transport_test.cpp)
target_link_libraries(oneintwo_tls_transport_tests
    PRIVATE oneintwo_tls oneintwo_session Qt6::Core Qt6::Network)
target_compile_features(oneintwo_tls_transport_tests PRIVATE cxx_std_17)
target_include_directories(oneintwo_tls_transport_tests
    PRIVATE "${ONEINTWO_ROOT}/tests/session")
add_test(NAME oneintwo_tls_transport COMMAND oneintwo_tls_transport_tests)

add_executable(oneintwo_pairing_tests
    pairing_test.cpp
    "${ONEINTWO_ROOT}/src/storage/pairing_store.cpp")
target_link_libraries(oneintwo_pairing_tests
    PRIVATE oneintwo_tls oneintwo_session Qt6::Core Qt6::Network)
target_compile_features(oneintwo_pairing_tests PRIVATE cxx_std_17)
target_include_directories(oneintwo_pairing_tests
    PRIVATE "${ONEINTWO_ROOT}/tests/session")
add_test(NAME oneintwo_pairing COMMAND oneintwo_pairing_tests)
