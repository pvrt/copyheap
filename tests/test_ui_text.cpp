#include "test_helper.h"
#include "ui/main_window.h"
#include "core/storage_manager.h"
#include "clipboard/clipboard_watcher.h"
#include "tray/tray_icon_manager.h"
#include <commctrl.h>

using namespace copyheap;

TEST(UI, ListViewAndStatusBarUnicodeText) {
    INITCOMMONCONTROLSEX icex{};
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);

    StorageManager sm;
    sm.clear();

    sm.add_or_update_from_clipboard("Первый русский сниппет");
    sm.add_item("Второй сниппет", "Заголовок сниппета", true);

    ClipboardWatcher cb;
    TrayIconManager tray;

    MainWindow win(sm, cb, tray);
    ASSERT_TRUE(win.create(GetModuleHandleW(nullptr)));

    HWND hwnd = win.get_hwnd();
    ASSERT_TRUE(hwnd != nullptr);

    // Find ListView child
    HWND hListView = FindWindowExW(hwnd, nullptr, WC_LISTVIEWW, nullptr);
    ASSERT_TRUE(hListView != nullptr);
    ASSERT_TRUE(IsWindowUnicode(hListView));

    int count = ListView_GetItemCount(hListView);
    ASSERT_EQ(count, 2);

    // Retrieve text of item 0 (unpinned, "Первый русский сниппет")
    wchar_t buf0[256] = {0};
    LVITEMW lvi0{};
    lvi0.iSubItem = 0;
    lvi0.cchTextMax = 256;
    lvi0.pszText = buf0;
    SendMessageW(hListView, LVM_GETITEMTEXTW, 0, (LPARAM)&lvi0);

    std::wstring text0(buf0);
    std::wcout << L"Item 0 text: " << text0 << std::endl;
    ASSERT_TRUE(text0 == L"Первый русский сниппет");

    // Retrieve text of item 1 (pinned, title "Заголовок сниппета")
    wchar_t buf1[256] = {0};
    LVITEMW lvi1{};
    lvi1.iSubItem = 0;
    lvi1.cchTextMax = 256;
    lvi1.pszText = buf1;
    SendMessageW(hListView, LVM_GETITEMTEXTW, 1, (LPARAM)&lvi1);

    std::wstring text1(buf1);
    std::wcout << L"Item 1 text: " << text1 << std::endl;
    ASSERT_TRUE(text1 == L"Заголовок сниппета");

    // Check StatusBar text
    HWND hStatus = FindWindowExW(hwnd, nullptr, STATUSCLASSNAMEW, nullptr);
    ASSERT_TRUE(hStatus != nullptr);
    ASSERT_TRUE(IsWindowUnicode(hStatus));

    wchar_t statusBuf[256] = {0};
    SendMessageW(hStatus, SB_GETTEXTW, 0, (LPARAM)statusBuf);
    std::wstring statusText(statusBuf);
    std::wcout << L"StatusBar text: " << statusText << std::endl;
    ASSERT_TRUE(statusText.find(L"Всего: 2") != std::wstring::npos);
    ASSERT_TRUE(statusText.find(L"Закреплено: 1") != std::wstring::npos);

    DestroyWindow(hwnd);
}
