#include "ui.hpp"
#include <dwmapi.h>
#include <algorithm>
namespace screen::ui {
Palette palette(int theme) {
    HIGHCONTRASTW contrast{sizeof(contrast)};
    SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0);
    if (contrast.dwFlags & HCF_HIGHCONTRASTON)
        return {GetSysColor(COLOR_WINDOW), GetSysColor(COLOR_WINDOWTEXT), GetSysColor(COLOR_WINDOWTEXT),
            GetSysColor(COLOR_HIGHLIGHT), GetSysColor(COLOR_GRAYTEXT), GetSysColor(COLOR_WINDOWTEXT), false};
    DWORD light = 1, size = sizeof(light);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &size);
    if (!theme) theme = light ? 1 : 2;
    switch (theme) {
    case 1: return {RGB(249,249,252), RGB(34,35,45), RGB(100,101,118), RGB(104,79,200), RGB(226,224,236), RGB(212,210,225), false};
    case 3: return {RGB(32,34,37), RGB(244,245,247), RGB(172,178,188), RGB(202,211,227), RGB(66,72,82), RGB(78,85,96), true};
    case 4: return {RGB(19,32,46), RGB(233,245,255), RGB(157,186,210), RGB(111,202,255), RGB(43,67,89), RGB(55,85,111), true};
    case 5: return {RGB(22,36,31), RGB(237,248,239), RGB(164,193,175), RGB(139,221,171), RGB(49,74,61), RGB(62,93,76), true};
    case 6: return {RGB(43,29,32), RGB(255,241,232), RGB(211,174,166), RGB(255,174,137), RGB(86,55,56), RGB(108,71,68), true};
    default: return {RGB(35,35,40), RGB(244,242,250), RGB(173,169,187), RGB(183,164,255), RGB(75,71,86), RGB(65,61,75), true};
    }
}
void windowTheme(HWND window, const Palette& colors) {
    const BOOL dark = colors.dark;
    DwmSetWindowAttribute(window, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    DwmSetWindowAttribute(window, DWMWA_BORDER_COLOR, &colors.border, sizeof(colors.border));
}
void roundFill(HDC dc, RECT rect, int radius, COLORREF color) {
    const auto brush = CreateSolidBrush(color);
    const auto previousBrush = SelectObject(dc, brush), previousPen = SelectObject(dc, GetStockObject(NULL_PEN));
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(dc, previousPen); SelectObject(dc, previousBrush); DeleteObject(brush);
}
void text(HDC dc, RECT rect, std::wstring_view value, COLORREF color, UINT flags) {
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, color);
    DrawTextW(dc, value.data(), static_cast<int>(value.size()), &rect, flags | DT_NOPREFIX);
}
void drawSlider(HWND slider, HDC target, const Palette& colors, UINT dpi) {
    RECT client{}, channel{}, thumb{}; GetClientRect(slider, &client);
    if (!client.right || !client.bottom) return;
    SendMessageW(slider, TBM_GETCHANNELRECT, 0, reinterpret_cast<LPARAM>(&channel));
    SendMessageW(slider, TBM_GETTHUMBRECT, 0, reinterpret_cast<LPARAM>(&thumb));
    const auto buffer = CreateCompatibleDC(target);
    const auto bitmap = CreateCompatibleBitmap(target, client.right, client.bottom);
    const auto dc = buffer && bitmap ? buffer : target;
    const auto previous = buffer && bitmap ? SelectObject(dc, bitmap) : nullptr;
    const auto px = [&](int value) { return MulDiv(value, static_cast<int>(dpi), 96); };
    const auto brush = CreateSolidBrush(colors.bg); FillRect(dc, &client, brush); DeleteObject(brush);
    const auto center = (thumb.left + thumb.right) / 2, y = client.bottom / 2;
    roundFill(dc, {channel.left, y-px(2), channel.right, y+px(3)}, px(5), colors.track);
    if (center > channel.left) roundFill(dc, {channel.left, y-px(2), center, y+px(3)}, px(5), colors.accent);
    const auto radius = px(GetCapture() == slider ? 7 : 8);
    const auto thumbBrush = CreateSolidBrush(colors.accent);
    const auto oldBrush = SelectObject(dc, thumbBrush), oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
    Ellipse(dc, center-radius, y-radius, center+radius+1, y+radius+1);
    SelectObject(dc, oldPen); SelectObject(dc, oldBrush); DeleteObject(thumbBrush);
    if (GetFocus() == slider && !(SendMessageW(slider, WM_QUERYUISTATE, 0, 0) & UISF_HIDEFOCUS)) {
        const auto pen = CreatePen(PS_SOLID, 1, colors.muted);
        const auto focusPen = SelectObject(dc, pen), focusBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
        const auto r = radius+px(3); Ellipse(dc, center-r, y-r, center+r+1, y+r+1);
        SelectObject(dc, focusBrush); SelectObject(dc, focusPen); DeleteObject(pen);
    }
    if (buffer && bitmap) { BitBlt(target, 0, 0, client.right, client.bottom, buffer, 0, 0, SRCCOPY); SelectObject(buffer, previous); }
    if (bitmap) DeleteObject(bitmap);
    if (buffer) DeleteDC(buffer);
}
LRESULT CALLBACK sliderProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR) {
    if (message == WM_ERASEBKGND) return 1;
    if (message == WM_PAINT) InvalidateRect(window, nullptr, FALSE);
    if (message == WM_PRINTCLIENT) return SendMessageW(GetParent(window), printSlider, wParam, reinterpret_cast<LPARAM>(window));
    if (message == WM_KEYDOWN && wParam == VK_ESCAPE) { PostMessageW(GetParent(window), WM_CLOSE, 0, 0); return 0; }
    if (message == WM_CONTEXTMENU) return SendMessageW(GetParent(window), message, wParam, lParam);
    if (message == WM_LBUTTONDOWN) SendMessageW(window, WM_UPDATEUISTATE, MAKEWPARAM(UIS_SET, UISF_HIDEFOCUS), 0);
    if (message == WM_KEYDOWN) SendMessageW(window, WM_UPDATEUISTATE, MAKEWPARAM(UIS_CLEAR, UISF_HIDEFOCUS), 0);
    if (message == WM_NCDESTROY) RemoveWindowSubclass(window, sliderProcedure, 1);
    const auto result = DefSubclassProc(window, message, wParam, lParam);
    switch (message) {
    case WM_MOUSEMOVE:
        if (wParam & MK_LBUTTON) InvalidateRect(window, nullptr, FALSE);
        break;
    case TBM_SETPOS: case TBM_SETRANGE: case TBM_SETRANGEMIN: case TBM_SETRANGEMAX:
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_CAPTURECHANGED:
    case WM_KEYDOWN: case WM_KEYUP: case WM_MOUSEWHEEL: case WM_SETFOCUS: case WM_KILLFOCUS: case WM_UPDATEUISTATE:
        InvalidateRect(window, nullptr, FALSE); break;
    }
    return result;
}
}
