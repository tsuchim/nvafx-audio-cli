foreach(required WAV_STDOUT_EXE INSPECT_EXE INPUT_WAV OUTPUT_WAV STDERR_FILE)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

execute_process(
    COMMAND "${WAV_STDOUT_EXE}" "${INPUT_WAV}"
    OUTPUT_FILE "${OUTPUT_WAV}"
    ERROR_FILE "${STDERR_FILE}"
    RESULT_VARIABLE stream_result
)

if(NOT stream_result EQUAL 0)
    message(FATAL_ERROR "wav stdout helper failed with exit code ${stream_result}")
endif()

execute_process(
    COMMAND "${INSPECT_EXE}" "${OUTPUT_WAV}" 48000 1 4
    RESULT_VARIABLE inspect_result
)

if(NOT inspect_result EQUAL 0)
    message(FATAL_ERROR "wav inspect failed with exit code ${inspect_result}")
endif()

