#pragma once

#include <string>
#include <cstdint>
#include <vector>

namespace copyheap {

constexpr size_t MAX_UNPINNED_ITEMS = 200;
constexpr size_t MAX_TRAY_UNPINNED = 30;
constexpr size_t MAX_TRAY_ITEMS = 200;

struct ClipboardItem {
    std::string id;
    std::string title;
    std::string content;
    uint64_t content_hash = 0;
    bool is_pinned = false;
    int64_t created_at = 0; // ms since epoch
    int64_t pinned_at = 0;  // ms since epoch

    // Display title: title if non-empty, else first non-empty line of content
    std::string get_display_title() const;
};

enum class PinResult {
    Success,
    NotFound,
    LimitReached
};

} // namespace copyheap
