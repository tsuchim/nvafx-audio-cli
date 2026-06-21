#pragma once

#include <string>

namespace nvafx {

struct PipeSafetyCheck {
    bool allowed = true;
    std::string parent_process;
    std::string message;
};

void set_stdin_binary_mode();
void set_stdout_binary_mode();
PipeSafetyCheck check_pipe_safety(bool allow_unsafe_pipe);
std::string pipe_safety_warning_text();

}  // namespace nvafx
