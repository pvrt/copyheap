#include "tray_icon_manager.h"
#include "core/string_utils.h"

#include <gdiplus.h>

namespace copyheap {

static HHOOK s_hMenuHook = nullptr;
static WNDPROC s_origMenuProc = nullptr;

static LRESULT CALLBACK MenuSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_WINDOWPOSCHANGING) {
        WINDOWPOS* pwp = reinterpret_cast<WINDOWPOS*>(lParam);
        if (pwp) {
            pwp->cx = 280; // Enforce exact 280px popup menu width
            pwp->flags &= ~SWP_NOSIZE;
        }
    }
    return CallWindowProcW(s_origMenuProc, hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK MenuCbtHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HCBT_CREATEWND) {
        HWND hwnd = reinterpret_cast<HWND>(wParam);
        wchar_t clsName[32] = {0};
        GetClassNameW(hwnd, clsName, 32);
        if (wcscmp(clsName, L"#32768") == 0) {
            s_origMenuProc = reinterpret_cast<WNDPROC>(
                SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(MenuSubclassProc))
            );
        }
    }
    return CallNextHookEx(s_hMenuHook, nCode, wParam, lParam);
}

TrayIconManager::TrayIconManager() {
    init_pin_bitmap();
}

TrayIconManager::~TrayIconManager() {
    remove();
    if (m_hPinBitmap) {
        DeleteObject(m_hPinBitmap);
        m_hPinBitmap = nullptr;
    }
}

void TrayIconManager::init_pin_bitmap() {
    if (m_hPinBitmap) return;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = 16;
    bmi.bmiHeader.biHeight = -16; // Top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    m_hPinBitmap = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    if (!m_hPinBitmap || !pBits) return;

    Gdiplus::Bitmap bmp(16, 16, 16 * 4, PixelFormat32bppPARGB, static_cast<BYTE*>(pBits));
    Gdiplus::Graphics g(&bmp);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.Clear(Gdiplus::Color(0, 0, 0, 0));

    // Draw pushpin icon in accent blue (#0067C0)
    Gdiplus::Color pinColor(255, 0, 103, 192);
    Gdiplus::SolidBrush brush(pinColor);

    // Pin head
    g.FillRectangle(&brush, 4.0f, 1.0f, 8.0f, 2.0f);
    // Pin body
    Gdiplus::PointF bodyPts[4] = {{5.0f, 3.0f}, {11.0f, 3.0f}, {10.0f, 8.5f}, {6.0f, 8.5f}};
    g.FillPolygon(&brush, bodyPts, 4);
    // Pin base
    g.FillRectangle(&brush, 3.0f, 8.5f, 10.0f, 2.0f);
    // Pin needle
    Gdiplus::Pen pen(pinColor, 1.5f);
    g.DrawLine(&pen, 8.0f, 10.5f, 8.0f, 14.5f);
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

    // Retrieve system menu font to measure text and ensure 280px menu width
    NONCLIENTMETRICSW ncm{};
    ncm.cbSize = sizeof(NONCLIENTMETRICSW);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), &ncm, 0);
    HFONT hMenuFont = CreateFontIndirectW(&ncm.lfMenuFont);

    HDC hdc = GetDC(nullptr);
    HFONT oldFont = (HFONT)SelectObject(hdc, hMenuFont);

    // Target text width in pixels for a 280px menu
    // Total menu width = text width + gutter/borders (approx 86px) = 188 + 86 = 274-280px
    const int targetTextWidthPx = 188;

    auto format_and_truncate = [&](const std::string& raw) -> std::wstring {
        std::wstring str = utf8_to_wide(clean_single_line(raw));
        if (str.empty()) str = L"(Пусто)";

        SIZE sz{};
        GetTextExtentPoint32W(hdc, str.c_str(), static_cast<int>(str.size()), &sz);

        if (sz.cx > targetTextWidthPx) {
            std::wstring ell = L"...";
            SIZE szEll{};
            GetTextExtentPoint32W(hdc, ell.c_str(), static_cast<int>(ell.size()), &szEll);

            while (!str.empty()) {
                str.pop_back();
                GetTextExtentPoint32W(hdc, str.c_str(), static_cast<int>(str.size()), &sz);
                if (sz.cx + szEll.cx <= targetTextWidthPx) {
                    str += ell;
                    break;
                }
            }
        }
        return str;
    };

    const auto& unpinned = sections.first;
    const auto& pinned = sections.second;

    struct PreparedItem {
        UINT cmd_id;
        std::wstring label;
        std::string content;
        bool is_pinned;
    };
    std::vector<PreparedItem> prepItems;

    UINT currentCmdId = ID_TRAY_ITEM_BASE;

    for (const auto& item : unpinned) {
        std::wstring lbl = format_and_truncate(item.get_display_title());
        prepItems.push_back({currentCmdId++, lbl, item.content, false});
    }

    for (const auto& item : pinned) {
        std::wstring lbl = format_and_truncate(item.get_display_title());
        prepItems.push_back({currentCmdId++, lbl, item.content, true});
    }

    SelectObject(hdc, oldFont);
    DeleteObject(hMenuFont);
    ReleaseDC(nullptr, hdc);

    // Insert items into popup menu
    auto add_item_to_menu = [&](const PreparedItem& pItem) {
        MENUITEMINFOW mii{};
        mii.cbSize = sizeof(MENUITEMINFOW);
        mii.fType = MFT_STRING;
        mii.wID = pItem.cmd_id;
        mii.dwTypeData = const_cast<LPWSTR>(pItem.label.c_str());

        if (pItem.is_pinned && m_hPinBitmap) {
            // Icon placed directly in left gutter to the left of the text
            mii.fMask = MIIM_STRING | MIIM_ID | MIIM_FTYPE | MIIM_BITMAP;
            mii.hbmpItem = m_hPinBitmap;
        } else {
            // No icon, text aligns perfectly with pinned items
            mii.fMask = MIIM_STRING | MIIM_ID | MIIM_FTYPE;
        }

        InsertMenuItemW(hMenu, GetMenuItemCount(hMenu), TRUE, &mii);
        m_active_menu_items.push_back({pItem.cmd_id, pItem.content});
    };

    // Block 1: Up to 10 unpinned items
    if (!unpinned.empty()) {
        for (const auto& p : prepItems) {
            if (!p.is_pinned) {
                add_item_to_menu(p);
            }
        }
    } else {
        AppendMenuW(hMenu, MF_GRAYED | MF_STRING, 0, L"(История пуста)");
    }

    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    // Block 2: Up to 20 pinned items with pin icon in gutter
    if (!pinned.empty()) {
        for (const auto& p : prepItems) {
            if (p.is_pinned) {
                add_item_to_menu(p);
            }
        }
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    }

    // Block 3: System items
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_OPEN, L"Открыть");
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Выход");

    // Track popup menu with hook to enforce exact 280px window width
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(m_hwnd);

    s_hMenuHook = SetWindowsHookExW(WH_CBT, MenuCbtHookProc, nullptr, GetCurrentThreadId());
    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hwnd, nullptr);
    if (s_hMenuHook) {
        UnhookWindowsHookEx(s_hMenuHook);
        s_hMenuHook = nullptr;
    }

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
