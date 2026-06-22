#include <windows.h>
#include <msiquery.h>

#include <algorithm>
#include <cwctype>
#include <string>
#include <vector>

namespace {

constexpr const wchar_t* kEnvironmentKey = L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment";
constexpr const wchar_t* kPathValueName = L"Path";
constexpr const wchar_t* kInstallFolderKey = L"INSTALLFOLDER=";

void log_message(MSIHANDLE install, const std::wstring& message) {
    PMSIHANDLE record = MsiCreateRecord(1);
    if (record != 0) {
        MsiRecordSetStringW(record, 0, message.c_str());
        MsiProcessMessage(install, INSTALLMESSAGE_INFO, record);
    }
}

std::wstring trim(std::wstring value) {
    auto is_space = [](wchar_t ch) { return std::iswspace(ch) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](wchar_t ch) { return !is_space(ch); }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](wchar_t ch) { return !is_space(ch); }).base(), value.end());
    if (value.size() >= 2 && value.front() == L'"' && value.back() == L'"') {
        value = value.substr(1, value.size() - 2);
    }
    return value;
}

bool is_drive_root(const std::wstring& value) {
    return value.size() == 3 && std::iswalpha(value[0]) && value[1] == L':' && value[2] == L'\\';
}

std::wstring without_trailing_slashes(std::wstring value) {
    value = trim(std::move(value));
    std::replace(value.begin(), value.end(), L'/', L'\\');
    while (value.size() > 1 && !is_drive_root(value) && (value.back() == L'\\' || value.back() == L'/')) {
        value.pop_back();
    }
    return value;
}

std::wstring normalized_path(std::wstring value) {
    value = without_trailing_slashes(std::move(value));
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
}

std::vector<std::wstring> split_path(const std::wstring& path) {
    std::vector<std::wstring> parts;
    std::size_t start = 0;
    while (start <= path.size()) {
        const std::size_t separator = path.find(L';', start);
        if (separator == std::wstring::npos) {
            parts.push_back(path.substr(start));
            break;
        }
        parts.push_back(path.substr(start, separator - start));
        start = separator + 1;
    }
    return parts;
}

std::wstring join_path(const std::vector<std::wstring>& parts) {
    std::wstring joined;
    for (const std::wstring& part : parts) {
        if (part.empty()) {
            continue;
        }
        if (!joined.empty()) {
            joined += L';';
        }
        joined += part;
    }
    return joined;
}

UINT get_custom_action_install_folder(MSIHANDLE install, std::wstring* install_folder) {
    wchar_t data[32768] = {};
    DWORD data_size = static_cast<DWORD>(std::size(data));
    const UINT status = MsiGetPropertyW(install, L"CustomActionData", data, &data_size);
    if (status != ERROR_SUCCESS) {
        log_message(install, L"nvafx-audio-cli MSI PATH action failed to read CustomActionData.");
        return ERROR_INSTALL_FAILURE;
    }

    const std::wstring custom_action_data(data);
    if (custom_action_data.rfind(kInstallFolderKey, 0) != 0) {
        log_message(install, L"nvafx-audio-cli MSI PATH action received malformed CustomActionData.");
        return ERROR_INSTALL_FAILURE;
    }

    *install_folder = without_trailing_slashes(custom_action_data.substr(std::wcslen(kInstallFolderKey)));
    if (install_folder->empty()) {
        log_message(install, L"nvafx-audio-cli MSI PATH action received an empty INSTALLFOLDER.");
        return ERROR_INSTALL_FAILURE;
    }

    return ERROR_SUCCESS;
}

UINT read_machine_path(MSIHANDLE install, std::wstring* path, DWORD* value_type) {
    DWORD type = 0;
    DWORD bytes = 0;
    LSTATUS status = RegGetValueW(HKEY_LOCAL_MACHINE, kEnvironmentKey, kPathValueName,
                                  RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND,
                                  &type, nullptr, &bytes);
    if (status == ERROR_FILE_NOT_FOUND) {
        *path = L"";
        *value_type = REG_EXPAND_SZ;
        return ERROR_SUCCESS;
    }
    if (status != ERROR_SUCCESS) {
        log_message(install, L"nvafx-audio-cli MSI PATH action failed to query machine PATH size.");
        return ERROR_INSTALL_FAILURE;
    }

    std::vector<wchar_t> buffer((bytes / sizeof(wchar_t)) + 1, L'\0');
    status = RegGetValueW(HKEY_LOCAL_MACHINE, kEnvironmentKey, kPathValueName,
                          RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND,
                          &type, buffer.data(), &bytes);
    if (status != ERROR_SUCCESS) {
        log_message(install, L"nvafx-audio-cli MSI PATH action failed to read machine PATH.");
        return ERROR_INSTALL_FAILURE;
    }

    *path = buffer.data();
    *value_type = (type == REG_SZ) ? REG_SZ : REG_EXPAND_SZ;
    return ERROR_SUCCESS;
}

UINT write_machine_path(MSIHANDLE install, const std::wstring& path, DWORD value_type) {
    HKEY key = nullptr;
    LSTATUS status = RegCreateKeyExW(HKEY_LOCAL_MACHINE, kEnvironmentKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);
    if (status != ERROR_SUCCESS) {
        log_message(install, L"nvafx-audio-cli MSI PATH action failed to open machine environment key.");
        return ERROR_INSTALL_FAILURE;
    }

    const DWORD bytes = static_cast<DWORD>((path.size() + 1) * sizeof(wchar_t));
    status = RegSetValueExW(key, kPathValueName, 0, value_type, reinterpret_cast<const BYTE*>(path.c_str()), bytes);
    RegCloseKey(key);
    if (status != ERROR_SUCCESS) {
        log_message(install, L"nvafx-audio-cli MSI PATH action failed to write machine PATH.");
        return ERROR_INSTALL_FAILURE;
    }

    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, reinterpret_cast<LPARAM>(L"Environment"),
                        SMTO_ABORTIFHUNG, 5000, nullptr);
    return ERROR_SUCCESS;
}

UINT update_machine_path(MSIHANDLE install, bool add_entry) {
    std::wstring install_folder;
    UINT status = get_custom_action_install_folder(install, &install_folder);
    if (status != ERROR_SUCCESS) {
        return status;
    }

    std::wstring current_path;
    DWORD value_type = REG_EXPAND_SZ;
    status = read_machine_path(install, &current_path, &value_type);
    if (status != ERROR_SUCCESS) {
        return status;
    }

    const std::wstring target_normalized = normalized_path(install_folder);
    std::vector<std::wstring> updated;
    for (const std::wstring& part : split_path(current_path)) {
        if (trim(part).empty()) {
            continue;
        }
        if (normalized_path(part) != target_normalized) {
            updated.push_back(part);
        }
    }

    if (add_entry) {
        updated.push_back(install_folder);
    }

    status = write_machine_path(install, join_path(updated), value_type);
    if (status == ERROR_SUCCESS) {
        log_message(install, add_entry ? L"nvafx-audio-cli MSI PATH entry registered." :
                                         L"nvafx-audio-cli MSI PATH entry removed.");
    }
    return status;
}

}  // namespace

extern "C" __declspec(dllexport) UINT __stdcall AddInstallFolderToPath(MSIHANDLE install) {
    return update_machine_path(install, true);
}

extern "C" __declspec(dllexport) UINT __stdcall RemoveInstallFolderFromPath(MSIHANDLE install) {
    return update_machine_path(install, false);
}
