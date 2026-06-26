#include "afx_sdk.hpp"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifdef NVAFX_WITH_SDK
#include <nvAudioEffects.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif
#endif

namespace nvafx {
namespace {

std::runtime_error sdk_error(const std::string& message) {
    return std::runtime_error("NVIDIA AFX SDK: " + message);
}

#ifdef NVAFX_WITH_SDK

std::string status_name(NvAFX_Status status) {
    switch (status) {
        case NVAFX_STATUS_SUCCESS:
            return "NVAFX_STATUS_SUCCESS";
        case NVAFX_STATUS_FAILED:
            return "NVAFX_STATUS_FAILED";
        case NVAFX_STATUS_INVALID_HANDLE:
            return "NVAFX_STATUS_INVALID_HANDLE";
        case NVAFX_STATUS_INVALID_PARAM:
            return "NVAFX_STATUS_INVALID_PARAM";
        case NVAFX_STATUS_IMMUTABLE_PARAM:
            return "NVAFX_STATUS_IMMUTABLE_PARAM";
        case NVAFX_STATUS_INSUFFICIENT_DATA:
            return "NVAFX_STATUS_INSUFFICIENT_DATA";
        case NVAFX_STATUS_EFFECT_NOT_AVAILABLE:
            return "NVAFX_STATUS_EFFECT_NOT_AVAILABLE";
        case NVAFX_STATUS_OUTPUT_BUFFER_TOO_SMALL:
            return "NVAFX_STATUS_OUTPUT_BUFFER_TOO_SMALL";
        case NVAFX_STATUS_MODEL_LOAD_FAILED:
            return "NVAFX_STATUS_MODEL_LOAD_FAILED";
#ifdef _WIN32
        case NVAFX_STATUS_32_SERVER_NOT_REGISTERED:
            return "NVAFX_STATUS_32_SERVER_NOT_REGISTERED";
        case NVAFX_STATUS_32_COM_ERROR:
            return "NVAFX_STATUS_32_COM_ERROR";
        case NVAFX_STATUS_GPU_UNSUPPORTED:
            return "NVAFX_STATUS_GPU_UNSUPPORTED";
        case NVAFX_STATUS_CUDA_CONTEXT_CREATION_FAILED:
            return "NVAFX_STATUS_CUDA_CONTEXT_CREATION_FAILED";
#else
        case NVAFX_STATUS_MODEL_NOT_LOADED:
            return "NVAFX_STATUS_MODEL_NOT_LOADED";
        case NVAFX_STATUS_INCOMPATIBLE_MODEL:
            return "NVAFX_STATUS_INCOMPATIBLE_MODEL";
        case NVAFX_STATUS_GPU_UNSUPPORTED:
            return "NVAFX_STATUS_GPU_UNSUPPORTED";
        case NVAFX_STATUS_NO_SUPPORTED_GPU_FOUND:
            return "NVAFX_STATUS_NO_SUPPORTED_GPU_FOUND";
        case NVAFX_STATUS_WRONG_GPU:
            return "NVAFX_STATUS_WRONG_GPU";
        case NVAFX_STATUS_CUDA_ERROR:
            return "NVAFX_STATUS_CUDA_ERROR";
        case NVAFX_STATUS_INVALID_OPERATION:
            return "NVAFX_STATUS_INVALID_OPERATION";
        case NVAFX_UNSUPPORTED_RUNTIME:
            return "NVAFX_UNSUPPORTED_RUNTIME";
#endif
        default:
            return "unknown status " + std::to_string(static_cast<int>(status));
    }
}

void check_status(NvAFX_Status status, const std::string& call) {
    if (status != NVAFX_STATUS_SUCCESS) {
        throw sdk_error(call + " failed with " + status_name(status));
    }
}

std::string parameter_name(NvAFX_ParameterSelector parameter) {
    return parameter == nullptr ? std::string("<null>") : std::string(parameter);
}

NvAFX_EffectSelector effect_selector(const std::string& effect) {
    if (effect == "denoiser") {
        return "denoiser";
    }
    if (effect == "dereverb") {
        return "dereverb";
    }
    if (effect == "dereverb_denoiser") {
        return "dereverb_denoiser";
    }
    throw sdk_error("unsupported effect: " + effect);
}

#ifdef _WIN32
std::wstring widen(const std::string& value) {
    if (value.empty()) {
        return {};
    }

    const int needed = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if (needed <= 0) {
        throw sdk_error("failed to convert runtime root to UTF-16");
    }

    std::wstring output(static_cast<std::size_t>(needed), L'\0');
    const int written = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, output.data(), needed);
    if (written <= 0) {
        throw sdk_error("failed to convert runtime root to UTF-16");
    }
    output.pop_back();
    return output;
}

void add_runtime_dll_directory(const std::string& runtime_root) {
    if (runtime_root.empty()) {
        throw sdk_error("runtime root is not set");
    }

    const std::filesystem::path root(runtime_root);
    if (!std::filesystem::is_directory(root)) {
        throw sdk_error("runtime root does not exist: " + runtime_root);
    }

    const std::filesystem::path dll = root / "NVAudioEffects.dll";
    if (!std::filesystem::is_regular_file(dll)) {
        throw sdk_error("NVAudioEffects.dll not found in runtime root: " + dll.string());
    }

    if (!SetDllDirectoryW(widen(runtime_root).c_str())) {
        throw sdk_error("SetDllDirectoryW failed for runtime root");
    }
}
#else
class SharedLibrary {
public:
    explicit SharedLibrary(std::filesystem::path path) : path_(std::move(path)) {
        handle_ = dlopen(path_.c_str(), RTLD_NOW | RTLD_GLOBAL);
        if (handle_ == nullptr) {
            const char* error = dlerror();
            throw sdk_error("failed to load feature library " + path_.string() +
                            (error == nullptr ? std::string() : std::string(": ") + error));
        }
    }

