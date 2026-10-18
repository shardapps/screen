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
constexpr int sliderId = 101, monitorId = 102, pauseId = 103, warmId = 104,
              smoothId = 105, backgroundId = 106, exitId = 107, restoreId = 108;
struct App {
    HWND window{}, slider{}, monitors{}, value{}, pause{}, warm{}, smooth{}, status{};
    HFONT regular{}, title{}, number{};
    HBRUSH background{}, cardBackground{};
    HICON icon{};
    UINT dpi = 96, taskbarCreated = 0;
    int scrollOffset = 0;
    bool trayPresent = false;
    std::wstring statusText = L"Ready. Enable dimming to get started.";
    screen::Settings settings = screen::loadSettings();
    std::vector<screen::Monitor> displays;
    std::unique_ptr<screen::OverlayEngine> engine;
    int px(int logical) const { return MulDiv(logical, static_cast<int>(dpi), 96); }
    void text(HWND child, const std::wstring& valueText) { SetWindowTextW(child, valueText.c_str()); }
    HWND control(const wchar_t* type, const wchar_t* label, DWORD style, int id,
                 int x, int y, int width, int height) {
        const auto child = CreateWindowExW(0, type, label, WS_CHILD | WS_VISIBLE | style,
            px(x), px(y), px(width), px(height), window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
        SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(regular), TRUE);
        return child;
    }
    void fonts() {
        for (auto font : {regular, title, number}) if (font) DeleteObject(font);
        regular = CreateFontW(-px(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        title = CreateFontW(-px(34), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI Variable Display");
        number = CreateFontW(-px(46), 0, 0, 0, FW_LIGHT, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    }
    void populateMonitors() {
        const auto selected = static_cast<int>(SendMessageW(monitors, CB_GETCURSEL, 0, 0));
        std::wstring device;
        if (selected > 0 && static_cast<size_t>(selected) <= displays.size()) device = displays[selected - 1].device;
        displays = screen::enumerateMonitors();
        SendMessageW(monitors, CB_RESETCONTENT, 0, 0);
        SendMessageW(monitors, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"All displays"));
        int selection = 0;
        for (size_t i = 0; i < displays.size(); ++i) {
            const auto& display = displays[i];
            const auto label = L"Display " + std::to_wstring(i + 1) + (display.primary ? L" · Primary" : L"") +
                L"  (" + std::to_wstring(display.bounds.right - display.bounds.left) + L" × " +
                std::to_wstring(display.bounds.bottom - display.bounds.top) + L")";
            SendMessageW(monitors, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
            if (display.device == device) selection = static_cast<int>(i + 1);
        }
        SendMessageW(monitors, CB_SETCURSEL, selection, 0);
    }
    std::wstring selectedDevice() const {
        const auto selected = static_cast<int>(SendMessageW(monitors, CB_GETCURSEL, 0, 0));
        return selected > 0 && static_cast<size_t>(selected) <= displays.size() ? displays[selected - 1].device : L"";
    }
    void sync() {
        const auto device = selectedDevice();
        const auto brightness = device.empty() ? settings.brightness : settings.forMonitor(device);
        SendMessageW(slider, TBM_SETPOS, TRUE, brightness);
        text(value, std::to_wstring(brightness) + L"%");
        text(pause, settings.paused ? L"Enable dimming" : L"Pause dimming");
        SendMessageW(warm, BM_SETCHECK, settings.warm ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(smooth, BM_SETCHECK, settings.smooth ? BST_CHECKED : BST_UNCHECKED, 0);
        if (status) text(status, statusText);
        if (trayPresent) {
            NOTIFYICONDATAW data{sizeof(data)};
            data.hWnd = window; data.uID = 1; data.uFlags = NIF_TIP;
            const auto tip = settings.paused ? std::wstring(L"Screen · Paused") : settings.monitors.empty() ?
                L"Screen · " + std::to_wstring(settings.brightness) + L"% · Dimmer active" :
                std::wstring(L"Screen · Per-display dimming active");
            wcscpy_s(data.szTip, tip.c_str());
            Shell_NotifyIconW(NIM_MODIFY, &data);
        }
    }
    void apply(bool persist = true, bool emergency = false) {
        if (emergency) {
            auto restored = settings; restored.smooth = false;
            engine->submit(restored, persist);
        } else engine->submit(settings, persist);
        sync();
    }
    void setBrightness(int brightness) {
        const auto device = selectedDevice();
        if (device.empty()) settings.setAll(brightness);
        else settings.monitors[device] = screen::clampBrightness(brightness);
        settings.paused = false;
        statusText = device.empty() ? L"Dimming all displays." : L"Dimming the selected display.";
    }
    void createControls() {
        control(L"STATIC", L"SHARDAPPS  /  DESKTOP ESSENTIALS", 0, 0, 28, 22, 385, 22);
        auto heading = control(L"STATIC", L"Screen", 0, 0, 26, 52, 390, 48);
        SendMessageW(heading, WM_SETFONT, reinterpret_cast<WPARAM>(title), TRUE);
        control(L"STATIC", L"A softer screen. Your focus, uninterrupted.", 0, 0, 28, 106, 390, 24);
        control(L"STATIC", L"Brightness", 0, 0, 42, 164, 180, 23);
        value = control(L"STATIC", L"70%", SS_RIGHT, 0, 254, 154, 150, 58);
        SendMessageW(value, WM_SETFONT, reinterpret_cast<WPARAM>(number), TRUE);
        slider = control(TRACKBAR_CLASSW, L"Screen brightness", TBS_HORZ | TBS_NOTICKS | WS_TABSTOP,
                         sliderId, 38, 225, 374, 36);
        SendMessageW(slider, TBM_SETRANGE, TRUE, MAKELPARAM(screen::minBrightness, screen::maxBrightness));
        SendMessageW(slider, TBM_SETPAGESIZE, 0, 5);
        control(L"STATIC", L"Softer", 0, 0, 42, 268, 110, 22);
        control(L"STATIC", L"Original", SS_RIGHT, 0, 294, 268, 110, 22);
        pause = control(L"BUTTON", L"Enable dimming", BS_PUSHBUTTON | WS_TABSTOP, pauseId, 42, 309, 362, 40);
        control(L"STATIC", L"Display", 0, 0, 42, 385, 92, 24);
        monitors = control(WC_COMBOBOXW, L"Display", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL,
                           monitorId, 135, 380, 269, 190);
        warm = control(L"BUTTON", L"Warm tint", BS_AUTOCHECKBOX | WS_TABSTOP, warmId, 42, 426, 168, 28);
        smooth = control(L"BUTTON", L"Smooth transitions", BS_AUTOCHECKBOX | WS_TABSTOP, smoothId, 220, 426, 190, 28);
        control(L"BUTTON", L"Restore all displays", BS_PUSHBUTTON | WS_TABSTOP, restoreId, 42, 471, 362, 34);
        status = control(L"STATIC", statusText.c_str(), 0, 0, 28, 540, 390, 38);
        control(L"BUTTON", L"Keep in tray", BS_PUSHBUTTON | WS_TABSTOP, backgroundId, 28, 590, 254, 36);
        control(L"BUTTON", L"Quit", BS_PUSHBUTTON | WS_TABSTOP, exitId, 296, 590, 122, 36);
        control(L"STATIC", L"Emergency restore: Ctrl + Alt + Shift + F12", 0, 0, 28, 644, 392, 22);
        populateMonitors();
        sync();
    }
    void updateScroll() {
        RECT client{}; GetClientRect(window, &client);
        SCROLLINFO info{sizeof(info), SIF_RANGE | SIF_PAGE | SIF_POS};
        info.nMin = 0; info.nMax = px(690) - 1;
        info.nPage = static_cast<UINT>(client.bottom);
        info.nPos = scrollOffset;
        SetScrollInfo(window, SB_VERT, &info, TRUE);
        const auto position = GetScrollPos(window, SB_VERT);
        if (position != scrollOffset) {
            ScrollWindowEx(window, 0, scrollOffset - position, nullptr, nullptr, nullptr, nullptr,
                           SW_SCROLLCHILDREN | SW_INVALIDATE);
            scrollOffset = position;
        }
    }
    void scroll(int delta) {
        const auto old = scrollOffset;
        SetScrollPos(window, SB_VERT, scrollOffset + delta, TRUE);
        scrollOffset = GetScrollPos(window, SB_VERT);
        ScrollWindowEx(window, 0, old - scrollOffset, nullptr, nullptr, nullptr, nullptr,
                       SW_SCROLLCHILDREN | SW_INVALIDATE);
    }
    bool addTray() {
        NOTIFYICONDATAW data{sizeof(data)};
        data.hWnd = window; data.uID = 1;
        data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
        data.hIcon = icon; data.uCallbackMessage = trayMessage;
        wcscpy_s(data.szTip, L"Screen by Shardapps");
        trayPresent = Shell_NotifyIconW(NIM_ADD, &data) != FALSE;
        if (trayPresent) { data.uVersion = NOTIFYICON_VERSION_4; Shell_NotifyIconW(NIM_SETVERSION, &data); }
        return trayPresent;
    }
    void show() {
        ShowWindow(window, SW_RESTORE);
        SetForegroundWindow(window);
    }
    void hide() {
        if (trayPresent || addTray()) ShowWindow(window, SW_HIDE);
        else {
            statusText = L"Tray unavailable. Keep this window open to control dimming.";
            sync(); show();
        }
    }
    void restore() {
        settings.paused = true;
        settings.setAll(100);
        statusText = L"Restored. All displays are at original brightness.";
        apply(true, true);
    }
    void menu() {
        const auto popup = CreatePopupMenu();
        AppendMenuW(popup, MF_STRING, 1, L"Open Screen");
        AppendMenuW(popup, MF_STRING, 2, settings.paused ? L"Enable dimming" : L"Pause dimming");
        AppendMenuW(popup, MF_STRING, 3, L"Restore all displays");
        AppendMenuW(popup, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(popup, MF_STRING, 4, L"Quit Screen");
        POINT cursor{}; GetCursorPos(&cursor);
        SetForegroundWindow(window);
        const auto command = TrackPopupMenu(popup, TPM_RETURNCMD | TPM_RIGHTBUTTON,
            cursor.x, cursor.y, 0, window, nullptr);
        DestroyMenu(popup);
        PostMessageW(window, WM_NULL, 0, 0);
        if (command == 1) show();
        if (command == 2) { settings.paused = !settings.paused; apply(); }
        if (command == 3) restore();
        if (command == 4) DestroyWindow(window);
    }
    void paint() {
        PAINTSTRUCT ps{};
        const auto dc = BeginPaint(window, &ps);
        RECT bounds{}; GetClientRect(window, &bounds);
        const auto buffer = CreateCompatibleDC(dc);
        const auto bitmap = CreateCompatibleBitmap(dc, bounds.right, bounds.bottom);
        const auto oldBitmap = SelectObject(buffer, bitmap);
        FillRect(buffer, &bounds, background);
        const auto card = CreateSolidBrush(RGB(255, 255, 255));
        const auto border = CreatePen(PS_SOLID, 1, RGB(226, 228, 236));
        const auto oldBrush = SelectObject(buffer, card);
        const auto oldPen = SelectObject(buffer, border);
        RoundRect(buffer, px(26), px(146) - scrollOffset, px(420), px(365) - scrollOffset, px(24), px(24));
        RoundRect(buffer, px(26), px(368) - scrollOffset, px(420), px(522) - scrollOffset, px(24), px(24));
        SelectObject(buffer, oldPen); SelectObject(buffer, oldBrush);
        DeleteObject(border); DeleteObject(card);
        BitBlt(dc, 0, 0, bounds.right, bounds.bottom, buffer, 0, 0, SRCCOPY);
        SelectObject(buffer, oldBitmap); DeleteObject(bitmap); DeleteDC(buffer);
        EndPaint(window, &ps);
    }
};
LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* app = reinterpret_cast<App*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        app = static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        app->window = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    if (!app) return DefWindowProcW(window, message, wParam, lParam);
    if (message == app->taskbarCreated && app->taskbarCreated) {
        app->trayPresent = false;
        if (!app->addTray()) app->show();
        app->sync(); return 0;
    }
    switch (message) {
    case WM_CREATE: {
        app->dpi = GetDpiForWindow(window);
        app->background = CreateSolidBrush(RGB(246, 247, 251));
        app->cardBackground = CreateSolidBrush(RGB(255, 255, 255));
        app->fonts(); app->createControls();
        app->taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
        app->icon = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1), IMAGE_ICON,
                                                0, 0, LR_DEFAULTSIZE | LR_SHARED));
        SendMessageW(window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(app->icon));
        SendMessageW(window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(app->icon));
        app->engine = std::make_unique<screen::OverlayEngine>(window);
        if (!app->engine->healthy()) return -1;
        app->engine->submit(app->settings, false, true);
        app->addTray();
        const auto mods = MOD_CONTROL | MOD_ALT | MOD_NOREPEAT;
        const bool pauseKey = RegisterHotKey(window, 1, mods, 'D') != FALSE;
        const bool downKey = RegisterHotKey(window, 2, mods, VK_DOWN) != FALSE;
        const bool upKey = RegisterHotKey(window, 3, mods, VK_UP) != FALSE;
        const bool emergency = RegisterHotKey(window, 4, mods | MOD_SHIFT, VK_F12) != FALSE;
        if (!pauseKey || !downKey || !upKey || !emergency) {
            app->statusText = L"A shortcut is in use. Tray controls and Restore remain available.";
            if (!emergency) app->statusText = L"Emergency shortcut unavailable. Use Restore or quit from the tray.";
            app->sync();
        }
        const DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUND;
        DwmSetWindowAttribute(window, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
        const COLORREF caption = RGB(246, 247, 251);
        DwmSetWindowAttribute(window, DWMWA_CAPTION_COLOR, &caption, sizeof(caption));
        return 0;
    }
    case WM_HSCROLL:
        if (reinterpret_cast<HWND>(lParam) == app->slider) {
            const auto brightness = static_cast<int>(SendMessageW(app->slider, TBM_GETPOS, 0, 0));
            app->setBrightness(brightness);
            // Thumb motion stays in memory; commit on release or a discrete keyboard step.
            app->apply(LOWORD(wParam) != TB_THUMBTRACK);
        }
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case monitorId: if (HIWORD(wParam) == CBN_SELCHANGE) app->sync(); break;
        case pauseId:
            app->settings.paused = !app->settings.paused;
            app->statusText = app->settings.paused ? L"Paused. Your original screen is restored." : L"Dimming enabled. Close the window to keep it in the tray.";
            app->apply(); break;
        case warmId: app->settings.warm = SendMessageW(app->warm, BM_GETCHECK, 0, 0) == BST_CHECKED; app->apply(); break;
        case smoothId: app->settings.smooth = SendMessageW(app->smooth, BM_GETCHECK, 0, 0) == BST_CHECKED; app->apply(); break;
        case restoreId: app->restore(); break;
        case backgroundId: app->hide(); break;
        case exitId: DestroyWindow(window); break;
        }
        return 0;
    case WM_HOTKEY:
        if (wParam == 1) { app->settings.paused = !app->settings.paused; app->apply(); }
        if (wParam == 2 || wParam == 3) {
            app->settings.setAll(app->settings.brightness + (wParam == 2 ? -5 : 5));
            app->settings.paused = false;
            app->apply();
        }
        if (wParam == 4) app->restore();
        return 0;
    case trayMessage:
        if (LOWORD(lParam) == WM_CONTEXTMENU) app->menu();
        else if (LOWORD(lParam) == NIN_SELECT || LOWORD(lParam) == NIN_KEYSELECT) app->show();
        return 0;
    case showMessage: app->show(); return 0;
    case WM_CLOSE: app->hide(); return 0;
    case WM_DISPLAYCHANGE: app->populateMonitors(); app->sync(); return 0;
    case WM_SIZE: app->updateScroll(); return 0;
    case WM_VSCROLL: {
        SCROLLINFO info{sizeof(info), SIF_TRACKPOS}; GetScrollInfo(window, SB_VERT, &info);
        switch (LOWORD(wParam)) {
        case SB_LINEUP: app->scroll(-app->px(28)); break;
        case SB_LINEDOWN: app->scroll(app->px(28)); break;
        case SB_PAGEUP: app->scroll(-app->px(180)); break;
        case SB_PAGEDOWN: app->scroll(app->px(180)); break;
        case SB_THUMBTRACK: app->scroll(info.nTrackPos - app->scrollOffset); break;
        }
        return 0;
    }
    case WM_MOUSEWHEEL: app->scroll(-GET_WHEEL_DELTA_WPARAM(wParam) * app->px(56) / WHEEL_DELTA); return 0;
    case screen::overlayFailure:
        app->settings.paused = true;
        app->statusText = L"Overlay unavailable. Displays restored; try enabling again.";
        app->sync(); app->show(); return 0;
    case screen::settingsFailure:
        app->statusText = L"Settings could not be saved. Current controls still work.";
        app->sync(); return 0;
    case WM_DPICHANGED: {
        app->dpi = HIWORD(wParam);
        // Rebuild controls at the new monitor's DPI, retaining settings and display selection.
        const auto rect = reinterpret_cast<RECT*>(lParam);
        MONITORINFO monitor{sizeof(monitor)};
        GetMonitorInfoW(MonitorFromRect(rect, MONITOR_DEFAULTTONEAREST), &monitor);
        const auto height = std::min(rect->bottom - rect->top, monitor.rcWork.bottom - monitor.rcWork.top);
        const auto top = std::clamp(rect->top, monitor.rcWork.top, monitor.rcWork.bottom - height);
        SetWindowPos(window, nullptr, rect->left, top, rect->right - rect->left, height,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        auto child = GetWindow(window, GW_CHILD);
        while (child) { const auto next = GetWindow(child, GW_HWNDNEXT); DestroyWindow(child); child = next; }
        app->scrollOffset = 0;
        app->fonts(); app->createControls(); app->updateScroll();
        InvalidateRect(window, nullptr, TRUE); return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: app->paint(); return 0;
    case WM_CTLCOLORSTATIC: case WM_CTLCOLORBTN: {
        const auto dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, RGB(37, 42, 62));
        SetBkMode(dc, TRANSPARENT);
        RECT child{}; GetWindowRect(reinterpret_cast<HWND>(lParam), &child);
        MapWindowPoints(nullptr, window, reinterpret_cast<POINT*>(&child), 2);
        const auto logicalY = child.top + app->scrollOffset;
        return reinterpret_cast<LRESULT>(logicalY >= app->px(146) && logicalY < app->px(522) ? app->cardBackground : app->background);
    }
    case WM_QUERYENDSESSION: return TRUE;
    case WM_ENDSESSION: if (wParam) DestroyWindow(window); return 0;
    case WM_DESTROY: {
        app->engine.reset();
        NOTIFYICONDATAW data{sizeof(data)}; data.hWnd = window; data.uID = 1;
        Shell_NotifyIconW(NIM_DELETE, &data);
        for (int id = 1; id <= 4; ++id) UnregisterHotKey(window, id);
        PostQuitMessage(0); return 0;
    }
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int showCommand) {
    // Per-monitor DPI awareness is also declared in the manifest.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const auto mutex = CreateMutexW(nullptr, FALSE, L"Local\\Shardapps.Screen.Instance");
    if (!mutex) return 1;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (auto existing = FindWindowW(windowClass, nullptr)) PostMessageW(existing, showMessage, 0, 0);
        CloseHandle(mutex); return 0;
    }
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);
    App app;
    WNDCLASSEXW type{sizeof(type)};
    type.hInstance = instance; type.lpfnWndProc = windowProcedure;
    type.lpszClassName = windowClass; type.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    type.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    if (!RegisterClassExW(&type)) { CloseHandle(mutex); return 1; }
    const auto dpi = GetDpiForSystem();
    RECT size{0, 0, MulDiv(446, static_cast<int>(dpi), 96), MulDiv(690, static_cast<int>(dpi), 96)};
    constexpr DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN | WS_VSCROLL;
    AdjustWindowRectExForDpi(&size, style, FALSE, 0, dpi);
    RECT workArea{}; SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    const auto height = std::min(size.bottom - size.top, workArea.bottom - workArea.top);
    const auto window = CreateWindowExW(0, windowClass, L"Screen by Shardapps", style,
        workArea.left + std::max(0L, (workArea.right - workArea.left - size.right + size.left) / 2),
        workArea.top + std::max(0L, (workArea.bottom - workArea.top - height) / 2), size.right - size.left, height,
        nullptr, nullptr, instance, &app);
    if (!window) {
        MessageBoxW(nullptr, L"Screen could not start. Your display has not been dimmed.", L"Screen", MB_OK | MB_ICONERROR);
        CloseHandle(mutex); return 1;
    }
    const bool background = commandLine && std::wstring(commandLine) == L"--background";
    if (!background || !app.trayPresent) ShowWindow(window, showCommand);
    MSG message{};
    int result;
    while ((result = static_cast<int>(GetMessageW(&message, nullptr, 0, 0))) > 0) {
        if (!IsDialogMessageW(window, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }
    }
    if (IsWindow(window)) DestroyWindow(window);
    for (auto font : {app.regular, app.title, app.number}) if (font) DeleteObject(font);
    DeleteObject(app.background);
    DeleteObject(app.cardBackground);
    CloseHandle(mutex);
    return result == -1 ? 1 : static_cast<int>(message.wParam);
}
