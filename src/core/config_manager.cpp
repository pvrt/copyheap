#include "config_manager.h"
#include "string_utils.h"

#include <windows.h>
#include <wincrypt.h>
#include <shlobj.h>
#include <sstream>
#include <vector>

namespace copyheap {

static std::string protect_token_dpapi(const std::string& token) {
    if (token.empty()) return "";
    DATA_BLOB dataIn;
    dataIn.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(token.data()));
    dataIn.cbData = static_cast<DWORD>(token.size());

    DATA_BLOB dataOut;
    if (CryptProtectData(&dataIn, L"CopyHeap GitHub Token", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &dataOut)) {
        DWORD strLen = 0;
        CryptBinaryToStringA(dataOut.pbData, dataOut.cbData, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &strLen);
        std::string b64(strLen, 0);
        if (CryptBinaryToStringA(dataOut.pbData, dataOut.cbData, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &b64[0], &strLen)) {
            while (!b64.empty() && (b64.back() == '\0' || b64.back() == '\r' || b64.back() == '\n')) {
                b64.pop_back();
            }
            LocalFree(dataOut.pbData);
            return b64;
        }
        LocalFree(dataOut.pbData);
    }
    return "";
}

static std::string unprotect_token_dpapi(const std::string& b64) {
    if (b64.empty()) return "";
    DWORD binLen = 0;
    if (!CryptStringToBinaryA(b64.data(), static_cast<DWORD>(b64.size()), CRYPT_STRING_BASE64, nullptr, &binLen, nullptr, nullptr)) {
        return "";
    }
    std::vector<BYTE> cipher(binLen);
    if (!CryptStringToBinaryA(b64.data(), static_cast<DWORD>(b64.size()), CRYPT_STRING_BASE64, cipher.data(), &binLen, nullptr, nullptr)) {
        return "";
    }

    DATA_BLOB dataIn;
    dataIn.pbData = cipher.data();
    dataIn.cbData = binLen;

    DATA_BLOB dataOut;
    if (CryptUnprotectData(&dataIn, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &dataOut)) {
        std::string token(reinterpret_cast<char*>(dataOut.pbData), dataOut.cbData);
        LocalFree(dataOut.pbData);
        return token;
    }
    return "";
}

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

void ConfigManager::set_autostart_enabled(bool enabled) {
    m_config.autostart_enabled = enabled;
}

std::wstring ConfigManager::get_config_path() const {
    return m_configPath;
}

static const wchar_t* AUTOSTART_REG_KEY = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t* AUTOSTART_APP_NAME = L"CopyHeap";

bool ConfigManager::is_autostart_registered() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, AUTOSTART_REG_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD type = 0;
        DWORD size = 0;
        LONG res = RegQueryValueExW(hKey, AUTOSTART_APP_NAME, nullptr, &type, nullptr, &size);
        RegCloseKey(hKey);
        return (res == ERROR_SUCCESS);
    }
    return false;
}

bool ConfigManager::set_autostart_registered(bool enable) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, AUTOSTART_REG_KEY, 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    bool success = false;
    if (enable) {
        wchar_t exePath[MAX_PATH];
        if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) > 0) {
            std::wstring cmd = L"\"" + std::wstring(exePath) + L"\"";
            LONG res = RegSetValueExW(hKey, AUTOSTART_APP_NAME, 0, REG_SZ,
                                      reinterpret_cast<const BYTE*>(cmd.c_str()),
                                      static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t)));
            success = (res == ERROR_SUCCESS);
        }
    } else {
        LONG res = RegDeleteValueW(hKey, AUTOSTART_APP_NAME);
        success = (res == ERROR_SUCCESS || res == ERROR_FILE_NOT_FOUND);
    }

    RegCloseKey(hKey);
    return success;
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

    std::string protectedToken = get_str_field("github_token_encrypted");
    if (!protectedToken.empty()) {
        m_config.github_token = unprotect_token_dpapi(protectedToken);
    } else {
        // Fallback for migration from unencrypted format
        m_config.github_token = get_str_field("github_token");
    }
    m_config.gist_id = get_str_field("gist_id");
    m_config.sync_enabled = get_bool_field("sync_enabled");
    m_config.last_sync_time = get_int64_field("last_sync_time");

    // Default autostart_enabled is true if missing from json
    if (content.find("\"autostart_enabled\":") != std::string::npos) {
        m_config.autostart_enabled = get_bool_field("autostart_enabled");
    } else {
        m_config.autostart_enabled = true;
    }

    return true;
}

bool ConfigManager::save() {
    std::string protectedToken = protect_token_dpapi(m_config.github_token);

    std::ostringstream ss;
    ss << "{\n"
       << "  \"github_token_encrypted\": \"" << json_escape(protectedToken) << "\",\n"
       << "  \"gist_id\": \"" << json_escape(m_config.gist_id) << "\",\n"
       << "  \"sync_enabled\": " << (m_config.sync_enabled ? "true" : "false") << ",\n"
       << "  \"last_sync_time\": " << m_config.last_sync_time << ",\n"
       << "  \"autostart_enabled\": " << (m_config.autostart_enabled ? "true" : "false") << "\n"
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
