#include "gdi_renderer.h"

#pragma comment(lib, "gdiplus.lib")

namespace copyheap {

ULONG_PTR GdiRenderer::s_gdiToken = 0;

void GdiRenderer::init() {
    Gdiplus::GdiplusStartupInput gdiInput;
    Gdiplus::GdiplusStartup(&s_gdiToken, &gdiInput, nullptr);
}

void GdiRenderer::shutdown() {
    if (s_gdiToken != 0) {
        Gdiplus::GdiplusShutdown(s_gdiToken);
        s_gdiToken = 0;
    }
}

void GdiRenderer::draw_rounded_rect(
    Gdiplus::Graphics& g,
    const Gdiplus::RectF& rect,
    float radius,
    const Gdiplus::Color& fillColor,
    const Gdiplus::Color& borderColor,
    float borderWidth)
{
    float diameter = radius * 2.0f;
    if (diameter > rect.Width) diameter = rect.Width;
    if (diameter > rect.Height) diameter = rect.Height;

    Gdiplus::GraphicsPath path;
    path.AddArc(rect.X, rect.Y, diameter, diameter, 180, 90);
    path.AddArc(rect.X + rect.Width - diameter, rect.Y, diameter, diameter, 270, 90);
    path.AddArc(rect.X + rect.Width - diameter, rect.Y + rect.Height - diameter, diameter, diameter, 0, 90);
    path.AddArc(rect.X, rect.Y + rect.Height - diameter, diameter, diameter, 90, 90);
    path.CloseFigure();

    if (fillColor.GetAlpha() > 0) {
        Gdiplus::SolidBrush brush(fillColor);
        g.FillPath(&brush, &path);
    }

    if (borderColor.GetAlpha() > 0 && borderWidth > 0.0f) {
        Gdiplus::Pen pen(borderColor, borderWidth);
        g.DrawPath(&pen, &path);
    }
}

void GdiRenderer::draw_pin_icon(Gdiplus::Graphics& g, float x, float y, float size, bool isPinned, bool isHovered) {
    Gdiplus::Color iconColor = isPinned
        ? Gdiplus::Color(255, 0, 103, 192) // Active blue
        : (isHovered ? Gdiplus::Color(255, 40, 40, 40) : Gdiplus::Color(255, 120, 120, 120));

    float s = size / 16.0f;
    g.TranslateTransform(x, y);

    if (isPinned) {
        // Filled pin
        Gdiplus::SolidBrush brush(iconColor);
        // Head / top bar
        g.FillRectangle(&brush, 4.0f * s, 1.0f * s, 8.0f * s, 2.5f * s);
        // Pin body
        Gdiplus::PointF bodyPts[4] = {
            {5.0f * s, 3.5f * s},
            {11.0f * s, 3.5f * s},
            {10.0f * s, 9.5f * s},
            {6.0f * s, 9.5f * s}
        };
        g.FillPolygon(&brush, bodyPts, 4);
        // Wide base
        g.FillRectangle(&brush, 3.0f * s, 9.5f * s, 10.0f * s, 2.0f * s);
        // Needle tip
        Gdiplus::Pen pen(iconColor, 1.5f * s);
        g.DrawLine(&pen, 8.0f * s, 11.5f * s, 8.0f * s, 15.0f * s);
    } else {
        // Outline pin
        Gdiplus::Pen pen(iconColor, 1.3f * s);
        // Top bar
        g.DrawLine(&pen, 5.0f * s, 2.0f * s, 11.0f * s, 2.0f * s);
        // Body lines
        g.DrawLine(&pen, 6.0f * s, 2.0f * s, 6.5f * s, 9.0f * s);
        g.DrawLine(&pen, 10.0f * s, 2.0f * s, 9.5f * s, 9.0f * s);
        // Base line
        g.DrawLine(&pen, 4.0f * s, 9.5f * s, 12.0f * s, 9.5f * s);
        // Needle
        g.DrawLine(&pen, 8.0f * s, 9.5f * s, 8.0f * s, 14.5f * s);
    }

    g.ResetTransform();
}

void GdiRenderer::draw_edit_icon(Gdiplus::Graphics& g, float x, float y, float size, bool isHovered) {
    Gdiplus::Color iconColor = isHovered
        ? Gdiplus::Color(255, 0, 103, 192) // Hover blue
        : Gdiplus::Color(255, 120, 120, 120);

    float s = size / 16.0f;
    g.TranslateTransform(x, y);

    Gdiplus::Pen pen(iconColor, 1.3f * s);
    // Slanted pencil body
    Gdiplus::PointF pts[4] = {
        {10.5f * s, 2.5f * s},
        {13.5f * s, 5.5f * s},
        {5.5f * s, 13.5f * s},
        {2.5f * s, 10.5f * s}
    };
    g.DrawPolygon(&pen, pts, 4);

    // Tip
    g.DrawLine(&pen, 2.5f * s, 10.5f * s, 1.5f * s, 14.5f * s);
    g.DrawLine(&pen, 1.5f * s, 14.5f * s, 5.5f * s, 13.5f * s);

    g.ResetTransform();
}

void GdiRenderer::draw_trash_icon(Gdiplus::Graphics& g, float x, float y, float size, bool isHovered) {
    Gdiplus::Color iconColor = isHovered
        ? Gdiplus::Color(255, 215, 45, 45) // Hover red
        : Gdiplus::Color(255, 120, 120, 120);

    float s = size / 16.0f;
    g.TranslateTransform(x, y);

    Gdiplus::Pen pen(iconColor, 1.3f * s);
    // Lid
    g.DrawLine(&pen, 2.5f * s, 4.0f * s, 13.5f * s, 4.0f * s);
    g.DrawLine(&pen, 6.0f * s, 2.0f * s, 10.0f * s, 2.0f * s);

    // Can body
    Gdiplus::PointF canPts[4] = {
        {4.0f * s, 4.5f * s},
        {12.0f * s, 4.5f * s},
        {11.0f * s, 14.0f * s},
        {5.0f * s, 14.0f * s}
    };
    g.DrawPolygon(&pen, canPts, 4);

    // Inner vertical slats
    g.DrawLine(&pen, 7.0f * s, 6.5f * s, 6.8f * s, 12.0f * s);
    g.DrawLine(&pen, 9.0f * s, 6.5f * s, 9.2f * s, 12.0f * s);

    g.ResetTransform();
}

void GdiRenderer::draw_search_icon(Gdiplus::Graphics& g, float x, float y, float size) {
    Gdiplus::Color iconColor(255, 150, 150, 150);
    float s = size / 16.0f;
    g.TranslateTransform(x, y);

    Gdiplus::Pen pen(iconColor, 1.4f * s);
    // Circle lens
    g.DrawEllipse(&pen, 3.0f * s, 3.0f * s, 7.5f * s, 7.5f * s);
    // Handle
    g.DrawLine(&pen, 9.0f * s, 9.0f * s, 13.5f * s, 13.5f * s);

    g.ResetTransform();
}

} // namespace copyheap
