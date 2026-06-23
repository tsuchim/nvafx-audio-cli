foreach(required CLI_EXE MATCH_TEXT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

execute_process(
    COMMAND "${CLI_EXE}" --help
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "--help failed with exit code ${result}: ${stderr}")
endif()

string(FIND "${stdout}" "${MATCH_TEXT}" match_index)
if(match_index EQUAL -1)
    message(FATAL_ERROR "--help output did not contain '${MATCH_TEXT}'")
endif()

