#include "overlay.hpp"
#include "settings.hpp"
#include "preferences.hpp"
#include "brightness_keys.hpp"
#include <wtsapi32.h>
#include <string_view>
#include <windowsx.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <memory>

namespace {
constexpr wchar_t windowClass[] = L"Shardapps.Screen.Control";
constexpr UINT trayMessage = WM_APP + 2, showMessage = WM_APP + 3;
constexpr int sliderId = 101, pauseId = 103, warmId = 104, smoothId = 105,
              exitId = 107, restoreId = 108, openId = 109, displayPickerId = 110, preferencesId = 111, appBrightnessLabelId = 112, displayBase = 200;
constexpr int panelWidth = 330, panelHeight = 136;


struct App {
    HWND window{}, slider{}, label{}, value{}, footer{}, shortcut{}, preferencesButton{};
    HFONT regular{}, number{}, footerFont{};
    HBRUSH background{};
    HICON icon{};
    UINT dpi = 96, taskbarCreated = 0;
    bool trayPresent = false, menuActive = false, animating = false, opening = false, placing = false;
    bool overlayOnly = false, desktopCoverage = true;
    bool timedPause = false;
    int registeredShortcut = -1;
    double panelAlpha = 255, fromAlpha = 255, toAlpha = 255;
    RECT destination{};
    std::chrono::steady_clock::time_point animationStart;
    screen::FrameTimer panelFrame;
    COLORREF bg{}, fg{}, muted{}, accent{}, track{}, border{};
    screen::ui::Palette colors{};
    std::wstring statusText, selected, shortcutText = L"Alt + S";
    screen::Settings settings = screen::loadSettings();
    std::vector<screen::Monitor> displays;
    std::unique_ptr<screen::OverlayEngine> engine;
    std::unique_ptr<screen::PreferencesWindow> preferences;
    std::unique_ptr<screen::VirtualDesktops> workspaces;
    std::unique_ptr<screen::BrightnessKeys> brightnessKeys;
    bool testingKeys=false,keysUnavailable=false,keyRepeatArmed=false,keyLevelDirty=false,keyFeedback=false;
    unsigned observedKeys=0;
    screen::StartupController startup;
    screen::StartupState startupState = screen::StartupState::unknown;
    ~App() {
        if (preferences) preferences->close();
        for (auto font : {regular, number, footerFont}) if (font) DeleteObject(font);
        if (background) DeleteObject(background);
    }
    int px(int valuePx) const { return MulDiv(valuePx, static_cast<int>(dpi), 96); }
    int brightness() const { return selected.empty() ? settings.currentBrightness() : settings.forMonitor(selected); }
    void keyStatus() {
        if(!preferences) return;
        std::wstring text;
        if(keysUnavailable) text=L"Brightness-key listening unavailable. Use Ctrl + Alt + Up / Down.";
        else if(testingKeys) text=observedKeys==3?L"Both brightness keys detected. Test mode does not change Screen's brightness.":
            observedKeys==1?L"Brightness up detected. Press brightness down to finish the check.":
            observedKeys==2?L"Brightness down detected. Press brightness up to finish the check.":
            L"Listening for 15 seconds. Press both sun keys (with Fn if your keyboard requires it).";
        else if(observedKeys==3) text=settings.brightnessKeys?L"Both keys detected. Brightness-key control is on.":L"Both keys detected. Turn on the option above to use them.";
        else if(observedKeys) text=L"One brightness key detected. Run the test again to check both keys.";
        else text=settings.brightnessKeys?L"Listening for exposed brightness keys. Run the test to check your keyboard.":
            L"Off. Run the test to check whether Windows exposes your brightness keys.";
        preferences->brightnessKeyStatus(text,testingKeys);
    }
    bool listenForKeys() {
        if(brightnessKeys) return true;
        auto listener=std::make_unique<screen::BrightnessKeys>();
        keysUnavailable=!listener->start(window);
        if(!keysUnavailable) brightnessKeys=std::move(listener);
        return !keysUnavailable;
    }
    void clearHeldKeys() {
        KillTimer(window,30);keyRepeatArmed=false;
        if(brightnessKeys) brightnessKeys->clear();
        if(keyLevelDirty && engine) engine->submit(settings,true);
        keyLevelDirty=false;
    }
    void stopKeyTest() {
        KillTimer(window,31); testingKeys=false;clearHeldKeys();
        if(!settings.brightnessKeys) brightnessKeys.reset();
        keyStatus();
    }
    void testKeys() {
        if(testingKeys) { stopKeyTest();return; }
        observedKeys=0;
        if(listenForKeys()) {
            clearHeldKeys();testingKeys=SetTimer(window,31,15000,nullptr)!=0;
            if(!testingKeys) {
                if(!settings.brightnessKeys) brightnessKeys.reset();
                if(preferences) preferences->brightnessKeyStatus(L"The test timer could not start. Try again.",false);
                return;
            }
        }
        keyStatus();
    }
    void keyAdjustment(int direction) {
        if(!direction || testingKeys || !settings.brightnessKeys) return;
        setBrightness(brightness()+direction*settings.adjustmentStep);apply(false);keyLevelDirty=true;
        showKeyFeedback();
    }
    void keyInput(HRAWINPUT input) {
        if(!brightnessKeys) return;
        const auto event=brightnessKeys->input(input,GetTickCount64());
        if(event.observed) { observedKeys|=event.observed;keyStatus(); }
        if(event.direction) refreshWorkspace(); // Once per physical press; repeat never polls the shell.
        keyAdjustment(event.direction);
        if(!testingKeys && brightnessKeys->repeating()) {
            if(!keyRepeatArmed) {
                keyRepeatArmed=SetTimer(window,30,30,nullptr)!=0;
                if(!keyRepeatArmed) clearHeldKeys();
            }
        } else {
            KillTimer(window,30);keyRepeatArmed=false;
            if(keyLevelDirty && engine) engine->submit(settings,true);
            keyLevelDirty=false;
        }
    }
    void workspaceTracking(bool settingsOpen = false) {
        const bool needed = settings.separateDesktops || !settings.monitors.empty() || settingsOpen || (preferences && preferences->window());
        if (needed && !workspaces) {
            workspaces=std::make_unique<screen::VirtualDesktops>(window);
            settings.activeDesktop=workspaces->current();
            if (engine) workspaces->moveWindow(window);
        } else if (!needed && workspaces) {
            workspaces.reset(); settings.activeDesktop.clear();
        }
    }
    void refreshWorkspace(bool notification = false) {
        if (!workspaces) return;
        workspaces->refresh(notification);
        const bool changed = settings.activeDesktop != workspaces->current();
        settings.activeDesktop = workspaces->current();
        if (changed) {
            clearHeldKeys();
            KillTimer(window,32);keyFeedback=false;
            KillTimer(window,10);panelFrame.cancel(); animating=false; panelAlpha=0; ShowWindow(window,SW_HIDE);
            workspaces->moveWindow(window);
            if (engine) apply(false);
        }
        if (preferences) preferences->desktops(workspaces->items());
    }
    void theme() {
        colors = screen::ui::palette(settings.theme);
        bg=colors.bg; fg=colors.fg; muted=colors.muted; accent=colors.accent; track=colors.track; border=colors.border;
        if (background) DeleteObject(background);
        background = CreateSolidBrush(bg);
        screen::ui::windowTheme(window,colors);
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
        move(footer, 20, 101, 187, 26); move(shortcut, 211, 105, 67, 18);
        move(preferencesButton, 287, 101, 26, 26);
        RECT client{}; GetClientRect(window, &client);
        SetWindowRgn(window, CreateRoundRectRgn(0, 0, client.right, client.bottom, px(22), px(22)), TRUE);
    }
    void createControls() {
        // Keep the native slider and preceding label for keyboard and MSAA/UIA support.
        label = control(L"STATIC", L"App brightness", SS_NOPREFIX, appBrightnessLabelId);
        slider = control(TRACKBAR_CLASSW, L"Brightness", TBS_HORZ | TBS_NOTICKS | WS_TABSTOP, sliderId);
        value = control(L"STATIC", L"70%", SS_RIGHT | SS_NOPREFIX, 0);
        footer = control(L"BUTTON", L"Paused · All displays", BS_OWNERDRAW | WS_TABSTOP, displayPickerId);
        shortcut = control(L"STATIC", shortcutText.c_str(), SS_RIGHT | SS_NOPREFIX, 0);
        preferencesButton = control(L"BUTTON", L"Settings", BS_OWNERDRAW | WS_TABSTOP, preferencesId);
        SetWindowSubclass(slider, screen::ui::sliderProcedure, 1, 0);
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
        if (settings.separateDesktops && workspaces && !settings.activeDesktop.empty()) displayName = workspaces->name() + L" · " + displayName;
        const auto summary = keyFeedback?L"Screen Dimmer · "+displayName:statusText.empty() ? (settings.paused ? L"Paused · " : L"") + displayName +
            (settings.paused ? L"" : desktopCoverage ? L" · Desktop" : L" · Overlay") : statusText;
        SetWindowTextW(footer, summary.c_str());
        SetWindowTextW(shortcut, shortcutText.c_str());
        if (preferences) preferences->refresh();
        if (trayPresent && !tracking) {
            NOTIFYICONDATAW data{sizeof(data)}; data.hWnd = window; data.uID = 1; data.uFlags = NIF_TIP;
            auto tip = settings.paused ? std::wstring(L"Screen Dimmer by Shardapps · Paused") :
                L"Screen · " + std::to_wstring(settings.currentBrightness()) + L"% · " + shortcutText;
            if (settings.separateDesktops && workspaces) tip += L" · " + workspaces->name();
            tip.resize(std::min<size_t>(tip.size(),127));
            wcscpy_s(data.szTip, tip.c_str()); Shell_NotifyIconW(NIM_MODIFY, &data);
        }
    }
    void apply(bool persist = true, bool immediate = false, bool tracking = false) {
        workspaceTracking();
        engine->submit(settings, persist, false, immediate);
        sync(tracking);
    }
    void setBrightness(int level) {
        cancelTimedPause();
        if (selected.empty()) settings.setAll(level);
        else settings.setMonitor(selected,level);
        settings.paused = false; statusText.clear();
    }
    bool addTray() {
        NOTIFYICONDATAW data{sizeof(data)};
        data.hWnd = window; data.uID = 1; data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
        data.hIcon = icon; data.uCallbackMessage = trayMessage;
        wcscpy_s(data.szTip, L"Screen Dimmer by Shardapps");
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
            KillTimer(window, 10); panelFrame.cancel(); animating = false; panelAlpha = toAlpha;
            SetLayeredWindowAttributes(window, 0, static_cast<BYTE>(panelAlpha), LWA_ALPHA);
            if (!show) ShowWindow(window, SW_HIDE);
            return;
        }
        animationStart = std::chrono::steady_clock::now();
        panelFrame.begin();
        animating = panelFrame.arm();
        if(animating) KillTimer(window,10);
        else animating = SetTimer(window,10,15,nullptr)!=0;
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
            panelFrame.cancel();
            if (!opening) ShowWindow(window, SW_HIDE);
        } else if(!panelFrame.arm() && !SetTimer(window,10,15,nullptr)) {
            animating=false;panelAlpha=toAlpha;panelFrame.cancel();
            SetLayeredWindowAttributes(window,0,static_cast<BYTE>(panelAlpha),LWA_ALPHA);
            SetWindowPos(window,nullptr,destination.left,destination.top,0,0,SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
            if(!opening) ShowWindow(window,SW_HIDE);
        }
    }
    void show() {
        KillTimer(window,32);keyFeedback=false;
        refreshWorkspace();
        place();sync();
        if (!IsWindowVisible(window)) { panelAlpha = 0; SetLayeredWindowAttributes(window, 0, 0, LWA_ALPHA); }
        ShowWindow(window, SW_SHOWNOACTIVATE); animate(true);
        SetForegroundWindow(window); SetFocus(slider);
    }
    void showKeyFeedback() {
        if(!settings.brightnessKeyFeedback) return;
        if(IsWindowVisible(window) && !keyFeedback && (!animating || opening)) return; // Keep an interactive panel open.
        if(!keyFeedback || !IsWindowVisible(window)) {
            keyFeedback=true;place();
            if(!IsWindowVisible(window)) { panelAlpha=0;SetLayeredWindowAttributes(window,0,0,LWA_ALPHA); }
            ShowWindow(window,SW_SHOWNOACTIVATE);sync();
        }
        if(panelAlpha<255 && (!animating || !opening)) animate(true);
        if(!SetTimer(window,32,2000,nullptr)) hide();
    }
    void adoptKeyFeedback() { KillTimer(window,32);keyFeedback=false;sync(); }
    void hide() {
        KillTimer(window,32);keyFeedback=false;
        if (!IsWindowVisible(window) && !animating) return;
        if (trayPresent || addTray()) animate(false);
        else { statusText = L"Tray unavailable"; sync(); }
    }
    void restore() {
        clearHeldKeys();
        cancelTimedPause();
        settings.paused = true; settings.setAll(100); statusText.clear(); apply(true, true);
    }
    void refreshDisplays() {
        displays = screen::enumerateMonitors();
        if (!selected.empty() && std::none_of(displays.begin(), displays.end(),
            [&](const auto& display) { return display.device == selected; })) { selected.clear(); settings.selectedDisplay.clear(); }
        if (preferences) preferences->refresh(true);
    }
    void cancelTimedPause() { KillTimer(window, 20); timedPause = false; }
    void pauseFor15Minutes() {
        clearHeldKeys();
        settings.paused = true; statusText = L"Paused for 15 minutes";
        timedPause = SetTimer(window, 20, 15 * 60 * 1000, nullptr) != 0;
        if (!timedPause) statusText = L"Paused · timer unavailable";
        apply(false);
    }
    bool bindShortcut(int index) {
        constexpr UINT modifiers[] = {MOD_ALT, MOD_CONTROL | MOD_ALT, MOD_CONTROL | MOD_SHIFT};
        constexpr const wchar_t* names[] = {L"Alt + S", L"Ctrl + Alt + S", L"Ctrl + Shift + S"};
        UnregisterHotKey(window, 5); registeredShortcut = -1;
        if (!RegisterHotKey(window, 5, modifiers[index] | MOD_NOREPEAT, 'S')) return false;
        registeredShortcut = index; shortcutText = names[index]; return true;
    }
    void openPreferences() {
        workspaceTracking(true);
        refreshWorkspace();
        refreshDisplays();
        if (!preferences) preferences = std::make_unique<screen::PreferencesWindow>(settings, displays, selected,
            [this](screen::PreferenceAction action, int amount) { preference(action, amount); });
        preferences->desktops(workspaces ? workspaces->items() : std::vector<screen::VirtualDesktop>{});
        if (workspaces) workspaces->moveWindow(preferences->window());
        hide(); preferences->show(window);
        preferences->startupState(startupState, true);
        keyStatus();
        startup.request(window);
    }
    void preference(screen::PreferenceAction action, int amount) {
        using enum screen::PreferenceAction;
        switch (action) {
        case display:
            if (amount >= 0 && static_cast<size_t>(amount) <= displays.size()) selected = amount ? displays[static_cast<size_t>(amount)-1].device : L"";
            settings.selectedDisplay = selected; statusText.clear(); apply(); return;
        case brightness: {
            const bool tracking = (amount & 0x10000) != 0;
            setBrightness(amount & 0xFFFF); apply(!tracking, false, tracking); return;
        }
        case theme: settings.theme = std::clamp(amount, 0, 6); this->theme(); break;
        case warm: settings.warm = amount != 0; break;
        case smooth: settings.smooth = amount != 0; break;
        case resume: settings.resumeOnLaunch = amount != 0; break;
        case brightnessKeys:
            clearHeldKeys();settings.brightnessKeys=amount!=0;
            if(settings.brightnessKeys && !listenForKeys()) settings.brightnessKeys=false;
            if(!settings.brightnessKeys && !testingKeys) this->brightnessKeys.reset();
            if(!settings.brightnessKeys && keyFeedback) hide();
            keyStatus();break;
        case brightnessKeyFeedback:
            settings.brightnessKeyFeedback=amount!=0;
            if(!settings.brightnessKeyFeedback && keyFeedback) hide();
            break;
        case testBrightnessKeys: testKeys();return;
        case desktopProfiles:
            settings.separateDesktops = amount != 0; break;
        case desktopBrightness: {
            const bool tracking = (amount & 0x10000) != 0;
            const auto id = preferences->profileDesktop();
            if (!settings.separateDesktops || !settings.setDesktopBrightness(id,amount & 0xFFFF)) return;
            if (id == settings.activeDesktop) { cancelTimedPause(); settings.paused=false; statusText.clear(); }
            apply(!tracking,false,tracking); return;
        }
        case resetDesktop:
            settings.desktops.erase(preferences->profileDesktop()); break;
        case step: settings.adjustmentStep = std::clamp(amount, 1, 10); break;
        case shortcut: {
            const auto previous = registeredShortcut;
            if (amount < 0 || amount > 2) return;
            if (bindShortcut(amount)) settings.openingShortcut = amount;
            else {
                if (previous >= 0) bindShortcut(previous);
                else shortcutText = L"Tray icon";
                statusText = L"Shortcut in use · choose another";
            }
            break;
        }
        case startup:
            if (this->startup.request(window, amount != 0)) preferences->startupState(startupState, true);
            return;
        case pause: clearHeldKeys();cancelTimedPause(); settings.paused = !settings.paused; statusText.clear(); break;
        case timedPause: pauseFor15Minutes(); return;
        case restore: this->restore(); return;
        }
        apply();
    }
    void displayMenu() {
        menuActive = true; refreshDisplays();
        const auto popup = CreatePopupMenu();
        AppendMenuW(popup, MF_STRING | (selected.empty() ? MF_CHECKED : 0), displayBase, L"All displays");
        for (size_t i=0; i<displays.size(); ++i) {
            const auto text = L"Display " + std::to_wstring(i+1) + (displays[i].primary ? L" · Primary" : L"");
            AppendMenuW(popup, MF_STRING | (selected == displays[i].device ? MF_CHECKED : 0), displayBase+i+1, text.c_str());
        }
        RECT bounds{}; GetWindowRect(footer, &bounds); SetForegroundWindow(window);
        const auto command = TrackPopupMenu(popup, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN,
            bounds.left, bounds.top, 0, window, nullptr);
        DestroyMenu(popup); menuActive = false;
        if (command) SendMessageW(window, WM_COMMAND, command, 0);
    }
    void menu() {
        menuActive = true; refreshDisplays();
        const auto popup = CreatePopupMenu(), monitors = CreatePopupMenu();
        const auto openText = L"Brightness\t" + shortcutText;
        AppendMenuW(popup, MF_STRING, openId, openText.c_str());
        AppendMenuW(popup, MF_STRING, preferencesId, L"Settings…");
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
    void renderSlider(HDC target) {
        screen::ui::drawSlider(slider,target,colors,dpi);
    }
    void drawButton(const DRAWITEMSTRUCT& draw) {
        const auto dc = draw.hDC; auto rect = draw.rcItem; FillRect(dc, &rect, background);
        const auto previousFont = SelectObject(dc, footerFont);
        if (draw.CtlID == displayPickerId) {
            wchar_t text[160]{}; GetWindowTextW(footer, text, 160);
            rect.left += px(4); rect.right -= px(13);
            screen::ui::text(dc, rect, text, muted, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            const auto pen = CreatePen(PS_SOLID, px(1), muted); const auto previous = SelectObject(dc, pen);
            const auto x = draw.rcItem.right-px(8), y = draw.rcItem.bottom/2;
            MoveToEx(dc,x-px(3),y-px(1),nullptr); LineTo(dc,x,y+px(2)); LineTo(dc,x+px(3),y-px(1));
            SelectObject(dc,previous); DeleteObject(pen);
        } else {
            const auto pen = CreatePen(PS_SOLID, px(2), (draw.itemState & ODS_SELECTED) ? accent : muted);
            const auto previous = SelectObject(dc,pen);
            for (int i=0;i<3;++i) {
                const auto y=px(7+i*6), x=px(i==1?16:10);
                MoveToEx(dc,px(5),y,nullptr); LineTo(dc,px(22),y);
                MoveToEx(dc,x,y-px(2),nullptr); LineTo(dc,x,y+px(3));
            }
            SelectObject(dc,previous); DeleteObject(pen);
        }
        if ((draw.itemState & ODS_FOCUS) && !(draw.itemState & ODS_NOFOCUSRECT)) DrawFocusRect(dc,&draw.rcItem);
        SelectObject(dc,previousFont);
    }
};
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
        app->workspaceTracking();
        app->dpi = GetDpiForWindow(window); app->theme(); app->refreshDisplays(); app->createControls();
        app->taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
        app->icon = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED));
        SendMessageW(window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(app->icon));
        app->engine = std::make_unique<screen::OverlayEngine>(window, app->overlayOnly);
        if (!app->engine->healthy()) return -1;
        app->engine->submit(app->settings, false, true); app->addTray();
        if(app->settings.brightnessKeys && !app->listenForKeys()) app->settings.brightnessKeys=false;
        constexpr UINT mods = MOD_CONTROL | MOD_ALT | MOD_NOREPEAT;
        RegisterHotKey(window, 1, mods, 'D'); RegisterHotKey(window, 2, mods, VK_DOWN);
        RegisterHotKey(window, 3, mods, VK_UP);
        if (!RegisterHotKey(window, 4, mods | MOD_SHIFT, VK_F12)) app->statusText = L"Restore shortcut in use";
        if (!app->bindShortcut(app->settings.openingShortcut) && !app->bindShortcut(1) && !app->bindShortcut(0)) app->shortcutText = L"Tray icon";
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
    case screen::ui::printSlider: app->renderSlider(reinterpret_cast<HDC>(wParam)); return 0;
    case WM_DRAWITEM: app->drawButton(*reinterpret_cast<DRAWITEMSTRUCT*>(lParam)); return TRUE;
    case WM_COMMAND: {
        const auto command = LOWORD(wParam);
        if (command == exitId) DestroyWindow(window);
        else if (command == openId) app->show();
        else if (command == preferencesId) app->openPreferences();
        else if (command == displayPickerId) app->displayMenu();
        else if (command == IDCANCEL) app->hide();
        else if (command == pauseId) { app->cancelTimedPause(); app->settings.paused = !app->settings.paused; app->statusText.clear(); app->apply(); }
        else if (command == restoreId) app->restore();
        else if (command == warmId) { app->settings.warm = !app->settings.warm; app->apply(); }
        else if (command == smoothId) { app->settings.smooth = !app->settings.smooth; app->apply(); }
        else if (command >= displayBase && static_cast<size_t>(command - displayBase) <= app->displays.size()) {
            app->selected = command == displayBase ? L"" : app->displays[command - displayBase - 1].device;
            app->settings.selectedDisplay = app->selected; app->statusText.clear(); app->apply();
        }
        return 0;
    }
    case WM_HOTKEY:
        if (wParam == 1) { app->clearHeldKeys();app->cancelTimedPause(); app->settings.paused = !app->settings.paused; app->statusText.clear(); app->apply(); }
        else if (wParam == 2 || wParam == 3) {
            app->refreshWorkspace();
            app->cancelTimedPause();
            app->settings.setAll(app->settings.currentBrightness() + (wParam == 2 ? -app->settings.adjustmentStep : app->settings.adjustmentStep));
            app->settings.paused = false; app->statusText.clear(); app->apply();app->showKeyFeedback();
        } else if (wParam == 4) app->restore();
        else if (wParam == 5) { if (!app->keyFeedback && IsWindowVisible(window) && (!app->animating || app->opening)) app->hide(); else app->show(); }
        return 0;
    case trayMessage:
        if (LOWORD(lParam) == WM_CONTEXTMENU) app->menu();
        else if (LOWORD(lParam) == NIN_SELECT || LOWORD(lParam) == NIN_KEYSELECT)
            SendMessageW(window, WM_HOTKEY, 5, 0);
        return 0;
    case showMessage: app->show(); return 0;
    case screen::virtualDesktopsChanged: app->refreshWorkspace(wParam != 0); return 0;
    case screen::preferencesClosed:
        app->stopKeyTest();
        app->workspaceTracking(); if(app->engine) app->engine->submit(app->settings); return 0;
    case WM_INPUT:
        app->keyInput(reinterpret_cast<HRAWINPUT>(lParam));
        return DefWindowProcW(window,message,wParam,lParam); // Foreground raw-input cleanup is mandatory.
    case WM_INPUT_DEVICE_CHANGE:
        if(wParam==GIDC_REMOVAL && app->brightnessKeys) {
            app->brightnessKeys->removed(reinterpret_cast<HANDLE>(lParam));app->clearHeldKeys();
        }
        return 0;
    case WM_WTSSESSION_CHANGE:
        if(wParam==WTS_SESSION_LOCK || wParam==WTS_CONSOLE_DISCONNECT || wParam==WTS_REMOTE_DISCONNECT) { app->clearHeldKeys();if(app->keyFeedback) app->hide(); }
        return 0;
    case WM_POWERBROADCAST:
        if(wParam==PBT_APMSUSPEND) { app->clearHeldKeys();if(app->keyFeedback) app->hide(); }
        return TRUE;
    case WM_CONTEXTMENU: app->menu(); return 0;
    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE && !app->keyFeedback && !app->menuActive && IsWindowVisible(window)) app->hide();
        return 0;
    case WM_MOUSEACTIVATE:
        if(app->keyFeedback) app->adoptKeyFeedback();
        return MA_ACTIVATE;
    case WM_CLOSE: app->hide(); return 0;
    case WM_TIMER:
        if (wParam == 10) app->animateTick();
        else if (wParam == 20 && app->timedPause) { app->cancelTimedPause(); app->settings.paused = false; app->statusText.clear(); app->apply(false); }
        else if(wParam==30 && app->brightnessKeys) {
            app->keyAdjustment(app->brightnessKeys->repeat(GetTickCount64()));
            if(!app->brightnessKeys->repeating()) app->clearHeldKeys();
        } else if(wParam==31) {
            const auto observed=app->observedKeys;app->stopKeyTest();
            if(!observed && app->preferences) app->preferences->brightnessKeyStatus(L"No brightness keys detected. Try Fn + the sun keys, or use Ctrl + Alt + Up / Down.",false);
        } else if(wParam==32) {
            if(!app->keyFeedback) { KillTimer(window,32);return 0; }
            POINT pointer{};GetCursorPos(&pointer);const auto target=WindowFromPoint(pointer);
            if(GetForegroundWindow()==window || GetCapture()==app->slider) app->adoptKeyFeedback();
            else if(target==window || IsChild(window,target)) { if(!SetTimer(window,32,600,nullptr)) app->hide(); }
            else app->hide();
        }
        return 0;
    case WM_DISPLAYCHANGE:
        app->refreshDisplays(); app->sync(); if (IsWindowVisible(window)) app->place(); return 0;
    case WM_SETTINGCHANGE: case WM_THEMECHANGED: app->theme(); if (app->preferences) app->preferences->refresh(true); return 0;
    case WM_DPICHANGED:
        app->dpi = HIWORD(wParam); app->fonts(); app->layout(); if (!app->placing) app->place(); return 0;
    case WM_MOUSEWHEEL:
        app->setBrightness(app->brightness() + GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA * app->settings.adjustmentStep); app->apply(); return 0;
    case screen::overlayFailure:
        app->settings.paused = true; app->statusText = L"Restored · try dimming again"; app->sync(); app->show(); return 0;
    case screen::settingsFailure:
        app->statusText = L"Preferences could not be saved"; app->sync(); return 0;
    case screen::coverageChanged:
        app->desktopCoverage = wParam != 0; app->sync(); return 0;
    case screen::startupResult:
        app->startupState = static_cast<screen::StartupState>(wParam);
        if (app->preferences) app->preferences->startupState(app->startupState,false);
        return 0;
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
        app->stopKeyTest();app->brightnessKeys.reset();
        KillTimer(window, 10);app->panelFrame.cancel(); app->cancelTimedPause(); app->preferences.reset(); app->engine.reset(); app->workspaces.reset();
        NOTIFYICONDATAW data{sizeof(data)}; data.hWnd = window; data.uID = 1; Shell_NotifyIconW(NIM_DELETE, &data);
        for (int id = 1; id <= 5; ++id) UnregisterHotKey(window, id);
        PostQuitMessage(0); return 0;
    }
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    bool background = false, overlayOnly = false, removeStartup = false, forcePaused = false, openSettings = false;
    int argumentCount = 0;
    if (auto arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount)) {
        for (int index = 1; index < argumentCount; ++index) {
            const std::wstring_view argument(arguments[index]);
            if (argument == L"--background") background = true;
            if (argument == L"--overlay") overlayOnly = true;
            if (argument == L"--remove-startup") removeStartup = true;
            if (argument == L"--paused") forcePaused = true;
            if (argument == L"--settings") openSettings = true;
        }
        LocalFree(arguments);
    }
    if (removeStartup) return screen::removeStartupForCurrentExecutable() ? 0 : 1;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const auto mutex = CreateMutexW(nullptr, FALSE, L"Local\\Shardapps.Screen.Instance");
    if (!mutex) return 1;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (auto existing = FindWindowW(windowClass, nullptr)) {
            if (openSettings) PostMessageW(existing, WM_COMMAND, preferencesId, 0);
            else if (!background) PostMessageW(existing, showMessage, 0, 0);
        }
        CloseHandle(mutex); return 0;
    }
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES}; InitCommonControlsEx(&controls);
    App app;
    app.selected = app.settings.selectedDisplay;
    app.overlayOnly = overlayOnly;
    if (forcePaused) app.settings.paused = true;
    WNDCLASSEXW type{sizeof(type)}; type.hInstance = instance; type.lpfnWndProc = windowProcedure;
    type.lpszClassName = windowClass; type.hCursor = LoadCursorW(nullptr, IDC_ARROW); type.style = CS_DROPSHADOW;
    type.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    if (!RegisterClassExW(&type)) { CloseHandle(mutex); return 1; }
    const auto dpi = GetDpiForSystem(); RECT work{}; SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const auto width = MulDiv(panelWidth, static_cast<int>(dpi), 96), height = MulDiv(panelHeight, static_cast<int>(dpi), 96);
    const auto window = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED,
        windowClass, L"Screen Dimmer by Shardapps", WS_POPUP | WS_CLIPCHILDREN,
        work.right - width - 12, work.bottom - height - 12, width, height, nullptr, nullptr, instance, &app);
    if (!window) { CloseHandle(mutex); return 1; }
    if (openSettings) app.openPreferences();
    else if (!background || !app.trayPresent) app.show();
    MSG message{}; int result=0;
    for(;;) {
        const auto event=app.panelFrame.wait(message);
        if(event==screen::FrameTimer::Event::frame) { app.animateTick();continue; }
        if(event==screen::FrameTimer::Event::quit) break;
        if(event==screen::FrameTimer::Event::error) { result=-1;break; }
        if (app.preferences && app.preferences->message(message)) continue;
        if (!IsDialogMessageW(window, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }
    }
    if (IsWindow(window)) DestroyWindow(window);
    CloseHandle(mutex); return result == -1 ? 1 : static_cast<int>(message.wParam);
}
