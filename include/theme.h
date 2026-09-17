#pragma once

#include "win32.h"

namespace theme {

constexpr COLORREF background = RGB(30, 31, 34);
constexpr COLORREF panel = RGB(43, 45, 49);
constexpr COLORREF card = RGB(49, 51, 56);
constexpr COLORREF cardHover = RGB(56, 58, 64);
constexpr COLORREF border = RGB(63, 65, 71);
constexpr COLORREF text = RGB(242, 243, 245);
constexpr COLORREF muted = RGB(181, 186, 193);
constexpr COLORREF accent = RGB(88, 101, 242);
constexpr COLORREF accentHover = RGB(71, 82, 196);
constexpr COLORREF accentPress = RGB(59, 68, 163);
constexpr COLORREF success = RGB(35, 165, 89);
constexpr COLORREF warning = RGB(240, 178, 50);
constexpr COLORREF danger = RGB(218, 55, 60);
constexpr COLORREF dangerHover = RGB(168, 40, 44);
constexpr COLORREF input = RGB(24, 25, 28);
constexpr COLORREF header = RGB(35, 36, 40);
constexpr COLORREF footer = RGB(35, 36, 40);
constexpr COLORREF hairline = RGB(54, 57, 63);
constexpr COLORREF well = RGB(24, 25, 28);

void startup();
void shutdown();

HFONT titleFont();
HFONT headingFont();
HFONT bodyFont();
HFONT smallFont();
HFONT buttonFont();

HBRUSH backgroundBrush();
HBRUSH panelBrush();
HBRUSH inputBrush();

void fillSolid(HDC dc, RECT rc, COLORREF color);
void fillRound(HDC dc, RECT rc, COLORREF color, int radius);
void strokeRound(HDC dc, RECT rc, COLORREF color, int radius);
void fillPill(HDC dc, RECT rc, COLORREF color);
void drawText(HDC dc, RECT rc, const wchar_t* text, COLORREF color, HFONT font, UINT format);

}
