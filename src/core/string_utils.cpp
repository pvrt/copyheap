#include "string_utils.h"
#include <windows.h>
#include <chrono>
#include <sstream>
#include <atomic>
#include <algorithm>

namespace copyheap {

std::wstring utf8_to_wide(std::string_view utf8) {
    if (utf8.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (size_needed <= 0) return L"";
    std::wstring result(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), &result[0], size_needed);
    return result;
}

std::string wide_to_utf8(std::wstring_view wide) {
    if (wide.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0) return "";
    std::string result(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), &result[0], size_needed, nullptr, nullptr);
    return result;
}

std::string trim(std::string_view str) {
    size_t first = 0;
    while (first < str.size() && (static_cast<unsigned char>(str[first]) <= ' ')) {
        first++;
    }
    if (first == str.size()) return "";
    size_t last = str.size() - 1;
    while (last > first && (static_cast<unsigned char>(str[last]) <= ' ')) {
        last--;
    }
    return std::string(str.substr(first, last - first + 1));
}

std::string clean_single_line(std::string_view text) {
    // Extract first non-empty line
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find_first_of("\r\n", start);
        if (end == std::string_view::npos) {
            end = text.size();
        }
        std::string_view line = text.substr(start, end - start);
        std::string trimmed = trim(line);
        if (!trimmed.empty()) {
            // Replace any tabs/multiple spaces with single spaces
            std::string result;
            result.reserve(trimmed.size());
            bool in_space = false;
            for (char c : trimmed) {
                if (c == '\t' || c == ' ') {
                    if (!in_space) {
                        result += ' ';
                        in_space = true;
                    }
                } else {
                    result += c;
                    in_space = false;
                }
            }
            return result;
        }
        if (end == text.size()) break;
        if (end + 1 < text.size() && text[end] == '\r' && text[end + 1] == '\n') {
            start = end + 2;
        } else {
            start = end + 1;
        }
    }
    return "";
}

bool contains_case_insensitive(std::string_view haystack, std::string_view needle) {
    if (needle.empty()) return true;
    if (haystack.empty()) return false;

    // Convert both to wide string and lowercase using Windows API CharLowerBuffW
    // This correctly handles Cyrillic, Latin, and other Unicode ranges
    std::wstring wHaystack = utf8_to_wide(haystack);
    std::wstring wNeedle = utf8_to_wide(needle);

    if (!wHaystack.empty()) {
        CharLowerBuffW(&wHaystack[0], static_cast<DWORD>(wHaystack.size()));
    }
    if (!wNeedle.empty()) {
        CharLowerBuffW(&wNeedle[0], static_cast<DWORD>(wNeedle.size()));
    }

    return wHaystack.find(wNeedle) != std::wstring::npos;
}

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

std::string generate_id() {
    static std::atomic<uint64_t> counter{0};
    uint64_t count = ++counter;
    int64_t ts = now_ms();
    char buf[64];
    snprintf(buf, sizeof(buf), "%lld-%llu", static_cast<long long>(ts), static_cast<unsigned long long>(count));
    return std::string(buf);
}

std::string json_escape(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    out += buf;
                } else {
                    out += c;
                }
                break;
        }
    }
    return out;
}

std::string json_unescape(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char next = s[++i];
            switch (next) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    if (i + 4 < s.size()) {
                        std::string hex_str(s.substr(i + 1, 4));
                        try {
                            unsigned int codepoint = std::stoul(hex_str, nullptr, 16);
                            i += 4;
                            // Encode codepoint as UTF-8
                            if (codepoint <= 0x7F) {
                                out += static_cast<char>(codepoint);
                            } else if (codepoint <= 0x7FF) {
                                out += static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F));
                                out += static_cast<char>(0x80 | (codepoint & 0x3F));
                            } else {
                                out += static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F));
                                out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                                out += static_cast<char>(0x80 | (codepoint & 0x3F));
                            }
                        } catch (...) {
                            out += "\\u";
                        }
                    } else {
                        out += "\\u";
                    }
                    break;
                }
                default:
                    out += next;
                    break;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

} // namespace copyheap
