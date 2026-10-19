#pragma once
#include <windows.h>
#include <commctrl.h>
#include <array>
#include <string_view>
namespace screen::ui {
inline constexpr UINT printSlider = WM_APP + 40;
inline constexpr std::array<const wchar_t*, 7> themeNames{
    L"System", L"Pearl", L"Midnight", L"Graphite", L"Ocean", L"Forest", L"Sunset"};
struct Palette { COLORREF bg, fg, muted, accent, track, border; bool dark; bool operator==(const Palette&) const = default; };
Palette palette(int theme);
void windowTheme(HWND window, const Palette& colors);
void roundFill(HDC dc, RECT rect, int radius, COLORREF color);
void text(HDC dc, RECT rect, std::wstring_view value, COLORREF color, UINT flags = DT_LEFT | DT_VCENTER | DT_SINGLELINE);
void drawSlider(HWND slider, HDC target, const Palette& colors, UINT dpi);
LRESULT CALLBACK sliderProcedure(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
}
