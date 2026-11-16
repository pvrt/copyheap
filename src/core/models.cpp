#include "models.h"
#include "string_utils.h"

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

} // namespace copyheap
