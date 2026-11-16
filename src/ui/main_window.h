#pragma once

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <optional>

#include "core/storage_manager.h"
#include "core/config_manager.h"
#include "sync/sync_manager.h"
#include "clipboard/clipboard_watcher.h"
#include "tray/tray_icon_manager.h"
#include "ui/theme.h"

namespace copyheap {

enum class AppScreen {
    List,
    Editor
};

class MainWindow {
public:
    MainWindow(StorageManager& storage, ClipboardWatcher& clipboard, TrayIconManager& tray, SyncManager& syncMgr, ConfigManager& configMgr);
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
    static LRESULT CALLBACK SearchSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
    static LRESULT CALLBACK EditorSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

    LRESULT handle_message(UINT msg, WPARAM wParam, LPARAM lParam);

    void create_controls();
    void layout_controls(int width, int height);
    void switch_to_screen(AppScreen screen);
    void apply_theme();

    void on_search_changed();
    void on_list_double_click();
    void on_list_context_menu(int x, int y);
    void on_list_selection_changed();
    void draw_list_item(LPNMLVCUSTOMDRAW pcd);

    void copy_selected_item();
    void toggle_pin_selected_item();
    void edit_selected_item();
    void delete_selected_item();

    void open_editor(const std::optional<ClipboardItem>& item = std::nullopt);
    void save_editor();
    void cancel_editor();

    void update_status_bar();
    void init_image_list();
    void open_sync_dialog();

    StorageManager& m_storage;
    ClipboardWatcher& m_clipboard;
    TrayIconManager& m_tray;
    SyncManager& m_sync;
    ConfigManager& m_config;

    HWND m_hwnd = nullptr;
    HINSTANCE m_hInstance = nullptr;

    AppScreen m_currentScreen = AppScreen::List;

    // Standard Win32 Controls - Screen 1 (List)
    HWND m_hSearchEdit = nullptr;
    HWND m_hAddBtn = nullptr;
    HWND m_hListView = nullptr;
    HWND m_hCopyBtn = nullptr;
    HWND m_hPinBtn = nullptr;
    HWND m_hEditBtn = nullptr;
    HWND m_hDeleteBtn = nullptr;
    HWND m_hStatusBar = nullptr;

    // Standard Win32 Controls - Screen 2 (Editor)
    HWND m_hTitleLabel = nullptr;
    HWND m_hTitleEdit = nullptr;
    HWND m_hContentLabel = nullptr;
    HWND m_hContentEdit = nullptr;
    HWND m_hSaveBtn = nullptr;
    HWND m_hCancelBtn = nullptr;

    // Fonts and ImageList
    HFONT m_hFontUI = nullptr;
    HFONT m_hFontBold = nullptr;
    HIMAGELIST m_hImageList = nullptr;

    // Data
    std::string m_searchQuery;
    std::vector<ClipboardItem> m_displayedItems;
    std::wstring m_notificationText;
    int64_t m_notificationExpiry = 0;

    // Editor state
    bool m_isEditingExisting = false;
    std::string m_editingId;

    // DPI scale & metrics
    float m_dpiScale = 1.0f;
    int m_singleLineEditHeight = 23;
    int scale_px(int px) const { return static_cast<int>(px * m_dpiScale); }

    FluentTheme m_theme;
    HBRUSH m_hWindowBrush = nullptr;
    HBRUSH m_hInputBrush = nullptr;
};

} // namespace copyheap
