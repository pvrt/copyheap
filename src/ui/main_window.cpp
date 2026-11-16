#include "main_window.h"
#include "core/string_utils.h"
#include "resource.h"

#include <dwmapi.h>
#include <windowsx.h>
#include <gdiplus.h>
#include <algorithm>

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif

namespace copyheap {

constexpr int IDC_SEARCH_EDIT   = 101;
constexpr int IDC_ADD_BTN       = 102;
constexpr int IDC_LISTVIEW      = 103;
constexpr int IDC_COPY_BTN      = 104;
constexpr int IDC_PIN_BTN       = 105;
constexpr int IDC_EDIT_BTN      = 106;
constexpr int IDC_DELETE_BTN    = 107;
constexpr int IDC_STATUSBAR     = 108;

constexpr int IDC_TITLE_LABEL   = 201;
constexpr int IDC_TITLE_EDIT    = 202;
constexpr int IDC_CONTENT_LABEL = 203;
constexpr int IDC_CONTENT_EDIT  = 204;
constexpr int IDC_SAVE_BTN      = 205;
constexpr int IDC_CANCEL_BTN    = 206;

constexpr int IDM_CTX_COPY      = 301;
constexpr int IDM_CTX_PIN       = 302;
constexpr int IDM_CTX_EDIT      = 303;
constexpr int IDM_CTX_DELETE    = 304;

constexpr UINT_PTR IDT_NOTIFICATION_TIMER = 401;

MainWindow::MainWindow(StorageManager& storage, ClipboardWatcher& clipboard, TrayIconManager& tray)
    : m_storage(storage), m_clipboard(clipboard), m_tray(tray)
{
}

MainWindow::~MainWindow() {
    if (m_hFontUI) DeleteObject(m_hFontUI);
    if (m_hFontBold) DeleteObject(m_hFontBold);
    if (m_hImageList) ImageList_Destroy(m_hImageList);
}

bool MainWindow::create(HINSTANCE hInstance) {
    m_hInstance = hInstance;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWindow::WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hIconSm = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON,
                                   GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1); // Native standard window background
    wc.lpszClassName = L"CopyHeap_MainWindowClass";
    RegisterClassExW(&wc);

    // Initial size: 440 x 540
    int width = 440;
    int height = 540;

    RECT workArea;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    int posX = workArea.right - width - 20;
    int posY = workArea.bottom - height - 20;

    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST,
        L"CopyHeap_MainWindowClass",
        L"CopyHeap",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
        posX, posY, width, height,
        nullptr, nullptr, hInstance, this
    );

    if (!m_hwnd) return false;

    // Windows 11 rounded corners
    DWORD cornerPref = DWMWCP_ROUND;
    DwmSetWindowAttribute(m_hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref, sizeof(cornerPref));

    // DPI scale
    HDC screenDC = GetDC(m_hwnd);
    int dpi = GetDeviceCaps(screenDC, LOGPIXELSY);
    ReleaseDC(m_hwnd, screenDC);
    m_dpiScale = dpi / 96.0f;
    if (m_dpiScale < 1.0f) m_dpiScale = 1.0f;

    // Standard Segoe UI font
    NONCLIENTMETRICSW ncm{};
    ncm.cbSize = sizeof(NONCLIENTMETRICSW);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), &ncm, 0);
    m_hFontUI = CreateFontIndirectW(&ncm.lfMessageFont);

    LOGFONTW lfBold = ncm.lfMessageFont;
    lfBold.lfWeight = FW_BOLD;
    m_hFontBold = CreateFontIndirectW(&lfBold);

    init_image_list();
    create_controls();

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    layout_controls(rc.right - rc.left, rc.bottom - rc.top);

    refresh_list();
    return true;
}

