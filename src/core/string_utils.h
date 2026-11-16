#pragma once

#include <string>
#include <string_view>
#include <cstdint>

namespace copyheap {

std::wstring utf8_to_wide(std::string_view utf8);
std::string wide_to_utf8(std::wstring_view wide);

std::string trim(std::string_view str);
std::string clean_single_line(std::string_view text);

// Case-insensitive substring search for UTF-8 (supports cyrillic & latin)
bool contains_case_insensitive(std::string_view haystack, std::string_view needle);

int64_t now_ms();
std::string generate_id();

// JSON helpers
std::string json_escape(std::string_view s);
std::string json_unescape(std::string_view s);

} // namespace copyheap
