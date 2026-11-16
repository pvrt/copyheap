#pragma once

#include <windows.h>
#include <gdiplus.h>
#include <string>

namespace copyheap {

struct FluentTheme {
    bool is_dark = false;

    // Background colors
    Gdiplus::Color window_bg;
    Gdiplus::Color card_bg;
    Gdiplus::Color card_bg_hover;
    Gdiplus::Color card_bg_pressed;
    Gdiplus::Color card_border;
    Gdiplus::Color card_border_hover;

    // Text colors
    Gdiplus::Color text_primary;
    Gdiplus::Color text_secondary;
    Gdiplus::Color text_muted;

    // Accent colors (Windows 11 Fluent Blue)
    Gdiplus::Color accent;
    Gdiplus::Color accent_hover;
    Gdiplus::Color accent_text;

    // Input colors
    Gdiplus::Color input_bg;
    Gdiplus::Color input_border;
    Gdiplus::Color input_border_focus;

    // Icon action colors
    Gdiplus::Color icon_color;
    Gdiplus::Color icon_color_hover;
    Gdiplus::Color pin_color;
};

class ThemeManager {
public:
    static bool is_system_dark_mode();
    static FluentTheme get_current_theme();
    static void apply_window_attributes(HWND hwnd, bool is_dark);
    static std::wstring get_best_font_name();
};

} // namespace copyheap
