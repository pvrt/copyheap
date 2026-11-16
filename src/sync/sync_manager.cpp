#include "sync_manager.h"
#include "gist_client.h"
#include "core/string_utils.h"

namespace copyheap {

SyncManager::SyncManager(StorageManager& storage, ConfigManager& config)
    : m_storage(storage), m_config(config)
{
    m_workerThread = std::thread(&SyncManager::worker_loop, this);
}

SyncManager::~SyncManager() {
    m_stopWorker = true;
    m_cv.notify_all();
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
}

void SyncManager::set_on_items_changed(OnItemsChangedCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_onItemsChanged = std::move(cb);
}

SyncStatus SyncManager::get_last_status() const {
    return m_lastStatus;
}

std::string SyncManager::get_last_message() const {
    return m_lastMessage;
}

void SyncManager::trigger_sync(SyncCallback on_complete) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_syncPending = true;
        m_pendingCallback = std::move(on_complete);
    }
    m_cv.notify_one();
}

void SyncManager::trigger_push_debounced() {
    if (!m_config.get_config().sync_enabled || m_config.get_config().github_token.empty()) {
        return;
    }
    m_pushPending = true;
    m_cv.notify_one();
}

bool SyncManager::setup_sync(const std::string& token, std::string& out_error) {
    if (token.empty()) {
        out_error = "Токен не может быть пустым";
        return false;
    }

    if (!GistClient::validate_token(token)) {
        out_error = "Неверный токен GitHub или отсутствует доступ к API";
        return false;
    }

    // Try finding existing gist
    auto existing_gist = GistClient::find_existing_gist(token);
    std::string gist_id;

    if (existing_gist) {
        gist_id = *existing_gist;
    } else {
        // Create new gist with current local pinned items
        std::string initial_content = m_storage.serialize_pinned_json();
        auto created = GistClient::create_gist(token, initial_content);
        if (!created) {
            out_error = "Не удалось создать Gist в GitHub аккаунте";
            return false;
        }
        gist_id = *created;
    }

    m_config.set_github_token(token);
    m_config.set_gist_id(gist_id);
    m_config.set_sync_enabled(true);
    m_config.set_last_sync_time(now_ms());
    m_config.save();

    return true;
}

void SyncManager::worker_loop() {
    while (!m_stopWorker) {
        SyncCallback cb = nullptr;
        bool do_sync = false;
        bool do_push = false;

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this]() {
                return m_stopWorker || m_syncPending || m_pushPending;
            });

            if (m_stopWorker) break;

            if (m_pushPending && !m_syncPending) {
                // Debounce push
                m_cv.wait_for(lock, std::chrono::milliseconds(1000), [this]() {
                    return m_stopWorker.load();
                });
                if (m_stopWorker) break;
            }

            do_sync = m_syncPending;
            do_push = m_pushPending;
            m_syncPending = false;
            m_pushPending = false;
            cb = std::move(m_pendingCallback);
        }

        if (do_sync) {
            do_sync_locked(cb);
        } else if (do_push) {
            const auto& cfg = m_config.get_config();
            if (cfg.sync_enabled && !cfg.github_token.empty() && !cfg.gist_id.empty()) {
                std::string content = m_storage.serialize_pinned_json();
                GistClient::push_pinned(cfg.github_token, cfg.gist_id, content);
            }
        }
    }
}

void SyncManager::do_sync_locked(SyncCallback on_complete) {
    const auto& cfg = m_config.get_config();
    if (!cfg.sync_enabled || cfg.github_token.empty()) {
        m_lastStatus = SyncStatus::Idle;
        m_lastMessage = "Синхронизация отключена";
        if (on_complete) on_complete(m_lastStatus, m_lastMessage);
        return;
    }

    m_lastStatus = SyncStatus::Syncing;
    m_lastMessage = "Синхронизация...";

    std::string gist_id = cfg.gist_id;
    if (gist_id.empty()) {
        auto found = GistClient::find_existing_gist(cfg.github_token);
        if (found) {
            gist_id = *found;
            m_config.set_gist_id(gist_id);
            m_config.save();
        } else {
            auto created = GistClient::create_gist(cfg.github_token, m_storage.serialize_pinned_json());
            if (created) {
                gist_id = *created;
                m_config.set_gist_id(gist_id);
                m_config.save();
            } else {
                m_lastStatus = SyncStatus::GistError;
                m_lastMessage = "Не удалось найти или создать Gist";
                if (on_complete) on_complete(m_lastStatus, m_lastMessage);
                return;
            }
        }
    }

    // 1. Pull
    auto remote = GistClient::pull_pinned(cfg.github_token, gist_id);
    if (!remote) {
        m_lastStatus = SyncStatus::NetworkError;
        m_lastMessage = "Ошибка загрузки из GitHub Gist";
        if (on_complete) on_complete(m_lastStatus, m_lastMessage);
        return;
    }

    // 2. Merge
    bool localChanged = m_storage.merge_pinned(*remote);

    // 3. Push merged state back to Gist
    std::string merged_json = m_storage.serialize_pinned_json();
    bool pushOk = GistClient::push_pinned(cfg.github_token, gist_id, merged_json);

    if (!pushOk) {
        m_lastStatus = SyncStatus::NetworkError;
        m_lastMessage = "Ошибка отправки данных в Gist";
        if (on_complete) on_complete(m_lastStatus, m_lastMessage);
        return;
    }

    m_config.set_last_sync_time(now_ms());
    m_config.save();

    m_lastStatus = SyncStatus::Success;
    m_lastMessage = "Успешно синхронизировано";

    if (localChanged) {
        OnItemsChangedCallback notifyCb = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            notifyCb = m_onItemsChanged;
        }
        if (notifyCb) notifyCb();
    }

    if (on_complete) on_complete(m_lastStatus, m_lastMessage);
}

} // namespace copyheap
