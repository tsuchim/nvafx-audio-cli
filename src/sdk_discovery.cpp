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

}  // namespace

std::string resolve_sdk_root(const std::string& cli_sdk_root) {
    if (!cli_sdk_root.empty()) {
        return cli_sdk_root;
    }

    return read_environment_variable("AFX_SDK_ROOT");
}

SdkProbeResult probe_sdk_root(const std::string& root) {
    SdkProbeResult result;
    result.root = root;

    if (root.empty()) {
        return result;
    }

    const std::filesystem::path root_path(root);
    result.exists = std::filesystem::exists(root_path);

    const std::vector<std::string> expected = {
        "include",
        "lib",
        "bin",
        "models",
    };

    for (const std::string& subpath : expected) {
        const bool present = std::filesystem::exists(root_path / subpath);
        if (present) {
            result.present_subpaths.push_back(subpath);
        } else {
            result.missing_subpaths.push_back(subpath);
        }
    }

    return result;
}

}  // namespace nvafx
