#include "fluent_renderer.h"

namespace copyheap {

void FluentRenderer::draw_rounded_rectangle(Gdiplus::Graphics& g,
                                            const Gdiplus::RectF& rect,
                                            float radius,
                                            const Gdiplus::Color& fillColor,
                                            const Gdiplus::Color& borderColor,
                                            float borderWidth)
{
    float d = radius * 2.0f;
    if (d > rect.Width) d = rect.Width;
    if (d > rect.Height) d = rect.Height;

    Gdiplus::GraphicsPath path;
    path.AddArc(rect.X, rect.Y, d, d, 180.0f, 90.0f);
    path.AddArc(rect.X + rect.Width - d, rect.Y, d, d, 270.0f, 90.0f);
    path.AddArc(rect.X + rect.Width - d, rect.Y + rect.Height - d, d, d, 0.0f, 90.0f);
    path.AddArc(rect.X, rect.Y + rect.Height - d, d, d, 90.0f, 90.0f);
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

void FluentRenderer::draw_fluent_button(Gdiplus::Graphics& g,
                                       const Gdiplus::RectF& rect,
                                       const std::wstring& text,
                                       Gdiplus::Font* font,
                                       bool is_primary,
                                       bool is_hovered,
                                       bool is_pressed,
                                       const FluentTheme& theme)
{
    Gdiplus::Color bg = is_primary ? (is_pressed ? theme.accent : (is_hovered ? theme.accent_hover : theme.accent))
                                   : (is_pressed ? theme.card_bg_pressed : (is_hovered ? theme.card_bg_hover : theme.card_bg));

    Gdiplus::Color border = is_primary ? Gdiplus::Color(0, 0, 0, 0)
                                       : (is_hovered ? theme.card_border_hover : theme.card_border);

    draw_rounded_rectangle(g, rect, 6.0f, bg, border, 1.0f);

    Gdiplus::Color textColor = is_primary ? theme.accent_text : theme.text_primary;
    Gdiplus::SolidBrush textBrush(textColor);

    Gdiplus::StringFormat sf;
    sf.SetAlignment(Gdiplus::StringAlignmentCenter);
    sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    g.DrawString(text.c_str(), -1, font, rect, &sf, &textBrush);
}

void FluentRenderer::draw_pin_icon(Gdiplus::Graphics& g, float x, float y, float size, bool is_pinned, const Gdiplus::Color& color) {
    float s = size / 16.0f;
    g.TranslateTransform(x, y);

    Gdiplus::SolidBrush brush(color);
    Gdiplus::Pen pen(color, 1.5f * s);

    if (is_pinned) {
        // Filled pin head
        g.FillRectangle(&brush, 4.0f * s, 1.0f * s, 8.0f * s, 2.0f * s);
        // Body
        Gdiplus::PointF body[4] = {{5.0f * s, 3.0f * s}, {11.0f * s, 3.0f * s}, {10.0f * s, 8.5f * s}, {6.0f * s, 8.5f * s}};
        g.FillPolygon(&brush, body, 4);
        // Base
        g.FillRectangle(&brush, 3.0f * s, 8.5f * s, 10.0f * s, 2.0f * s);
        // Needle
        g.DrawLine(&pen, 8.0f * s, 10.5f * s, 8.0f * s, 14.5f * s);
    } else {
        // Outline pin
        g.DrawLine(&pen, 4.0f * s, 2.0f * s, 12.0f * s, 2.0f * s);
        g.DrawLine(&pen, 5.5f * s, 2.0f * s, 6.5f * s, 8.5f * s);
        g.DrawLine(&pen, 10.5f * s, 2.0f * s, 9.5f * s, 8.5f * s);
        g.DrawLine(&pen, 3.0f * s, 9.0f * s, 13.0f * s, 9.0f * s);
        g.DrawLine(&pen, 8.0f * s, 9.0f * s, 8.0f * s, 14.5f * s);
    }

    g.TranslateTransform(-x, -y);
}

void FluentRenderer::draw_copy_icon(Gdiplus::Graphics& g, float x, float y, float size, const Gdiplus::Color& color) {
    float s = size / 16.0f;
    g.TranslateTransform(x, y);

    Gdiplus::Pen pen(color, 1.4f * s);
    // Back card
    g.DrawRectangle(&pen, 5.0f * s, 2.0f * s, 8.0f * s, 9.0f * s);
    // Front card
    Gdiplus::SolidBrush clearBrush(Gdiplus::Color(255, 43, 43, 43)); // will be covered by card fill
    g.DrawRectangle(&pen, 2.5f * s, 5.0f * s, 8.0f * s, 9.0f * s);

    g.TranslateTransform(-x, -y);
}

void FluentRenderer::draw_edit_icon(Gdiplus::Graphics& g, float x, float y, float size, const Gdiplus::Color& color) {
    float s = size / 16.0f;
    g.TranslateTransform(x, y);

    Gdiplus::Pen pen(color, 1.4f * s);
    // Slanted pen
    g.DrawLine(&pen, 3.0f * s, 13.0f * s, 4.0f * s, 9.5f * s);
    g.DrawLine(&pen, 4.0f * s, 9.5f * s, 11.5f * s, 2.0f * s);
    g.DrawLine(&pen, 11.5f * s, 2.0f * s, 14.0f * s, 4.5f * s);
    g.DrawLine(&pen, 14.0f * s, 4.5f * s, 6.5f * s, 12.0f * s);
    g.DrawLine(&pen, 6.5f * s, 12.0f * s, 3.0f * s, 13.0f * s);

    g.TranslateTransform(-x, -y);
}

void FluentRenderer::draw_delete_icon(Gdiplus::Graphics& g, float x, float y, float size, const Gdiplus::Color& color) {
    float s = size / 16.0f;
    g.TranslateTransform(x, y);

    Gdiplus::Pen pen(color, 1.4f * s);
    // Can lid
    g.DrawLine(&pen, 3.0f * s, 4.0f * s, 13.0f * s, 4.0f * s);
    g.DrawLine(&pen, 6.0f * s, 2.5f * s, 10.0f * s, 2.5f * s);
    // Body
    g.DrawLine(&pen, 4.5f * s, 4.0f * s, 5.5f * s, 14.0f * s);
    g.DrawLine(&pen, 11.5f * s, 4.0f * s, 10.5f * s, 14.0f * s);
    g.DrawLine(&pen, 5.5f * s, 14.0f * s, 10.5f * s, 14.0f * s);
    // Vertical ribs
    g.DrawLine(&pen, 7.0f * s, 6.5f * s, 7.0f * s, 12.0f * s);
    g.DrawLine(&pen, 9.0f * s, 6.5f * s, 9.0f * s, 12.0f * s);

    g.TranslateTransform(-x, -y);
}

void FluentRenderer::draw_search_icon(Gdiplus::Graphics& g, float x, float y, float size, const Gdiplus::Color& color) {
    float s = size / 16.0f;
    g.TranslateTransform(x, y);

    Gdiplus::Pen pen(color, 1.5f * s);
    g.DrawEllipse(&pen, 2.5f * s, 2.5f * s, 8.0f * s, 8.0f * s);
    g.DrawLine(&pen, 8.5f * s, 8.5f * s, 13.5f * s, 13.5f * s);

    g.TranslateTransform(-x, -y);
}

} // namespace copyheap
