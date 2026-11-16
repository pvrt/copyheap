#pragma once

#include "models.h"
#include <vector>
#include <string>
#include <optional>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>

namespace copyheap {

class StorageManager {
public:
    StorageManager();
    ~StorageManager();

    // Prevent copying
    StorageManager(const StorageManager&) = delete;
    StorageManager& operator=(const StorageManager&) = delete;

    // Clipboard ingestion
    bool add_or_update_from_clipboard(const std::string& content);

    // Manual item creation / editing
    std::string add_item(const std::string& content, const std::string& title = "", bool is_pinned = false);
    bool update_item(const std::string& id, const std::string& new_title, const std::string& new_content);
    bool delete_item(const std::string& id);

    // Pinning
    PinResult set_pinned(const std::string& id, bool pinned);
    PinResult toggle_pin(const std::string& id);

    // Cloud Synchronization
    bool merge_pinned(const std::vector<ClipboardItem>& remote_pinned);
    std::string serialize_pinned_json() const;
    static std::vector<ClipboardItem> deserialize_pinned_json(const std::string& json_str);

    // Queries
    std::vector<ClipboardItem> get_all_items() const;
    std::vector<ClipboardItem> get_unpinned_items() const;
    std::vector<ClipboardItem> get_pinned_items() const;
    std::pair<std::vector<ClipboardItem>, std::vector<ClipboardItem>> get_tray_sections() const;
    std::vector<ClipboardItem> search(const std::string& query) const;
    std::optional<ClipboardItem> get_item(const std::string& id) const;

    size_t unpinned_count() const;
    size_t pinned_count() const;
    size_t total_count() const;

    void clear();

    // Persistence
    static std::wstring get_default_storage_path();
    bool save_to_file(const std::wstring& file_path) const;
    bool load_from_file(const std::wstring& file_path);

    void set_storage_path(const std::wstring& path);
    const std::wstring& get_storage_path() const;

    void request_save_debounced();
    void flush_save();

private:
    void sort_items_locked();
    void enforce_unpinned_limit_locked();

    mutable std::mutex m_mutex;
    std::vector<ClipboardItem> m_unpinned; // Max 200, created_at DESC
    std::vector<ClipboardItem> m_pinned;   // Max 20, pinned_at DESC

    std::wstring m_storage_path;

    // Background debounced save worker
    std::atomic<bool> m_stop_worker{false};
    std::atomic<bool> m_save_pending{false};
    std::condition_variable m_cv;
    std::thread m_save_thread;
    void save_worker_loop();
};

} // namespace copyheap
