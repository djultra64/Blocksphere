if(NOT DEFINED TEST_EXECUTABLE OR NOT DEFINED TEST_ROM)
    message(FATAL_ERROR "runtime boot assertion is missing an input")
endif()

execute_process(
    COMMAND "${TEST_EXECUTABLE}" "${TEST_ROM}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    TIMEOUT 3
)
if(NOT "${result}" MATCHES "[Tt]imeout")
    message(FATAL_ERROR "expected expanded diagnostic to remain active, got ${result}; stdout=${stdout}; stderr=${stderr}")
endif()
if(NOT stdout MATCHES "rom_validated" OR NOT stdout MATCHES "diagnostic_start")
    message(FATAL_ERROR "diagnostic did not enter validated generated code: ${stdout}")
endif()
if(stderr MATCHES "fatal_unsupported")
    message(FATAL_ERROR "expanded diagnostic regressed to an unsupported boot route: ${stderr}")
endif()
