cmake_minimum_required(VERSION 3.20)

project(nvafx_audio_cli VERSION 0.3.0 LANGUAGES CXX)

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

    if(NOT NVAFX_API_ROOT AND NVAFX_RUNTIME_ROOT AND EXISTS "${NVAFX_RUNTIME_ROOT}/nvafx/include/nvAudioEffects.h")
        set(NVAFX_API_ROOT "${NVAFX_RUNTIME_ROOT}/nvafx" CACHE PATH "Path to the external Maxine AFX SDK API root containing include/ and lib/" FORCE)
    endif()

    if(NOT NVAFX_API_ROOT)
        message(FATAL_ERROR "NVAFX_ENABLE_SDK=ON requires NVAFX_API_ROOT or environment variable NVAFX_API_ROOT")
    endif()

    if(NOT NVAFX_RUNTIME_ROOT)
        message(FATAL_ERROR "NVAFX_ENABLE_SDK=ON requires NVAFX_RUNTIME_ROOT, environment variable NVAFX_RUNTIME_ROOT, or AFX_SDK_ROOT")
    endif()

    find_path(NVAFX_INCLUDE_DIR
        NAMES nvAudioEffects.h
        HINTS
            "${NVAFX_API_ROOT}/include"
            "${NVAFX_API_ROOT}/nvafx/include"
            "${NVAFX_RUNTIME_ROOT}/nvafx/include"
        NO_DEFAULT_PATH)

    if(NOT NVAFX_INCLUDE_DIR)
        message(FATAL_ERROR "NVIDIA AFX header not found under NVAFX_API_ROOT=${NVAFX_API_ROOT} or NVAFX_RUNTIME_ROOT=${NVAFX_RUNTIME_ROOT}")
    endif()

    if(WIN32)
        set(NVAFX_IMPORT_LIB "${NVAFX_API_ROOT}/lib/NVAudioEffects.lib")
        set(NVAFX_RUNTIME_DLL "${NVAFX_RUNTIME_ROOT}/NVAudioEffects.dll")

        if(NOT EXISTS "${NVAFX_IMPORT_LIB}")
            message(FATAL_ERROR "NVIDIA AFX import library not found: ${NVAFX_IMPORT_LIB}")
        endif()

        if(NOT EXISTS "${NVAFX_RUNTIME_DLL}")
            message(FATAL_ERROR "NVIDIA AFX runtime DLL not found: ${NVAFX_RUNTIME_DLL}")
        endif()
    else()
        find_library(NVAFX_SHARED_LIBRARY
            NAMES nv_audiofx libnv_audiofx.so
            HINTS
                "${NVAFX_API_ROOT}/lib"
                "${NVAFX_API_ROOT}/nvafx/lib"
                "${NVAFX_RUNTIME_ROOT}/nvafx/lib"
            NO_DEFAULT_PATH)
        if(NOT NVAFX_SHARED_LIBRARY)
            file(GLOB NVAFX_SHARED_LIBRARY_CANDIDATES
                "${NVAFX_API_ROOT}/lib/libnv_audiofx.so*"
                "${NVAFX_API_ROOT}/nvafx/lib/libnv_audiofx.so*"
                "${NVAFX_RUNTIME_ROOT}/nvafx/lib/libnv_audiofx.so*")
            list(SORT NVAFX_SHARED_LIBRARY_CANDIDATES)
            list(FILTER NVAFX_SHARED_LIBRARY_CANDIDATES EXCLUDE REGEX "\\.a$")
            if(NVAFX_SHARED_LIBRARY_CANDIDATES)
                list(GET NVAFX_SHARED_LIBRARY_CANDIDATES 0 NVAFX_SHARED_LIBRARY)
            endif()
        endif()

        if(NOT NVAFX_SHARED_LIBRARY)
            message(FATAL_ERROR "NVIDIA AFX shared library libnv_audiofx.so not found under NVAFX_API_ROOT=${NVAFX_API_ROOT} or NVAFX_RUNTIME_ROOT=${NVAFX_RUNTIME_ROOT}")
        endif()

        get_filename_component(NVAFX_SHARED_LIBRARY_DIR "${NVAFX_SHARED_LIBRARY}" DIRECTORY)

        set(NVAFX_RPATH_DIRS
            "${NVAFX_SHARED_LIBRARY_DIR}"
            "${NVAFX_RUNTIME_ROOT}/external/cuda/lib")
        file(GLOB NVAFX_FEATURE_LIBRARY_DIRS LIST_DIRECTORIES true "${NVAFX_RUNTIME_ROOT}/features/*/lib")
        foreach(NVAFX_FEATURE_LIBRARY_DIR IN LISTS NVAFX_FEATURE_LIBRARY_DIRS)
            if(IS_DIRECTORY "${NVAFX_FEATURE_LIBRARY_DIR}")
                list(APPEND NVAFX_RPATH_DIRS "${NVAFX_FEATURE_LIBRARY_DIR}")
            endif()
        endforeach()
        list(REMOVE_DUPLICATES NVAFX_RPATH_DIRS)
    endif()
