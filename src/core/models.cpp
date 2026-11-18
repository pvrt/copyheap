#include "models.h"
#include "string_utils.h"
#include <windows.h>

namespace copyheap {

std::string ClipboardItem::get_display_title() const {
    if (!title.empty()) {
        return title;
    }
    std::string first_line = clean_single_line(content);
    if (!first_line.empty()) {
        return first_line;
    }
    return "...";
}

void ClipboardItem::update_cached_lower() {
    title_lower = utf8_to_wide(title);
    if (!title_lower.empty()) {
        CharLowerBuffW(&title_lower[0], static_cast<DWORD>(title_lower.size()));
    }
    content_lower = utf8_to_wide(content);
    if (!content_lower.empty()) {
        CharLowerBuffW(&content_lower[0], static_cast<DWORD>(content_lower.size()));
    }
}

} // namespace copyheap
