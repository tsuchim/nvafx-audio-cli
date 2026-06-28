foreach(required PYTHON_EXECUTABLE SCRIPT_PATH TEST_DATA_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

set(helper_work_dir "${TEST_DATA_DIR}/build-linux-sdk-local-helper")
file(REMOVE_RECURSE "${helper_work_dir}")
file(MAKE_DIRECTORY "${helper_work_dir}")

execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" "${SCRIPT_PATH}" --help
    RESULT_VARIABLE help_result
    OUTPUT_VARIABLE help_output
    ERROR_VARIABLE help_error
)
if(NOT help_result EQUAL 0)
    message(FATAL_ERROR "--help failed with exit code ${help_result}: ${help_error}")
endif()
if(NOT help_output MATCHES "--sdk-root")
    message(FATAL_ERROR "--help output does not document --sdk-root")
endif()
if(NOT help_output MATCHES "Default:" OR NOT help_output MATCHES "nvafx-audio-cli")
    message(FATAL_ERROR "--help output does not document the default processing wrapper name")
endif()

execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" "${SCRIPT_PATH}" --sdk-root "${helper_work_dir}/missing-sdk" --dry-run
    RESULT_VARIABLE missing_result
    OUTPUT_VARIABLE missing_output
    ERROR_VARIABLE missing_error
)
if(missing_result EQUAL 0)
    message(FATAL_ERROR "missing SDK root unexpectedly succeeded")
endif()
if(NOT "${missing_output}${missing_error}" MATCHES "SDK root does not exist")
    message(FATAL_ERROR "missing SDK root error was not clear: ${missing_output}${missing_error}")
endif()

set(incomplete_sdk "${helper_work_dir}/incomplete-sdk")
file(MAKE_DIRECTORY "${incomplete_sdk}")
execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" "${SCRIPT_PATH}" --sdk-root "${incomplete_sdk}" --dry-run
    RESULT_VARIABLE incomplete_result
    OUTPUT_VARIABLE incomplete_output
    ERROR_VARIABLE incomplete_error
)
if(incomplete_result EQUAL 0)
    message(FATAL_ERROR "incomplete SDK root unexpectedly succeeded")
endif()
if(NOT "${incomplete_output}${incomplete_error}" MATCHES "Missing NVIDIA AFX header")
    message(FATAL_ERROR "incomplete SDK error was not clear: ${incomplete_output}${incomplete_error}")
endif()

set(fake_sdk "${helper_work_dir}/fake-sdk")
set(fake_model "${fake_sdk}/features/denoiser/models/sm_89/denoiser_48k.trtpkg")
file(MAKE_DIRECTORY
    "${fake_sdk}/nvafx/include"
    "${fake_sdk}/nvafx/lib"
    "${fake_sdk}/features/denoiser/lib"
    "${fake_sdk}/features/denoiser/models/sm_89")
file(WRITE "${fake_sdk}/nvafx/include/nvAudioEffects.h" "/* fake header for dry-run validation */\n")
file(WRITE "${fake_sdk}/nvafx/lib/libnv_audiofx.so" "")
file(WRITE "${fake_sdk}/features/denoiser/lib/libnv_audiofx_denoiser.so" "")
file(WRITE "${fake_model}" "")

set(dry_build_dir "${helper_work_dir}/dry-build")
set(dry_install_dir "${helper_work_dir}/dry-install")
file(REMOVE_RECURSE "${dry_build_dir}" "${dry_install_dir}")
execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" "${SCRIPT_PATH}"
        --sdk-root "${fake_sdk}"
        --model "${fake_model}"
        --build-dir "${dry_build_dir}"
        --install-prefix "${dry_install_dir}"
        --dry-run
    RESULT_VARIABLE dry_result
    OUTPUT_VARIABLE dry_output
    ERROR_VARIABLE dry_error
)
if(NOT dry_result EQUAL 0)
    message(FATAL_ERROR "--dry-run failed with exit code ${dry_result}: ${dry_output}${dry_error}")
endif()
if(NOT dry_output MATCHES "Dry run")
    message(FATAL_ERROR "--dry-run output did not report dry-run mode")
endif()
if(EXISTS "${dry_build_dir}" OR EXISTS "${dry_install_dir}")
    message(FATAL_ERROR "--dry-run created build or install directories")
endif()

file(READ "${SCRIPT_PATH}" script_text)
string(TOLOWER "${script_text}" script_text_lower)
string(FIND "${script_text_lower}" "ngc" ngc_position)
string(FIND "${script_text_lower}" "api_key" api_key_position)
if(NOT ngc_position EQUAL -1 OR NOT api_key_position EQUAL -1)
    message(FATAL_ERROR "helper script must not mention credential-specific SDK acquisition terms")
endif()

set(versioned_sdk "${helper_work_dir}/versioned-sdk")
set(versioned_model "${versioned_sdk}/features/denoiser/models/sm_89/denoiser_48k.trtpkg")
file(MAKE_DIRECTORY
    "${versioned_sdk}/nvafx/include"
    "${versioned_sdk}/nvafx/lib"
    "${versioned_sdk}/features/denoiser/lib"
    "${versioned_sdk}/features/denoiser/models/sm_89")
file(WRITE "${versioned_sdk}/nvafx/include/nvAudioEffects.h" "/* fake header for versioned dry-run validation */\n")
file(WRITE "${versioned_sdk}/nvafx/lib/libnv_audiofx.so.2.1.0" "")
file(WRITE "${versioned_sdk}/features/denoiser/lib/libnv_audiofx_denoiser.so.2.1.0" "")
file(WRITE "${versioned_model}" "")

execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" "${SCRIPT_PATH}"
        --sdk-root "${versioned_sdk}"
        --model "${versioned_model}"
        --build-dir "${helper_work_dir}/versioned-dry-build"
        --dry-run
    RESULT_VARIABLE versioned_result
    OUTPUT_VARIABLE versioned_output
    ERROR_VARIABLE versioned_error
)
if(NOT versioned_result EQUAL 0)
    message(FATAL_ERROR "versioned .so SDK dry-run failed with exit code ${versioned_result}: ${versioned_output}${versioned_error}")
endif()
