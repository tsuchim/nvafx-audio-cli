foreach(required CLI_EXE FIXTURE_EXE INSPECT_EXE TEST_DATA_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

if(NOT DEFINED ENV{NVAFX_LINUX_SDK_ROOT} OR
   NOT DEFINED ENV{NVAFX_LINUX_FEATURE_ROOT} OR
   NOT DEFINED ENV{NVAFX_TEST_MODEL})
    message("Skipping manual Linux SDK processing test: set NVAFX_LINUX_SDK_ROOT, NVAFX_LINUX_FEATURE_ROOT, and NVAFX_TEST_MODEL.")
    return()
endif()

set(sdk_root "$ENV{NVAFX_LINUX_SDK_ROOT}")
set(feature_root "$ENV{NVAFX_LINUX_FEATURE_ROOT}")
set(model_path "$ENV{NVAFX_TEST_MODEL}")
set(api_root "${sdk_root}/nvafx")
set(work_dir "${TEST_DATA_DIR}/manual-linux-sdk")
set(input_wav "${work_dir}/input.wav")
set(output_wav "${work_dir}/output.wav")

file(MAKE_DIRECTORY "${work_dir}")

foreach(path sdk_root feature_root model_path)
    if(NOT EXISTS "${${path}}")
        message(FATAL_ERROR "${path} does not exist: ${${path}}")
    endif()
endforeach()

execute_process(
    COMMAND "${CLI_EXE}" --check-sdk --api-root "${api_root}" --runtime-root "${sdk_root}"
    RESULT_VARIABLE check_result
)
if(NOT check_result EQUAL 0)
    message(FATAL_ERROR "--check-sdk failed with exit code ${check_result}")
endif()

execute_process(
    COMMAND "${FIXTURE_EXE}" "${input_wav}" 48000 1 pcm16
    RESULT_VARIABLE fixture_result
)
if(NOT fixture_result EQUAL 0)
    message(FATAL_ERROR "fixture generation failed with exit code ${fixture_result}")
endif()

execute_process(
    COMMAND "${CLI_EXE}"
        --input "${input_wav}"
        --output "${output_wav}"
        --effect denoiser
        --sample-rate 48000
        --model "${model_path}"
        --runtime-root "${sdk_root}"
    RESULT_VARIABLE process_result
)
if(NOT process_result EQUAL 0)
    message(FATAL_ERROR "SDK processing failed with exit code ${process_result}")
endif()

if(NOT EXISTS "${output_wav}")
    message(FATAL_ERROR "SDK processing did not create output WAV")
endif()

execute_process(
    COMMAND "${INSPECT_EXE}" "${output_wav}" 48000 1 4
    RESULT_VARIABLE inspect_result
)
if(NOT inspect_result EQUAL 0)
    message(FATAL_ERROR "output WAV inspection failed with exit code ${inspect_result}")
endif()
