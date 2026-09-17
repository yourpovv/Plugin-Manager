#include "theme.h"
#include <objidl.h>
#include <gdiplus.h>

static HFONT fontTitle = nullptr;
static HFONT fontHeading = nullptr;
static HFONT fontBody = nullptr;
static HFONT fontSmall = nullptr;
static HFONT fontButton = nullptr;
static HBRUSH brushBackground = nullptr;
static HBRUSH brushPanel = nullptr;
static HBRUSH brushInput = nullptr;
static ULONG_PTR gdiplusToken = 0;

static HFONT makeFont(int pixelSize, int weight) {
    return CreateFontW(
        -pixelSize, 0, 0, 0, weight, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
    );
}

static Gdiplus::Color gpColor(COLORREF color) {
    return Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color));
}

static void addRoundRect(Gdiplus::GraphicsPath& path, RECT rc, int radius) {
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;
    if (width < 1 || height < 1)
        return;
    int d = radius * 2;
    if (d > width)
        d = width;
    if (d > height)
        d = height;
    Gdiplus::Rect r(rc.left, rc.top, width, height);
    path.AddArc(r.X, r.Y, d, d, 180, 90);
    path.AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
    path.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90);
    path.AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90);
    path.CloseFigure();
}

void theme::startup() {
    Gdiplus::GdiplusStartupInput startupInput;
    Gdiplus::GdiplusStartup(&gdiplusToken, &startupInput, nullptr);
    fontTitle = makeFont(20, FW_SEMIBOLD);
    fontHeading = makeFont(14, FW_SEMIBOLD);
    fontBody = makeFont(13, FW_NORMAL);
    fontSmall = makeFont(12, FW_NORMAL);
    fontButton = makeFont(13, FW_SEMIBOLD);
    brushBackground = CreateSolidBrush(background);
    brushPanel = CreateSolidBrush(panel);
    brushInput = CreateSolidBrush(theme::input);
}

void theme::shutdown() {
    auto dropFont = [](HFONT& font) { if (font) { DeleteObject(font); font = nullptr; } };
    auto dropBrush = [](HBRUSH& brush) { if (brush) { DeleteObject(brush); brush = nullptr; } };
    dropFont(fontTitle);
    dropFont(fontHeading);
    dropFont(fontBody);
    dropFont(fontSmall);
    dropFont(fontButton);
    dropBrush(brushBackground);
    dropBrush(brushPanel);
    dropBrush(brushInput);
    if (gdiplusToken) {
        Gdiplus::GdiplusShutdown(gdiplusToken);
        gdiplusToken = 0;
    }
}

HFONT theme::titleFont() { return fontTitle; }
HFONT theme::headingFont() { return fontHeading; }
HFONT theme::bodyFont() { return fontBody; }
HFONT theme::smallFont() { return fontSmall; }
HFONT theme::buttonFont() { return fontButton; }
HBRUSH theme::backgroundBrush() { return brushBackground; }
HBRUSH theme::panelBrush() { return brushPanel; }
HBRUSH theme::inputBrush() { return brushInput; }

void theme::fillSolid(HDC dc, RECT rc, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rc, brush);
    DeleteObject(brush);
}

void theme::fillRound(HDC dc, RECT rc, COLORREF color, int radius) {
    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::GraphicsPath path;
    addRoundRect(path, rc, radius);
    Gdiplus::SolidBrush brush(gpColor(color));
    graphics.FillPath(&brush, &path);
}

void theme::strokeRound(HDC dc, RECT rc, COLORREF color, int radius) {
    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::GraphicsPath path;
    RECT inset = rc;
    inset.right -= 1;
    inset.bottom -= 1;
    addRoundRect(path, inset, radius);
    Gdiplus::Pen pen(gpColor(color), 1.0f);
    graphics.DrawPath(&pen, &path);
}

void theme::fillPill(HDC dc, RECT rc, COLORREF color) {
    int height = rc.bottom - rc.top;
    fillRound(dc, rc, color, height / 2);
}

void theme::drawText(HDC dc, RECT rc, const wchar_t* text, COLORREF color, HFONT font, UINT format) {
    HGDIOBJ oldFont = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text, -1, &rc, format);
    SelectObject(dc, oldFont);
}
