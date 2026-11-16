#pragma once

#include <string>
#include <cstdint>

namespace copyheap {

struct AppConfig {
    std::string github_token;
    std::string gist_id;
    bool sync_enabled = false;
    int64_t last_sync_time = 0;
    bool autostart_enabled = true;
};

class ConfigManager {
public:
    explicit ConfigManager(const std::wstring& customPath = L"");
    ~ConfigManager() = default;

    bool load();
    bool save();

    const AppConfig& get_config() const;
    void set_config(const AppConfig& cfg);

    void set_github_token(const std::string& token);
    void set_gist_id(const std::string& gist_id);
    void set_sync_enabled(bool enabled);
    void set_last_sync_time(int64_t ts);
    void set_autostart_enabled(bool enabled);

    std::wstring get_config_path() const;

    // Windows startup registry helpers
    static bool is_autostart_registered();
    static bool set_autostart_registered(bool enable);

private:
    std::wstring m_configPath;
    AppConfig m_config;
};

} // namespace copyheap
