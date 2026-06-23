foreach(required CLI_EXE INPUT_WAV STDOUT_FILE STDERR_FILE)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

file(REMOVE "${STDOUT_FILE}" "${STDERR_FILE}")
execute_process(
    COMMAND "${CLI_EXE}" --input "${INPUT_WAV}" --output - --effect denoiser --dry-run --allow-unsafe-pipe
    OUTPUT_FILE "${STDOUT_FILE}"
    ERROR_FILE "${STDERR_FILE}"
    RESULT_VARIABLE result
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "stdout dry-run failed with exit code ${result}")
endif()

file(SIZE "${STDOUT_FILE}" stdout_size)
if(NOT stdout_size EQUAL 0)
    message(FATAL_ERROR "dry-run wrote ${stdout_size} byte(s) to stdout")
endif()

