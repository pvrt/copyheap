#pragma once

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <memory>
#include <optional>

#include "core/storage_manager.h"
#include "clipboard/clipboard_watcher.h"
#include "tray/tray_icon_manager.h"

namespace copyheap {

enum class AppScreen {
    List,
    Editor
};

enum class HoverTarget {
    None,
    Card,
    PinIcon,
    EditIcon,
    DeleteIcon,
    AddButton,
    BackButton,
    SaveButton
};

class MainWindow {
public:
    MainWindow(StorageManager& storage, ClipboardWatcher& clipboard, TrayIconManager& tray);
    ~MainWindow();

    bool create(HINSTANCE hInstance);
    void show();
    void hide();
    bool is_visible() const;

    HWND get_hwnd() const { return m_hwnd; }

    void refresh_list();
    void show_notification(const std::wstring& message, int duration_ms = 2500);

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

    LRESULT handle_message(UINT msg, WPARAM wParam, LPARAM lParam);

    void on_paint(HDC hdc);
    void on_size(int width, int height);
    void on_mouse_move(int x, int y);
    void on_mouse_leave();
    void on_lbutton_down(int x, int y);
    void on_lbutton_up(int x, int y);
    void on_mouse_wheel(short delta);
    void on_vscroll(WPARAM wParam);

    void switch_to_screen(AppScreen screen);
    void open_editor(const std::optional<ClipboardItem>& item = std::nullopt);
    void save_editor();
    void update_scroll_info();

    StorageManager& m_storage;
    ClipboardWatcher& m_clipboard;
    TrayIconManager& m_tray;

    HWND m_hwnd = nullptr;
    HINSTANCE m_hInstance = nullptr;

    AppScreen m_currentScreen = AppScreen::List;

    // Controls
    HWND m_hSearchEdit = nullptr;
    HWND m_hTitleEdit = nullptr;
    HWND m_hContentEdit = nullptr;
    HFONT m_hFontUI = nullptr;
    HFONT m_hFontBold = nullptr;
    HFONT m_hFontMono = nullptr;

    // Search and display items
    std::string m_searchQuery;
    std::vector<ClipboardItem> m_displayedItems;

    // Scrolling
    int m_scrollPos = 0;
    int m_maxScroll = 0;
    int m_itemHeight = 52;
    int m_itemSpacing = 6;
    int m_listTop = 60;

    // Hover state
    int m_hoveredIndex = -1;
    HoverTarget m_hoverTarget = HoverTarget::None;
    bool m_trackingMouse = false;

    // Copied feedback
    std::string m_copiedItemId;
    int64_t m_copiedTime = 0;

    // Inline notification
    std::wstring m_notificationMsg;
    int64_t m_notificationExpiry = 0;

    // Editor state
    bool m_isEditingExisting = false;
    std::string m_editingId;

    // DPI scaling
    float m_dpiScale = 1.0f;
    int scale_px(int px) const { return static_cast<int>(px * m_dpiScale); }
};

} // namespace copyheap
