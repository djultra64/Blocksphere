execute_process(
    COMMAND "${TEST_EXECUTABLE}" "${CMAKE_CURRENT_BINARY_DIR}/missing-tetrisphere.z64"
    RESULT_VARIABLE result
    ERROR_VARIABLE stderr
    OUTPUT_VARIABLE stdout)
if(result EQUAL 0)
    message(FATAL_ERROR "missing ROM was accepted")
endif()
if(NOT stderr MATCHES "fatal_rom_validation.*io_error.*hint")
    message(FATAL_ERROR "ROM error lacks an actionable hint: ${stderr}")
endif()
