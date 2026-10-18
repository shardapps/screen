#include "overlay.hpp"
#include "settings.hpp"
#include <windowsx.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <memory>

namespace {
constexpr wchar_t windowClass[] = L"Shardapps.Screen.Control";
constexpr UINT trayMessage = WM_APP + 2, showMessage = WM_APP + 3;
constexpr int sliderId = 101, pauseId = 103, warmId = 104, smoothId = 105,
              exitId = 107, restoreId = 108, openId = 109, displayBase = 200;
constexpr int panelWidth = 330, panelHeight = 136;
LRESULT CALLBACK sliderProcedure(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);

struct App {
    HWND window{}, slider{}, label{}, value{}, footer{}, shortcut{};
    HFONT regular{}, number{}, footerFont{};
    HBRUSH background{};
    HICON icon{};
    UINT dpi = 96, taskbarCreated = 0;
    bool trayPresent = false, menuActive = false, animating = false, opening = false, placing = false;
    double panelAlpha = 255, fromAlpha = 255, toAlpha = 255;
    RECT destination{};
    std::chrono::steady_clock::time_point animationStart;
    COLORREF bg{}, fg{}, muted{}, accent{}, track{}, border{};
    std::wstring statusText, selected, shortcutText = L"Alt + S";
    screen::Settings settings = screen::loadSettings();
    std::vector<screen::Monitor> displays;
    std::unique_ptr<screen::OverlayEngine> engine;
    ~App() {
        for (auto font : {regular, number, footerFont}) if (font) DeleteObject(font);
        if (background) DeleteObject(background);
    }
    int px(int valuePx) const { return MulDiv(valuePx, static_cast<int>(dpi), 96); }
    int brightness() const { return selected.empty() ? settings.brightness : settings.forMonitor(selected); }
    void theme() {
        HIGHCONTRASTW contrast{sizeof(contrast)};
        SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0);
        DWORD light = 1, size = sizeof(light);
        RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &size);
        if (contrast.dwFlags & HCF_HIGHCONTRASTON) {
            bg = GetSysColor(COLOR_WINDOW); fg = muted = GetSysColor(COLOR_WINDOWTEXT);
            accent = GetSysColor(COLOR_HIGHLIGHT); track = GetSysColor(COLOR_GRAYTEXT); border = fg;
        } else if (light) {
            bg = RGB(249, 249, 252); fg = RGB(34, 35, 45); muted = RGB(108, 108, 125);
            accent = RGB(112, 87, 210); track = RGB(222, 221, 231); border = RGB(220, 219, 231);
        } else {
            bg = RGB(35, 35, 40); fg = RGB(244, 242, 250); muted = RGB(173, 169, 187);
            accent = RGB(183, 164, 255); track = RGB(75, 71, 86); border = RGB(65, 61, 75);
        }
        if (background) DeleteObject(background);
        background = CreateSolidBrush(bg);
        const BOOL dark = light == 0;
        DwmSetWindowAttribute(window, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
        DwmSetWindowAttribute(window, DWMWA_BORDER_COLOR, &border, sizeof(border));
        InvalidateRect(window, nullptr, TRUE);
        if (slider) InvalidateRect(slider, nullptr, TRUE);
    }
    void fonts() {
        const auto previous = std::initializer_list<HFONT>{regular, number, footerFont};
        const auto create = [&](int height, int weight) {
            return CreateFontW(-px(height), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        };
        regular = create(14, FW_SEMIBOLD); number = create(22, FW_NORMAL); footerFont = create(11, FW_NORMAL);
        for (auto child : {label, value, footer, shortcut}) if (child)
            SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(child == value ? number :
                child == label ? regular : footerFont), TRUE);
        for (auto font : previous) if (font) DeleteObject(font);
    }
    HWND control(const wchar_t* type, const wchar_t* text, DWORD style, int id) {
        return CreateWindowExW(0, type, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0,
            window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
    }
    void layout() {
        const auto move = [&](HWND child, int x, int y, int width, int height) {
            SetWindowPos(child, nullptr, px(x), px(y), px(width), px(height), SWP_NOZORDER | SWP_NOACTIVATE);
        };
        move(label, 49, 22, 180, 23); move(value, 238, 15, 68, 33);
        move(slider, 18, 57, 294, 34);
        move(footer, 24, 105, 207, 18); move(shortcut, 235, 105, 71, 18);
        RECT client{}; GetClientRect(window, &client);
        SetWindowRgn(window, CreateRoundRectRgn(0, 0, client.right, client.bottom, px(22), px(22)), TRUE);
    }
    void createControls() {
        // Keep the native slider and preceding label for keyboard and MSAA/UIA support.
        label = control(L"STATIC", L"Brightness", SS_NOPREFIX, 0);
        slider = control(TRACKBAR_CLASSW, L"Brightness", TBS_HORZ | TBS_NOTICKS | WS_TABSTOP, sliderId);
        value = control(L"STATIC", L"70%", SS_RIGHT | SS_NOPREFIX, 0);
        footer = control(L"STATIC", L"Paused · All displays", SS_NOPREFIX, 0);
        shortcut = control(L"STATIC", shortcutText.c_str(), SS_RIGHT | SS_NOPREFIX, 0);
        SetWindowSubclass(slider, sliderProcedure, 1, reinterpret_cast<DWORD_PTR>(this));
        SendMessageW(slider, TBM_SETRANGE, TRUE, MAKELPARAM(screen::minBrightness, screen::maxBrightness));
        SendMessageW(slider, TBM_SETPAGESIZE, 0, 5);
        fonts(); layout(); sync();
    }
    void sync(bool tracking = false) {
        if (!tracking && SendMessageW(slider, TBM_GETPOS, 0, 0) != brightness())
            SendMessageW(slider, TBM_SETPOS, TRUE, brightness());
        const auto percent = std::to_wstring(brightness()) + L"%";
        SetWindowTextW(value, percent.c_str());
        std::wstring displayName = L"All displays";
        for (size_t i = 0; i < displays.size(); ++i)
            if (displays[i].device == selected) displayName = L"Display " + std::to_wstring(i + 1);
        const auto summary = statusText.empty() ? (settings.paused ? L"Paused · " : L"") + displayName : statusText;
        SetWindowTextW(footer, summary.c_str());
        SetWindowTextW(shortcut, shortcutText.c_str());
        if (trayPresent && !tracking) {
            NOTIFYICONDATAW data{sizeof(data)}; data.hWnd = window; data.uID = 1; data.uFlags = NIF_TIP;
            const auto tip = settings.paused ? std::wstring(L"Screen by Shardapps · Paused") : settings.monitors.empty() ?
                L"Screen · " + std::to_wstring(settings.brightness) + L"% · " + shortcutText :
                L"Screen · Per-display dimming · " + shortcutText;
            wcscpy_s(data.szTip, tip.c_str()); Shell_NotifyIconW(NIM_MODIFY, &data);
        }
    }
    void apply(bool persist = true, bool immediate = false, bool tracking = false) {
        auto update = settings;
        if (immediate) update.smooth = false;
        engine->submit(update, persist);
        sync(tracking);
    }
    void setBrightness(int level) {
        if (selected.empty()) settings.setAll(level);
        else settings.monitors[selected] = screen::clampBrightness(level);
        settings.paused = false; statusText.clear();
    }
    bool addTray() {
        NOTIFYICONDATAW data{sizeof(data)};
        data.hWnd = window; data.uID = 1; data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
        data.hIcon = icon; data.uCallbackMessage = trayMessage;
        wcscpy_s(data.szTip, L"Screen by Shardapps");
        trayPresent = Shell_NotifyIconW(NIM_ADD, &data) != FALSE;
        if (trayPresent) { data.uVersion = NOTIFYICON_VERSION_4; Shell_NotifyIconW(NIM_SETVERSION, &data); }
        return trayPresent;
    }
    void place() {
        if (placing) return;
        placing = true;
        NOTIFYICONIDENTIFIER identifier{sizeof(identifier)}; identifier.hWnd = window; identifier.uID = 1;
        RECT iconBounds{}; POINT cursor{}; GetCursorPos(&cursor);
        const auto monitor = SUCCEEDED(Shell_NotifyIconGetRect(&identifier, &iconBounds)) ?
            MonitorFromRect(&iconBounds, MONITOR_DEFAULTTONEAREST) : MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
        MONITORINFO info{sizeof(info)}; GetMonitorInfoW(monitor, &info);
        // Moving to the target monitor can synchronously deliver WM_DPICHANGED.
        SetWindowPos(window, nullptr, info.rcWork.right - px(panelWidth + 12), info.rcWork.bottom - px(panelHeight + 12),
            0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
        const auto width = std::min<LONG>(px(panelWidth), info.rcWork.right - info.rcWork.left);
        const auto height = std::min<LONG>(px(panelHeight), info.rcWork.bottom - info.rcWork.top);
        destination = {std::max(info.rcWork.left, info.rcWork.right - width - px(12)),
            std::max(info.rcWork.top, info.rcWork.bottom - height - px(12)), 0, 0};
        destination.right = destination.left + width; destination.bottom = destination.top + height;
        SetWindowPos(window, HWND_TOPMOST, destination.left, destination.top, width, height, SWP_NOACTIVATE);
        layout(); placing = false;
    }
    void animate(bool show) {
        BOOL allowed = TRUE; SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &allowed, 0);
        opening = show; fromAlpha = panelAlpha; toAlpha = show ? 255.0 : 0.0;
        if (!settings.smooth || !allowed) {
            KillTimer(window, 10); animating = false; panelAlpha = toAlpha;
            SetLayeredWindowAttributes(window, 0, static_cast<BYTE>(panelAlpha), LWA_ALPHA);
            if (!show) ShowWindow(window, SW_HIDE);
            return;
        }
        animationStart = std::chrono::steady_clock::now();
        animating = SetTimer(window, 10, 15, nullptr) != 0;
        if (!animating) { panelAlpha = toAlpha; SetLayeredWindowAttributes(window, 0, static_cast<BYTE>(panelAlpha), LWA_ALPHA); if (!show) ShowWindow(window, SW_HIDE); }
    }
    void animateTick() {
        const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - animationStart).count();
        const auto duration = opening ? 0.16 : 0.09;
        panelAlpha = fromAlpha + (toAlpha - fromAlpha) * screen::ease(seconds * screen::transitionSeconds / duration);
        const auto offset = static_cast<int>(std::lround((255.0 - panelAlpha) / 255.0 * px(6)));
        SetWindowPos(window, nullptr, destination.left, destination.top + offset, 0, 0,
                     SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
        SetLayeredWindowAttributes(window, 0, static_cast<BYTE>(std::lround(panelAlpha)), LWA_ALPHA);
        if (seconds >= duration) {
            KillTimer(window, 10); animating = false;
            if (!opening) ShowWindow(window, SW_HIDE);
        }
    }
    void show() {
        place();
        if (!IsWindowVisible(window)) { panelAlpha = 0; SetLayeredWindowAttributes(window, 0, 0, LWA_ALPHA); }
        ShowWindow(window, SW_SHOWNOACTIVATE); animate(true);
        SetForegroundWindow(window); SetFocus(slider);
    }
    void hide() {
        if (!IsWindowVisible(window) && !animating) return;
        if (trayPresent || addTray()) animate(false);
        else { statusText = L"Tray unavailable"; sync(); }
    }
    void restore() {
        settings.paused = true; settings.setAll(100); statusText.clear(); apply(true, true);
    }
    void refreshDisplays() {
        displays = screen::enumerateMonitors();
        if (!selected.empty() && std::none_of(displays.begin(), displays.end(),
            [&](const auto& display) { return display.device == selected; })) selected.clear();
    }
    void menu() {
        menuActive = true; refreshDisplays();
        const auto popup = CreatePopupMenu(), monitors = CreatePopupMenu();
        const auto openText = L"Brightness\t" + shortcutText;
        AppendMenuW(popup, MF_STRING, openId, openText.c_str());
        AppendMenuW(popup, MF_STRING, pauseId, settings.paused ? L"Enable dimming\tCtrl+Alt+D" : L"Pause dimming\tCtrl+Alt+D");
        AppendMenuW(popup, MF_STRING, restoreId, L"Restore all displays");
        AppendMenuW(popup, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(monitors, MF_STRING | (selected.empty() ? MF_CHECKED : 0), displayBase, L"All displays");
        for (size_t i = 0; i < displays.size(); ++i) {
            const auto name = L"Display " + std::to_wstring(i + 1) + (displays[i].primary ? L" · Primary" : L"");
            AppendMenuW(monitors, MF_STRING | (selected == displays[i].device ? MF_CHECKED : 0), displayBase + i + 1, name.c_str());
        }
        AppendMenuW(popup, MF_POPUP, reinterpret_cast<UINT_PTR>(monitors), L"Display");
        AppendMenuW(popup, MF_STRING | (settings.warm ? MF_CHECKED : 0), warmId, L"Warm tint");
        AppendMenuW(popup, MF_STRING | (settings.smooth ? MF_CHECKED : 0), smoothId, L"Smooth transitions");
        AppendMenuW(popup, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(popup, MF_STRING, exitId, L"Quit Screen");
        POINT point{}; GetCursorPos(&point); SetForegroundWindow(window);
        const auto command = TrackPopupMenu(popup, TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, 0, window, nullptr);
        DestroyMenu(popup); menuActive = false;
        PostMessageW(window, WM_NULL, 0, 0);
        if (command) SendMessageW(window, WM_COMMAND, command, 0);
        else if (IsWindowVisible(window) && GetForegroundWindow() != window) hide();
    }
    void paint() {
        PAINTSTRUCT ps{}; const auto dc = BeginPaint(window, &ps);
        RECT client{}; GetClientRect(window, &client); FillRect(dc, &client, background);
        const auto pen = CreatePen(PS_SOLID, px(2), accent);
        const auto previousPen = SelectObject(dc, pen), previousBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
        Ellipse(dc, px(24), px(25), px(36), px(37));
        for (int i = 0; i < 8; ++i) {
            const double angle = i * 3.141592653589793 / 4.0;
            MoveToEx(dc, px(30) + static_cast<int>(std::lround(px(9) * std::cos(angle))),
                px(31) + static_cast<int>(std::lround(px(9) * std::sin(angle))), nullptr);
            LineTo(dc, px(30) + static_cast<int>(std::lround(px(12) * std::cos(angle))),
                px(31) + static_cast<int>(std::lround(px(12) * std::sin(angle))));
        }
        SelectObject(dc, previousPen); SelectObject(dc, previousBrush); DeleteObject(pen);
        EndPaint(window, &ps);
    }
    void paintSlider(HDC dc) {
        RECT client{}, channel{}, thumb{}; GetClientRect(slider, &client);
        SendMessageW(slider, TBM_GETCHANNELRECT, 0, reinterpret_cast<LPARAM>(&channel));
        SendMessageW(slider, TBM_GETTHUMBRECT, 0, reinterpret_cast<LPARAM>(&thumb));
        FillRect(dc, &client, background);
        const auto center = (thumb.left + thumb.right) / 2, y = (client.bottom + client.top) / 2;
        const auto paintRound = [&](RECT rect, int radius, COLORREF color) {
            const auto brush = CreateSolidBrush(color);
            const auto oldBrush = SelectObject(dc, brush), oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
            RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
            SelectObject(dc, oldPen); SelectObject(dc, oldBrush); DeleteObject(brush);
        };
        paintRound({channel.left, y - px(2), channel.right, y + px(3)}, px(5), track);
        if (center > channel.left) paintRound({channel.left, y - px(2), center, y + px(3)}, px(5), accent);
        const auto radius = px(GetCapture() == slider ? 7 : 8);
        const auto brush = CreateSolidBrush(accent);
        const auto oldBrush = SelectObject(dc, brush), oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
        Ellipse(dc, center - radius, y - radius, center + radius + 1, y + radius + 1);
        SelectObject(dc, oldPen); SelectObject(dc, oldBrush); DeleteObject(brush);
        if (GetFocus() == slider && !(SendMessageW(slider, WM_QUERYUISTATE, 0, 0) & UISF_HIDEFOCUS)) {
            const auto pen = CreatePen(PS_SOLID, 1, muted);
            const auto focusPen = SelectObject(dc, pen), focusBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
            const auto focusRadius = radius + px(3);
            Ellipse(dc, center - focusRadius, y - focusRadius, center + focusRadius + 1, y + focusRadius + 1);
            SelectObject(dc, focusBrush); SelectObject(dc, focusPen); DeleteObject(pen);
        }
    }
    void renderSlider(HDC target) {
        RECT client{}; GetClientRect(slider, &client);
        if (!client.right || !client.bottom) return;
        // Native trackbar invalidation only covers its own thumb geometry. Paint a
        // complete buffered frame so our circular thumb never leaves stale pixels.
        const auto buffer = CreateCompatibleDC(target);
        const auto bitmap = CreateCompatibleBitmap(target, client.right, client.bottom);
        if (buffer && bitmap) {
            const auto previous = SelectObject(buffer, bitmap);
            paintSlider(buffer);
            BitBlt(target, 0, 0, client.right, client.bottom, buffer, 0, 0, SRCCOPY);
            SelectObject(buffer, previous);
        } else paintSlider(target);
        if (bitmap) DeleteObject(bitmap);
        if (buffer) DeleteDC(buffer);
    }
};

LRESULT CALLBACK sliderProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR data) {
    auto* app = reinterpret_cast<App*>(data);
    if (message == WM_ERASEBKGND) return 1;
    // Let the native paint path initialize its theme/input geometry, but expand
    // partial invalidations before BeginPaint so the entire custom frame fits.
    if (message == WM_PAINT) InvalidateRect(window, nullptr, FALSE);
    if (message == WM_PRINTCLIENT) { app->renderSlider(reinterpret_cast<HDC>(wParam)); return 0; }
    if (message == WM_KEYDOWN && wParam == VK_ESCAPE) { PostMessageW(GetParent(window), WM_CLOSE, 0, 0); return 0; }
    if (message == WM_CONTEXTMENU) { SendMessageW(GetParent(window), message, wParam, lParam); return 0; }
    if (message == WM_LBUTTONDOWN)
        SendMessageW(window, WM_UPDATEUISTATE, MAKEWPARAM(UIS_SET, UISF_HIDEFOCUS), 0);
    if (message == WM_KEYDOWN)
        SendMessageW(window, WM_UPDATEUISTATE, MAKEWPARAM(UIS_CLEAR, UISF_HIDEFOCUS), 0);
    if (message == WM_NCDESTROY) RemoveWindowSubclass(window, sliderProcedure, 1);
    const auto result = DefSubclassProc(window, message, wParam, lParam);
    switch (message) {
    case TBM_SETPOS: case TBM_SETRANGE: case TBM_SETRANGEMIN: case TBM_SETRANGEMAX:
    case WM_MOUSEMOVE:
        if (wParam & MK_LBUTTON) InvalidateRect(window, nullptr, FALSE);
        break;
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_CAPTURECHANGED:
    case WM_KEYDOWN: case WM_KEYUP: case WM_MOUSEWHEEL:
    case WM_SETFOCUS: case WM_KILLFOCUS: case WM_UPDATEUISTATE:
        InvalidateRect(window, nullptr, FALSE); break;
    }
    return result;
}
LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* app = reinterpret_cast<App*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        app = static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        app->window = window; SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    if (!app) return DefWindowProcW(window, message, wParam, lParam);
    if (message == app->taskbarCreated && app->taskbarCreated) {
        app->trayPresent = false; if (!app->addTray()) app->show(); app->sync(); return 0;
    }
    switch (message) {
    case WM_CREATE: {
        app->dpi = GetDpiForWindow(window); app->theme(); app->refreshDisplays(); app->createControls();
        app->taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
        app->icon = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED));
        SendMessageW(window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(app->icon));
        app->engine = std::make_unique<screen::OverlayEngine>(window);
        if (!app->engine->healthy()) return -1;
        app->engine->submit(app->settings, false, true); app->addTray();
        constexpr UINT mods = MOD_CONTROL | MOD_ALT | MOD_NOREPEAT;
        RegisterHotKey(window, 1, mods, 'D'); RegisterHotKey(window, 2, mods, VK_DOWN);
        RegisterHotKey(window, 3, mods, VK_UP);
        if (!RegisterHotKey(window, 4, mods | MOD_SHIFT, VK_F12)) app->statusText = L"Restore shortcut in use";
        if (!RegisterHotKey(window, 5, MOD_ALT | MOD_NOREPEAT, 'S')) {
            app->shortcutText = RegisterHotKey(window, 5, mods, 'S') ? L"Ctrl + Alt + S" : L"Tray icon";
        }
        const DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUND;
        DwmSetWindowAttribute(window, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
        SetLayeredWindowAttributes(window, 0, 255, LWA_ALPHA); app->sync(); return 0;
    }
    case WM_HSCROLL:
        if (reinterpret_cast<HWND>(lParam) == app->slider) {
            app->setBrightness(static_cast<int>(SendMessageW(app->slider, TBM_GETPOS, 0, 0)));
            const bool tracking = LOWORD(wParam) == TB_THUMBTRACK;
            app->apply(!tracking, false, tracking);
        }
        return 0;
    case WM_NOTIFY:
        if (reinterpret_cast<NMHDR*>(lParam)->hwndFrom == app->slider &&
            reinterpret_cast<NMHDR*>(lParam)->code == NM_CUSTOMDRAW) {
            const auto* draw = reinterpret_cast<NMCUSTOMDRAW*>(lParam);
            if (draw->dwDrawStage == CDDS_PREPAINT) { app->renderSlider(draw->hdc); return CDRF_SKIPDEFAULT; }
        }
        break;
    case WM_COMMAND: {
        const auto command = LOWORD(wParam);
        if (command == exitId) DestroyWindow(window);
        else if (command == openId) app->show();
        else if (command == IDCANCEL) app->hide();
        else if (command == pauseId) { app->settings.paused = !app->settings.paused; app->statusText.clear(); app->apply(); }
        else if (command == restoreId) app->restore();
        else if (command == warmId) { app->settings.warm = !app->settings.warm; app->apply(); }
        else if (command == smoothId) { app->settings.smooth = !app->settings.smooth; app->apply(); }
        else if (command >= displayBase && static_cast<size_t>(command - displayBase) <= app->displays.size()) {
            app->selected = command == displayBase ? L"" : app->displays[command - displayBase - 1].device;
            app->statusText.clear(); app->sync();
        }
        return 0;
    }
    case WM_HOTKEY:
        if (wParam == 1) { app->settings.paused = !app->settings.paused; app->statusText.clear(); app->apply(); }
        else if (wParam == 2 || wParam == 3) {
            app->settings.setAll(app->settings.brightness + (wParam == 2 ? -5 : 5));
            app->settings.paused = false; app->statusText.clear(); app->apply();
        } else if (wParam == 4) app->restore();
        else if (wParam == 5) { if (IsWindowVisible(window) && (!app->animating || app->opening)) app->hide(); else app->show(); }
        return 0;
    case trayMessage:
        if (LOWORD(lParam) == WM_CONTEXTMENU) app->menu();
        else if (LOWORD(lParam) == NIN_SELECT || LOWORD(lParam) == NIN_KEYSELECT)
            SendMessageW(window, WM_HOTKEY, 5, 0);
        return 0;
    case showMessage: app->show(); return 0;
    case WM_CONTEXTMENU: app->menu(); return 0;
    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE && !app->menuActive && IsWindowVisible(window)) app->hide();
        return 0;
    case WM_CLOSE: app->hide(); return 0;
    case WM_TIMER: if (wParam == 10) app->animateTick(); return 0;
    case WM_DISPLAYCHANGE:
        app->refreshDisplays(); app->sync(); if (IsWindowVisible(window)) app->place(); return 0;
    case WM_SETTINGCHANGE: case WM_THEMECHANGED: app->theme(); return 0;
    case WM_DPICHANGED:
        app->dpi = HIWORD(wParam); app->fonts(); app->layout(); if (!app->placing) app->place(); return 0;
    case WM_MOUSEWHEEL:
        app->setBrightness(app->brightness() + GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA * 2); app->apply(); return 0;
    case screen::overlayFailure:
        app->settings.paused = true; app->statusText = L"Restored · try dimming again"; app->sync(); app->show(); return 0;
    case screen::settingsFailure:
        app->statusText = L"Preferences could not be saved"; app->sync(); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: app->paint(); return 0;
    case WM_CTLCOLORSTATIC:
        SetBkMode(reinterpret_cast<HDC>(wParam), TRANSPARENT);
        SetTextColor(reinterpret_cast<HDC>(wParam), reinterpret_cast<HWND>(lParam) == app->footer ||
            reinterpret_cast<HWND>(lParam) == app->shortcut ? app->muted : app->fg);
        return reinterpret_cast<LRESULT>(app->background);
    case WM_QUERYENDSESSION: return TRUE;
    case WM_ENDSESSION: if (wParam) DestroyWindow(window); return 0;
    case WM_DESTROY: {
        KillTimer(window, 10); app->engine.reset();
        NOTIFYICONDATAW data{sizeof(data)}; data.hWnd = window; data.uID = 1; Shell_NotifyIconW(NIM_DELETE, &data);
        for (int id = 1; id <= 5; ++id) UnregisterHotKey(window, id);
        PostQuitMessage(0); return 0;
    }
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const auto mutex = CreateMutexW(nullptr, FALSE, L"Local\\Shardapps.Screen.Instance");
    if (!mutex) return 1;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (auto existing = FindWindowW(windowClass, nullptr)) PostMessageW(existing, showMessage, 0, 0);
        CloseHandle(mutex); return 0;
    }
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES}; InitCommonControlsEx(&controls);
    App app;
    WNDCLASSEXW type{sizeof(type)}; type.hInstance = instance; type.lpfnWndProc = windowProcedure;
    type.lpszClassName = windowClass; type.hCursor = LoadCursorW(nullptr, IDC_ARROW); type.style = CS_DROPSHADOW;
    type.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    if (!RegisterClassExW(&type)) { CloseHandle(mutex); return 1; }
    const auto dpi = GetDpiForSystem(); RECT work{}; SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const auto width = MulDiv(panelWidth, static_cast<int>(dpi), 96), height = MulDiv(panelHeight, static_cast<int>(dpi), 96);
    const auto window = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED,
        windowClass, L"Screen by Shardapps", WS_POPUP | WS_CLIPCHILDREN,
        work.right - width - 12, work.bottom - height - 12, width, height, nullptr, nullptr, instance, &app);
    if (!window) { CloseHandle(mutex); return 1; }
    const bool background = commandLine && std::wstring(commandLine) == L"--background";
    if (!background || !app.trayPresent) app.show();
    MSG message{}; int result;
    while ((result = static_cast<int>(GetMessageW(&message, nullptr, 0, 0))) > 0) {
        if (!IsDialogMessageW(window, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }
    }
    if (IsWindow(window)) DestroyWindow(window);
    CloseHandle(mutex); return result == -1 ? 1 : static_cast<int>(message.wParam);
}
