if(NOT DEFINED TEST_EXECUTABLE OR NOT DEFINED TEST_MODE OR NOT DEFINED EXPECTED_PATTERN)
    message(FATAL_ERROR "runtime fatal assertion is missing an input")
endif()

if(DEFINED TEST_ARGUMENT)
    execute_process(
        COMMAND "${TEST_EXECUTABLE}" "${TEST_MODE}" "${TEST_ARGUMENT}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
    )
else()
    execute_process(
        COMMAND "${TEST_EXECUTABLE}" "${TEST_MODE}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
    )
endif()
if(NOT result EQUAL 71)
    message(FATAL_ERROR "expected exit 71, got ${result}; stdout=${stdout}; stderr=${stderr}")
endif()
if(NOT stderr MATCHES "${EXPECTED_PATTERN}")
    message(FATAL_ERROR "fatal diagnostic did not match: ${stderr}")
endif()
if(DEFINED EXPECTED_JSON_LINES)
    string(REGEX MATCHALL "\\{[^\n]*\\}" json_lines "${stderr}")
    list(LENGTH json_lines json_line_count)
    if(NOT json_line_count EQUAL EXPECTED_JSON_LINES)
        message(FATAL_ERROR "expected ${EXPECTED_JSON_LINES} JSON line(s), got ${json_line_count}: ${stderr}")
    endif()
endif()
