#include "clipboard_watcher.h"
#include "core/fnv_hash.h"
#include "core/string_utils.h"

#include <thread>
#include <chrono>

namespace copyheap {

ClipboardWatcher::ClipboardWatcher() = default;

ClipboardWatcher::~ClipboardWatcher() {
    stop();
}

bool ClipboardWatcher::start(HWND hwnd, ClipboardCallback callback) {
    if (!hwnd || !IsWindow(hwnd)) {
        return false;
    }
    m_hwnd = hwnd;
    m_callback = std::move(callback);

    if (AddClipboardFormatListener(m_hwnd)) {
        m_is_listening = true;
        return true;
    }
    return false;
}

void ClipboardWatcher::stop() {
    if (m_is_listening && m_hwnd && IsWindow(m_hwnd)) {
        RemoveClipboardFormatListener(m_hwnd);
        m_is_listening = false;
    }
}

void ClipboardWatcher::on_clipboard_update() {
    std::string text;
    if (!read_clipboard_text(text)) {
        return;
    }

    std::string trimmed = trim(text);
    if (trimmed.empty()) {
        return;
    }

    uint64_t hash = fnv1a_64(text);
    int64_t now = now_ms();

    // Check self-copy protection:
    // If the hash matches our self-copied hash and was copied recently (< 1500 ms)
    if (m_last_self_copied_hash.load() == hash && (now - m_last_self_copied_time.load() < 1500)) {
        // Reset and ignore
        m_last_self_copied_hash.store(0);
        return;
    }

    if (m_callback) {
        m_callback(text);
    }
}

bool ClipboardWatcher::read_clipboard_text(std::string& out_text) {
    // Retry opening clipboard up to 5 times with 10ms delay
    bool opened = false;
    for (int i = 0; i < 5; ++i) {
        if (OpenClipboard(m_hwnd)) {
            opened = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (!opened) {
        return false;
    }

    bool success = false;
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData) {
        const wchar_t* pWide = static_cast<const wchar_t*>(GlobalLock(hData));
        if (pWide) {
            out_text = wide_to_utf8(pWide);
            GlobalUnlock(hData);
            success = true;
        }
    }

    CloseClipboard();
    return success;
}

bool ClipboardWatcher::copy_to_clipboard(const std::string& text) {
    std::wstring wide = utf8_to_wide(text);
    if (wide.empty()) {
        return false;
    }

    // Set self-copy protection before putting into clipboard
    m_last_self_copied_hash.store(fnv1a_64(text));
    m_last_self_copied_time.store(now_ms());

    bool opened = false;
    for (int i = 0; i < 5; ++i) {
        if (OpenClipboard(m_hwnd)) {
            opened = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (!opened) {
        return false;
    }

    EmptyClipboard();

    size_t byteSize = (wide.size() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, byteSize);
    if (!hMem) {
        CloseClipboard();
        return false;
    }

    void* pMem = GlobalLock(hMem);
    if (!pMem) {
        GlobalFree(hMem);
        CloseClipboard();
        return false;
    }

    memcpy(pMem, wide.c_str(), byteSize);
    GlobalUnlock(hMem);

    if (!SetClipboardData(CF_UNICODETEXT, hMem)) {
        GlobalFree(hMem);
        CloseClipboard();
        return false;
    }

    CloseClipboard();
    return true;
}

} // namespace copyheap
