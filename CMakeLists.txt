cmake_minimum_required(VERSION 3.20)

project(nvafx_audio_cli VERSION 0.1.0 LANGUAGES CXX)

option(NVAFX_ENABLE_SDK "Enable NVIDIA Audio Effects SDK integration" OFF)
set(NVAFX_API_ROOT "" CACHE PATH "Path to the external Maxine AFX SDK API root containing include/ and lib/")
set(NVAFX_RUNTIME_ROOT "" CACHE PATH "Path to the installed NVIDIA Audio Effects SDK runtime root")

if(NVAFX_ENABLE_SDK)
    if(NOT NVAFX_API_ROOT AND DEFINED ENV{NVAFX_API_ROOT})
        set(NVAFX_API_ROOT "$ENV{NVAFX_API_ROOT}" CACHE PATH "Path to the external Maxine AFX SDK API root containing include/ and lib/" FORCE)
    endif()

    if(NOT NVAFX_RUNTIME_ROOT AND DEFINED ENV{NVAFX_RUNTIME_ROOT})
        set(NVAFX_RUNTIME_ROOT "$ENV{NVAFX_RUNTIME_ROOT}" CACHE PATH "Path to the installed NVIDIA Audio Effects SDK runtime root" FORCE)
    elseif(NOT NVAFX_RUNTIME_ROOT AND DEFINED ENV{AFX_SDK_ROOT})
        set(NVAFX_RUNTIME_ROOT "$ENV{AFX_SDK_ROOT}" CACHE PATH "Path to the installed NVIDIA Audio Effects SDK runtime root" FORCE)
    endif()

    if(NOT NVAFX_API_ROOT)
        message(FATAL_ERROR "NVAFX_ENABLE_SDK=ON requires NVAFX_API_ROOT or environment variable NVAFX_API_ROOT")
    endif()

    if(NOT NVAFX_RUNTIME_ROOT)
        message(FATAL_ERROR "NVAFX_ENABLE_SDK=ON requires NVAFX_RUNTIME_ROOT, environment variable NVAFX_RUNTIME_ROOT, or AFX_SDK_ROOT")
    endif()

    set(NVAFX_HEADER "${NVAFX_API_ROOT}/include/nvAudioEffects.h")
    set(NVAFX_IMPORT_LIB "${NVAFX_API_ROOT}/lib/NVAudioEffects.lib")
    set(NVAFX_RUNTIME_DLL "${NVAFX_RUNTIME_ROOT}/NVAudioEffects.dll")

    if(NOT EXISTS "${NVAFX_HEADER}")
        message(FATAL_ERROR "NVIDIA AFX header not found: ${NVAFX_HEADER}")
    endif()

    if(NOT EXISTS "${NVAFX_IMPORT_LIB}")
        message(FATAL_ERROR "NVIDIA AFX import library not found: ${NVAFX_IMPORT_LIB}")
    endif()

    if(NOT EXISTS "${NVAFX_RUNTIME_DLL}")
        message(FATAL_ERROR "NVIDIA AFX runtime DLL not found: ${NVAFX_RUNTIME_DLL}")
    endif()
endif()

add_executable(nvafx-audio-cli
    src/afx_sdk.cpp
    src/cli.cpp
    src/main.cpp
    src/sdk_discovery.cpp
    src/wav.cpp
)

target_compile_features(nvafx-audio-cli PRIVATE cxx_std_17)

if(NVAFX_ENABLE_SDK)
    target_compile_definitions(nvafx-audio-cli PRIVATE NVAFX_WITH_SDK)
    target_include_directories(nvafx-audio-cli PRIVATE "${NVAFX_API_ROOT}/include")
    target_link_libraries(nvafx-audio-cli PRIVATE "${NVAFX_IMPORT_LIB}")
    if(MSVC)
        target_link_options(nvafx-audio-cli PRIVATE /DELAYLOAD:NVAudioEffects.dll)
        target_link_libraries(nvafx-audio-cli PRIVATE delayimp)
    endif()
endif()

if(MSVC)
    target_compile_options(nvafx-audio-cli PRIVATE /W4 /permissive-)
else()
    target_compile_options(nvafx-audio-cli PRIVATE -Wall -Wextra -pedantic)
endif()

include(CTest)

