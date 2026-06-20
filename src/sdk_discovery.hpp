#pragma once

#include <string>
#include <vector>

namespace nvafx {

struct SdkProbeResult {
    std::string api_root;
    std::string runtime_root;
    bool api_root_exists = false;
    bool runtime_root_exists = false;
    bool header_found = false;
    bool import_library_found = false;
    bool runtime_dll_found = false;
    bool models_path_found = false;
    bool denoiser_model_found = false;
    bool dereverb_model_found = false;
    bool dereverb_denoiser_model_found = false;
    bool structurally_plausible = false;
    std::vector<std::string> present_subpaths;
    std::vector<std::string> missing_subpaths;
    std::vector<std::string> libraries;
    std::vector<std::string> runtime_dlls;
    std::vector<std::string> models;
};

std::string resolve_api_root(const std::string& cli_api_root);
std::string resolve_runtime_root(const std::string& cli_runtime_root, const std::string& cli_sdk_root);
SdkProbeResult probe_sdk_roots(const std::string& api_root, const std::string& runtime_root);

}  // namespace nvafx
