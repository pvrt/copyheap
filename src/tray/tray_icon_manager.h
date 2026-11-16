#pragma once

#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <functional>
#include "core/models.h"

namespace copyheap {

constexpr UINT WM_TRAY_ICON_NOTIFY = WM_APP + 1;
constexpr UINT ID_TRAY_OPEN = 1001;
constexpr UINT ID_TRAY_EXIT = 1002;
constexpr UINT ID_TRAY_ITEM_BASE = 2000;

class TrayIconManager {
public:
    using OnItemCopyCallback = std::function<void(const std::string& content)>;
    using OnOpenCallback = std::function<void()>;
    using OnExitCallback = std::function<void()>;

    TrayIconManager();
    ~TrayIconManager();

    bool create(HWND hwnd, HICON hIcon, const std::wstring& tooltip = L"CopyHeap");
    void remove();

    void show_menu(const std::pair<std::vector<ClipboardItem>, std::vector<ClipboardItem>>& sections,
                   OnItemCopyCallback on_copy,
                   OnOpenCallback on_open,
                   OnExitCallback on_exit);

    bool handle_command(UINT cmdId);

private:
    void init_pin_bitmap();

    HWND m_hwnd = nullptr;
    NOTIFYICONDATAW m_nid{};
    bool m_installed = false;
    HBITMAP m_hPinBitmap = nullptr;

    struct MenuItemData {
        UINT cmd_id = 0;
        std::string content;
    };
    std::vector<MenuItemData> m_active_menu_items;
    OnItemCopyCallback m_on_copy;
    OnOpenCallback m_on_open;
    OnExitCallback m_on_exit;
};

} // namespace copyheap
