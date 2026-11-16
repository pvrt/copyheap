#pragma once

#include <windows.h>
#include <gdiplus.h>
#include <string>

namespace copyheap {

class GdiRenderer {
public:
    static void init();
    static void shutdown();

    static void draw_rounded_rect(Gdiplus::Graphics& g, const Gdiplus::RectF& rect, float radius,
                                  const Gdiplus::Color& fillColor, const Gdiplus::Color& borderColor = Gdiplus::Color(0, 0, 0, 0), float borderWidth = 1.0f);

    static void draw_pin_icon(Gdiplus::Graphics& g, float x, float y, float size, bool isPinned, bool isHovered);
    static void draw_edit_icon(Gdiplus::Graphics& g, float x, float y, float size, bool isHovered);
    static void draw_trash_icon(Gdiplus::Graphics& g, float x, float y, float size, bool isHovered);
    static void draw_search_icon(Gdiplus::Graphics& g, float x, float y, float size);

private:
    static ULONG_PTR s_gdiToken;
};

} // namespace copyheap
