#include <windows.h>
#include "core/storage_manager.h"
#include "core/config_manager.h"
#include "sync/sync_manager.h"
#include "clipboard/clipboard_watcher.h"
#include "tray/tray_icon_manager.h"
#include "ui/main_window.h"
#include "ui/gdi_renderer.h"
#include "core/string_utils.h"
#include "sync/update_checker.h"
#include "resource.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    // 1. Single Instance check
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"Local\\CopyHeap_SingleInstance_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        UINT showMsg = RegisterWindowMessageW(L"CopyHeap_ShowWindow");
        PostMessageW(HWND_BROADCAST, showMsg, 0, 0);
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    // 2. Initialize Common Controls & GDI+
    INITCOMMONCONTROLSEX icex{};
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);

    copyheap::GdiRenderer::init();

    // 3. Storage manager & Config manager
    copyheap::StorageManager storage;
    storage.load_from_file(storage.get_storage_path());

    copyheap::ConfigManager config;
    config.load();

    // Ensure autostart setting matches registry on launch (default true)
    if (config.get_config().autostart_enabled) {
        copyheap::ConfigManager::set_autostart_registered(true);
    }

    copyheap::SyncManager sync(storage, config);

    // 4. Clipboard watcher
    copyheap::ClipboardWatcher clipboard;

    // 5. Tray icon manager
    copyheap::TrayIconManager tray;

    // 6. Main window
    copyheap::MainWindow mainWindow(storage, clipboard, tray, sync, config);
    if (!mainWindow.create(hInstance)) {
        copyheap::GdiRenderer::shutdown();
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    // Wire sync change notification to UI
    sync.set_on_items_changed([&mainWindow]() {
        mainWindow.refresh_list();
    });

    // Initial background sync pull on app start
    if (config.get_config().sync_enabled) {
        sync.trigger_sync();
    }

    // Check for updates asynchronously in background (no overhead, no blocks)
    copyheap::UpdateChecker::check_for_updates_async([hwnd = mainWindow.get_hwnd()](const copyheap::UpdateInfo& info) {
        std::wstring wVer = copyheap::utf8_to_wide(info.latest_version);
        std::wstring msg = L"Доступна новая версия CopyHeap (" + wVer + L")!\n\nОткрыть страницу загрузки?";
        if (MessageBoxW(hwnd, msg.c_str(), L"Обновление CopyHeap", MB_YESNO | MB_ICONINFORMATION) == IDYES) {
            std::wstring wUrl = copyheap::utf8_to_wide(info.release_url);
            ShellExecuteW(nullptr, L"open", wUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
    });

    // Start clipboard listener
    clipboard.start(mainWindow.get_hwnd(), [&](const std::string& text) {
        storage.add_or_update_from_clipboard(text);
        mainWindow.refresh_list();
    });

    // Create tray icon with crisp small icon metrics
    int smIconX = GetSystemMetrics(SM_CXSMICON);
    int smIconY = GetSystemMetrics(SM_CYSMICON);
    HICON hAppIcon = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON,
                                       smIconX, smIconY, LR_DEFAULTCOLOR);
    if (!hAppIcon) {
        hAppIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    }
    tray.create(mainWindow.get_hwnd(), hAppIcon, L"CopyHeap");

    // Message loop
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // Clean up
    clipboard.stop();
    tray.remove();
    storage.flush_save();
    copyheap::GdiRenderer::shutdown();

    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }

    return static_cast<int>(msg.wParam);
}