if(BUILD_TESTING)
    add_executable(wav-fixture tests/wav_fixture.cpp)
    target_compile_features(wav-fixture PRIVATE cxx_std_17)

    add_executable(wav-roundtrip tests/wav_roundtrip.cpp src/wav.cpp)
    target_compile_features(wav-roundtrip PRIVATE cxx_std_17)
    target_include_directories(wav-roundtrip PRIVATE src)

    add_executable(wav-inspect tests/wav_inspect.cpp src/wav.cpp)
    target_compile_features(wav-inspect PRIVATE cxx_std_17)
    target_include_directories(wav-inspect PRIVATE src)

    set(TEST_DATA_DIR "${CMAKE_CURRENT_BINARY_DIR}/testdata")
    set(CLI_EXE "$<TARGET_FILE:nvafx-audio-cli>")
    set(FIXTURE_EXE "$<TARGET_FILE:wav-fixture>")
    set(ROUNDTRIP_EXE "$<TARGET_FILE:wav-roundtrip>")

    add_test(NAME fixture-valid-pcm16-48k-mono
        COMMAND "${FIXTURE_EXE}" "${TEST_DATA_DIR}/valid_pcm16_48k_mono.wav" 48000 1 pcm16)
    set_tests_properties(fixture-valid-pcm16-48k-mono PROPERTIES FIXTURES_SETUP wav_files)

    add_test(NAME fixture-unsupported-sample-rate
        COMMAND "${FIXTURE_EXE}" "${TEST_DATA_DIR}/unsupported_44100_mono.wav" 44100 1 pcm16)
    set_tests_properties(fixture-unsupported-sample-rate PROPERTIES FIXTURES_SETUP wav_files)

    add_test(NAME fixture-unsupported-channel-count
        COMMAND "${FIXTURE_EXE}" "${TEST_DATA_DIR}/unsupported_48000_3ch.wav" 48000 3 pcm16)
    set_tests_properties(fixture-unsupported-channel-count PROPERTIES FIXTURES_SETUP wav_files)

    add_test(NAME fixture-extensible-pcm16-48k-mono
        COMMAND "${FIXTURE_EXE}" "${TEST_DATA_DIR}/extensible_pcm16_48k_mono.wav" 48000 1 extensible-pcm16)
    set_tests_properties(fixture-extensible-pcm16-48k-mono PROPERTIES FIXTURES_SETUP wav_files)

    add_test(NAME fixture-extensible-float32-48k-stereo
        COMMAND "${FIXTURE_EXE}" "${TEST_DATA_DIR}/extensible_float32_48k_stereo.wav" 48000 2 extensible-float32)
    set_tests_properties(fixture-extensible-float32-48k-stereo PROPERTIES FIXTURES_SETUP wav_files)

    add_test(NAME fixture-truncated-wav
        COMMAND "${FIXTURE_EXE}" "${TEST_DATA_DIR}/truncated.wav" 48000 1 truncated)
    set_tests_properties(fixture-truncated-wav PROPERTIES FIXTURES_SETUP wav_files)

    add_test(NAME fixture-valid-pcm16-16k-mono
        COMMAND "${FIXTURE_EXE}" "${TEST_DATA_DIR}/valid_pcm16_16k_mono.wav" 16000 1 pcm16)
    set_tests_properties(fixture-valid-pcm16-16k-mono PROPERTIES FIXTURES_SETUP wav_files)

    add_test(NAME fixture-dummy-model
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -Command
            "New-Item -ItemType Directory -Force -Path '${TEST_DATA_DIR}' | Out-Null; Set-Content -LiteralPath '${TEST_DATA_DIR}/dummy_model.trtpkg' -Value 'not a real model'")
    set_tests_properties(fixture-dummy-model PROPERTIES FIXTURES_SETUP wav_files)

    add_test(NAME cli-help COMMAND "${CLI_EXE}" --help)
    add_test(NAME cli-version COMMAND "${CLI_EXE}" --version)

    add_test(NAME cli-invalid-effect COMMAND "${CLI_EXE}" --effect invalid)
    set_tests_properties(cli-invalid-effect PROPERTIES WILL_FAIL TRUE)

    add_test(NAME cli-invalid-intensity-nan
        COMMAND "${CLI_EXE}" --input in.wav --output out.wav --effect denoiser --intensity nan --dry-run)
    set_tests_properties(cli-invalid-intensity-nan PROPERTIES WILL_FAIL TRUE)

    add_test(NAME cli-invalid-intensity-inf
        COMMAND "${CLI_EXE}" --input in.wav --output out.wav --effect denoiser --intensity inf --dry-run)
    set_tests_properties(cli-invalid-intensity-inf PROPERTIES WILL_FAIL TRUE)

    add_test(NAME cli-invalid-intensity-text
        COMMAND "${CLI_EXE}" --input in.wav --output out.wav --effect denoiser --intensity abc --dry-run)
    set_tests_properties(cli-invalid-intensity-text PROPERTIES WILL_FAIL TRUE)

    add_test(NAME cli-invalid-sample-rate-text
        COMMAND "${CLI_EXE}" --input in.wav --output out.wav --effect denoiser --sample-rate abc --dry-run)
    set_tests_properties(cli-invalid-sample-rate-text PROPERTIES WILL_FAIL TRUE)

    add_test(NAME cli-processing-missing-required-options
        COMMAND "${CLI_EXE}" --input in.wav)
    set_tests_properties(cli-processing-missing-required-options PROPERTIES WILL_FAIL TRUE)

    add_test(NAME cli-sdk-root-alone-invalid
        COMMAND "${CLI_EXE}" --sdk-root "${TEST_DATA_DIR}")
    set_tests_properties(cli-sdk-root-alone-invalid PROPERTIES WILL_FAIL TRUE)

    add_test(NAME cli-model-dry-run-parsing
        COMMAND "${CLI_EXE}" --input "${TEST_DATA_DIR}/valid_pcm16_48k_mono.wav" --output "${TEST_DATA_DIR}/model_parse_out.wav" --effect denoiser --model "${TEST_DATA_DIR}/dummy_model.trtpkg" --dry-run)
    set_tests_properties(cli-model-dry-run-parsing PROPERTIES FIXTURES_REQUIRED wav_files)

    add_test(NAME cli-dry-run-does-not-create-output
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -Command
            "Remove-Item -LiteralPath '${TEST_DATA_DIR}/dry_run_should_not_exist.wav' -ErrorAction SilentlyContinue; & '${CLI_EXE}' --input '${TEST_DATA_DIR}/valid_pcm16_48k_mono.wav' --output '${TEST_DATA_DIR}/dry_run_should_not_exist.wav' --effect denoiser --model '${TEST_DATA_DIR}/dummy_model.trtpkg' --dry-run; if (Test-Path -LiteralPath '${TEST_DATA_DIR}/dry_run_should_not_exist.wav') { exit 1 }")
    set_tests_properties(cli-dry-run-does-not-create-output PROPERTIES FIXTURES_REQUIRED wav_files)

    add_test(NAME cli-dry-run-valid-pcm16-48k-mono
        COMMAND "${CLI_EXE}" --input "${TEST_DATA_DIR}/valid_pcm16_48k_mono.wav" --output "${TEST_DATA_DIR}/out.wav" --effect denoiser --sample-rate 48000 --intensity 1.0 --dry-run)
    set_tests_properties(cli-dry-run-valid-pcm16-48k-mono PROPERTIES FIXTURES_REQUIRED wav_files)

    add_test(NAME cli-dry-run-extensible-pcm16-48k-mono
        COMMAND "${CLI_EXE}" --input "${TEST_DATA_DIR}/extensible_pcm16_48k_mono.wav" --output "${TEST_DATA_DIR}/out.wav" --effect denoiser --sample-rate 48000 --intensity 1.0 --dry-run)
    set_tests_properties(cli-dry-run-extensible-pcm16-48k-mono PROPERTIES FIXTURES_REQUIRED wav_files)

    add_test(NAME cli-dry-run-extensible-float32-48k-stereo
        COMMAND "${CLI_EXE}" --input "${TEST_DATA_DIR}/extensible_float32_48k_stereo.wav" --output "${TEST_DATA_DIR}/out.wav" --effect dereverb --sample-rate 48000 --intensity 0.5 --dry-run)
    set_tests_properties(cli-dry-run-extensible-float32-48k-stereo PROPERTIES FIXTURES_REQUIRED wav_files)

    add_test(NAME cli-unsupported-sample-rate
        COMMAND "${CLI_EXE}" --input "${TEST_DATA_DIR}/unsupported_44100_mono.wav" --output "${TEST_DATA_DIR}/out.wav" --effect denoiser --dry-run)
    set_tests_properties(cli-unsupported-sample-rate PROPERTIES FIXTURES_REQUIRED wav_files WILL_FAIL TRUE)

    add_test(NAME cli-unsupported-channel-count
        COMMAND "${CLI_EXE}" --input "${TEST_DATA_DIR}/unsupported_48000_3ch.wav" --output "${TEST_DATA_DIR}/out.wav" --effect denoiser --dry-run)
    set_tests_properties(cli-unsupported-channel-count PROPERTIES FIXTURES_REQUIRED wav_files WILL_FAIL TRUE)

    add_test(NAME cli-truncated-wav
        COMMAND "${CLI_EXE}" --input "${TEST_DATA_DIR}/truncated.wav" --output "${TEST_DATA_DIR}/out.wav" --effect denoiser --dry-run)
    set_tests_properties(cli-truncated-wav PROPERTIES FIXTURES_REQUIRED wav_files WILL_FAIL TRUE)

    add_test(NAME wav-writer-roundtrip
        COMMAND "${ROUNDTRIP_EXE}" "${TEST_DATA_DIR}/extensible_float32_48k_stereo.wav" "${TEST_DATA_DIR}/roundtrip.wav")
    set_tests_properties(wav-writer-roundtrip PROPERTIES FIXTURES_REQUIRED wav_files)

    add_test(NAME cli-processing-missing-model
        COMMAND "${CLI_EXE}" --input "${TEST_DATA_DIR}/valid_pcm16_48k_mono.wav" --output "${TEST_DATA_DIR}/out.wav" --effect denoiser)
    set_tests_properties(cli-processing-missing-model PROPERTIES FIXTURES_REQUIRED wav_files WILL_FAIL TRUE)

    add_test(NAME cli-sdk-disabled-processing-fails
        COMMAND "${CLI_EXE}" --input "${TEST_DATA_DIR}/valid_pcm16_48k_mono.wav" --output "${TEST_DATA_DIR}/disabled_out.wav" --effect denoiser --model "${TEST_DATA_DIR}/dummy_model.trtpkg" --runtime-root "${TEST_DATA_DIR}")
    set_tests_properties(cli-sdk-disabled-processing-fails PROPERTIES FIXTURES_REQUIRED wav_files WILL_FAIL TRUE)

    add_test(NAME cli-check-sdk-missing-roots
        COMMAND "${CLI_EXE}" --check-sdk --api-root "${TEST_DATA_DIR}/missing_api_root" --runtime-root "${TEST_DATA_DIR}/missing_runtime_root")
    set_tests_properties(cli-check-sdk-missing-roots PROPERTIES WILL_FAIL TRUE)

    add_test(NAME cli-check-sdk-fake-tree
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -Command
            "New-Item -ItemType Directory -Force -Path '${TEST_DATA_DIR}/fake_api/include', '${TEST_DATA_DIR}/fake_api/lib', '${TEST_DATA_DIR}/fake_runtime/models' | Out-Null; Set-Content -LiteralPath '${TEST_DATA_DIR}/fake_api/include/nvAudioEffects.h' -Value ''; Set-Content -LiteralPath '${TEST_DATA_DIR}/fake_api/lib/NVAudioEffects.lib' -Value ''; Set-Content -LiteralPath '${TEST_DATA_DIR}/fake_runtime/NVAudioEffects.dll' -Value ''; Set-Content -LiteralPath '${TEST_DATA_DIR}/fake_runtime/models/denoiser_48k.trtpkg' -Value ''; Set-Content -LiteralPath '${TEST_DATA_DIR}/fake_runtime/models/dereverb_48k.trtpkg' -Value ''; Set-Content -LiteralPath '${TEST_DATA_DIR}/fake_runtime/models/dereverb_denoiser_48k.trtpkg' -Value ''; & '${CLI_EXE}' --check-sdk --api-root '${TEST_DATA_DIR}/fake_api' --runtime-root '${TEST_DATA_DIR}/fake_runtime'")

    add_test(NAME repo-hygiene
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_repo_hygiene.ps1")
    set_tests_properties(repo-hygiene PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
