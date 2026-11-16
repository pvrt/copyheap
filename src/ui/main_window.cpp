#include "main_window.h"
#include "gdi_renderer.h"
#include "core/string_utils.h"
#include "resource.h"

#include <dwmapi.h>
#include <windowsx.h>
#include <algorithm>

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

namespace copyheap {

constexpr UINT_PTR IDT_COPY_HIGHLIGHT = 101;
constexpr UINT_PTR IDT_NOTIFICATION = 102;
constexpr int SEARCH_EDIT_ID = 201;
constexpr int TITLE_EDIT_ID = 202;
constexpr int CONTENT_EDIT_ID = 203;

MainWindow::MainWindow(StorageManager& storage, ClipboardWatcher& clipboard, TrayIconManager& tray)
    : m_storage(storage), m_clipboard(clipboard), m_tray(tray)
{
}

MainWindow::~MainWindow() {
    if (m_hFontUI) DeleteObject(m_hFontUI);
    if (m_hFontBold) DeleteObject(m_hFontBold);
    if (m_hFontMono) DeleteObject(m_hFontMono);
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
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"CopyHeap_MainWindowClass";
    RegisterClassExW(&wc);

    // Initial size: 440 x 540
    int width = 440;
    int height = 540;

    // Centered or bottom-right near tray
    RECT workArea;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    int posX = workArea.right - width - 20;
    int posY = workArea.bottom - height - 20;

    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST, // Floating popup style
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

    // DPI Scale
    HDC screenDC = GetDC(m_hwnd);
    int dpi = GetDeviceCaps(screenDC, LOGPIXELSY);
    ReleaseDC(m_hwnd, screenDC);
    m_dpiScale = dpi / 96.0f;
    if (m_dpiScale < 1.0f) m_dpiScale = 1.0f;

    // Fonts
    m_hFontUI = CreateFontW(-scale_px(13), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    m_hFontBold = CreateFontW(-scale_px(13), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    m_hFontMono = CreateFontW(-scale_px(13), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, FIXED_PITCH | FF_DONTCARE, L"Consolas");

    // Create child controls
    // 1. Search Edit
    m_hSearchEdit = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        scale_px(38), scale_px(16), scale_px(280), scale_px(24),
        m_hwnd, (HMENU)(INT_PTR)SEARCH_EDIT_ID, hInstance, nullptr
    );
    SendMessageW(m_hSearchEdit, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    SendMessageW(m_hSearchEdit, EM_SETCUEBANNER, TRUE, (LPARAM)L"Поиск...");
    SetWindowSubclass(m_hSearchEdit, EditSubclassProc, 1, (DWORD_PTR)this);

    // 2. Editor Title Edit
    m_hTitleEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | ES_AUTOHSCROLL,
        scale_px(16), scale_px(20), scale_px(390), scale_px(28),
        m_hwnd, (HMENU)(INT_PTR)TITLE_EDIT_ID, hInstance, nullptr
    );
    SendMessageW(m_hTitleEdit, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    SendMessageW(m_hTitleEdit, EM_SETCUEBANNER, TRUE, (LPARAM)L"Название (необязательно)");
    SetWindowSubclass(m_hTitleEdit, EditSubclassProc, 2, (DWORD_PTR)this);

    // 3. Editor Content Edit
    m_hContentEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_WANTRETURN,
        scale_px(16), scale_px(60), scale_px(390), scale_px(390),
        m_hwnd, (HMENU)(INT_PTR)CONTENT_EDIT_ID, hInstance, nullptr
    );
    SendMessageW(m_hContentEdit, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    SetWindowSubclass(m_hContentEdit, EditSubclassProc, 3, (DWORD_PTR)this);

    refresh_list();
    return true;
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
    update_scroll_info();
    if (m_hwnd) {
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void MainWindow::show_notification(const std::wstring& message, int duration_ms) {
    m_notificationMsg = message;
    m_notificationExpiry = now_ms() + duration_ms;
    SetTimer(m_hwnd, IDT_NOTIFICATION, duration_ms, nullptr);
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void MainWindow::update_scroll_info() {
    if (!m_hwnd) return;

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int clientHeight = rc.bottom - rc.top;
    int listHeight = clientHeight - scale_px(68);

    int totalHeight = static_cast<int>(m_displayedItems.size()) * scale_px(m_itemHeight + m_itemSpacing);
    m_maxScroll = (std::max)(0, totalHeight - listHeight);

    if (m_scrollPos > m_maxScroll) {
        m_scrollPos = m_maxScroll;
    }

    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = totalHeight;
    si.nPage = listHeight > 0 ? listHeight : 1;
    si.nPos = m_scrollPos;
    SetScrollInfo(m_hwnd, SB_VERT, &si, TRUE);
}

void MainWindow::switch_to_screen(AppScreen screen) {
    m_currentScreen = screen;
    if (screen == AppScreen::List) {
        ShowWindow(m_hTitleEdit, SW_HIDE);
        ShowWindow(m_hContentEdit, SW_HIDE);
        ShowWindow(m_hSearchEdit, SW_SHOW);
        SetFocus(m_hSearchEdit);
        refresh_list();
    } else {
        ShowWindow(m_hSearchEdit, SW_HIDE);
        ShowWindow(m_hTitleEdit, SW_SHOW);
        ShowWindow(m_hContentEdit, SW_SHOW);
        SetFocus(m_hTitleEdit);
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void MainWindow::open_editor(const std::optional<ClipboardItem>& item) {
    if (item.has_value()) {
        m_isEditingExisting = true;
        m_editingId = item->id;
        std::wstring wideTitle = utf8_to_wide(item->title);
        std::wstring wideContent = utf8_to_wide(item->content);
        SetWindowTextW(m_hTitleEdit, wideTitle.c_str());
        SetWindowTextW(m_hContentEdit, wideContent.c_str());
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

LRESULT CALLBACK MainWindow::EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR dwRefData) {
    MainWindow* pThis = reinterpret_cast<MainWindow*>(dwRefData);
    if (!pThis) return DefSubclassProc(hwnd, msg, wParam, lParam);

    if (msg == WM_KEYDOWN) {
        if (wParam == VK_ESCAPE) {
            if (pThis->m_currentScreen == AppScreen::Editor) {
                pThis->switch_to_screen(AppScreen::List);
                return 0;
            } else if (pThis->m_currentScreen == AppScreen::List) {
                if (!pThis->m_searchQuery.empty()) {
                    SetWindowTextW(pThis->m_hSearchEdit, L"");
                    pThis->m_searchQuery.clear();
                    pThis->refresh_list();
                    return 0;
                } else {
                    pThis->hide();
                    return 0;
                }
            }
        } else if (wParam == VK_RETURN && (GetKeyState(VK_CONTROL) & 0x8000)) {
            if (pThis->m_currentScreen == AppScreen::Editor) {
                pThis->save_editor();
                return 0;
            }
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
        case WM_CREATE:
            return 0;

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

            if (id == SEARCH_EDIT_ID && code == EN_CHANGE) {
                int len = GetWindowTextLengthW(m_hSearchEdit);
                std::wstring wText(len + 1, 0);
                GetWindowTextW(m_hSearchEdit, &wText[0], len + 1);
                wText.resize(len);
                m_searchQuery = wide_to_utf8(wText);
                m_scrollPos = 0;
                refresh_list();
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

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(m_hwnd, &ps);
            on_paint(hdc);
            EndPaint(m_hwnd, &ps);
            return 0;
        }

        case WM_SIZE:
            on_size(LOWORD(lParam), HIWORD(lParam));
            return 0;

        case WM_MOUSEMOVE:
            on_mouse_move(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;

        case WM_MOUSELEAVE:
            on_mouse_leave();
            return 0;

        case WM_LBUTTONDOWN:
            on_lbutton_down(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;

        case WM_LBUTTONUP:
            on_lbutton_up(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;

        case WM_MOUSEWHEEL:
            on_mouse_wheel(GET_WHEEL_DELTA_WPARAM(wParam));
            return 0;

        case WM_VSCROLL:
            on_vscroll(wParam);
            return 0;

        case WM_KEYDOWN: {
            if (wParam == 'F' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                if (m_currentScreen == AppScreen::List) {
                    SetFocus(m_hSearchEdit);
                    return 0;
                }
            } else if (wParam == VK_ESCAPE) {
                if (m_currentScreen == AppScreen::Editor) {
                    switch_to_screen(AppScreen::List);
                    return 0;
                } else {
                    if (!m_searchQuery.empty()) {
                        SetWindowTextW(m_hSearchEdit, L"");
                        m_searchQuery.clear();
                        refresh_list();
                        return 0;
                    } else {
                        hide();
                        return 0;
                    }
                }
            }
            break;
        }

        case WM_TIMER:
            if (wParam == IDT_COPY_HIGHLIGHT) {
                KillTimer(m_hwnd, IDT_COPY_HIGHLIGHT);
                m_copiedItemId.clear();
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return 0;
            } else if (wParam == IDT_NOTIFICATION) {
                KillTimer(m_hwnd, IDT_NOTIFICATION);
                m_notificationMsg.clear();
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return 0;
            }
            break;

        case WM_CLOSE:
            // Hide to tray on X
            hide();
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

void MainWindow::on_size(int width, int height) {
    if (m_currentScreen == AppScreen::List) {
        int searchWidth = width - scale_px(16 + 105 + 16);
        SetWindowPos(m_hSearchEdit, nullptr, scale_px(40), scale_px(17), searchWidth - scale_px(32), scale_px(22), SWP_NOZORDER);
    } else {
        int padding = scale_px(16);
        int editWidth = width - padding * 2;
        int buttonHeight = scale_px(38);
        int bottomArea = buttonHeight + padding * 2;
        int contentHeight = height - scale_px(65) - bottomArea;

        SetWindowPos(m_hTitleEdit, nullptr, padding, scale_px(20), editWidth, scale_px(28), SWP_NOZORDER);
        SetWindowPos(m_hContentEdit, nullptr, padding, scale_px(58), editWidth, contentHeight, SWP_NOZORDER);
    }
    update_scroll_info();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void MainWindow::on_mouse_move(int x, int y) {
    if (!m_trackingMouse) {
        TRACKMOUSEEVENT tme{};
        tme.cbSize = sizeof(TRACKMOUSEEVENT);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = m_hwnd;
        TrackMouseEvent(&tme);
        m_trackingMouse = true;
    }

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;

    int oldHovered = m_hoveredIndex;
    HoverTarget oldTarget = m_hoverTarget;

    m_hoveredIndex = -1;
    m_hoverTarget = HoverTarget::None;

    if (m_currentScreen == AppScreen::List) {
        // Check "Добавить" button
        int addBtnWidth = scale_px(95);
        int addBtnX = width - scale_px(16) - addBtnWidth;
        int addBtnY = scale_px(12);
        int addBtnH = scale_px(34);
        if (x >= addBtnX && x <= addBtnX + addBtnWidth && y >= addBtnY && y <= addBtnY + addBtnH) {
            m_hoverTarget = HoverTarget::AddButton;
        } else {
            // Check list items
            int listTop = scale_px(m_listTop);
            int cardW = width - scale_px(32);
            int itemTotalH = scale_px(m_itemHeight + m_itemSpacing);

            if (x >= scale_px(16) && x <= scale_px(16) + cardW && y >= listTop) {
                int adjustedY = y - listTop + m_scrollPos;
                int idx = adjustedY / itemTotalH;
                if (idx >= 0 && idx < static_cast<int>(m_displayedItems.size())) {
                    int cardLocalY = adjustedY % itemTotalH;
                    if (cardLocalY <= scale_px(m_itemHeight)) {
                        m_hoveredIndex = idx;
                        m_hoverTarget = HoverTarget::Card;

                        // Check right action icons
                        int cardRight = scale_px(16) + cardW;
                        int iconSize = scale_px(22);
                        int iconY = listTop + idx * itemTotalH - m_scrollPos + scale_px(m_itemHeight - 22) / 2;

                        if (y >= iconY && y <= iconY + iconSize) {
                            if (x >= cardRight - scale_px(28) && x <= cardRight - scale_px(6)) {
                                m_hoverTarget = HoverTarget::DeleteIcon;
                            } else if (x >= cardRight - scale_px(56) && x <= cardRight - scale_px(34)) {
                                m_hoverTarget = HoverTarget::EditIcon;
                            } else if (x >= cardRight - scale_px(84) && x <= cardRight - scale_px(62)) {
                                m_hoverTarget = HoverTarget::PinIcon;
                            }
                        }
                    }
                }
            }
        }
    } else {
        // Screen 2 buttons
        int padding = scale_px(16);
        int btnH = scale_px(38);
        int btnY = height - padding - btnH;
        int btnW = (width - padding * 3) / 2;

        if (y >= btnY && y <= btnY + btnH) {
            if (x >= padding && x <= padding + btnW) {
                m_hoverTarget = HoverTarget::BackButton;
            } else if (x >= padding * 2 + btnW && x <= padding * 2 + btnW * 2) {
                m_hoverTarget = HoverTarget::SaveButton;
            }
        }
    }

    if (oldHovered != m_hoveredIndex || oldTarget != m_hoverTarget) {
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void MainWindow::on_mouse_leave() {
    m_trackingMouse = false;
    m_hoveredIndex = -1;
    m_hoverTarget = HoverTarget::None;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void MainWindow::on_lbutton_down(int, int) {
}

void MainWindow::on_lbutton_up(int x, int y) {
    if (m_currentScreen == AppScreen::List) {
        if (m_hoverTarget == HoverTarget::AddButton) {
            open_editor(std::nullopt);
            return;
        }

        if (m_hoveredIndex >= 0 && m_hoveredIndex < static_cast<int>(m_displayedItems.size())) {
            const auto& item = m_displayedItems[m_hoveredIndex];

            if (m_hoverTarget == HoverTarget::PinIcon) {
                PinResult res = m_storage.toggle_pin(item.id);
                if (res == PinResult::LimitReached) {
                    show_notification(L"Достигнут лимит закрепленных записей (макс. 20)");
                } else {
                    refresh_list();
                }
            } else if (m_hoverTarget == HoverTarget::EditIcon) {
                open_editor(item);
            } else if (m_hoverTarget == HoverTarget::DeleteIcon) {
                m_storage.delete_item(item.id);
                refresh_list();
            } else if (m_hoverTarget == HoverTarget::Card) {
                // Copy item to clipboard
                m_clipboard.copy_to_clipboard(item.content);
                m_copiedItemId = item.id;
                m_copiedTime = now_ms();
                SetTimer(m_hwnd, IDT_COPY_HIGHLIGHT, 300, nullptr);
                InvalidateRect(m_hwnd, nullptr, FALSE);
            }
        }
    } else {
        if (m_hoverTarget == HoverTarget::BackButton) {
            switch_to_screen(AppScreen::List);
        } else if (m_hoverTarget == HoverTarget::SaveButton) {
            save_editor();
        }
    }
}

void MainWindow::on_mouse_wheel(short delta) {
    if (m_currentScreen != AppScreen::List) return;

    int scrollStep = scale_px(m_itemHeight + m_itemSpacing);
    if (delta > 0) {
        m_scrollPos -= scrollStep;
        if (m_scrollPos < 0) m_scrollPos = 0;
    } else {
        m_scrollPos += scrollStep;
        if (m_scrollPos > m_maxScroll) m_scrollPos = m_maxScroll;
    }

    SetScrollPos(m_hwnd, SB_VERT, m_scrollPos, TRUE);
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void MainWindow::on_vscroll(WPARAM wParam) {
    if (m_currentScreen != AppScreen::List) return;

    int action = LOWORD(wParam);
    int step = scale_px(m_itemHeight + m_itemSpacing);

    switch (action) {
        case SB_LINEUP: m_scrollPos -= step; break;
        case SB_LINEDOWN: m_scrollPos += step; break;
        case SB_PAGEUP: m_scrollPos -= step * 3; break;
        case SB_PAGEDOWN: m_scrollPos += step * 3; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION:
            m_scrollPos = HIWORD(wParam);
            break;
    }

    m_scrollPos = (std::clamp)(m_scrollPos, 0, m_maxScroll);
    SetScrollPos(m_hwnd, SB_VERT, m_scrollPos, TRUE);
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void MainWindow::on_paint(HDC hdc) {
    RECT clientRc;
    GetClientRect(m_hwnd, &clientRc);
    int width = clientRc.right - clientRc.left;
    int height = clientRc.bottom - clientRc.top;

    // Double buffering
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, width, height);
    HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

    Gdiplus::Graphics g(memDC);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

    // Clear background #F5F5F5
    Gdiplus::SolidBrush bgBrush(Gdiplus::Color(255, 245, 245, 245));
    g.FillRectangle(&bgBrush, 0, 0, width, height);

    if (m_currentScreen == AppScreen::List) {
        // --- TOP BAR ---
        // Search container: rounded rect #FFFFFF, border #D0D0D0
        int addBtnWidth = scale_px(95);
        int searchWidth = width - scale_px(16 + 12) - addBtnWidth - scale_px(16);
        Gdiplus::RectF searchRect(static_cast<float>(scale_px(16)), static_cast<float>(scale_px(12)),
                                  static_cast<float>(searchWidth), static_cast<float>(scale_px(34)));
        GdiRenderer::draw_rounded_rect(g, searchRect, static_cast<float>(scale_px(6)),
                                      Gdiplus::Color(255, 255, 255, 255),
                                      Gdiplus::Color(255, 208, 208, 208), 1.0f);

        // Search icon inside search container
        GdiRenderer::draw_search_icon(g, static_cast<float>(scale_px(22)), static_cast<float>(scale_px(21)), static_cast<float>(scale_px(16)));

        // "Добавить" button: accent #0067C0, hover #005A9E
        int addBtnX = width - scale_px(16) - addBtnWidth;
        Gdiplus::RectF addBtnRect(static_cast<float>(addBtnX), static_cast<float>(scale_px(12)),
                                  static_cast<float>(addBtnWidth), static_cast<float>(scale_px(34)));
        Gdiplus::Color addBtnColor = (m_hoverTarget == HoverTarget::AddButton)
            ? Gdiplus::Color(255, 0, 90, 158)
            : Gdiplus::Color(255, 0, 103, 192);
        GdiRenderer::draw_rounded_rect(g, addBtnRect, static_cast<float>(scale_px(6)), addBtnColor);

        // Text "+ Добавить"
        Gdiplus::Font btnFont(L"Segoe UI", static_cast<float>(scale_px(10)), Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush textWhite(Gdiplus::Color(255, 255, 255, 255));
        Gdiplus::StringFormat sf;
        sf.SetAlignment(Gdiplus::StringAlignmentCenter);
        sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        g.DrawString(L"+ Добавить", -1, &btnFont, addBtnRect, &sf, &textWhite);

        // --- LIST ITEMS ---
        int listTop = scale_px(m_listTop);
        int cardW = width - scale_px(32);
        int cardH = scale_px(m_itemHeight);
        int totalItemH = scale_px(m_itemHeight + m_itemSpacing);

        Gdiplus::Font titleFont(L"Segoe UI", static_cast<float>(scale_px(13)), Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::Font pinnedTitleFont(L"Segoe UI", static_cast<float>(scale_px(13)), Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::Font copiedFont(L"Segoe UI", static_cast<float>(scale_px(11)), Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush textDark(Gdiplus::Color(255, 32, 32, 32));
        Gdiplus::SolidBrush textCopied(Gdiplus::Color(255, 46, 125, 50)); // Dark green text

        // Visible range optimization
        int startIdx = (std::max)(0, (m_scrollPos - listTop) / totalItemH);
        int endIdx = (std::min)(static_cast<int>(m_displayedItems.size()), (m_scrollPos + height - listTop) / totalItemH + 1);

        for (int i = startIdx; i < endIdx; ++i) {
            const auto& item = m_displayedItems[i];
            int yPos = listTop + i * totalItemH - m_scrollPos;

            Gdiplus::RectF cardRect(static_cast<float>(scale_px(16)), static_cast<float>(yPos),
                                    static_cast<float>(cardW), static_cast<float>(cardH));

            bool isHovered = (m_hoveredIndex == i);
            bool isCopied = (item.id == m_copiedItemId && now_ms() - m_copiedTime < 300);

            Gdiplus::Color cardColor;
            if (isCopied) {
                cardColor = Gdiplus::Color(255, 200, 230, 201); // Soft green #C8E6C9
            } else if (isHovered) {
                cardColor = Gdiplus::Color(255, 225, 225, 225); // Hover #E1E1E1
            } else {
                cardColor = Gdiplus::Color(255, 234, 234, 234); // Normal #EAEAEA
            }

            GdiRenderer::draw_rounded_rect(g, cardRect, static_cast<float>(scale_px(6)), cardColor);

            // Card title
            std::wstring displayTitle = utf8_to_wide(item.get_display_title());
            float titleX = cardRect.X + scale_px(14);
            float titleY = cardRect.Y + scale_px(14);
            float titleMaxW = cardRect.Width - scale_px(108);

            Gdiplus::RectF titleRect(titleX, titleY, titleMaxW, static_cast<float>(scale_px(22)));
            Gdiplus::StringFormat titleFormat;
            titleFormat.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
            titleFormat.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);

            g.DrawString(displayTitle.c_str(), -1, item.is_pinned ? &pinnedTitleFont : &titleFont, titleRect, &titleFormat, &textDark);

            if (isCopied) {
                Gdiplus::RectF copiedBadgeRect(cardRect.X + cardRect.Width - scale_px(190), cardRect.Y + scale_px(14), scale_px(90), scale_px(20));
                g.DrawString(L"Скопировано!", -1, &copiedFont, copiedBadgeRect, nullptr, &textCopied);
            }

            // Action buttons on right
            float iconSize = static_cast<float>(scale_px(16));
            float iconCenterY = cardRect.Y + (cardRect.Height - iconSize) / 2.0f;
            float cardRight = cardRect.X + cardRect.Width;

            bool pinHovered = isHovered && (m_hoverTarget == HoverTarget::PinIcon);
            bool editHovered = isHovered && (m_hoverTarget == HoverTarget::EditIcon);
            bool trashHovered = isHovered && (m_hoverTarget == HoverTarget::DeleteIcon);

            // Pin icon
            GdiRenderer::draw_pin_icon(g, cardRight - scale_px(76), iconCenterY, iconSize, item.is_pinned, pinHovered);
            // Edit icon
            GdiRenderer::draw_edit_icon(g, cardRight - scale_px(48), iconCenterY, iconSize, editHovered);
            // Trash icon
            GdiRenderer::draw_trash_icon(g, cardRight - scale_px(22), iconCenterY, iconSize, trashHovered);
        }

        if (m_displayedItems.empty()) {
            Gdiplus::Font emptyFont(L"Segoe UI", static_cast<float>(scale_px(13)), Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
            Gdiplus::SolidBrush emptyBrush(Gdiplus::Color(255, 140, 140, 140));
            Gdiplus::RectF emptyRect(0, static_cast<float>(listTop + scale_px(40)), static_cast<float>(width), static_cast<float>(scale_px(40)));
            Gdiplus::StringFormat sfEmpty;
            sfEmpty.SetAlignment(Gdiplus::StringAlignmentCenter);
            g.DrawString(m_searchQuery.empty() ? L"Буфер пуст. Скопируйте текст в любой программе!" : L"Ничего не найдено", -1, &emptyFont, emptyRect, &sfEmpty, &emptyBrush);
        }

        // Inline notification banner (e.g. Pin Limit Reached)
        if (!m_notificationMsg.empty() && now_ms() < m_notificationExpiry) {
            float bannerW = width - scale_px(32.0f);
            float bannerH = static_cast<float>(scale_px(36));
            float bannerX = static_cast<float>(scale_px(16));
            float bannerY = static_cast<float>(height - scale_px(50));

            Gdiplus::RectF bannerRect(bannerX, bannerY, bannerW, bannerH);
            GdiRenderer::draw_rounded_rect(g, bannerRect, static_cast<float>(scale_px(6)),
                                          Gdiplus::Color(255, 254, 237, 237),
                                          Gdiplus::Color(255, 245, 108, 108), 1.0f);

            Gdiplus::Font bannerFont(L"Segoe UI", static_cast<float>(scale_px(12)), Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
            Gdiplus::SolidBrush bannerText(Gdiplus::Color(255, 200, 30, 30));
            Gdiplus::StringFormat sfBanner;
            sfBanner.SetAlignment(Gdiplus::StringAlignmentCenter);
            sfBanner.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            g.DrawString(m_notificationMsg.c_str(), -1, &bannerFont, bannerRect, &sfBanner, &bannerText);
        }
    } else {
        // --- SCREEN 2: EDITOR ---
        int padding = scale_px(16);
        int btnH = scale_px(38);
        int btnY = height - padding - btnH;
        int btnW = (width - padding * 3) / 2;

        // "Назад" button: light gray #EAEAEA, hover #E1E1E1
        Gdiplus::RectF backBtnRect(static_cast<float>(padding), static_cast<float>(btnY),
                                   static_cast<float>(btnW), static_cast<float>(btnH));
        Gdiplus::Color backColor = (m_hoverTarget == HoverTarget::BackButton)
            ? Gdiplus::Color(255, 225, 225, 225)
            : Gdiplus::Color(255, 234, 234, 234);
        GdiRenderer::draw_rounded_rect(g, backBtnRect, static_cast<float>(scale_px(6)), backColor);

        Gdiplus::Font btnFont(L"Segoe UI", static_cast<float>(scale_px(13)), Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush textDark(Gdiplus::Color(255, 32, 32, 32));
        Gdiplus::StringFormat sfCenter;
        sfCenter.SetAlignment(Gdiplus::StringAlignmentCenter);
        sfCenter.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        g.DrawString(L"Назад", -1, &btnFont, backBtnRect, &sfCenter, &textDark);

        // "Сохранить" button: blue #0067C0, hover #005A9E
        Gdiplus::RectF saveBtnRect(static_cast<float>(padding * 2 + btnW), static_cast<float>(btnY),
                                   static_cast<float>(btnW), static_cast<float>(btnH));
        Gdiplus::Color saveColor = (m_hoverTarget == HoverTarget::SaveButton)
            ? Gdiplus::Color(255, 0, 90, 158)
            : Gdiplus::Color(255, 0, 103, 192);
        GdiRenderer::draw_rounded_rect(g, saveBtnRect, static_cast<float>(scale_px(6)), saveColor);

        Gdiplus::SolidBrush textWhite(Gdiplus::Color(255, 255, 255, 255));
        g.DrawString(L"Сохранить", -1, &btnFont, saveBtnRect, &sfCenter, &textWhite);
    }

    // Blit to screen
    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

} // namespace copyheap
