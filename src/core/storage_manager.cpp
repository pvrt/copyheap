#include "storage_manager.h"
#include "fnv_hash.h"
#include "string_utils.h"

#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <algorithm>
#include <sstream>

namespace copyheap {

StorageManager::StorageManager() {
    m_storage_path = get_default_storage_path();
    m_save_thread = std::thread(&StorageManager::save_worker_loop, this);
}

StorageManager::~StorageManager() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stop_worker = true;
    }
    m_cv.notify_all();
    if (m_save_thread.joinable()) {
        m_save_thread.join();
    }
    // Final flush
    if (m_save_pending) {
        save_to_file(m_storage_path);
        m_save_pending = false;
    }
}

std::wstring StorageManager::get_default_storage_path() {
    wchar_t localAppData[MAX_PATH] = {0};
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH) > 0) {
        std::wstring dir = std::wstring(localAppData) + L"\\CopyHeap";
        CreateDirectoryW(dir.c_str(), nullptr);
        return dir + L"\\history.json";
    }
    return L"history.json";
}

void StorageManager::set_storage_path(const std::wstring& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_storage_path = path;
}

const std::wstring& StorageManager::get_storage_path() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_storage_path;
}

void StorageManager::sort_items_locked() {
    // Unpinned: created_at DESC
    std::sort(m_unpinned.begin(), m_unpinned.end(), [](const ClipboardItem& a, const ClipboardItem& b) {
        return a.created_at > b.created_at;
    });

    // Pinned: pinned_at DESC (newer pinned top, older below)
    std::sort(m_pinned.begin(), m_pinned.end(), [](const ClipboardItem& a, const ClipboardItem& b) {
        return a.pinned_at > b.pinned_at;
    });
}

void StorageManager::enforce_unpinned_limit_locked() {
    if (m_unpinned.size() > MAX_UNPINNED_ITEMS) {
        m_unpinned.resize(MAX_UNPINNED_ITEMS);
    }
}

bool StorageManager::add_or_update_from_clipboard(const std::string& content) {
    if (trim(content).empty()) {
        return false;
    }

    uint64_t hash = fnv1a_64(content);
    int64_t ts = now_ms();

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // 1. Check pinned items: if matches pinned item, duplicate is NOT added to unpinned
        for (const auto& pin : m_pinned) {
            if (pin.content_hash == hash && pin.content == content) {
                return false;
            }
        }

        // 2. Check unpinned items: if matches, update created_at, preserve title, move to top
        for (auto it = m_unpinned.begin(); it != m_unpinned.end(); ++it) {
            if (it->content_hash == hash && it->content == content) {
                ClipboardItem existing = *it;
                existing.created_at = ts;
                m_unpinned.erase(it);
                m_unpinned.insert(m_unpinned.begin(), existing);
                request_save_debounced();
                return true;
            }
        }

        // 3. New unpinned item
        ClipboardItem newItem;
        newItem.id = generate_id();
        newItem.content = content;
        newItem.content_hash = hash;
        newItem.is_pinned = false;
        newItem.created_at = ts;
        newItem.pinned_at = 0;

        m_unpinned.insert(m_unpinned.begin(), newItem);
        enforce_unpinned_limit_locked();
    }

    request_save_debounced();
    return true;
}

std::string StorageManager::add_item(const std::string& content, const std::string& title, bool is_pinned) {
    if (trim(content).empty() && trim(title).empty()) {
        return "";
    }

    int64_t ts = now_ms();
    ClipboardItem item;
    item.id = generate_id();
    item.title = title;
    item.content = content;
    item.content_hash = fnv1a_64(content);
    item.is_pinned = is_pinned;
    item.created_at = ts;
    item.pinned_at = is_pinned ? ts : 0;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (is_pinned) {
            if (m_pinned.size() >= MAX_PINNED_ITEMS) {
                return "";
            }
            m_pinned.insert(m_pinned.begin(), item);
        } else {
            m_unpinned.insert(m_unpinned.begin(), item);
            enforce_unpinned_limit_locked();
        }
    }

    request_save_debounced();
    return item.id;
}

