foreach(required CLI_EXE INPUT_WAV OUTPUT_WAV STDOUT_FILE STDERR_FILE)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

execute_process(
    COMMAND "${CLI_EXE}" --input - --output "${OUTPUT_WAV}" --effect denoiser --dry-run --allow-unsafe-pipe
    INPUT_FILE "${INPUT_WAV}"
    OUTPUT_FILE "${STDOUT_FILE}"
    ERROR_FILE "${STDERR_FILE}"
    RESULT_VARIABLE result
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "stdin dry-run failed with exit code ${result}")
endif()

