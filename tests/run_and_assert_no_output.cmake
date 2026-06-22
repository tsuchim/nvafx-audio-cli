foreach(required CLI_EXE INPUT_WAV OUTPUT_WAV MODEL_PATH)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

file(REMOVE "${OUTPUT_WAV}")
execute_process(
    COMMAND "${CLI_EXE}" --input "${INPUT_WAV}" --output "${OUTPUT_WAV}" --effect denoiser --model "${MODEL_PATH}" --dry-run
    RESULT_VARIABLE result
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "dry-run command failed with exit code ${result}")
endif()

if(EXISTS "${OUTPUT_WAV}")
    message(FATAL_ERROR "dry-run created unexpected output: ${OUTPUT_WAV}")
endif()

