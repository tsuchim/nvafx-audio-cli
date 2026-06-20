#include "sdk_discovery.hpp"

#include <cstdlib>
#include <filesystem>
#include <memory>

namespace nvafx {
namespace {

std::string read_environment_variable(const char* name) {
#ifdef _WIN32
    char* value = nullptr;
    std::size_t size = 0;
    if (_dupenv_s(&value, &size, name) != 0 || value == nullptr) {
        return {};
    }

    std::unique_ptr<char, decltype(&std::free)> owned(value, &std::free);
    return std::string(owned.get());
#else
    const char* value = std::getenv(name);
    return value == nullptr ? std::string() : std::string(value);
#endif
}

void collect_files_with_extension(const std::filesystem::path& root, const std::string& extension,
                                  std::vector<std::string>* output) {
    if (!std::filesystem::is_directory(root)) {
        return;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (entry.is_regular_file() && entry.path().extension() == extension) {
            output->push_back(entry.path().string());
        }
    }
}

}  // namespace

std::string resolve_api_root(const std::string& cli_api_root) {
    if (!cli_api_root.empty()) {
        return cli_api_root;
    }

    return read_environment_variable("NVAFX_API_ROOT");
}

std::string resolve_runtime_root(const std::string& cli_runtime_root, const std::string& cli_sdk_root) {
    if (!cli_runtime_root.empty()) {
        return cli_runtime_root;
    }

    if (!cli_sdk_root.empty()) {
        return cli_sdk_root;
    }

    std::string root = read_environment_variable("NVAFX_RUNTIME_ROOT");
    if (!root.empty()) {
        return root;
    }

    return read_environment_variable("AFX_SDK_ROOT");
}

SdkProbeResult probe_sdk_roots(const std::string& api_root, const std::string& runtime_root) {
    SdkProbeResult result;
    result.api_root = api_root;
    result.runtime_root = runtime_root;

    const std::filesystem::path api_path(api_root);
    const std::filesystem::path runtime_path(runtime_root);

    result.api_root_exists = !api_root.empty() && std::filesystem::is_directory(api_path);
    result.runtime_root_exists = !runtime_root.empty() && std::filesystem::is_directory(runtime_path);
    result.header_found = std::filesystem::is_regular_file(api_path / "include" / "nvAudioEffects.h");
    result.import_library_found = std::filesystem::is_regular_file(api_path / "lib" / "NVAudioEffects.lib");
    result.runtime_dll_found = std::filesystem::is_regular_file(runtime_path / "NVAudioEffects.dll");
    result.models_path_found = std::filesystem::is_directory(runtime_path / "models");
    result.denoiser_model_found = std::filesystem::is_regular_file(runtime_path / "models" / "denoiser_48k.trtpkg") ||
                                  std::filesystem::is_regular_file(runtime_path / "models" / "denoiser_16k.trtpkg");
    result.dereverb_model_found = std::filesystem::is_regular_file(runtime_path / "models" / "dereverb_48k.trtpkg") ||
                                  std::filesystem::is_regular_file(runtime_path / "models" / "dereverb_16k.trtpkg");
    result.dereverb_denoiser_model_found =
        std::filesystem::is_regular_file(runtime_path / "models" / "dereverb_denoiser_48k.trtpkg") ||
        std::filesystem::is_regular_file(runtime_path / "models" / "dereverb_denoiser_16k.trtpkg");

    collect_files_with_extension(api_path / "lib", ".lib", &result.libraries);
    collect_files_with_extension(runtime_path, ".dll", &result.runtime_dlls);
    collect_files_with_extension(runtime_path / "models", ".trtpkg", &result.models);

    const std::vector<std::pair<std::string, bool>> checks = {
        {"api include/nvAudioEffects.h", result.header_found},
        {"api lib/NVAudioEffects.lib", result.import_library_found},
        {"runtime NVAudioEffects.dll", result.runtime_dll_found},
        {"runtime models", result.models_path_found},
        {"runtime denoiser model", result.denoiser_model_found},
        {"runtime dereverb model", result.dereverb_model_found},
        {"runtime dereverb_denoiser model", result.dereverb_denoiser_model_found},
    };

    for (const auto& [label, present] : checks) {
        if (present) {
            result.present_subpaths.push_back(label);
        } else {
            result.missing_subpaths.push_back(label);
        }
    }

    result.structurally_plausible = result.api_root_exists && result.runtime_root_exists &&
                                    result.header_found && result.import_library_found &&
                                    result.runtime_dll_found && result.models_path_found;

    return result;
}

}  // namespace nvafx
