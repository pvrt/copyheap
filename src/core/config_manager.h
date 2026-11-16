#pragma once

#include <string>
#include <cstdint>

namespace copyheap {

struct AppConfig {
    std::string github_token;
    std::string gist_id;
    bool sync_enabled = false;
    int64_t last_sync_time = 0;
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

    std::wstring get_config_path() const;

private:
    std::wstring m_configPath;
    AppConfig m_config;
};

} // namespace copyheap