void MainWindow::init_image_list() {
    m_hImageList = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 2, 2);
    if (!m_hImageList) return;

    // 1. Create Pin bitmap (index 0)
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = 16;
    bmi.bmiHeader.biHeight = -16;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBitsPin = nullptr;
    HBITMAP hPinBmp = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &pBitsPin, nullptr, 0);
    if (hPinBmp && pBitsPin) {
        Gdiplus::Bitmap bmp(16, 16, 16 * 4, PixelFormat32bppPARGB, static_cast<BYTE*>(pBitsPin));
        Gdiplus::Graphics g(&bmp);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));

        Gdiplus::Color pinColor(255, 0, 103, 192); // Accent blue
        Gdiplus::SolidBrush brush(pinColor);
        g.FillRectangle(&brush, 4.0f, 1.0f, 8.0f, 2.0f);
        Gdiplus::PointF pts[4] = {{5.0f, 3.0f}, {11.0f, 3.0f}, {10.0f, 8.5f}, {6.0f, 8.5f}};
        g.FillPolygon(&brush, pts, 4);
        g.FillRectangle(&brush, 3.0f, 8.5f, 10.0f, 2.0f);
        Gdiplus::Pen pen(pinColor, 1.5f);
        g.DrawLine(&pen, 8.0f, 10.5f, 8.0f, 14.5f);

        ImageList_Add(m_hImageList, hPinBmp, nullptr);
        DeleteObject(hPinBmp);
    }

    // 2. Create Document / Note bitmap (index 1)
    void* pBitsDoc = nullptr;
    HBITMAP hDocBmp = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &pBitsDoc, nullptr, 0);
    if (hDocBmp && pBitsDoc) {
        Gdiplus::Bitmap bmp(16, 16, 16 * 4, PixelFormat32bppPARGB, static_cast<BYTE*>(pBitsDoc));
        Gdiplus::Graphics g(&bmp);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));

        Gdiplus::Color docColor(255, 120, 120, 120); // Dark gray
        Gdiplus::Pen pen(docColor, 1.3f);
        g.DrawRectangle(&pen, 3.5f, 1.5f, 9.0f, 13.0f);
        g.DrawLine(&pen, 5.5f, 5.0f, 10.5f, 5.0f);
        g.DrawLine(&pen, 5.5f, 8.0f, 10.5f, 8.0f);
        g.DrawLine(&pen, 5.5f, 11.0f, 9.0f, 11.0f);

        ImageList_Add(m_hImageList, hDocBmp, nullptr);
        DeleteObject(hDocBmp);
    }
}

