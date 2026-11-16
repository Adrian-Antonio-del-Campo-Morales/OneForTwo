if(NOT TARGET oneintwo_session)
    message(FATAL_ERROR "oneintwo_session debe existir antes de las pruebas de sesión")
endif()

enable_testing()

add_executable(oneintwo_protocol_tests protocol_test.cpp)
target_link_libraries(oneintwo_protocol_tests PRIVATE oneintwo_session)
target_compile_features(oneintwo_protocol_tests PRIVATE cxx_std_17)
add_test(NAME oneintwo_protocol COMMAND oneintwo_protocol_tests)

add_executable(oneintwo_session_link_tests session_link_test.cpp)
target_link_libraries(oneintwo_session_link_tests PRIVATE oneintwo_session)
target_compile_features(oneintwo_session_link_tests PRIVATE cxx_std_17)
add_test(NAME oneintwo_session_link COMMAND oneintwo_session_link_tests)