bool StorageManager::update_item(const std::string& id, const std::string& new_title, const std::string& new_content) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto update_in_vec = [&](std::vector<ClipboardItem>& vec) -> bool {
        for (auto& item : vec) {
            if (item.id == id) {
                item.title = new_title;
                item.content = new_content;
                item.content_hash = fnv1a_64(new_content);
                return true;
            }
        }
        return false;
    };

    if (update_in_vec(m_unpinned) || update_in_vec(m_pinned)) {
        request_save_debounced();
        return true;
    }
    return false;
}

bool StorageManager::delete_item(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto erase_from_vec = [&](std::vector<ClipboardItem>& vec) -> bool {
        for (auto it = vec.begin(); it != vec.end(); ++it) {
            if (it->id == id) {
                vec.erase(it);
                return true;
            }
        }
        return false;
    };

    if (erase_from_vec(m_unpinned) || erase_from_vec(m_pinned)) {
        request_save_debounced();
        return true;
    }
    return false;
}

PinResult StorageManager::set_pinned(const std::string& id, bool pinned) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (pinned) {
        // Find in unpinned
        for (auto it = m_unpinned.begin(); it != m_unpinned.end(); ++it) {
            if (it->id == id) {
                if (m_pinned.size() >= MAX_PINNED_ITEMS) {
                    return PinResult::LimitReached;
                }
                ClipboardItem item = *it;
                item.is_pinned = true;
                item.pinned_at = now_ms();
                m_unpinned.erase(it);
                m_pinned.insert(m_pinned.begin(), item);
                sort_items_locked();
                request_save_debounced();
                return PinResult::Success;
            }
        }
        // Already pinned?
        for (const auto& item : m_pinned) {
            if (item.id == id) {
                return PinResult::Success;
            }
        }
        return PinResult::NotFound;
    } else {
        // Unpin
        for (auto it = m_pinned.begin(); it != m_pinned.end(); ++it) {
            if (it->id == id) {
                ClipboardItem item = *it;
                item.is_pinned = false;
                item.pinned_at = 0;
                m_pinned.erase(it);
                m_unpinned.insert(m_unpinned.begin(), item);
                enforce_unpinned_limit_locked();
                sort_items_locked();
                request_save_debounced();
                return PinResult::Success;
            }
        }
        // Already unpinned?
        for (const auto& item : m_unpinned) {
            if (item.id == id) {
                return PinResult::Success;
            }
        }
        return PinResult::NotFound;
    }
}

PinResult StorageManager::toggle_pin(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& item : m_pinned) {
        if (item.id == id) {
            // currently pinned -> unpin
            ClipboardItem itemCopy = item;
            itemCopy.is_pinned = false;
            itemCopy.pinned_at = 0;
            auto it = std::find_if(m_pinned.begin(), m_pinned.end(), [&](const ClipboardItem& i){ return i.id == id; });
            if (it != m_pinned.end()) m_pinned.erase(it);
            m_unpinned.insert(m_unpinned.begin(), itemCopy);
            enforce_unpinned_limit_locked();
            sort_items_locked();
            request_save_debounced();
            return PinResult::Success;
        }
    }
    for (const auto& item : m_unpinned) {
        if (item.id == id) {
            // currently unpinned -> pin
            if (m_pinned.size() >= MAX_PINNED_ITEMS) {
                return PinResult::LimitReached;
            }
            ClipboardItem itemCopy = item;
            itemCopy.is_pinned = true;
            itemCopy.pinned_at = now_ms();
            auto it = std::find_if(m_unpinned.begin(), m_unpinned.end(), [&](const ClipboardItem& i){ return i.id == id; });
            if (it != m_unpinned.end()) m_unpinned.erase(it);
            m_pinned.insert(m_pinned.begin(), itemCopy);
            sort_items_locked();
            request_save_debounced();
            return PinResult::Success;
        }
    }
    return PinResult::NotFound;
}

std::vector<ClipboardItem> StorageManager::get_all_items() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ClipboardItem> result;
    result.reserve(m_unpinned.size() + m_pinned.size());
    result.insert(result.end(), m_unpinned.begin(), m_unpinned.end());
    result.insert(result.end(), m_pinned.begin(), m_pinned.end());
    return result;
}

std::vector<ClipboardItem> StorageManager::get_unpinned_items() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_unpinned;
}

std::vector<ClipboardItem> StorageManager::get_pinned_items() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pinned;
}