    SharedLibrary(const SharedLibrary&) = delete;
    SharedLibrary& operator=(const SharedLibrary&) = delete;

    ~SharedLibrary() {
        if (handle_ != nullptr) {
            (void)dlclose(handle_);
        }
    }

private:
    std::filesystem::path path_;
    void* handle_ = nullptr;
};

std::string feature_directory_name(const std::string& effect) {
    if (effect == "denoiser" || effect == "dereverb" || effect == "dereverb_denoiser") {
        return effect;
    }
    throw sdk_error("unsupported effect: " + effect);
}

SharedLibrary load_linux_feature_library(const std::string& runtime_root, const std::string& effect) {
    if (runtime_root.empty()) {
        throw sdk_error("runtime root is not set");
    }

    const std::filesystem::path root(runtime_root);
    if (!std::filesystem::is_directory(root)) {
        throw sdk_error("runtime root does not exist: " + runtime_root);
    }

    const std::filesystem::path base_library = root / "nvafx" / "lib" / "libnv_audiofx.so";
    if (!std::filesystem::is_regular_file(base_library)) {
        throw sdk_error("Linux runtime library not found: " + base_library.string());
    }

    const std::string feature = feature_directory_name(effect);
    const std::filesystem::path feature_library =
        root / "features" / feature / "lib" / ("libnv_audiofx_" + feature + ".so");
    if (!std::filesystem::is_regular_file(feature_library)) {
        throw sdk_error("Linux feature library not found: " + feature_library.string());
    }

    return SharedLibrary(feature_library);
}

NvAFX_ParameterSelector input_frame_size_parameter() {
    return NVAFX_PARAM_NUM_SAMPLES_PER_INPUT_FRAME;
}

NvAFX_ParameterSelector output_frame_size_parameter() {
    return NVAFX_PARAM_NUM_SAMPLES_PER_OUTPUT_FRAME;
}
#endif

class EffectHandle {
public:
    explicit EffectHandle(const std::string& effect) {
        check_status(NvAFX_CreateEffect(effect_selector(effect), &handle_), "NvAFX_CreateEffect");
    }

    EffectHandle(const EffectHandle&) = delete;
    EffectHandle& operator=(const EffectHandle&) = delete;

    ~EffectHandle() {
        if (handle_ != nullptr) {
            (void)NvAFX_DestroyEffect(handle_);
        }
    }

