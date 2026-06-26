#include "sdk_discovery.hpp"

#include <algorithm>
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
    std::error_code error;
    if (!std::filesystem::is_directory(root, error) || error) {
        return;
    }

    std::vector<std::string> found;

    try {
        const auto options = std::filesystem::directory_options::skip_permission_denied;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root, options)) {
            std::error_code entry_error;
            if (entry.is_regular_file(entry_error) && !entry_error && entry.path().extension() == extension) {
                found.push_back(entry.path().string());
            }
        }
    } catch (const std::filesystem::filesystem_error&) {
        // SDK probing is structural and best-effort; unreadable subtrees are skipped.
    }

    std::sort(found.begin(), found.end());
    output->insert(output->end(), found.begin(), found.end());
}

bool safe_is_directory(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::is_directory(path, error) && !error;
}

bool safe_is_regular_file(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::is_regular_file(path, error) && !error;
}

bool has_filename(const std::vector<std::string>& paths, const std::string& filename) {
    return std::any_of(paths.begin(), paths.end(), [&filename](const std::string& path) {
        return std::filesystem::path(path).filename() == filename;
    });
}

bool has_any_filename(const std::vector<std::string>& paths, const std::vector<std::string>& filenames) {
    return std::any_of(filenames.begin(), filenames.end(), [&paths](const std::string& filename) {
        return has_filename(paths, filename);
    });
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

    result.api_root_exists = !api_root.empty() && safe_is_directory(api_path);
    result.runtime_root_exists = !runtime_root.empty() && safe_is_directory(runtime_path);

    result.header_found = (result.api_root_exists &&
                           (safe_is_regular_file(api_path / "include" / "nvAudioEffects.h") ||
                            safe_is_regular_file(api_path / "nvafx" / "include" / "nvAudioEffects.h"))) ||
                          (result.runtime_root_exists &&
                           safe_is_regular_file(runtime_path / "nvafx" / "include" / "nvAudioEffects.h"));
    result.import_library_found =
        result.api_root_exists && safe_is_regular_file(api_path / "lib" / "NVAudioEffects.lib");
    result.shared_library_found =
        (result.api_root_exists &&
         (safe_is_regular_file(api_path / "lib" / "libnv_audiofx.so") ||
          safe_is_regular_file(api_path / "nvafx" / "lib" / "libnv_audiofx.so"))) ||
        (result.runtime_root_exists &&
         safe_is_regular_file(runtime_path / "nvafx" / "lib" / "libnv_audiofx.so"));
    result.runtime_dll_found =
        result.runtime_root_exists && safe_is_regular_file(runtime_path / "NVAudioEffects.dll");
    result.feature_root_found = result.runtime_root_exists && safe_is_directory(runtime_path / "features");
    result.denoiser_feature_library_found =
        result.runtime_root_exists && safe_is_regular_file(runtime_path / "features" / "denoiser" / "lib" /
                                                           "libnv_audiofx_denoiser.so");
    result.dereverb_feature_library_found =
        result.runtime_root_exists && safe_is_regular_file(runtime_path / "features" / "dereverb" / "lib" /
                                                           "libnv_audiofx_dereverb.so");
    result.dereverb_denoiser_feature_library_found =
        result.runtime_root_exists &&
        safe_is_regular_file(runtime_path / "features" / "dereverb_denoiser" / "lib" /
                             "libnv_audiofx_dereverb_denoiser.so");
    result.models_path_found = result.runtime_root_exists &&
                               (safe_is_directory(runtime_path / "models") ||
                                safe_is_directory(runtime_path / "features"));

    if (result.api_root_exists) {
        collect_files_with_extension(api_path / "lib", ".lib", &result.libraries);
        collect_files_with_extension(api_path / "lib", ".so", &result.shared_libraries);
        collect_files_with_extension(api_path / "nvafx" / "lib", ".so", &result.shared_libraries);
    }
    if (result.runtime_root_exists) {
        collect_files_with_extension(runtime_path, ".dll", &result.runtime_dlls);
        collect_files_with_extension(runtime_path / "nvafx" / "lib", ".so", &result.shared_libraries);
        collect_files_with_extension(runtime_path / "features", ".so", &result.feature_libraries);
    }
    if (result.models_path_found) {
        collect_files_with_extension(runtime_path / "models", ".trtpkg", &result.models);
        collect_files_with_extension(runtime_path / "features", ".trtpkg", &result.models);
    }

    std::sort(result.shared_libraries.begin(), result.shared_libraries.end());
    result.shared_libraries.erase(std::unique(result.shared_libraries.begin(), result.shared_libraries.end()),
                                  result.shared_libraries.end());
    std::sort(result.feature_libraries.begin(), result.feature_libraries.end());
    result.feature_libraries.erase(std::unique(result.feature_libraries.begin(), result.feature_libraries.end()),
                                   result.feature_libraries.end());
    std::sort(result.models.begin(), result.models.end());
    result.models.erase(std::unique(result.models.begin(), result.models.end()), result.models.end());

    result.denoiser_model_found =
        result.models_path_found &&
        has_any_filename(result.models, {"denoiser_48k.trtpkg", "denoiser_16k.trtpkg"});
    result.dereverb_model_found =
        result.models_path_found &&
        has_any_filename(result.models, {"dereverb_48k.trtpkg", "dereverb_16k.trtpkg"});
    result.dereverb_denoiser_model_found =
        result.models_path_found &&
        has_any_filename(result.models, {"dereverb_denoiser_48k.trtpkg", "dereverb_denoiser_16k.trtpkg"});

    const std::vector<std::pair<std::string, bool>> checks = {
        {"api include/nvAudioEffects.h", result.header_found},
        {"api lib/NVAudioEffects.lib", result.import_library_found},
        {"linux nvafx/lib/libnv_audiofx.so", result.shared_library_found},
        {"runtime NVAudioEffects.dll", result.runtime_dll_found},
        {"linux runtime features", result.feature_root_found},
        {"linux denoiser feature library", result.denoiser_feature_library_found},
        {"linux dereverb feature library", result.dereverb_feature_library_found},
        {"linux dereverb_denoiser feature library", result.dereverb_denoiser_feature_library_found},
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

    const bool windows_plausible = result.header_found && result.import_library_found &&
                                   result.runtime_dll_found && result.models_path_found;
    const bool linux_plausible = result.header_found && result.shared_library_found &&
                                 result.feature_root_found && result.models_path_found;
    result.structurally_plausible =
        result.api_root_exists && result.runtime_root_exists && (windows_plausible || linux_plausible);

    return result;
}

}  // namespace nvafx
