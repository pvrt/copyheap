#pragma once

#include "core/storage_manager.h"
#include "core/config_manager.h"

#include <string>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>

namespace copyheap {

enum class SyncStatus {
    Idle,
    Syncing,
    Success,
    AuthError,
    NetworkError,
    GistError
};

using SyncCallback = std::function<void(SyncStatus status, const std::string& message)>;
using OnItemsChangedCallback = std::function<void()>;

class SyncManager {
public:
    SyncManager(StorageManager& storage, ConfigManager& config);
    ~SyncManager();

    // Trigger full sync in background (Pull -> Merge -> Push if needed)
    void trigger_sync(SyncCallback on_complete = nullptr);

    // Trigger debounced push when local pinned items are changed
    void trigger_push_debounced();

    // Verify token & setup gist synchronously or asynchronously
    bool setup_sync(const std::string& token, std::string& out_error);

    SyncStatus get_last_status() const;
    std::string get_last_message() const;

    void set_on_items_changed(OnItemsChangedCallback cb);

private:
    void worker_loop();
    void do_sync_locked(SyncCallback on_complete);

    StorageManager& m_storage;
    ConfigManager& m_config;

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::thread m_workerThread;
    std::atomic<bool> m_stopWorker{false};
    std::atomic<bool> m_syncPending{false};
    std::atomic<bool> m_pushPending{false};

    SyncStatus m_lastStatus = SyncStatus::Idle;
    std::string m_lastMessage;

    SyncCallback m_pendingCallback;
    OnItemsChangedCallback m_onItemsChanged;
};

} // namespace copyheap
