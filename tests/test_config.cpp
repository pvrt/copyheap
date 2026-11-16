#include "test_helper.h"
#include "core/config_manager.h"

#include <filesystem>

using namespace copyheap;

TEST(Config, SaveAndLoad) {
    std::wstring testPath = L"test_config_unit.json";
    if (std::filesystem::exists(testPath)) {
        std::filesystem::remove(testPath);
    }

    {
        ConfigManager mgr(testPath);
        mgr.set_github_token("ghp_test12345");
        mgr.set_gist_id("gist_abc987");
        mgr.set_sync_enabled(true);
        mgr.set_last_sync_time(1700000000);
        mgr.set_autostart_enabled(false);
        bool ok = mgr.save();
        ASSERT_TRUE(ok);
    }

    {
        ConfigManager mgr(testPath);
        bool ok = mgr.load();
        ASSERT_TRUE(ok);
        const auto& cfg = mgr.get_config();
        ASSERT_EQ(cfg.github_token, "ghp_test12345");
        ASSERT_EQ(cfg.gist_id, "gist_abc987");
        ASSERT_TRUE(cfg.sync_enabled);
        ASSERT_EQ(cfg.last_sync_time, 1700000000);
        ASSERT_FALSE(cfg.autostart_enabled);
    }

    std::filesystem::remove(testPath);
}
