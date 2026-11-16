#include <windows.h>
#include "core/storage_manager.h"
#include "clipboard/clipboard_watcher.h"
#include "tray/tray_icon_manager.h"
#include "ui/main_window.h"
#include "ui/gdi_renderer.h"
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

    // 3. Storage manager
    copyheap::StorageManager storage;
    storage.load_from_file(storage.get_storage_path());

    // 4. Clipboard watcher
    copyheap::ClipboardWatcher clipboard;

    // 5. Tray icon manager
    copyheap::TrayIconManager tray;

    // 6. Main window
    copyheap::MainWindow mainWindow(storage, clipboard, tray);
    if (!mainWindow.create(hInstance)) {
        copyheap::GdiRenderer::shutdown();
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

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
