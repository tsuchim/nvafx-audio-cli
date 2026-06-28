foreach(required CLI_EXE TEST_DATA_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

set(api_root "${TEST_DATA_DIR}/fake_api")
set(runtime_root "${TEST_DATA_DIR}/fake_runtime")

file(MAKE_DIRECTORY
    "${api_root}/include"
    "${api_root}/lib"
    "${runtime_root}/models"
)
file(WRITE "${api_root}/include/nvAudioEffects.h" "")
file(WRITE "${api_root}/lib/NVAudioEffects.lib" "")
file(WRITE "${runtime_root}/NVAudioEffects.dll" "")
file(WRITE "${runtime_root}/models/denoiser_48k.trtpkg" "")
file(WRITE "${runtime_root}/models/dereverb_48k.trtpkg" "")
file(WRITE "${runtime_root}/models/dereverb_denoiser_48k.trtpkg" "")

execute_process(
    COMMAND "${CLI_EXE}" --check-sdk --api-root "${api_root}" --runtime-root "${runtime_root}"
        --model "${runtime_root}/models/denoiser_48k.trtpkg"
    RESULT_VARIABLE result
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "fake SDK tree check failed with exit code ${result}")
endif()
