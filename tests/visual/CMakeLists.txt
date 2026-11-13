if(NOT TARGET oneintwo_visual)
    message(FATAL_ERROR "oneintwo_visual debe existir antes de las pruebas visuales")
endif()

get_filename_component(ONEINTWO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets)
enable_testing()

add_executable(oneintwo_visual_share_tests visual_share_test.cpp)
target_link_libraries(oneintwo_visual_share_tests PRIVATE oneintwo_visual Qt6::Gui)
target_compile_features(oneintwo_visual_share_tests PRIVATE cxx_std_17)
add_test(NAME oneintwo_visual_share COMMAND oneintwo_visual_share_tests)
set_tests_properties(oneintwo_visual_share PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(oneintwo_visual_codec_tests visual_codec_test.cpp)
target_link_libraries(oneintwo_visual_codec_tests PRIVATE oneintwo_visual Qt6::Gui)
target_compile_features(oneintwo_visual_codec_tests PRIVATE cxx_std_17)
add_test(NAME oneintwo_visual_codec COMMAND oneintwo_visual_codec_tests)
set_tests_properties(oneintwo_visual_codec PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(oneintwo_visual_viewer_tests visual_viewer_test.cpp)
target_link_libraries(oneintwo_visual_viewer_tests PRIVATE oneintwo_visual Qt6::Widgets)
target_compile_features(oneintwo_visual_viewer_tests PRIVATE cxx_std_17)
add_test(NAME oneintwo_visual_viewer COMMAND oneintwo_visual_viewer_tests)
set_tests_properties(oneintwo_visual_viewer PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Arnes de composicion: UI real + coordinador real, con dobles de sesion,
# captura, visor y modulo de avisos. No enlaza los paquetes de V ni N.
add_executable(oneintwo_composition_harness
    composition_harness.cpp
    "${ONEINTWO_ROOT}/src/app/coordinator.cpp"
    "${ONEINTWO_ROOT}/src/ui/main_window.cpp"
)
target_link_libraries(oneintwo_composition_harness
    PRIVATE oneintwo_visual Qt6::Widgets)
target_compile_features(oneintwo_composition_harness PRIVATE cxx_std_17)
target_include_directories(oneintwo_composition_harness PRIVATE "${ONEINTWO_ROOT}/src")
add_test(NAME oneintwo_composition_harness COMMAND oneintwo_composition_harness)
set_tests_properties(oneintwo_composition_harness PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
