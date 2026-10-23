#pragma once
#include <windows.h>
#include <commctrl.h>
#include <array>
#include <string>
#include <string_view>
namespace screen::ui {
inline constexpr UINT printSlider = WM_APP + 40;
inline constexpr UINT printCombo = WM_APP + 41;
inline constexpr UINT motionAllowed = WM_APP + 42;
inline constexpr UINT visibilityChanged=WM_APP+43;
double interactionAmount(HWND window);
// Avoid native text/layout work when a frequently refreshed label is unchanged.
bool setText(HWND window, const std::wstring& value);
inline constexpr std::array<const wchar_t*, 7> themeNames{
    L"System", L"Pearl", L"Midnight", L"Graphite", L"Ocean", L"Forest", L"Sunset"};
struct Palette { COLORREF bg, fg, muted, accent, track, border; bool dark; bool highContrast = false; bool operator==(const Palette&) const = default; };
Palette palette(int theme);
COLORREF blend(COLORREF from, COLORREF to, double amount);
Palette blend(const Palette& from, const Palette& to, double amount);
void windowTheme(HWND window, const Palette& colors);
void roundFill(HDC dc, RECT rect, int radius, COLORREF color);
void text(HDC dc, RECT rect, std::wstring_view value, COLORREF color, UINT flags = DT_LEFT | DT_VCENTER | DT_SINGLELINE);
void drawSlider(HWND slider, HDC target, const Palette& colors, UINT dpi);
void drawCombo(HWND combo, HDC target, const Palette& colors, UINT dpi);
LRESULT CALLBACK comboProcedure(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
LRESULT CALLBACK buttonProcedure(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
LRESULT CALLBACK sliderProcedure(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
}
