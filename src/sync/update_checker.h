#pragma once

#include <string>
#include <optional>
#include <functional>

namespace copyheap {

struct UpdateInfo {
    std::string latest_version;
    std::string release_url;
    std::string release_notes;
    bool update_available = false;
};

class UpdateChecker {
public:
    static constexpr const char* CURRENT_VERSION = "v1.0.0";

    // Checks GitHub latest release for copyheap repo via WinHTTP asynchronously
    static void check_for_updates_async(std::function<void(const UpdateInfo& info)> callback);

    // Synchronous check (runs in worker thread)
    static std::optional<UpdateInfo> check_for_updates_sync();
};

} // namespace copyheap