void MainWindow::create_controls() {
    // --- Screen 1: List Controls ---
    // Search Edit
    m_hSearchEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, 0, 0,
        m_hwnd, (HMENU)(INT_PTR)IDC_SEARCH_EDIT, m_hInstance, nullptr
    );
    SendMessageW(m_hSearchEdit, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    SendMessageW(m_hSearchEdit, EM_SETCUEBANNER, TRUE, (LPARAM)L"Поиск сниппетов (Ctrl+F)...");
    SetWindowSubclass(m_hSearchEdit, SearchSubclassProc, 1, (DWORD_PTR)this);

    // "Добавить..." Button
    m_hAddBtn = CreateWindowExW(
        0, L"BUTTON", L"+ Добавить...",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        0, 0, 0, 0,
        m_hwnd, (HMENU)(INT_PTR)IDC_ADD_BTN, m_hInstance, nullptr
    );
    SendMessageW(m_hAddBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // ListView
    m_hListView = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_NOCOLUMNHEADER | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        0, 0, 0, 0,
        m_hwnd, (HMENU)(INT_PTR)IDC_LISTVIEW, m_hInstance, nullptr
    );
    SendMessageW(m_hListView, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    ListView_SetExtendedListViewStyle(m_hListView, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_SUBITEMIMAGES);

    if (m_hImageList) {
        ListView_SetImageList(m_hListView, m_hImageList, LVSIL_SMALL);
    }

    LVCOLUMNW lvc{};
    lvc.mask = LVCF_WIDTH;
    lvc.cx = 400;
    ListView_InsertColumn(m_hListView, 0, &lvc);

    // Action buttons
    m_hCopyBtn = CreateWindowExW(
        0, L"BUTTON", L"Копировать",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON | WS_DISABLED,
        0, 0, 0, 0,
        m_hwnd, (HMENU)(INT_PTR)IDC_COPY_BTN, m_hInstance, nullptr
    );
    SendMessageW(m_hCopyBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    m_hPinBtn = CreateWindowExW(
        0, L"BUTTON", L"Закрепить",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON | WS_DISABLED,
        0, 0, 0, 0,
        m_hwnd, (HMENU)(INT_PTR)IDC_PIN_BTN, m_hInstance, nullptr
    );
    SendMessageW(m_hPinBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    m_hEditBtn = CreateWindowExW(
        0, L"BUTTON", L"Изменить...",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON | WS_DISABLED,
        0, 0, 0, 0,
        m_hwnd, (HMENU)(INT_PTR)IDC_EDIT_BTN, m_hInstance, nullptr
    );
    SendMessageW(m_hEditBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    m_hDeleteBtn = CreateWindowExW(
        0, L"BUTTON", L"Удалить",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON | WS_DISABLED,
        0, 0, 0, 0,
        m_hwnd, (HMENU)(INT_PTR)IDC_DELETE_BTN, m_hInstance, nullptr
    );
    SendMessageW(m_hDeleteBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // Status bar
    m_hStatusBar = CreateWindowExW(
        0, STATUSCLASSNAMEW, L"",
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0,
        m_hwnd, (HMENU)(INT_PTR)IDC_STATUSBAR, m_hInstance, nullptr
    );
    SendMessageW(m_hStatusBar, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // --- Screen 2: Editor Controls (initially hidden) ---
    m_hTitleLabel = CreateWindowExW(
        0, L"STATIC", L"Название сниппета (необязательно):",
        WS_CHILD | SS_LEFT,
        0, 0, 0, 0,
        m_hwnd, (HMENU)(INT_PTR)IDC_TITLE_LABEL, m_hInstance, nullptr
    );
    SendMessageW(m_hTitleLabel, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    m_hTitleEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, 0, 0,
        m_hwnd, (HMENU)(INT_PTR)IDC_TITLE_EDIT, m_hInstance, nullptr
    );
    SendMessageW(m_hTitleEdit, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    SetWindowSubclass(m_hTitleEdit, EditorSubclassProc, 2, (DWORD_PTR)this);

    m_hContentLabel = CreateWindowExW(
        0, L"STATIC", L"Содержимое:",
        WS_CHILD | SS_LEFT,
        0, 0, 0, 0,
        m_hwnd, (HMENU)(INT_PTR)IDC_CONTENT_LABEL, m_hInstance, nullptr
    );
    SendMessageW(m_hContentLabel, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    m_hContentEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_TABSTOP | ES_MULTILINE | WS_VSCROLL | ES_AUTOVSCROLL | ES_WANTRETURN,
        0, 0, 0, 0,
        m_hwnd, (HMENU)(INT_PTR)IDC_CONTENT_EDIT, m_hInstance, nullptr
    );
    SendMessageW(m_hContentEdit, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    SetWindowSubclass(m_hContentEdit, EditorSubclassProc, 3, (DWORD_PTR)this);

    m_hSaveBtn = CreateWindowExW(
        0, L"BUTTON", L"Сохранить (Ctrl+Enter)",
        WS_CHILD | WS_TABSTOP | BS_DEFPUSHBUTTON,
        0, 0, 0, 0,
        m_hwnd, (HMENU)(INT_PTR)IDC_SAVE_BTN, m_hInstance, nullptr
    );
    SendMessageW(m_hSaveBtn, WM_SETFONT, (WPARAM)m_hFontBold, TRUE);

    m_hCancelBtn = CreateWindowExW(
        0, L"BUTTON", L"Назад (Esc)",
        WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
        0, 0, 0, 0,
        m_hwnd, (HMENU)(INT_PTR)IDC_CANCEL_BTN, m_hInstance, nullptr
    );
    SendMessageW(m_hCancelBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
}

void MainWindow::layout_controls(int width, int height) {
    int pad = scale_px(10);
    int topH = scale_px(28);
    int addW = scale_px(100);

    // Status bar height
    RECT rcStatus{};
    GetWindowRect(m_hStatusBar, &rcStatus);
    int statusH = rcStatus.bottom - rcStatus.top;
    if (statusH <= 0) statusH = scale_px(22);

    SendMessageW(m_hStatusBar, WM_SIZE, 0, 0);

    if (m_currentScreen == AppScreen::List) {
        // Search edit & Add button
        int searchW = width - pad * 2 - addW - pad;
        SetWindowPos(m_hSearchEdit, nullptr, pad, pad, searchW, topH, SWP_NOZORDER);
        SetWindowPos(m_hAddBtn, nullptr, pad + searchW + pad, pad, addW, topH, SWP_NOZORDER);

        // Action buttons
        int btnH = scale_px(28);
        int btnY = height - statusH - pad - btnH;
        int numBtns = 4;
        int totalBtnGap = pad * (numBtns - 1);
        int btnW = (width - pad * 2 - totalBtnGap) / numBtns;

        SetWindowPos(m_hCopyBtn, nullptr, pad, btnY, btnW, btnH, SWP_NOZORDER);
        SetWindowPos(m_hPinBtn, nullptr, pad + (btnW + pad) * 1, btnY, btnW, btnH, SWP_NOZORDER);
        SetWindowPos(m_hEditBtn, nullptr, pad + (btnW + pad) * 2, btnY, btnW, btnH, SWP_NOZORDER);
        SetWindowPos(m_hDeleteBtn, nullptr, pad + (btnW + pad) * 3, btnY, btnW, btnH, SWP_NOZORDER);

        // List view
        int listY = pad + topH + pad;
        int listH = btnY - pad - listY;
        SetWindowPos(m_hListView, nullptr, pad, listY, width - pad * 2, listH, SWP_NOZORDER);
        ListView_SetColumnWidth(m_hListView, 0, width - pad * 2 - scale_px(24));
    } else {
        // Screen 2: Editor layout
        int labelH = scale_px(18);
        int titleEditH = scale_px(26);
        int btnH = scale_px(32);
        int btnW = (width - pad * 3) / 2;
        int btnY = height - statusH - pad - btnH;

        SetWindowPos(m_hTitleLabel, nullptr, pad, pad, width - pad * 2, labelH, SWP_NOZORDER);
        SetWindowPos(m_hTitleEdit, nullptr, pad, pad + labelH + scale_px(4), width - pad * 2, titleEditH, SWP_NOZORDER);

        int contentLabelY = pad + labelH + scale_px(4) + titleEditH + pad;
        SetWindowPos(m_hContentLabel, nullptr, pad, contentLabelY, width - pad * 2, labelH, SWP_NOZORDER);

        int contentEditY = contentLabelY + labelH + scale_px(4);
        int contentEditH = btnY - pad - contentEditY;
        SetWindowPos(m_hContentEdit, nullptr, pad, contentEditY, width - pad * 2, contentEditH, SWP_NOZORDER);

        SetWindowPos(m_hCancelBtn, nullptr, pad, btnY, btnW, btnH, SWP_NOZORDER);
        SetWindowPos(m_hSaveBtn, nullptr, pad * 2 + btnW, btnY, btnW, btnH, SWP_NOZORDER);
    }
}

void MainWindow::switch_to_screen(AppScreen screen) {
    m_currentScreen = screen;
    int showList = (screen == AppScreen::List) ? SW_SHOW : SW_HIDE;
    int showEditor = (screen == AppScreen::Editor) ? SW_SHOW : SW_HIDE;

    ShowWindow(m_hSearchEdit, showList);
    ShowWindow(m_hAddBtn, showList);
    ShowWindow(m_hListView, showList);
    ShowWindow(m_hCopyBtn, showList);
    ShowWindow(m_hPinBtn, showList);
    ShowWindow(m_hEditBtn, showList);
    ShowWindow(m_hDeleteBtn, showList);

    ShowWindow(m_hTitleLabel, showEditor);
    ShowWindow(m_hTitleEdit, showEditor);
    ShowWindow(m_hContentLabel, showEditor);
    ShowWindow(m_hContentEdit, showEditor);
    ShowWindow(m_hSaveBtn, showEditor);
    ShowWindow(m_hCancelBtn, showEditor);

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    layout_controls(rc.right - rc.left, rc.bottom - rc.top);

    if (screen == AppScreen::List) {
        refresh_list();
        SetFocus(m_hSearchEdit);
    } else {
        SetFocus(m_hTitleEdit);
    }
}

void MainWindow::show() {
    if (!m_hwnd) return;
    refresh_list();
    ShowWindow(m_hwnd, SW_SHOW);
    SetForegroundWindow(m_hwnd);
    if (m_currentScreen == AppScreen::List) {
        SetFocus(m_hSearchEdit);
    }
}

void MainWindow::hide() {
    if (!m_hwnd) return;
    ShowWindow(m_hwnd, SW_HIDE);
}

bool MainWindow::is_visible() const {
    return m_hwnd && IsWindowVisible(m_hwnd);
}

void MainWindow::refresh_list() {
    m_displayedItems = m_storage.search(m_searchQuery);

    ListView_DeleteAllItems(m_hListView);

    for (size_t i = 0; i < m_displayedItems.size(); ++i) {
        const auto& item = m_displayedItems[i];
        std::wstring title = utf8_to_wide(item.get_display_title());

        LVITEMW lvi{};
        lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
        lvi.iItem = static_cast<int>(i);
        lvi.iSubItem = 0;
        lvi.pszText = const_cast<LPWSTR>(title.c_str());
        lvi.iImage = item.is_pinned ? 0 : 1;
        lvi.lParam = static_cast<LPARAM>(i);

        ListView_InsertItem(m_hListView, &lvi);
    }

    on_list_selection_changed();
    update_status_bar();
}

void MainWindow::on_list_selection_changed() {
    int selected = ListView_GetNextItem(m_hListView, -1, LVNI_SELECTED);
    BOOL hasSel = (selected >= 0 && selected < static_cast<int>(m_displayedItems.size()));

    EnableWindow(m_hCopyBtn, hasSel);
    EnableWindow(m_hPinBtn, hasSel);
    EnableWindow(m_hEditBtn, hasSel);
    EnableWindow(m_hDeleteBtn, hasSel);

    if (hasSel) {
        const auto& item = m_displayedItems[selected];
        SetWindowTextW(m_hPinBtn, item.is_pinned ? L"Открепить" : L"Закрепить");
    } else {
        SetWindowTextW(m_hPinBtn, L"Закрепить");
    }
}

void MainWindow::update_status_bar() {
    if (!m_hStatusBar) return;

    if (!m_notificationText.empty() && now_ms() < m_notificationExpiry) {
        SendMessageW(m_hStatusBar, SB_SETTEXT, 0, (LPARAM)m_notificationText.c_str());
        return;
    }

    m_notificationText.clear();
    wchar_t buf[128];
    swprintf_s(buf, L"Всего: %zu (Закреплено: %zu / %zu)",
               m_storage.total_count(), m_storage.pinned_count(), MAX_PINNED_ITEMS);
    SendMessageW(m_hStatusBar, SB_SETTEXT, 0, (LPARAM)buf);
}

void MainWindow::show_notification(const std::wstring& message, int duration_ms) {
    m_notificationText = message;
    m_notificationExpiry = now_ms() + duration_ms;
    SetTimer(m_hwnd, IDT_NOTIFICATION_TIMER, duration_ms, nullptr);
    update_status_bar();
}

void MainWindow::copy_selected_item() {
    int selected = ListView_GetNextItem(m_hListView, -1, LVNI_SELECTED);
    if (selected >= 0 && selected < static_cast<int>(m_displayedItems.size())) {
        const auto& item = m_displayedItems[selected];
        m_clipboard.copy_to_clipboard(item.content);
        show_notification(L"✓ Скопировано в буфер обмена!");
    }
}

void MainWindow::toggle_pin_selected_item() {
    int selected = ListView_GetNextItem(m_hListView, -1, LVNI_SELECTED);
    if (selected >= 0 && selected < static_cast<int>(m_displayedItems.size())) {
        const auto& item = m_displayedItems[selected];
        PinResult res = m_storage.toggle_pin(item.id);
        if (res == PinResult::LimitReached) {
            show_notification(L"⚠ Достигнут лимит закрепленных записей (макс. 20)", 3000);
        } else {
            refresh_list();
        }
    }
}

void MainWindow::edit_selected_item() {
    int selected = ListView_GetNextItem(m_hListView, -1, LVNI_SELECTED);
    if (selected >= 0 && selected < static_cast<int>(m_displayedItems.size())) {
        open_editor(m_displayedItems[selected]);
    }
}

void MainWindow::delete_selected_item() {
    int selected = ListView_GetNextItem(m_hListView, -1, LVNI_SELECTED);
    if (selected >= 0 && selected < static_cast<int>(m_displayedItems.size())) {
        m_storage.delete_item(m_displayedItems[selected].id);
        refresh_list();
    }
}

void MainWindow::open_editor(const std::optional<ClipboardItem>& item) {
    if (item.has_value()) {
        m_isEditingExisting = true;
        m_editingId = item->id;
        SetWindowTextW(m_hTitleEdit, utf8_to_wide(item->title).c_str());
        SetWindowTextW(m_hContentEdit, utf8_to_wide(item->content).c_str());
    } else {
        m_isEditingExisting = false;
        m_editingId.clear();
        SetWindowTextW(m_hTitleEdit, L"");
        SetWindowTextW(m_hContentEdit, L"");
    }
    switch_to_screen(AppScreen::Editor);
}

void MainWindow::save_editor() {
    int titleLen = GetWindowTextLengthW(m_hTitleEdit);
    std::wstring wTitle(titleLen + 1, 0);
    GetWindowTextW(m_hTitleEdit, &wTitle[0], titleLen + 1);
    wTitle.resize(titleLen);

    int contentLen = GetWindowTextLengthW(m_hContentEdit);
    std::wstring wContent(contentLen + 1, 0);
    GetWindowTextW(m_hContentEdit, &wContent[0], contentLen + 1);
    wContent.resize(contentLen);

    std::string title = wide_to_utf8(wTitle);
    std::string content = wide_to_utf8(wContent);

    if (trim(content).empty() && trim(title).empty()) {
        switch_to_screen(AppScreen::List);
        return;
    }

    if (m_isEditingExisting) {
        m_storage.update_item(m_editingId, title, content);
    } else {
        m_storage.add_item(content, title, false);
    }

    switch_to_screen(AppScreen::List);
}

void MainWindow::cancel_editor() {
    switch_to_screen(AppScreen::List);
}

void MainWindow::on_list_double_click() {
    copy_selected_item();
}

void MainWindow::on_list_context_menu(int x, int y) {
    int selected = ListView_GetNextItem(m_hListView, -1, LVNI_SELECTED);
    if (selected < 0 || selected >= static_cast<int>(m_displayedItems.size())) {
        return;
    }

    const auto& item = m_displayedItems[selected];

    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    AppendMenuW(hMenu, MF_STRING, IDM_CTX_COPY, L"Копировать\tEnter");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_CTX_PIN, item.is_pinned ? L"Открепить" : L"Закрепить");
    AppendMenuW(hMenu, MF_STRING, IDM_CTX_EDIT, L"Изменить...");
    AppendMenuW(hMenu, MF_STRING, IDM_CTX_DELETE, L"Удалить\tDel");

    SetForegroundWindow(m_hwnd);
    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, x, y, 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);

    switch (cmd) {
        case IDM_CTX_COPY: copy_selected_item(); break;
        case IDM_CTX_PIN: toggle_pin_selected_item(); break;
        case IDM_CTX_EDIT: edit_selected_item(); break;
        case IDM_CTX_DELETE: delete_selected_item(); break;
    }
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MainWindow* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hwnd = hwnd;
    } else {
        pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->handle_message(msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK MainWindow::SearchSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR dwRefData) {
    MainWindow* pThis = reinterpret_cast<MainWindow*>(dwRefData);
    if (!pThis) return DefSubclassProc(hwnd, msg, wParam, lParam);

    if (msg == WM_KEYDOWN) {
        if (wParam == VK_ESCAPE) {
            if (!pThis->m_searchQuery.empty()) {
                SetWindowTextW(pThis->m_hSearchEdit, L"");
                pThis->m_searchQuery.clear();
                pThis->refresh_list();
                return 0;
            } else {
                pThis->hide();
                return 0;
            }
        } else if (wParam == VK_DOWN) {
            SetFocus(pThis->m_hListView);
            if (ListView_GetItemCount(pThis->m_hListView) > 0) {
                ListView_SetItemState(pThis->m_hListView, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            }
            return 0;
        }
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK MainWindow::EditorSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR dwRefData) {
    MainWindow* pThis = reinterpret_cast<MainWindow*>(dwRefData);
    if (!pThis) return DefSubclassProc(hwnd, msg, wParam, lParam);

    if (msg == WM_KEYDOWN) {
        if (wParam == VK_ESCAPE) {
            pThis->cancel_editor();
            return 0;
        } else if (wParam == VK_RETURN && (GetKeyState(VK_CONTROL) & 0x8000)) {
            pThis->save_editor();
            return 0;
        }
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT MainWindow::handle_message(UINT msg, WPARAM wParam, LPARAM lParam) {
    static UINT s_showMsg = RegisterWindowMessageW(L"CopyHeap_ShowWindow");

    if (msg == s_showMsg) {
        show();
        return 0;
    }

    switch (msg) {
        case WM_CLIPBOARDUPDATE:
            m_clipboard.on_clipboard_update();
            refresh_list();
            return 0;

        case WM_TRAY_ICON_NOTIFY: {
            UINT event = LOWORD(lParam);
            if (event == WM_LBUTTONUP || event == WM_RBUTTONUP) {
                auto sections = m_storage.get_tray_sections();
                m_tray.show_menu(
                    sections,
                    [this](const std::string& content) {
                        m_clipboard.copy_to_clipboard(content);
                    },
                    [this]() {
                        show();
                    },
                    [this]() {
                        m_storage.flush_save();
                        m_tray.remove();
                        PostQuitMessage(0);
                    }
                );
            }
            return 0;
        }

        case WM_COMMAND: {
            UINT id = LOWORD(wParam);
            UINT code = HIWORD(wParam);

            if (id == IDC_SEARCH_EDIT && code == EN_CHANGE) {
                int len = GetWindowTextLengthW(m_hSearchEdit);
                std::wstring wText(len + 1, 0);
                GetWindowTextW(m_hSearchEdit, &wText[0], len + 1);
                wText.resize(len);
                m_searchQuery = wide_to_utf8(wText);
                refresh_list();
                return 0;
            }

            if (id == IDC_ADD_BTN) {
                open_editor(std::nullopt);
                return 0;
            }
            if (id == IDC_COPY_BTN) {
                copy_selected_item();
                return 0;
            }
            if (id == IDC_PIN_BTN) {
                toggle_pin_selected_item();
                return 0;
            }
            if (id == IDC_EDIT_BTN) {
                edit_selected_item();
                return 0;
            }
            if (id == IDC_DELETE_BTN) {
                delete_selected_item();
                return 0;
            }
            if (id == IDC_SAVE_BTN) {
                save_editor();
                return 0;
            }
            if (id == IDC_CANCEL_BTN) {
                cancel_editor();
                return 0;
            }

            if (id == ID_TRAY_OPEN) {
                show();
                return 0;
            }
            if (id == ID_TRAY_EXIT) {
                m_storage.flush_save();
                m_tray.remove();
                PostQuitMessage(0);
                return 0;
            }

            if (m_tray.handle_command(id)) {
                return 0;
            }
            break;
        }

        case WM_NOTIFY: {
            LPNMHDR pnm = reinterpret_cast<LPNMHDR>(lParam);
            if (pnm->idFrom == IDC_LISTVIEW) {
                if (pnm->code == NM_DBLCLK) {
                    on_list_double_click();
                    return 0;
                } else if (pnm->code == LVN_ITEMCHANGED) {
                    on_list_selection_changed();
                    return 0;
                } else if (pnm->code == NM_RCLICK) {
                    POINT pt;
                    GetCursorPos(&pt);
                    on_list_context_menu(pt.x, pt.y);
                    return 0;
                }
            }
            break;
        }

        case WM_SIZE: {
            int w = LOWORD(lParam);
            int h = HIWORD(lParam);
            layout_controls(w, h);
            return 0;
        }

        case WM_KEYDOWN: {
            if (wParam == 'F' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                if (m_currentScreen == AppScreen::List) {
                    SetFocus(m_hSearchEdit);
                    return 0;
                }
            } else if (wParam == VK_ESCAPE) {
                if (m_currentScreen == AppScreen::Editor) {
                    cancel_editor();
                    return 0;
                } else {
                    hide();
                    return 0;
                }
            }
            break;
        }

        case WM_TIMER:
            if (wParam == IDT_NOTIFICATION_TIMER) {
                KillTimer(m_hwnd, IDT_NOTIFICATION_TIMER);
                update_status_bar();
                return 0;
            }
            break;

        case WM_CLOSE:
            hide();
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

} // namespace copyheap