std::pair<std::vector<ClipboardItem>, std::vector<ClipboardItem>> StorageManager::get_tray_sections() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ClipboardItem> trayUnpinned;
    size_t countUnpinned = (std::min)(m_unpinned.size(), MAX_TRAY_UNPINNED);
    trayUnpinned.assign(m_unpinned.begin(), m_unpinned.begin() + countUnpinned);

    std::vector<ClipboardItem> trayPinned;
    size_t countPinned = (std::min)(m_pinned.size(), MAX_TRAY_PINNED);
    trayPinned.assign(m_pinned.begin(), m_pinned.begin() + countPinned);

    return {trayUnpinned, trayPinned};
}

std::vector<ClipboardItem> StorageManager::search(const std::string& query) const {
    std::string q = trim(query);
    if (q.empty()) {
        return get_all_items();
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ClipboardItem> result;

    for (const auto& item : m_unpinned) {
        if (contains_case_insensitive(item.title, q) || contains_case_insensitive(item.content, q)) {
            result.push_back(item);
        }
    }
    for (const auto& item : m_pinned) {
        if (contains_case_insensitive(item.title, q) || contains_case_insensitive(item.content, q)) {
            result.push_back(item);
        }
    }
    return result;
}

std::optional<ClipboardItem> StorageManager::get_item(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& item : m_unpinned) {
        if (item.id == id) return item;
    }
    for (const auto& item : m_pinned) {
        if (item.id == id) return item;
    }
    return std::nullopt;
}

size_t StorageManager::unpinned_count() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_unpinned.size();
}

size_t StorageManager::pinned_count() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pinned.size();
}

size_t StorageManager::total_count() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_unpinned.size() + m_pinned.size();
}

void StorageManager::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_unpinned.clear();
    m_pinned.clear();
    request_save_debounced();
}

// Minimal streaming JSON serializer & parser
bool StorageManager::save_to_file(const std::wstring& file_path) const {
    std::vector<ClipboardItem> allItems;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        allItems.reserve(m_unpinned.size() + m_pinned.size());
        allItems.insert(allItems.end(), m_unpinned.begin(), m_unpinned.end());
        allItems.insert(allItems.end(), m_pinned.begin(), m_pinned.end());
    }

    std::ostringstream ss;
    ss << "[\n";
    for (size_t i = 0; i < allItems.size(); ++i) {
        const auto& item = allItems[i];
        ss << "  {\n";
        ss << "    \"id\": \"" << json_escape(item.id) << "\",\n";
        ss << "    \"title\": \"" << json_escape(item.title) << "\",\n";
        ss << "    \"content\": \"" << json_escape(item.content) << "\",\n";
        ss << "    \"content_hash\": " << item.content_hash << ",\n";
        ss << "    \"is_pinned\": " << (item.is_pinned ? "true" : "false") << ",\n";
        ss << "    \"created_at\": " << item.created_at << ",\n";
        ss << "    \"pinned_at\": " << item.pinned_at << "\n";
        ss << "  }" << (i + 1 < allItems.size() ? "," : "") << "\n";
    }
    ss << "]\n";

    std::string jsonStr = ss.str();
    std::wstring tmpPath = file_path + L".tmp";

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

    // Atomic replace
    if (!MoveFileExW(tmpPath.c_str(), file_path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        // Fallback for some filesystems
        CopyFileW(tmpPath.c_str(), file_path.c_str(), FALSE);
        DeleteFileW(tmpPath.c_str());
    }

    return true;
}

