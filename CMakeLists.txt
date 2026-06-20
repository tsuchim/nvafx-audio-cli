cmake_minimum_required(VERSION 3.20)

project(nvafx_audio_cli VERSION 0.1.0 LANGUAGES CXX)

add_executable(nvafx-audio-cli
    src/cli.cpp
    src/main.cpp
    src/sdk_discovery.cpp
    src/wav.cpp
)

target_compile_features(nvafx-audio-cli PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(nvafx-audio-cli PRIVATE /W4 /permissive-)
else()
    target_compile_options(nvafx-audio-cli PRIVATE -Wall -Wextra -pedantic)
endif()

include(CTest)

if(BUILD_TESTING)
    add_executable(wav-fixture tests/wav_fixture.cpp)
    target_compile_features(wav-fixture PRIVATE cxx_std_17)

    set(TEST_DATA_DIR "${CMAKE_CURRENT_BINARY_DIR}/testdata")
    set(CLI_EXE "$<TARGET_FILE:nvafx-audio-cli>")
    set(FIXTURE_EXE "$<TARGET_FILE:wav-fixture>")

    add_test(NAME fixture-valid-pcm16-48k-mono
        COMMAND "${FIXTURE_EXE}" "${TEST_DATA_DIR}/valid_pcm16_48k_mono.wav" 48000 1 pcm16)
    set_tests_properties(fixture-valid-pcm16-48k-mono PROPERTIES FIXTURES_SETUP wav_files)

    add_test(NAME fixture-unsupported-sample-rate
        COMMAND "${FIXTURE_EXE}" "${TEST_DATA_DIR}/unsupported_44100_mono.wav" 44100 1 pcm16)
    set_tests_properties(fixture-unsupported-sample-rate PROPERTIES FIXTURES_SETUP wav_files)

    add_test(NAME fixture-unsupported-channel-count
        COMMAND "${FIXTURE_EXE}" "${TEST_DATA_DIR}/unsupported_48000_3ch.wav" 48000 3 pcm16)
    set_tests_properties(fixture-unsupported-channel-count PROPERTIES FIXTURES_SETUP wav_files)

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

    add_test(NAME cli-processing-missing-required-options
        COMMAND "${CLI_EXE}" --input in.wav)
    set_tests_properties(cli-processing-missing-required-options PROPERTIES WILL_FAIL TRUE)

    add_test(NAME cli-sdk-root-alone-invalid
        COMMAND "${CLI_EXE}" --sdk-root "${TEST_DATA_DIR}")
    set_tests_properties(cli-sdk-root-alone-invalid PROPERTIES WILL_FAIL TRUE)

    add_test(NAME cli-dry-run-valid-pcm16-48k-mono
        COMMAND "${CLI_EXE}" --input "${TEST_DATA_DIR}/valid_pcm16_48k_mono.wav" --output "${TEST_DATA_DIR}/out.wav" --effect denoiser --sample-rate 48000 --intensity 1.0 --dry-run)
    set_tests_properties(cli-dry-run-valid-pcm16-48k-mono PROPERTIES FIXTURES_REQUIRED wav_files)

    add_test(NAME cli-unsupported-sample-rate
        COMMAND "${CLI_EXE}" --input "${TEST_DATA_DIR}/unsupported_44100_mono.wav" --output "${TEST_DATA_DIR}/out.wav" --effect denoiser --dry-run)
    set_tests_properties(cli-unsupported-sample-rate PROPERTIES FIXTURES_REQUIRED wav_files WILL_FAIL TRUE)

    add_test(NAME cli-unsupported-channel-count
        COMMAND "${CLI_EXE}" --input "${TEST_DATA_DIR}/unsupported_48000_3ch.wav" --output "${TEST_DATA_DIR}/out.wav" --effect denoiser --dry-run)
    set_tests_properties(cli-unsupported-channel-count PROPERTIES FIXTURES_REQUIRED wav_files WILL_FAIL TRUE)

    add_test(NAME cli-processing-not-implemented
        COMMAND "${CLI_EXE}" --input "${TEST_DATA_DIR}/valid_pcm16_48k_mono.wav" --output "${TEST_DATA_DIR}/out.wav" --effect denoiser)
    set_tests_properties(cli-processing-not-implemented PROPERTIES FIXTURES_REQUIRED wav_files WILL_FAIL TRUE)
endif()