endif()

add_executable(nvafx-audio-cli
    src/afx_sdk.cpp
    src/cli.cpp
    src/main.cpp
    src/sdk_discovery.cpp
    src/stdio_binary.cpp
    src/wav.cpp
)

target_compile_features(nvafx-audio-cli PRIVATE cxx_std_17)
if(WIN32)
    add_library(nvafx-msi-custom-actions SHARED
        packaging/msi/path_custom_action.cpp
    )
    target_compile_features(nvafx-msi-custom-actions PRIVATE cxx_std_17)
    target_link_libraries(nvafx-msi-custom-actions PRIVATE Msi Advapi32 User32)
    if(MSVC)
        target_compile_options(nvafx-msi-custom-actions PRIVATE /W4 /permissive-)
    endif()
endif()

if(NVAFX_ENABLE_SDK)
    target_compile_definitions(nvafx-audio-cli PRIVATE NVAFX_WITH_SDK)
    target_include_directories(nvafx-audio-cli PRIVATE "${NVAFX_INCLUDE_DIR}")
    if(WIN32)
        target_link_libraries(nvafx-audio-cli PRIVATE "${NVAFX_IMPORT_LIB}")
    else()
        target_link_libraries(nvafx-audio-cli PRIVATE "${NVAFX_SHARED_LIBRARY}" ${CMAKE_DL_LIBS})
        target_link_options(nvafx-audio-cli PRIVATE -Wl,--disable-new-dtags)
        set_target_properties(nvafx-audio-cli PROPERTIES
            BUILD_RPATH "${NVAFX_RPATH_DIRS}"
            INSTALL_RPATH "")
    endif()
    if(MSVC)
        target_link_options(nvafx-audio-cli PRIVATE /DELAYLOAD:NVAudioEffects.dll)
        target_link_libraries(nvafx-audio-cli PRIVATE delayimp)
    endif()
endif()

if(WIN32)
    target_link_libraries(nvafx-audio-cli PRIVATE Version)
endif()

if(MSVC)
    target_compile_options(nvafx-audio-cli PRIVATE /W4 /permissive-)
else()
    target_compile_options(nvafx-audio-cli PRIVATE -Wall -Wextra -pedantic)
endif()

include(CTest)
include(GNUInstallDirs)

set(CMAKE_INSTALL_DOCDIR "${CMAKE_INSTALL_DATAROOTDIR}/doc/nvafx-audio-cli")