// Simple state-machine parser for the specific JSON format
bool StorageManager::load_from_file(const std::wstring& file_path) {
    HANDLE hFile = CreateFileW(file_path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize) || fileSize.QuadPart == 0 || fileSize.QuadPart > 50 * 1024 * 1024) {
        CloseHandle(hFile);
        return false;
    }

    std::string content(static_cast<size_t>(fileSize.QuadPart), 0);
    DWORD bytesRead = 0;
    BOOL ok = ReadFile(hFile, &content[0], static_cast<DWORD>(content.size()), &bytesRead, nullptr);
    CloseHandle(hFile);

    if (!ok) return false;

    std::vector<ClipboardItem> unpinned;
    std::vector<ClipboardItem> pinned;

    size_t pos = 0;
    auto skip_ws = [&]() {
        while (pos < content.size() && (unsigned char)content[pos] <= ' ') pos++;
    };

    auto match_char = [&](char c) -> bool {
        skip_ws();
        if (pos < content.size() && content[pos] == c) {
            pos++;
            return true;
        }
        return false;
    };

    auto parse_string = [&]() -> std::string {
        skip_ws();
        if (pos >= content.size() || content[pos] != '"') return "";
        pos++; // skip open quote
        size_t start = pos;
        while (pos < content.size()) {
            if (content[pos] == '\\') {
                pos += 2;
            } else if (content[pos] == '"') {
                break;
            } else {
                pos++;
            }
        }
        std::string raw = content.substr(start, pos - start);
        if (pos < content.size() && content[pos] == '"') pos++;
        return json_unescape(raw);
    };

    if (!match_char('[')) return false;

    while (pos < content.size()) {
        skip_ws();
        if (match_char(']')) break;
        if (!match_char('{')) break;

        ClipboardItem item;

        while (pos < content.size()) {
            skip_ws();
            if (match_char('}')) break;

            std::string key = parse_string();
            if (!match_char(':')) break;
            skip_ws();

            if (key == "id") {
                item.id = parse_string();
            } else if (key == "title") {
                item.title = parse_string();
            } else if (key == "content") {
                item.content = parse_string();
            } else if (key == "content_hash") {
                size_t num_start = pos;
                while (pos < content.size() && (content[pos] >= '0' && content[pos] <= '9')) pos++;
                try {
                    item.content_hash = std::stoull(content.substr(num_start, pos - num_start));
                } catch (...) {}
            } else if (key == "is_pinned") {
                if (content.compare(pos, 4, "true") == 0) {
                    item.is_pinned = true;
                    pos += 4;
                } else if (content.compare(pos, 5, "false") == 0) {
                    item.is_pinned = false;
                    pos += 5;
                }
            } else if (key == "created_at") {
                size_t num_start = pos;
                while (pos < content.size() && (content[pos] == '-' || (content[pos] >= '0' && content[pos] <= '9'))) pos++;
                try {
                    item.created_at = std::stoll(content.substr(num_start, pos - num_start));
                } catch (...) {}
            } else if (key == "pinned_at") {
                size_t num_start = pos;
                while (pos < content.size() && (content[pos] == '-' || (content[pos] >= '0' && content[pos] <= '9'))) pos++;
                try {
                    item.pinned_at = std::stoll(content.substr(num_start, pos - num_start));
                } catch (...) {}
            } else {
                // Unknown field: skip
                if (content[pos] == '"') {
                    parse_string();
                } else {
                    while (pos < content.size() && content[pos] != ',' && content[pos] != '}') pos++;
                }
            }

            match_char(',');
        }

        if (item.content_hash == 0 && !item.content.empty()) {
            item.content_hash = fnv1a_64(item.content);
        }

        if (item.is_pinned) {
            if (pinned.size() < MAX_PINNED_ITEMS) {
                pinned.push_back(item);
            }
        } else {
            if (unpinned.size() < MAX_UNPINNED_ITEMS) {
                unpinned.push_back(item);
            }
        }

        match_char(',');
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_unpinned = std::move(unpinned);
        m_pinned = std::move(pinned);
        sort_items_locked();
        enforce_unpinned_limit_locked();
    }

    return true;
}

void StorageManager::request_save_debounced() {
    m_save_pending = true;
    m_cv.notify_one();
}

void StorageManager::flush_save() {
    std::wstring path;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        path = m_storage_path;
        m_save_pending = false;
    }
    save_to_file(path);
}

void StorageManager::save_worker_loop() {
    while (true) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this]() {
            return m_stop_worker.load() || m_save_pending.load();
        });

        if (m_stop_worker) {
            break;
        }

        // Debounce: wait 250ms for more changes
        m_cv.wait_for(lock, std::chrono::milliseconds(250), [this]() {
            return m_stop_worker.load();
        });

        if (m_stop_worker) {
            break;
        }

        if (m_save_pending) {
            m_save_pending = false;
            std::wstring path = m_storage_path;
            lock.unlock();
            save_to_file(path);
        }
    }
}

} // namespace copyheap
