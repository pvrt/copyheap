#include "tray_icon_manager.h"
#include "core/string_utils.h"

namespace copyheap {

TrayIconManager::TrayIconManager() = default;

TrayIconManager::~TrayIconManager() {
    remove();
}

bool TrayIconManager::create(HWND hwnd, HICON hIcon, const std::wstring& tooltip) {
    if (!hwnd) return false;
    m_hwnd = hwnd;

    ZeroMemory(&m_nid, sizeof(m_nid));
    m_nid.cbSize = sizeof(NOTIFYICONDATAW);
    m_nid.hWnd = m_hwnd;
    m_nid.uID = 1;
    m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAY_ICON_NOTIFY;
    m_nid.hIcon = hIcon;

    wcsncpy_s(m_nid.szTip, tooltip.c_str(), _TRUNCATE);

    if (Shell_NotifyIconW(NIM_ADD, &m_nid)) {
        m_installed = true;
        return true;
    }
    return false;
}

void TrayIconManager::remove() {
    if (m_installed) {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
        m_installed = false;
    }
}

void TrayIconManager::show_menu(
    const std::pair<std::vector<ClipboardItem>, std::vector<ClipboardItem>>& sections,
    OnItemCopyCallback on_copy,
    OnOpenCallback on_open,
    OnExitCallback on_exit)
{
    m_on_copy = std::move(on_copy);
    m_on_open = std::move(on_open);
    m_on_exit = std::move(on_exit);
    m_active_menu_items.clear();

    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    UINT currentCmdId = ID_TRAY_ITEM_BASE;

    auto truncate_title = [](const std::string& title, size_t maxLen = 45) -> std::wstring {
        std::wstring wide = utf8_to_wide(title);
        if (wide.size() > maxLen) {
            wide = wide.substr(0, maxLen) + L"...";
        }
        return wide;
    };

    const auto& unpinned = sections.first;
    const auto& pinned = sections.second;

    // Block 1: Up to 10 unpinned items
    if (!unpinned.empty()) {
        for (const auto& item : unpinned) {
            std::wstring label = truncate_title(item.get_display_title());
            AppendMenuW(hMenu, MF_STRING, currentCmdId, label.c_str());
            m_active_menu_items.push_back({currentCmdId, item.content});
            currentCmdId++;
        }
    } else {
        AppendMenuW(hMenu, MF_GRAYED | MF_STRING, 0, L"(История пуста)");
    }

    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    // Block 2: Up to 20 pinned items
    if (!pinned.empty()) {
        for (const auto& item : pinned) {
            std::wstring label = L"📌 " + truncate_title(item.get_display_title());
            AppendMenuW(hMenu, MF_STRING, currentCmdId, label.c_str());
            m_active_menu_items.push_back({currentCmdId, item.content});
            currentCmdId++;
        }
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    }

    // Block 3: System items
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_OPEN, L"Открыть");
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Выход");

    // Track popup menu properly: mandatory SetForegroundWindow before TrackPopupMenu
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(m_hwnd);

    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hwnd, nullptr);
    PostMessage(m_hwnd, WM_NULL, 0, 0);

    DestroyMenu(hMenu);
}

bool TrayIconManager::handle_command(UINT cmdId) {
    if (cmdId == ID_TRAY_OPEN) {
        if (m_on_open) m_on_open();
        return true;
    }
    if (cmdId == ID_TRAY_EXIT) {
        if (m_on_exit) m_on_exit();
        return true;
    }

    for (const auto& item : m_active_menu_items) {
        if (item.cmd_id == cmdId) {
            if (m_on_copy) {
                m_on_copy(item.content);
            }
            return true;
        }
    }
    return false;
}

} // namespace copyheap
