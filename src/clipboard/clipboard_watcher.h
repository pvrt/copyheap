#pragma once

#include <windows.h>
#include <string>
#include <functional>
#include <atomic>
#include <cstdint>

namespace copyheap {

class ClipboardWatcher {
public:
    using ClipboardCallback = std::function<void(const std::string& text)>;

    ClipboardWatcher();
    ~ClipboardWatcher();

    bool start(HWND hwnd, ClipboardCallback callback);
    void stop();

    // Call when WM_CLIPBOARDUPDATE is received
    void on_clipboard_update();

    // Copies text to system clipboard with self-copy protection
    bool copy_to_clipboard(const std::string& text);

private:
    bool read_clipboard_text(std::string& out_text);

    HWND m_hwnd = nullptr;
    ClipboardCallback m_callback;
    std::atomic<uint64_t> m_last_self_copied_hash{0};
    std::atomic<int64_t> m_last_self_copied_time{0};
    bool m_is_listening = false;
};

} // namespace copyheap
