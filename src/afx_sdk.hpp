#pragma once

#include "wav.hpp"

#include <string>

namespace nvafx {

struct AfxProcessOptions {
    std::string effect;
    std::string model_path;
    std::string runtime_root;
    float intensity = 1.0F;
};

AudioBuffer process_with_afx_sdk(const AudioBuffer& input, const AfxProcessOptions& options);
bool afx_sdk_build_enabled();

}  // namespace nvafx
