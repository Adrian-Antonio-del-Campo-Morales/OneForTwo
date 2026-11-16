if(NOT DEFINED PROBE)
    message(FATAL_ERROR "PROBE executable was not provided")
endif()

set(case_names missing-dll missing-entry-point unsupported)
set(expected_codes 2 3 0)
foreach(case_name expected_code IN ZIP_LISTS case_names expected_codes)
    execute_process(
        COMMAND "${PROBE}" "${case_name}"
        RESULT_VARIABLE actual_code
        OUTPUT_VARIABLE probe_stdout
        ERROR_VARIABLE probe_stderr)

    if(NOT actual_code MATCHES "^[0-9]+$")
        message(FATAL_ERROR
            "WGC probe case '${case_name}' could not be executed: '${actual_code}'. "
            "stdout: ${probe_stdout} stderr: ${probe_stderr}")
    endif()
    if(NOT actual_code EQUAL expected_code)
        message(FATAL_ERROR
            "WGC probe case '${case_name}' exited ${actual_code}, expected ${expected_code}. "
            "stdout: ${probe_stdout} stderr: ${probe_stderr}")
    endif()

    message(STATUS "WGC probe case '${case_name}' exited ${actual_code} as expected")
endforeach()
