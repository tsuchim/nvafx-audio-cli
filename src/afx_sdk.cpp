#include "afx_sdk.hpp"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef NVAFX_WITH_SDK
#include <Windows.h>
#include <nvAudioEffects.h>
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
        case NVAFX_STATUS_32_SERVER_NOT_REGISTERED:
            return "NVAFX_STATUS_32_SERVER_NOT_REGISTERED";
        case NVAFX_STATUS_32_COM_ERROR:
            return "NVAFX_STATUS_32_COM_ERROR";
        case NVAFX_STATUS_GPU_UNSUPPORTED:
            return "NVAFX_STATUS_GPU_UNSUPPORTED";
        case NVAFX_STATUS_CUDA_CONTEXT_CREATION_FAILED:
            return "NVAFX_STATUS_CUDA_CONTEXT_CREATION_FAILED";
        default:
            return "unknown status " + std::to_string(static_cast<int>(status));
    }
}

void check_status(NvAFX_Status status, const std::string& call) {
    if (status != NVAFX_STATUS_SUCCESS) {
        throw sdk_error(call + " failed with " + status_name(status));
    }
}

NvAFX_EffectSelector effect_selector(const std::string& effect) {
    if (effect == "denoiser") {
        return NVAFX_EFFECT_DENOISER;
    }
    if (effect == "dereverb") {
        return NVAFX_EFFECT_DEREVERB;
    }
    if (effect == "dereverb_denoiser") {
        return NVAFX_EFFECT_DEREVERB_DENOISER;
    }
    throw sdk_error("unsupported effect: " + effect);
}

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
    check_status(NvAFX_GetU32(handle, parameter, &value), std::string("NvAFX_GetU32(") + parameter + ")");
    return value;
}

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
    add_runtime_dll_directory(options.runtime_root);

    if (options.model_path.empty()) {
        throw sdk_error("--model is required for SDK processing");
    }

    if (!std::filesystem::is_regular_file(options.model_path)) {
        throw sdk_error("model path does not exist: " + options.model_path);
    }

    EffectHandle effect(options.effect);
    const NvAFX_Handle handle = effect.get();

    check_status(NvAFX_SetString(handle, NVAFX_PARAM_MODEL_PATH, options.model_path.c_str()),
                 "NvAFX_SetString(model_path)");
    check_status(NvAFX_SetFloat(handle, NVAFX_PARAM_INTENSITY_RATIO, options.intensity),
                 "NvAFX_SetFloat(intensity_ratio)");
    check_status(NvAFX_Load(handle), "NvAFX_Load");

    const unsigned sdk_input_rate = get_u32(handle, NVAFX_PARAM_INPUT_SAMPLE_RATE);
    const unsigned sdk_output_rate = get_u32(handle, NVAFX_PARAM_OUTPUT_SAMPLE_RATE);
    const unsigned sdk_input_channels = get_u32(handle, NVAFX_PARAM_NUM_INPUT_CHANNELS);
    const unsigned sdk_output_channels = get_u32(handle, NVAFX_PARAM_NUM_OUTPUT_CHANNELS);
    const unsigned input_frame_size = get_u32(handle, NVAFX_PARAM_NUM_INPUT_SAMPLES_PER_FRAME);
    const unsigned output_frame_size = get_u32(handle, NVAFX_PARAM_NUM_OUTPUT_SAMPLES_PER_FRAME);

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
