#include "theme.h"
#include <dwmapi.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif

#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif

namespace copyheap {

bool ThemeManager::is_system_dark_mode() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD appsUseLightTheme = 1;
        DWORD size = sizeof(appsUseLightTheme);
        if (RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr,
                             reinterpret_cast<LPBYTE>(&appsUseLightTheme), &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return appsUseLightTheme == 0;
        }
        RegCloseKey(hKey);
    }
    return false;
}

std::wstring ThemeManager::get_best_font_name() {
    // Check if Segoe UI Variable Text is installed (Windows 11 standard)
    HDC hdc = GetDC(nullptr);
    bool hasVariable = false;
    EnumFontFamiliesW(hdc, L"Segoe UI Variable Text", [](const LOGFONTW*, const TEXTMETRICW*, DWORD, LPARAM lParam) -> int {
        *reinterpret_cast<bool*>(lParam) = true;
        return 0;
    }, reinterpret_cast<LPARAM>(&hasVariable));
    ReleaseDC(nullptr, hdc);

    return hasVariable ? L"Segoe UI Variable Text" : L"Segoe UI";
}

void ThemeManager::apply_window_attributes(HWND hwnd, bool is_dark) {
    if (!hwnd) return;

    // 1. Dark Mode titlebar
    BOOL darkVal = is_dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkVal, sizeof(darkVal));

    // 2. Round Corners (Windows 11)
    DWORD corner = 2; // DWMWCP_ROUND
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));

    // 3. System Backdrop (Mica Alt = 4, Mica = 2, Acrylic = 3)
    int backdrop = 2; // Mica
    DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
}

FluentTheme ThemeManager::get_current_theme() {
    bool dark = is_system_dark_mode();
    FluentTheme t;
    t.is_dark = dark;

    if (dark) {
        // Windows 11 Dark Fluent Theme
        t.window_bg          = Gdiplus::Color(255, 32, 32, 32);
        t.card_bg            = Gdiplus::Color(255, 43, 43, 43);
        t.card_bg_hover      = Gdiplus::Color(255, 52, 52, 52);
        t.card_bg_pressed    = Gdiplus::Color(255, 38, 38, 38);
        t.card_border        = Gdiplus::Color(25, 255, 255, 255); // 10% white
        t.card_border_hover  = Gdiplus::Color(45, 255, 255, 255); // 18% white

        t.text_primary       = Gdiplus::Color(255, 255, 255, 255);
        t.text_secondary     = Gdiplus::Color(255, 200, 200, 200);
        t.text_muted         = Gdiplus::Color(255, 140, 140, 140);

        t.accent             = Gdiplus::Color(255, 76, 194, 255); // Win 11 Dark Accent Blue
        t.accent_hover       = Gdiplus::Color(255, 105, 205, 255);
        t.accent_text        = Gdiplus::Color(255, 0, 0, 0);

        t.input_bg           = Gdiplus::Color(255, 43, 43, 43);
        t.input_border       = Gdiplus::Color(35, 255, 255, 255);
        t.input_border_focus = Gdiplus::Color(255, 76, 194, 255);

        t.icon_color         = Gdiplus::Color(255, 160, 160, 160);
        t.icon_color_hover   = Gdiplus::Color(255, 255, 255, 255);
        t.pin_color          = Gdiplus::Color(255, 76, 194, 255);
    } else {
        // Windows 11 Light Fluent Theme
        t.window_bg          = Gdiplus::Color(255, 243, 243, 243);
        t.card_bg            = Gdiplus::Color(255, 255, 255, 255);
        t.card_bg_hover      = Gdiplus::Color(255, 249, 249, 249);
        t.card_bg_pressed    = Gdiplus::Color(255, 240, 240, 240);
        t.card_border        = Gdiplus::Color(255, 229, 229, 229);
        t.card_border_hover  = Gdiplus::Color(255, 200, 200, 200);

        t.text_primary       = Gdiplus::Color(255, 27, 27, 27);
        t.text_secondary     = Gdiplus::Color(255, 94, 94, 94);
        t.text_muted         = Gdiplus::Color(255, 140, 140, 140);

        t.accent             = Gdiplus::Color(255, 0, 103, 192); // Win 11 Light Accent Blue
        t.accent_hover       = Gdiplus::Color(255, 26, 120, 201);
        t.accent_text        = Gdiplus::Color(255, 255, 255, 255);

        t.input_bg           = Gdiplus::Color(255, 255, 255, 255);
        t.input_border       = Gdiplus::Color(255, 215, 215, 215);
        t.input_border_focus = Gdiplus::Color(255, 0, 103, 192);

        t.icon_color         = Gdiplus::Color(255, 120, 120, 120);
        t.icon_color_hover   = Gdiplus::Color(255, 20, 20, 20);
        t.pin_color          = Gdiplus::Color(255, 0, 103, 192);
    }

    return t;
}

} // namespace copyheap
