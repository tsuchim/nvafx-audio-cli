#pragma once

#include <string>
#include <vector>

namespace nvafx {

struct SdkProbeResult {
    std::string root;
    bool exists = false;
    bool structurally_plausible = false;
    std::vector<std::string> present_subpaths;
    std::vector<std::string> missing_subpaths;
};

std::string resolve_sdk_root(const std::string& cli_sdk_root);
SdkProbeResult probe_sdk_root(const std::string& root);

}  // namespace nvafx