    NvAFX_Handle get() const {
        return handle_;
    }

private:
    NvAFX_Handle handle_ = nullptr;
};

unsigned get_u32(NvAFX_Handle handle, NvAFX_ParameterSelector parameter) {
    unsigned value = 0;
    check_status(NvAFX_GetU32(handle, parameter, &value),
                 "NvAFX_GetU32(" + parameter_name(parameter) + ")");
    return value;
}

#ifndef _WIN32
void set_u32_with_fallback(NvAFX_Handle handle, NvAFX_ParameterSelector primary,
                           NvAFX_ParameterSelector fallback, unsigned value,
                           const std::string& call) {
    NvAFX_Status status = NvAFX_SetU32(handle, primary, value);
    if (status == NVAFX_STATUS_INVALID_PARAM) {
        status = NvAFX_SetU32(handle, fallback, value);
    }
    check_status(status, call);
}

std::vector<unsigned> get_u32_list(NvAFX_Handle handle, NvAFX_ParameterSelector parameter) {
    unsigned list_size = 0;
    NvAFX_Status status = NvAFX_GetU32List(handle, parameter, nullptr, &list_size);
    if (status != NVAFX_STATUS_OUTPUT_BUFFER_TOO_SMALL || list_size == 0) {
        check_status(status, "NvAFX_GetU32List(" + parameter_name(parameter) + ")");
    }

    std::vector<unsigned> values(list_size, 0);
    check_status(NvAFX_GetU32List(handle, parameter, values.data(), &list_size),
                 "NvAFX_GetU32List(" + parameter_name(parameter) + ")");
    values.resize(list_size);
    return values;
}
#endif

#ifdef _WIN32
NvAFX_ParameterSelector input_frame_size_parameter() {
    return NVAFX_PARAM_NUM_INPUT_SAMPLES_PER_FRAME;
}

NvAFX_ParameterSelector output_frame_size_parameter() {
    return NVAFX_PARAM_NUM_OUTPUT_SAMPLES_PER_FRAME;
}
#endif

#endif

}  // namespace

bool afx_sdk_build_enabled() {
#ifdef NVAFX_WITH_SDK
    return true;
#else
    return false;
#endif
}

