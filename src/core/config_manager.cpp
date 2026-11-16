#include "config_manager.h"
#include "string_utils.h"

#include <windows.h>
#include <shlobj.h>
#include <sstream>

namespace copyheap {

static std::wstring get_default_config_path() {
    wchar_t localAppData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, localAppData))) {
        std::wstring dir = std::wstring(localAppData) + L"\\CopyHeap";
        CreateDirectoryW(dir.c_str(), nullptr);
        return dir + L"\\config.json";
    }
    return L"config.json";
}

ConfigManager::ConfigManager(const std::wstring& customPath) {
    if (!customPath.empty()) {
        m_configPath = customPath;
    } else {
        m_configPath = get_default_config_path();
    }
}

const AppConfig& ConfigManager::get_config() const {
    return m_config;
}

void ConfigManager::set_config(const AppConfig& cfg) {
    m_config = cfg;
}

void ConfigManager::set_github_token(const std::string& token) {
    m_config.github_token = token;
}

void ConfigManager::set_gist_id(const std::string& gist_id) {
    m_config.gist_id = gist_id;
}

void ConfigManager::set_sync_enabled(bool enabled) {
    m_config.sync_enabled = enabled;
}

void ConfigManager::set_last_sync_time(int64_t ts) {
    m_config.last_sync_time = ts;
}

std::wstring ConfigManager::get_config_path() const {
    return m_configPath;
}

bool ConfigManager::load() {
    HANDLE hFile = CreateFileW(m_configPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize) || fileSize.QuadPart == 0 || fileSize.QuadPart > 5 * 1024 * 1024) {
        CloseHandle(hFile);
        return false;
    }

    std::string content(static_cast<size_t>(fileSize.QuadPart), 0);
    DWORD bytesRead = 0;
    BOOL ok = ReadFile(hFile, &content[0], static_cast<DWORD>(content.size()), &bytesRead, nullptr);
    CloseHandle(hFile);

    if (!ok) return false;

    auto get_str_field = [&](const std::string& key) -> std::string {
        std::string pattern = "\"" + key + "\":";
        size_t pos = content.find(pattern);
        if (pos == std::string::npos) return "";
        pos = content.find('"', pos + pattern.size());
        if (pos == std::string::npos) return "";
        size_t end = content.find('"', pos + 1);
        if (end == std::string::npos) return "";
        return content.substr(pos + 1, end - pos - 1);
    };

    auto get_bool_field = [&](const std::string& key) -> bool {
        std::string pattern = "\"" + key + "\":";
        size_t pos = content.find(pattern);
        if (pos == std::string::npos) return false;
        size_t valPos = content.find_first_not_of(" \t\r\n", pos + pattern.size());
        if (valPos == std::string::npos) return false;
        return content.substr(valPos, 4) == "true";
    };

    auto get_int64_field = [&](const std::string& key) -> int64_t {
        std::string pattern = "\"" + key + "\":";
        size_t pos = content.find(pattern);
        if (pos == std::string::npos) return 0;
        size_t valPos = content.find_first_not_of(" \t\r\n", pos + pattern.size());
        if (valPos == std::string::npos) return 0;
        try {
            return std::stoll(content.substr(valPos));
        } catch (...) {
            return 0;
        }
    };

    m_config.github_token = get_str_field("github_token");
    m_config.gist_id = get_str_field("gist_id");
    m_config.sync_enabled = get_bool_field("sync_enabled");
    m_config.last_sync_time = get_int64_field("last_sync_time");

    return true;
}

bool ConfigManager::save() {
    std::ostringstream ss;
    ss << "{\n"
       << "  \"github_token\": \"" << json_escape(m_config.github_token) << "\",\n"
       << "  \"gist_id\": \"" << json_escape(m_config.gist_id) << "\",\n"
       << "  \"sync_enabled\": " << (m_config.sync_enabled ? "true" : "false") << ",\n"
       << "  \"last_sync_time\": " << m_config.last_sync_time << "\n"
       << "}\n";

    std::string jsonStr = ss.str();
    std::wstring tmpPath = m_configPath + L".tmp";

    HANDLE hFile = CreateFileW(tmpPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD bytesWritten = 0;
    BOOL ok = WriteFile(hFile, jsonStr.data(), static_cast<DWORD>(jsonStr.size()), &bytesWritten, nullptr);
    CloseHandle(hFile);

    if (!ok || bytesWritten != jsonStr.size()) {
        DeleteFileW(tmpPath.c_str());
        return false;
    }

    if (!MoveFileExW(tmpPath.c_str(), m_configPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        CopyFileW(tmpPath.c_str(), m_configPath.c_str(), FALSE);
        DeleteFileW(tmpPath.c_str());
    }

    return true;
}

} // namespace copyheap
