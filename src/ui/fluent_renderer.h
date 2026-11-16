#pragma once

#include "theme.h"
#include <windows.h>
#include <gdiplus.h>
#include <string>

namespace copyheap {

class FluentRenderer {
public:
    static void draw_rounded_rectangle(Gdiplus::Graphics& g,
                                       const Gdiplus::RectF& rect,
                                       float radius,
                                       const Gdiplus::Color& fillColor,
                                       const Gdiplus::Color& borderColor,
                                       float borderWidth = 1.0f);

    static void draw_fluent_button(Gdiplus::Graphics& g,
                                   const Gdiplus::RectF& rect,
                                   const std::wstring& text,
                                   Gdiplus::Font* font,
                                   bool is_primary,
                                   bool is_hovered,
                                   bool is_pressed,
                                   const FluentTheme& theme);

    // Vector Action Icons (crisp GDI+ vectors at any DPI)
    static void draw_pin_icon(Gdiplus::Graphics& g, float x, float y, float size, bool is_pinned, const Gdiplus::Color& color);
    static void draw_copy_icon(Gdiplus::Graphics& g, float x, float y, float size, const Gdiplus::Color& color);
    static void draw_edit_icon(Gdiplus::Graphics& g, float x, float y, float size, const Gdiplus::Color& color);
    static void draw_delete_icon(Gdiplus::Graphics& g, float x, float y, float size, const Gdiplus::Color& color);
    static void draw_search_icon(Gdiplus::Graphics& g, float x, float y, float size, const Gdiplus::Color& color);
};

} // namespace copyheap