AudioBuffer process_with_afx_sdk(const AudioBuffer& input, const AfxProcessOptions& options) {
#ifndef NVAFX_WITH_SDK
    (void)input;
    (void)options;
    throw sdk_error("SDK support is not enabled in this build. Reconfigure with NVAFX_ENABLE_SDK=ON.");
#else
#ifdef _WIN32
    add_runtime_dll_directory(options.runtime_root);
#else
    SharedLibrary feature_library = load_linux_feature_library(options.runtime_root, options.effect);
#endif

    if (options.model_path.empty()) {
        throw sdk_error("--model is required for SDK processing");
    }

    if (!std::filesystem::is_regular_file(options.model_path)) {
        throw sdk_error("model path does not exist: " + options.model_path);
    }

    EffectHandle effect(options.effect);
    const NvAFX_Handle handle = effect.get();

#ifdef _WIN32
    check_status(NvAFX_SetString(handle, NVAFX_PARAM_MODEL_PATH, options.model_path.c_str()),
                 "NvAFX_SetString(model_path)");
    check_status(NvAFX_SetFloat(handle, NVAFX_PARAM_INTENSITY_RATIO, options.intensity),
                 "NvAFX_SetFloat(intensity_ratio)");
    check_status(NvAFX_Load(handle), "NvAFX_Load");
#else
    check_status(NvAFX_SetU32(handle, NVAFX_PARAM_USE_DEFAULT_GPU, 0U),
                 "NvAFX_SetU32(use_default_gpu)");
    set_u32_with_fallback(handle, NVAFX_PARAM_INPUT_SAMPLE_RATE, NVAFX_PARAM_SAMPLE_RATE,
                          input.sample_rate, "NvAFX_SetU32(input_sample_rate)");

    const char* model_paths[] = {options.model_path.c_str()};
    check_status(NvAFX_SetStringList(handle, NVAFX_PARAM_MODEL_PATH, model_paths, 1U),
                 "NvAFX_SetStringList(model_path)");
    check_status(NvAFX_SetU32(handle, NVAFX_PARAM_NUM_STREAMS, 1U),
                 "NvAFX_SetU32(num_streams)");

    const std::vector<unsigned> supported_frame_sizes =
        get_u32_list(handle, NVAFX_PARAM_SUPPORTED_NUM_SAMPLES_PER_FRAME);
    const unsigned configured_frame_size = supported_frame_sizes.front();
    set_u32_with_fallback(handle, NVAFX_PARAM_NUM_SAMPLES_PER_INPUT_FRAME,
                          NVAFX_PARAM_NUM_SAMPLES_PER_FRAME, configured_frame_size,
                          "NvAFX_SetU32(input_frame_size)");

    check_status(NvAFX_Load(handle), "NvAFX_Load");
    check_status(NvAFX_SetFloat(handle, NVAFX_PARAM_INTENSITY_RATIO, options.intensity),
                 "NvAFX_SetFloat(intensity_ratio)");
#endif

    const unsigned sdk_input_rate = get_u32(handle, NVAFX_PARAM_INPUT_SAMPLE_RATE);
    const unsigned sdk_output_rate = get_u32(handle, NVAFX_PARAM_OUTPUT_SAMPLE_RATE);
    const unsigned sdk_input_channels = get_u32(handle, NVAFX_PARAM_NUM_INPUT_CHANNELS);
    const unsigned sdk_output_channels = get_u32(handle, NVAFX_PARAM_NUM_OUTPUT_CHANNELS);
    const unsigned input_frame_size = get_u32(handle, input_frame_size_parameter());
    const unsigned output_frame_size = get_u32(handle, output_frame_size_parameter());

    if (sdk_input_rate != input.sample_rate || sdk_output_rate != input.sample_rate) {
        throw sdk_error("model sample rate does not match input WAV sample rate");
    }

    if (sdk_input_channels != input.channels || sdk_output_channels != input.channels) {
        throw sdk_error("channel layout is not supported by the loaded SDK effect/model; input WAV has " +
                        std::to_string(input.channels) + " channel(s), SDK expects " +
                        std::to_string(sdk_input_channels) + " input channel(s) and " +
                        std::to_string(sdk_output_channels) + " output channel(s)");
    }

    if (input_frame_size == 0 || output_frame_size == 0) {
        throw sdk_error("SDK returned an invalid frame size");
    }

    if (input_frame_size != output_frame_size) {
        throw sdk_error("SDK effect changes frame size; this CLI preserves frame count only");
    }

    const std::size_t original_frames = input.samples.empty() ? 0U : input.samples.front().size();
    AudioBuffer output;
    output.channels = input.channels;
    output.sample_rate = input.sample_rate;
    output.samples.assign(output.channels, std::vector<float>(original_frames, 0.0F));

    std::vector<std::vector<float>> input_frame(input.channels,
                                                std::vector<float>(input_frame_size, 0.0F));
    std::vector<std::vector<float>> output_frame(input.channels,
                                                 std::vector<float>(output_frame_size, 0.0F));
    std::vector<const float*> input_ptrs(input.channels, nullptr);
    std::vector<float*> output_ptrs(input.channels, nullptr);

    for (std::size_t offset = 0; offset < original_frames; offset += input_frame_size) {
        const std::size_t remaining = original_frames - offset;
        const std::size_t to_copy = std::min<std::size_t>(remaining, input_frame_size);

        for (std::uint16_t channel = 0; channel < input.channels; ++channel) {
            std::fill(input_frame[channel].begin(), input_frame[channel].end(), 0.0F);
            std::fill(output_frame[channel].begin(), output_frame[channel].end(), 0.0F);
            std::copy_n(input.samples[channel].begin() + static_cast<std::ptrdiff_t>(offset), to_copy,
                        input_frame[channel].begin());
            input_ptrs[channel] = input_frame[channel].data();
            output_ptrs[channel] = output_frame[channel].data();
        }

        check_status(NvAFX_Run(handle, input_ptrs.data(), output_ptrs.data(), input_frame_size,
                               input.channels),
                     "NvAFX_Run");

        for (std::uint16_t channel = 0; channel < input.channels; ++channel) {
            std::copy_n(output_frame[channel].begin(), to_copy,
                        output.samples[channel].begin() + static_cast<std::ptrdiff_t>(offset));
        }
    }

    return output;
#endif
}

}  // namespace nvafx