install(TARGETS nvafx-audio-cli
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

install(FILES
    README.md
    LICENSE
    THIRD_PARTY_NOTICES.md
    docs/design.md
    docs/linux.md
    docs/release-packaging.md
    DESTINATION "${CMAKE_INSTALL_DOCDIR}")

if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    add_executable(wav-fixture tests/wav_fixture.cpp)
    target_compile_features(wav-fixture PRIVATE cxx_std_17)

    add_executable(wav-roundtrip tests/wav_roundtrip.cpp src/wav.cpp)
    target_compile_features(wav-roundtrip PRIVATE cxx_std_17)
    target_include_directories(wav-roundtrip PRIVATE src)

    add_executable(wav-inspect tests/wav_inspect.cpp src/wav.cpp)
    target_compile_features(wav-inspect PRIVATE cxx_std_17)
    target_include_directories(wav-inspect PRIVATE src)

    add_executable(wav-stdout tests/wav_stdout.cpp src/stdio_binary.cpp src/wav.cpp)
    target_compile_features(wav-stdout PRIVATE cxx_std_17)
    target_include_directories(wav-stdout PRIVATE src)
    if(WIN32)
        target_link_libraries(wav-stdout PRIVATE Version)
    endif()

    set(TEST_DATA_DIR "${CMAKE_CURRENT_BINARY_DIR}/testdata")
    set(CLI_EXE "$<TARGET_FILE:nvafx-audio-cli>")
    set(FIXTURE_EXE "$<TARGET_FILE:wav-fixture>")
    set(ROUNDTRIP_EXE "$<TARGET_FILE:wav-roundtrip>")
    set(INSPECT_EXE "$<TARGET_FILE:wav-inspect>")
    set(WAV_STDOUT_EXE "$<TARGET_FILE:wav-stdout>")
    set(TEST_HELPERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests")

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
        COMMAND "${CMAKE_COMMAND}"
            "-DDUMMY_MODEL=${TEST_DATA_DIR}/dummy_model.trtpkg"
            -P "${TEST_HELPERS_DIR}/create_dummy_model.cmake")
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
        COMMAND "${CMAKE_COMMAND}"
            "-DCLI_EXE=${CLI_EXE}"
            "-DINPUT_WAV=${TEST_DATA_DIR}/valid_pcm16_48k_mono.wav"
            "-DOUTPUT_WAV=${TEST_DATA_DIR}/dry_run_should_not_exist.wav"
            "-DMODEL_PATH=${TEST_DATA_DIR}/dummy_model.trtpkg"
            -P "${TEST_HELPERS_DIR}/run_and_assert_no_output.cmake")
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

    if(NOT NVAFX_ENABLE_SDK)
        add_test(NAME cli-sdk-disabled-processing-fails
            COMMAND "${CLI_EXE}" --input "${TEST_DATA_DIR}/valid_pcm16_48k_mono.wav" --output "${TEST_DATA_DIR}/disabled_out.wav" --effect denoiser --model "${TEST_DATA_DIR}/dummy_model.trtpkg" --runtime-root "${TEST_DATA_DIR}")
        set_tests_properties(cli-sdk-disabled-processing-fails PROPERTIES FIXTURES_REQUIRED wav_files WILL_FAIL TRUE)
    endif()

    add_test(NAME cli-help-documents-allow-unsafe-pipe
        COMMAND "${CMAKE_COMMAND}"
            "-DCLI_EXE=${CLI_EXE}"
            "-DMATCH_TEXT=--allow-unsafe-pipe"
            -P "${TEST_HELPERS_DIR}/assert_help_contains.cmake")

    add_test(NAME cli-help-documents-sdk-build-status
        COMMAND "${CMAKE_COMMAND}"
            "-DCLI_EXE=${CLI_EXE}"
            "-DMATCH_TEXT=SDK processing in this binary:"
            -P "${TEST_HELPERS_DIR}/assert_help_contains.cmake")

    add_test(NAME cli-help-documents-local-sdk-helper
        COMMAND "${CMAKE_COMMAND}"
            "-DCLI_EXE=${CLI_EXE}"
            "-DMATCH_TEXT=scripts/build_linux_sdk_local.py"
            -P "${TEST_HELPERS_DIR}/assert_help_contains.cmake")

    add_test(NAME cli-help-documents-sdk-free-boundary
        COMMAND "${CMAKE_COMMAND}"
            "-DCLI_EXE=${CLI_EXE}"
            "-DMATCH_TEXT=Public release binaries/packages are SDK-free"
            -P "${TEST_HELPERS_DIR}/assert_help_contains.cmake")

    add_test(NAME cli-stdin-dry-run-valid
        COMMAND "${CMAKE_COMMAND}"
            "-DCLI_EXE=${CLI_EXE}"
            "-DINPUT_WAV=${TEST_DATA_DIR}/valid_pcm16_48k_mono.wav"
            "-DOUTPUT_WAV=${TEST_DATA_DIR}/stdin_dry.wav"
            "-DSTDOUT_FILE=${TEST_DATA_DIR}/stdin_dry_stdout.txt"
            "-DSTDERR_FILE=${TEST_DATA_DIR}/stdin_dry_stderr.txt"
            -P "${TEST_HELPERS_DIR}/run_stdin_dry_run.cmake")
    set_tests_properties(cli-stdin-dry-run-valid PROPERTIES FIXTURES_REQUIRED wav_files)

    add_test(NAME cli-dry-run-output-stdout-empty
        COMMAND "${CMAKE_COMMAND}"
            "-DCLI_EXE=${CLI_EXE}"
            "-DINPUT_WAV=${TEST_DATA_DIR}/valid_pcm16_48k_mono.wav"
            "-DSTDOUT_FILE=${TEST_DATA_DIR}/dry_pipe_stdout.bin"
            "-DSTDERR_FILE=${TEST_DATA_DIR}/dry_pipe_stderr.txt"
            -P "${TEST_HELPERS_DIR}/run_dry_run_stdout_empty.cmake")
    set_tests_properties(cli-dry-run-output-stdout-empty PROPERTIES FIXTURES_REQUIRED wav_files)

    add_test(NAME cli-invalid-stdin-helpful-error
        COMMAND "${CMAKE_COMMAND}"
            "-DCLI_EXE=${CLI_EXE}"
            "-DINVALID_STDIN=${TEST_DATA_DIR}/invalid_stdin.txt"
            "-DOUTPUT_WAV=${TEST_DATA_DIR}/invalid_stdin_out.wav"
            "-DSTDOUT_FILE=${TEST_DATA_DIR}/invalid_stdin_stdout.txt"
            "-DSTDERR_FILE=${TEST_DATA_DIR}/invalid_stdin_stderr.txt"
            "-DEXPECTED_TEXT=valid RIFF/WAVE stream"
            -P "${TEST_HELPERS_DIR}/run_invalid_stdin_error.cmake")
    set_tests_properties(cli-invalid-stdin-helpful-error PROPERTIES FIXTURES_REQUIRED wav_files)

    if(NOT NVAFX_ENABLE_SDK)
        add_test(NAME cli-sdk-disabled-output-stdout-clean
            COMMAND "${CMAKE_COMMAND}"
                "-DCLI_EXE=${CLI_EXE}"
                "-DINPUT_WAV=${TEST_DATA_DIR}/valid_pcm16_48k_mono.wav"
                "-DMODEL_PATH=${TEST_DATA_DIR}/dummy_model.trtpkg"
                "-DRUNTIME_ROOT=${TEST_DATA_DIR}"
                "-DSTDOUT_FILE=${TEST_DATA_DIR}/disabled_pipe_stdout.bin"
                "-DSTDERR_FILE=${TEST_DATA_DIR}/disabled_pipe_stderr.txt"
                "-DEXPECTED_TEXT=SDK-free"
                -P "${TEST_HELPERS_DIR}/run_sdk_disabled_stdout_clean.cmake")
        set_tests_properties(cli-sdk-disabled-output-stdout-clean PROPERTIES FIXTURES_REQUIRED wav_files)
    endif()

    add_test(NAME wav-stream-stdout-roundtrip
        COMMAND "${CMAKE_COMMAND}"
            "-DWAV_STDOUT_EXE=${WAV_STDOUT_EXE}"
            "-DINSPECT_EXE=${INSPECT_EXE}"
            "-DINPUT_WAV=${TEST_DATA_DIR}/valid_pcm16_48k_mono.wav"
            "-DOUTPUT_WAV=${TEST_DATA_DIR}/stream_stdout.wav"
            "-DSTDERR_FILE=${TEST_DATA_DIR}/stream_stdout_stderr.txt"
            -P "${TEST_HELPERS_DIR}/run_wav_stream_stdout_roundtrip.cmake")
    set_tests_properties(wav-stream-stdout-roundtrip PROPERTIES FIXTURES_REQUIRED wav_files)

    add_test(NAME cli-check-sdk-missing-roots
        COMMAND "${CLI_EXE}" --check-sdk --api-root "${TEST_DATA_DIR}/missing_api_root" --runtime-root "${TEST_DATA_DIR}/missing_runtime_root")
    set_tests_properties(cli-check-sdk-missing-roots PROPERTIES WILL_FAIL TRUE)

    add_test(NAME cli-check-sdk-fake-tree
        COMMAND "${CMAKE_COMMAND}"
            "-DCLI_EXE=${CLI_EXE}"
            "-DTEST_DATA_DIR=${TEST_DATA_DIR}"
            -P "${TEST_HELPERS_DIR}/run_check_sdk_fake_tree.cmake")

    if(UNIX AND NOT APPLE)
        add_test(NAME manual-linux-sdk-processing
            COMMAND "${CMAKE_COMMAND}"
                "-DCLI_EXE=${CLI_EXE}"
                "-DFIXTURE_EXE=${FIXTURE_EXE}"
                "-DINSPECT_EXE=${INSPECT_EXE}"
                "-DTEST_DATA_DIR=${TEST_DATA_DIR}"
                -P "${TEST_HELPERS_DIR}/run_manual_linux_sdk_processing.cmake")
        set_tests_properties(manual-linux-sdk-processing PROPERTIES
            SKIP_REGULAR_EXPRESSION "Skipping manual Linux SDK processing test")
    endif()

    add_test(NAME repo-hygiene
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_repo_hygiene.py")
    set_tests_properties(repo-hygiene PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

    add_test(NAME helper-linux-sdk-local-ci
        COMMAND "${CMAKE_COMMAND}"
            "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DSCRIPT_PATH=${CMAKE_CURRENT_SOURCE_DIR}/scripts/build_linux_sdk_local.py"
            "-DTEST_DATA_DIR=${TEST_DATA_DIR}"
            -P "${TEST_HELPERS_DIR}/run_build_linux_sdk_local_tests.cmake")
endif()

set(CPACK_PACKAGE_NAME "nvafx-audio-cli")
set(CPACK_PACKAGE_VENDOR "Eikai Intelligent Systems")
set(CPACK_PACKAGE_CONTACT "Eikai Intelligent Systems <packages@eikai.co.jp>")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/tsuchim/nvafx-audio-cli")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "SDK-free CLI checks for NVIDIA Audio Effects WAV workflows")
set(CPACK_PACKAGE_DESCRIPTION
    "This public Debian package is SDK-free and cannot perform real NVIDIA Audio Effects processing by itself. It installs nvafx-audio-cli for help/version, dry-run WAV validation, stdin/stdout guardrails, SDK structure checks, and direct actionable refusal when processing is requested from an SDK-free build. Real processing requires a separate SDK-enabled local or internal build plus externally supplied NVIDIA Audio Effects SDK runtime, matching feature library, model .trtpkg file, and visible NVIDIA GPU runtime. Public packages intentionally omit NVIDIA SDK/runtime/model/CUDA material.")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
endif()

set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Eikai Intelligent Systems <packages@eikai.co.jp>")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${CPACK_PACKAGE_HOMEPAGE_URL}")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

include(CPack)
