#include "overlay.hpp"
#include "settings.hpp"
#include <future>
#include <wtsapi32.h>
#include <dwmapi.h>

namespace screen {
namespace {
constexpr UINT updateMessage = WM_APP + 1;
constexpr UINT fallbackMessage = WM_APP + 2;
constexpr wchar_t controlClass[] = L"Shardapps.Screen.Engine";
constexpr wchar_t surfaceClass[] = L"Shardapps.Screen.Overlay";
LRESULT CALLBACK surfaceProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCHITTEST) return HTTRANSPARENT;
    if (message == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (message == WM_ERASEBKGND) return 1;
    if (message == WM_PAINT) {
        PAINTSTRUCT paint{};
        const auto dc = BeginPaint(window, &paint);
        const auto brush = CreateSolidBrush(GetWindowLongPtrW(window, GWLP_USERDATA) ? RGB(66, 30, 8) : RGB(0, 0, 0));
        FillRect(dc, &paint.rcPaint, brush);
        DeleteObject(brush);
        EndPaint(window, &paint);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
BOOL CALLBACK collectMonitor(HMONITOR monitor, HDC, LPRECT, LPARAM context) {
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, &info))
        reinterpret_cast<std::vector<Monitor>*>(context)->push_back(
            {info.szDevice, info.rcMonitor, (info.dwFlags & MONITORINFOF_PRIMARY) != 0});
    return TRUE;
}
}
std::vector<Monitor> enumerateMonitors() {
    std::vector<Monitor> monitors;
    EnumDisplayMonitors(nullptr, nullptr, collectMonitor, reinterpret_cast<LPARAM>(&monitors));
    return monitors;
}
thread_local OverlayEngine* OverlayEngine::observer_ = nullptr;
OverlayEngine::OverlayEngine(HWND owner, bool overlayOnly) : owner_(owner), overlayOnly_(overlayOnly) {
    std::promise<HWND> ready;
    auto future = ready.get_future();
    thread_ = std::thread([this, promise = std::move(ready)]() mutable {
        WNDCLASSW control{};
        control.hInstance = GetModuleHandleW(nullptr);
        control.lpfnWndProc = procedure;
        control.lpszClassName = controlClass;
        RegisterClassW(&control);
        WNDCLASSW surface = control;
        surface.lpfnWndProc = surfaceProcedure;
        surface.lpszClassName = surfaceClass;
        RegisterClassW(&surface);
        // A hidden top-level window receives session and display broadcasts; message-only windows do not.
        auto window = CreateWindowExW(WS_EX_TOOLWINDOW, controlClass, L"Screen engine", WS_POPUP,
                                      0, 0, 0, 0, nullptr, nullptr, control.hInstance, this);
        promise.set_value(window);
        if (!window) return;
        WTSRegisterSessionNotification(window, NOTIFY_FOR_THIS_SESSION);
        observer_ = this;
        constexpr DWORD flags = WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS;
        foregroundHook_ = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
            nullptr, windowEvent, 0, 0, flags);
        windowHook_ = SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_REORDER,
            nullptr, windowEvent, 0, 0, flags);
        desktopHook_ = SetWinEventHook(EVENT_SYSTEM_DESKTOPSWITCH, EVENT_SYSTEM_DESKTOPSWITCH,
            nullptr, windowEvent, 0, 0, flags);
        locationHook_ = SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE,
            nullptr, windowEvent, 0, 0, flags);
        MSG message{};
        for(;;) {
            const auto event=frame_.wait(message);
            if(event==FrameTimer::Event::frame) { tick();continue; }
            if(event==FrameTimer::Event::quit) break;
            if(event==FrameTimer::Event::error) { reportFailure();break; }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        for (auto hook : {foregroundHook_, windowHook_, desktopHook_, locationHook_}) if (hook) UnhookWinEvent(hook);
        desktop_.reset();
        observer_ = nullptr;
        WTSUnRegisterSessionNotification(window);
        if(virtualManager_) { virtualManager_->Release(); virtualManager_=nullptr; }
        if(virtualApartment_) CoUninitialize();
    });
    (void)future.get(); // Synchronizes the worker's initialization of control_.
}
OverlayEngine::~OverlayEngine() {
    if (control_) PostMessageW(control_, WM_CLOSE, 0, 0);
    if (thread_.joinable()) thread_.join();
}
void OverlayEngine::submit(const Settings& settings, bool persist, bool rebuildRequested, bool immediate) {
    if (!control_) return;
    {
        const std::lock_guard lock(mutex_);
        desired_ = settings.renderingSnapshot();
        if(persist) saved_ = settings;
        immediate_ = immediate;
        persist_ = persist_ || persist;
        rebuild_ = rebuild_ || rebuildRequested;
    }
    if (!pending_.exchange(true)) PostMessageW(control_, updateMessage, 0, 0);
}
void OverlayEngine::reportFailure() {
    // Fail open: remove every dimming surface if the overlay cannot be maintained safely.
    active_.paused = true;
    desktop_.reset(); desktopAlpha_ = {};
    {
        const std::lock_guard lock(mutex_);
        desired_.paused = true;
    }
    // Display broadcasts must not revive the last active opacity after an error.
    KillTimer(control_, 1);
    frame_.cancel();
    KillTimer(control_, 3); auditing_ = false;
    fading_ = false;
    for (const auto& surface : surfaces_) DestroyWindow(surface.window);
    surfaces_.clear();
    PostMessageW(owner_, overlayFailure, 0, 0);
}
void OverlayEngine::rebuild() {
    higherBandWindows_.clear();
    for (const auto& surface : surfaces_) DestroyWindow(surface.window);
    surfaces_.clear();
    for (const auto& monitor : enumerateMonitors()) {
        auto window = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
            surfaceClass, L"Screen dimming overlay", WS_POPUP,
            monitor.bounds.left, monitor.bounds.top,
            monitor.bounds.right - monitor.bounds.left, monitor.bounds.bottom - monitor.bounds.top,
            nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!window) { reportFailure(); return; }
        surfaces_.push_back({window, monitor.device, {}, 0});
        SetWindowLongPtrW(window, GWLP_USERDATA, active_.warm ? 1 : 0);
        if (!SetLayeredWindowAttributes(window, 0, 0, LWA_ALPHA) ||
            !SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE)) {
            reportFailure(); return;
        }
        // Kept hidden at zero opacity. Visible surfaces appear only for an active fade.
    }
}
void OverlayEngine::consume() {
    bool persist, refresh, immediate;
    Settings saved;
    const bool previousWarm = active_.warm;
    const auto previousDesktop = active_.activeDesktop;
    {
        const std::lock_guard lock(mutex_);
        active_ = desired_;
        desktopBlocked_ = false; // Retry the desktop API on a new user adjustment.
        persist = persist_;
        if(persist) saved = std::move(saved_);
        refresh = rebuild_;
        immediate = immediate_;
        persist_ = rebuild_ = false;
        pending_ = false;
    }
    if (refresh || surfaces_.empty()) rebuild();
    if (previousDesktop != active_.activeDesktop && !previousDesktop.empty() && !active_.activeDesktop.empty()) {
        if(!virtualManager_) {
            if(!virtualApartment_) virtualApartment_=SUCCEEDED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED));
            CoCreateInstance(CLSID_VirtualDesktopManager,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&virtualManager_));
        }
        for(const auto& surface:surfaces_) moveToDesktop(surface.window,active_.activeDesktop,virtualManager_);
    }
    if (previousWarm != active_.warm) {
        for (const auto& surface : surfaces_) {
            SetWindowLongPtrW(surface.window, GWLP_USERDATA, active_.warm ? 1 : 0);
            InvalidateRect(surface.window, nullptr, FALSE);
        }
    }
    const auto smooth = active_.smooth;
    if (immediate) active_.smooth = false;
    transition();
    active_.smooth = smooth; // Emergency restore must not overwrite the saved animation preference.
    if (persist && !saveSettings(saved)) PostMessageW(owner_, settingsFailure, 0, 0);
}
void OverlayEngine::transition() {
    BOOL allowed = TRUE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &allowed, 0);
    motionAllowed_ = allowed != FALSE;
    if (fading_ && active_.smooth && motionAllowed_) tick(); // Integrate time before changing a moving target.
    start_ = std::chrono::steady_clock::now();
    const int level = surfaces_.empty() ? 100 : active_.forMonitor(surfaces_.front().device);
    const bool uniform = !surfaces_.empty() && std::all_of(surfaces_.begin(), surfaces_.end(),
        [&](const auto& surface) { return active_.forMonitor(surface.device) == level; });
    bool useDesktop = !overlayOnly_ && uniform && !desktopBlocked_;
    if (useDesktop && opacity(level, active_.paused || sessionLocked_) && !desktop_.prepare()) {
        desktopBlocked_ = true; useDesktop = false;
    }
    if (useDesktop != desktopMode_) {
        desktop_.reset(); desktopAlpha_ = {};
        for (auto& surface : surfaces_) {
            surface.alpha = {}; surface.applied = 0;
            SetLayeredWindowAttributes(surface.window, 0, 0, LWA_ALPHA);
            ShowWindow(surface.window, SW_HIDE);
        }
        KillTimer(control_, 1); frame_.cancel(); fading_ = false;
        desktopMode_ = useDesktop;
    }
    const int mode = desktopMode_ ? 1 : 0;
    if (reportedMode_ != mode) { reportedMode_ = mode; PostMessageW(owner_, coverageChanged, mode, 0); }
    bool needsAudit = false;
    desktopAlpha_.target = desktopMode_ ? opacity(level, active_.paused || sessionLocked_) : 0;
    needsAudit = desktopAlpha_.target > 0;
    for (auto& surface : surfaces_) {
        surface.alpha.target = desktopMode_ ? 0 : opacity(active_.forMonitor(surface.device), active_.paused || sessionLocked_);
        needsAudit = needsAudit || surface.alpha.target > 0;
        if (surface.alpha.target > 0 || surface.alpha.value > 0) ShowWindow(surface.window, SW_SHOWNOACTIVATE);
    }
    // Some nonactivating SetWindowPos promotions emit no usable WinEvent.
    // A cheap 4 Hz audit while dimming covers that gap; paused/100% has no audit.
    if (needsAudit && !auditing_) {
        auditing_ = SetTimer(control_, 3, 250, nullptr) != 0;
        if (!auditing_) { reportFailure(); return; }
    }
    else if (!needsAudit) { KillTimer(control_, 3); auditing_ = false; }
    tick();
    ensureCoverage();
}
void OverlayEngine::tick() {
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration<double>(now - start_).count();
    start_ = now;
    bool animating = false;
    if (desktopMode_ && !desktopBlocked_) {
        if (active_.smooth && motionAllowed_) animating = desktopAlpha_.advance(elapsed);
        else desktopAlpha_.snap();
        if (!desktop_.apply(static_cast<BYTE>(std::lround(desktopAlpha_.value)), active_.warm)) {
            desktopBlocked_ = true;
            KillTimer(control_, 1); frame_.cancel(); fading_ = false;
            PostMessageW(control_, fallbackMessage, 0, 0);
            return;
        }
    }
    for (auto& surface : surfaces_) {
        if (active_.smooth && motionAllowed_) animating = surface.alpha.advance(elapsed) || animating;
        else surface.alpha.snap();
        const auto alpha = static_cast<BYTE>(std::lround(surface.alpha.value));
        if (alpha != surface.applied) {
            if (!SetLayeredWindowAttributes(surface.window, 0, alpha, LWA_ALPHA)) {
                reportFailure(); return;
            }
            surface.applied = alpha;
        }
        if (surface.alpha.value == 0 && surface.alpha.target == 0) ShowWindow(surface.window, SW_HIDE);
    }
    if (animating) {
        if(!fading_) frame_.begin();
        if(frame_.arm()) { KillTimer(control_,1);fading_=true; }
        else {
            if(!SetTimer(control_,1,15,nullptr)) reportFailure();
            else fading_=true;
        }
    } else if (!animating) {
        KillTimer(control_, 1);
        frame_.cancel();
        fading_ = false;
    }
}
void CALLBACK OverlayEngine::windowEvent(HWINEVENTHOOK, DWORD event, HWND window, LONG object, LONG child, DWORD, DWORD) {
    auto* self = observer_;
    if (!self) return;
    if(event == EVENT_SYSTEM_FOREGROUND && !self->active_.activeDesktop.empty()) PostMessageW(self->owner_,virtualDesktopsChanged,0,0);
    if (event == EVENT_OBJECT_LOCATIONCHANGE &&
        (!window || (GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOPMOST) == 0)) return;
    // Retry a previously unreachable Windows band only after a genuine change
    // to that window, not the desktop-reorder notification from our own repair.
    if (window && window != GetDesktopWindow()) std::erase(self->higherBandWindows_, window);
    if (event == EVENT_SYSTEM_DESKTOPSWITCH || event == EVENT_SYSTEM_FOREGROUND ||
        (object == OBJID_WINDOW && child == CHILDID_SELF && window && GetAncestor(window, GA_ROOT) == window))
        self->queueCoverage();
}
void OverlayEngine::queueCoverage() {
    // A one-shot event debounce; no periodic idle polling or injected input hook.
    if (active_.paused || sessionLocked_) return;
    if (!coveragePending_ && SetTimer(control_, 2, 20, nullptr)) coveragePending_ = true;
}
void OverlayEngine::ensureCoverage() {
    if (sessionLocked_ || active_.paused) return;
    if (desktopMode_) {
        if (!desktopBlocked_ && !desktop_.intact()) {
            desktop_.reset(); desktopBlocked_ = true;
            PostMessageW(control_, fallbackMessage, 0, 0);
        }
        return;
    }
    bool surfacesRaised = false;
    for (const auto& surface : surfaces_) {
        if (!IsWindow(surface.window)) { rebuild(); transition(); return; }
        if (surface.alpha.target == 0 && surface.alpha.value == 0) continue;
        bool raise = (GetWindowLongPtrW(surface.window, GWL_EXSTYLE) & WS_EX_TOPMOST) == 0;
        HWND candidate = nullptr;
        RECT bounds{}; GetWindowRect(surface.window, &bounds);
        // Inspect windows above us before reordering. This also prevents our own
        // desktop reorder events from producing a perpetual repair loop.
        auto above = GetWindow(surface.window, GW_HWNDPREV);
        for (int inspected = 0; above && !raise && inspected < 512; ++inspected,
             above = GetWindow(above, GW_HWNDPREV)) {
            DWORD process = 0; GetWindowThreadProcessId(above, &process);
            if (process == GetCurrentProcessId() || !IsWindowVisible(above)) continue;
            if (higherBandWindows_.size() >= 512 || std::find(higherBandWindows_.begin(), higherBandWindows_.end(), above) != higherBandWindows_.end()) continue;
            DWORD cloaked = 0;
            DwmGetWindowAttribute(above, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
            RECT other{}, intersection{}; GetWindowRect(above, &other);
            if (!cloaked && IntersectRect(&intersection, &bounds, &other)) { raise = true; candidate = above; }
        }
        if (raise && !SetWindowPos(surface.window, HWND_TOPMOST, 0, 0, 0, 0,
                                  SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE)) { reportFailure(); return; }
        surfacesRaised = surfacesRaised || raise;
        if (candidate) {
            auto remaining = GetWindow(surface.window, GW_HWNDPREV);
            for (int inspected = 0; remaining && inspected < 512; ++inspected,
                 remaining = GetWindow(remaining, GW_HWNDPREV)) {
                if (remaining == candidate) {
                    // Protected/shell bands cannot be overtaken by a normal
                    // desktop process. Do not fight them with a timer loop.
                    higherBandWindows_.push_back(candidate); break;
                }
            }
        }
        if (!IsWindowVisible(surface.window)) ShowWindow(surface.window, SW_SHOWNOACTIVATE);
    }
    // Keep the small control panel readable above our own dimming surfaces.
    if (IsWindowVisible(owner_)) {
        bool panelCovered = surfacesRaised;
        auto above = GetWindow(owner_, GW_HWNDPREV);
        for (int inspected = 0; above && !panelCovered && inspected < 512; ++inspected,
             above = GetWindow(above, GW_HWNDPREV)) {
            panelCovered = std::any_of(surfaces_.begin(), surfaces_.end(),
                [&](const auto& surface) { return surface.window == above; });
        }
        if (panelCovered) SetWindowPos(owner_, HWND_TOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
    }
}
LRESULT CALLBACK OverlayEngine::procedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<OverlayEngine*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        self = static_cast<OverlayEngine*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        // This value is set on the worker before any worker message handlers execute.
        self->control_ = window;
    }
    if (!self) return DefWindowProcW(window, message, wParam, lParam);
    switch (message) {
    case updateMessage: self->consume(); return 0;
    case fallbackMessage: self->transition(); return 0;
    case WM_TIMER:
        if (wParam == 1) self->tick();
        else if (wParam == 2) {
            KillTimer(window, 2); self->coveragePending_ = false; self->ensureCoverage();
        }
        else if (wParam == 3) self->ensureCoverage();
        return 0;
    case WM_DISPLAYCHANGE: self->rebuild(); self->transition(); return 0;
    case WM_SETTINGCHANGE: self->transition(); return 0;
    case WM_POWERBROADCAST:
        if (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND) {
            self->rebuild(); self->transition();
        }
        return TRUE;
    case WM_WTSSESSION_CHANGE:
        if (wParam == WTS_SESSION_LOCK) {
            self->sessionLocked_ = true;
            self->desktop_.reset(); self->desktopAlpha_ = {};
            // Lock transitions must restore immediately, regardless of animation preference.
            for (auto& surface : self->surfaces_) {
                surface.alpha = {}; surface.applied = 0;
                SetLayeredWindowAttributes(surface.window, 0, 0, LWA_ALPHA);
                ShowWindow(surface.window, SW_HIDE);
            }
            KillTimer(window, 1);
            self->frame_.cancel();
            KillTimer(window, 3); self->auditing_ = false;
            self->fading_ = false;
        } else if (wParam == WTS_SESSION_UNLOCK) {
            self->sessionLocked_ = false;
            self->transition();
        }
        return 0;
    case WM_CLOSE:
        self->desktop_.reset();
        KillTimer(window, 1);
        self->frame_.cancel();
        KillTimer(window, 2);
        KillTimer(window, 3);
        for (const auto& surface : self->surfaces_) DestroyWindow(surface.window);
        self->surfaces_.clear();
        DestroyWindow(window);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
}
