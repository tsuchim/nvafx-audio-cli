foreach(required CLI_EXE INPUT_WAV MODEL_PATH RUNTIME_ROOT STDOUT_FILE STDERR_FILE EXPECTED_TEXT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

file(REMOVE "${STDOUT_FILE}" "${STDERR_FILE}")
execute_process(
    COMMAND "${CLI_EXE}" --input "${INPUT_WAV}" --output - --effect denoiser --model "${MODEL_PATH}" --runtime-root "${RUNTIME_ROOT}" --allow-unsafe-pipe
    OUTPUT_FILE "${STDOUT_FILE}"
    ERROR_FILE "${STDERR_FILE}"
    RESULT_VARIABLE result
)

if(result EQUAL 0)
    message(FATAL_ERROR "SDK-disabled processing unexpectedly succeeded")
endif()

file(SIZE "${STDOUT_FILE}" stdout_size)
if(NOT stdout_size EQUAL 0)
    message(FATAL_ERROR "SDK-disabled processing wrote ${stdout_size} byte(s) to stdout")
endif()

file(READ "${STDERR_FILE}" stderr)
string(FIND "${stderr}" "${EXPECTED_TEXT}" match_index)
if(match_index EQUAL -1)
    message(FATAL_ERROR "stderr did not contain '${EXPECTED_TEXT}'")
endif()

