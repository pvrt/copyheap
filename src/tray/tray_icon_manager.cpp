#include "tray_icon_manager.h"
#include "core/string_utils.h"

#include <gdiplus.h>

namespace copyheap {

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

    // Draw a neat modern blue circle dot for pinned items (#0067C0 / RGB 0, 103, 192)
    Gdiplus::Color dotColor(255, 0, 103, 192);
    Gdiplus::SolidBrush brush(dotColor);
    g.FillEllipse(&brush, 4.5f, 4.5f, 7.0f, 7.0f);
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
    OnSyncCallback on_sync,
    OnClearHistoryCallback on_clear_history,
    OnExitCallback on_exit)
{
    m_on_copy = std::move(on_copy);
    m_on_open = std::move(on_open);
    m_on_sync = std::move(on_sync);
    m_on_clear_history = std::move(on_clear_history);
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

    const auto& unpinned = sections.first;
    const auto& pinned = sections.second;

    // Windows 10/11 popup menu total overhead (gutter/check margin + text padding + accelerator margin + borders)
    // is ~90px. Thus to achieve exactly 280px total menu width: 280 - 90 = 190px target text width.
    const int targetTextWidthPx = 190;

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

    struct PreparedItem {
        UINT cmd_id;
        std::wstring label;
        std::string content;
        bool is_pinned;
    };
    std::vector<PreparedItem> prepItems;

    int maxTextWidth = 0;
    UINT currentCmdId = ID_TRAY_ITEM_BASE;

    if (!unpinned.empty() || !pinned.empty()) {
        for (const auto& item : unpinned) {
            std::wstring lbl = format_and_truncate(item.get_display_title());
            SIZE sz{};
            GetTextExtentPoint32W(hdc, lbl.c_str(), static_cast<int>(lbl.size()), &sz);
            if (sz.cx > maxTextWidth) maxTextWidth = sz.cx;
            prepItems.push_back({currentCmdId++, lbl, item.content, false});
        }

        for (const auto& item : pinned) {
            std::wstring lbl = format_and_truncate(item.get_display_title());
            SIZE sz{};
            GetTextExtentPoint32W(hdc, lbl.c_str(), static_cast<int>(lbl.size()), &sz);
            if (sz.cx > maxTextWidth) maxTextWidth = sz.cx;
            prepItems.push_back({currentCmdId++, lbl, item.content, true});
        }
    } else {
        std::wstring emptyLabel = L"(История пуста)";
        SIZE sz{};
        GetTextExtentPoint32W(hdc, emptyLabel.c_str(), static_cast<int>(emptyLabel.size()), &sz);
        maxTextWidth = sz.cx;
    }

    // Also measure system items to account for them
    std::wstring openLabel = L"Открыть";
    SIZE szOpen{};
    GetTextExtentPoint32W(hdc, openLabel.c_str(), static_cast<int>(openLabel.size()), &szOpen);
    if (szOpen.cx > maxTextWidth) maxTextWidth = szOpen.cx;

    // Pad widest item up to targetTextWidthPx if text is short, to keep 280px menu width
    SIZE szSpace{};
    GetTextExtentPoint32W(hdc, L" ", 1, &szSpace);
    int spaceWidth = szSpace.cx > 0 ? szSpace.cx : 3;

    std::wstring paddingSpaces;
    if (maxTextWidth < targetTextWidthPx) {
        int needSpaces = (targetTextWidthPx - maxTextWidth) / spaceWidth;
        if (needSpaces > 0) {
            paddingSpaces.assign(needSpaces, L' ');
        }
    }

    SelectObject(hdc, oldFont);
    DeleteObject(hMenuFont);
    ReleaseDC(nullptr, hdc);

    // Insert items into popup menu
    auto add_item_to_menu = [&](const PreparedItem& pItem, bool addPadding) {
        std::wstring text = pItem.label + (addPadding ? paddingSpaces : L"");

        MENUITEMINFOW mii{};
        mii.cbSize = sizeof(MENUITEMINFOW);
        mii.fType = MFT_STRING;
        mii.wID = pItem.cmd_id;
        mii.dwTypeData = const_cast<LPWSTR>(text.c_str());

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

    bool paddingApplied = false;

    // Block 1: Up to 10 unpinned items
    if (!unpinned.empty()) {
        for (const auto& p : prepItems) {
            if (!p.is_pinned) {
                add_item_to_menu(p, !paddingApplied);
                paddingApplied = true;
            }
        }
    } else {
        std::wstring emptyText = L"(История пуста)" + paddingSpaces;
        AppendMenuW(hMenu, MF_GRAYED | MF_STRING, 0, emptyText.c_str());
        paddingApplied = true;
    }

    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    // Block 2: Up to 20 pinned items with pin icon in gutter
    if (!pinned.empty()) {
        for (const auto& p : prepItems) {
            if (p.is_pinned) {
                add_item_to_menu(p, !paddingApplied);
                paddingApplied = true;
            }
        }
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    }

    // Block 3: System items
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_OPEN, L"Открыть");
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_SYNC, L"Настройки...");
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_CLEAR_HISTORY, L"Очистить историю");
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Выход");

    // Track popup menu properly
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
    if (cmdId == ID_TRAY_SYNC) {
        if (m_on_sync) m_on_sync();
        return true;
    }
    if (cmdId == ID_TRAY_CLEAR_HISTORY) {
        if (m_on_clear_history) m_on_clear_history();
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
