#include "stdio_binary.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#include <tlhelp32.h>
#include <vector>
#include <winver.h>
#endif

namespace nvafx {
namespace {

std::runtime_error stdio_error(const std::string& message) {
    return std::runtime_error(message);
}

#ifdef _WIN32

std::string narrow_ascii(const wchar_t* value) {
    std::string output;
    if (value == nullptr) {
        return output;
    }
    while (*value != L'\0') {
        const wchar_t ch = *value++;
        output.push_back(ch >= 0 && ch <= 127 ? static_cast<char>(ch) : '?');
    }
    return output;
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

DWORD parent_process_id() {
    const DWORD current_pid = GetCurrentProcessId();
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    DWORD parent = 0;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (entry.th32ProcessID == current_pid) {
                parent = entry.th32ParentProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return parent;
}

std::string process_name(DWORD pid) {
    if (pid == 0) {
        return {};
    }

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return {};
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    std::string name;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (entry.th32ProcessID == pid) {
                name = narrow_ascii(entry.szExeFile);
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return name;
}

std::wstring process_path(DWORD pid) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process == nullptr) {
        return {};
    }

    std::wstring path(32768, L'\0');
    DWORD size = static_cast<DWORD>(path.size());
    if (!QueryFullProcessImageNameW(process, 0, path.data(), &size)) {
        CloseHandle(process);
        return {};
    }
    CloseHandle(process);
    path.resize(size);
    return path;
}

bool file_version_at_least_7_4(const std::wstring& path) {
    if (path.empty()) {
        return false;
    }

    DWORD handle = 0;
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &handle);
    if (size == 0) {
        return false;
    }

    std::vector<unsigned char> data(size);
    if (!GetFileVersionInfoW(path.c_str(), handle, size, data.data())) {
        return false;
    }

    VS_FIXEDFILEINFO* info = nullptr;
    UINT info_size = 0;
    if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&info), &info_size) ||
        info == nullptr || info_size == 0) {
        return false;
    }

    const auto major = static_cast<unsigned>(HIWORD(info->dwFileVersionMS));
    const auto minor = static_cast<unsigned>(LOWORD(info->dwFileVersionMS));
    return major > 7 || (major == 7 && minor >= 4);
}

void set_binary_mode(FILE* stream, int fd, const char* name) {
    (void)stream;
    if (_setmode(fd, _O_BINARY) == -1) {
        throw stdio_error(std::string("failed to set ") + name + " to binary mode");
    }
}

#endif

}  // namespace

std::string pipe_safety_warning_text() {
    return "PowerShell 7.4+ is required for native byte-stream piping. Do not use 2>&1 with --output -. Do not place PowerShell cmdlets in the middle of binary WAV pipelines. Use temporary WAV files if unsure.";
}

void set_stdin_binary_mode() {
#ifdef _WIN32
    set_binary_mode(stdin, _fileno(stdin), "stdin");
#endif
}

void set_stdout_binary_mode() {
#ifdef _WIN32
    set_binary_mode(stdout, _fileno(stdout), "stdout");
#endif
}

PipeSafetyCheck check_pipe_safety(bool allow_unsafe_pipe) {
    PipeSafetyCheck result;
#ifdef _WIN32
    const DWORD parent_pid = parent_process_id();
    result.parent_process = process_name(parent_pid);
    const std::string parent = lowercase(result.parent_process);

    if (allow_unsafe_pipe) {
        result.allowed = true;
        result.message = "--allow-unsafe-pipe was used. " + pipe_safety_warning_text();
        return result;
    }

    if (parent == "cmd.exe") {
        result.allowed = true;
        return result;
    }

    if (parent == "powershell.exe") {
        result.allowed = false;
        result.message = "Windows PowerShell is not safe for binary WAV pipes. " + pipe_safety_warning_text();
        return result;
    }

    if (parent == "pwsh.exe") {
        if (file_version_at_least_7_4(process_path(parent_pid))) {
            result.allowed = true;
            return result;
        }
        result.allowed = false;
        result.message = "PowerShell 7.4 or newer is required for binary WAV pipes. " + pipe_safety_warning_text();
        return result;
    }

    result.allowed = false;
    result.message = "Cannot determine a safe parent shell for binary WAV pipes. " + pipe_safety_warning_text();
#else
    (void)allow_unsafe_pipe;
    result.allowed = true;
#endif
    return result;
}

}  // namespace nvafx
