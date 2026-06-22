foreach(required CLI_EXE INVALID_STDIN OUTPUT_WAV STDOUT_FILE STDERR_FILE EXPECTED_TEXT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

file(WRITE "${INVALID_STDIN}" "not a wav")
execute_process(
    COMMAND "${CLI_EXE}" --input - --output "${OUTPUT_WAV}" --effect denoiser --dry-run --allow-unsafe-pipe
    INPUT_FILE "${INVALID_STDIN}"
    OUTPUT_FILE "${STDOUT_FILE}"
    ERROR_FILE "${STDERR_FILE}"
    RESULT_VARIABLE result
)

if(result EQUAL 0)
    message(FATAL_ERROR "invalid stdin command unexpectedly succeeded")
endif()

file(READ "${STDERR_FILE}" stderr)
string(FIND "${stderr}" "${EXPECTED_TEXT}" match_index)
if(match_index EQUAL -1)
    message(FATAL_ERROR "stderr did not contain '${EXPECTED_TEXT}'")
endif()

