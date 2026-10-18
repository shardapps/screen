#include "overlay.hpp"
#include "settings.hpp"
#include <future>
#include <wtsapi32.h>

namespace screen {
namespace {
constexpr UINT updateMessage = WM_APP + 1;
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
OverlayEngine::OverlayEngine(HWND owner) : owner_(owner) {
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
        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        WTSUnRegisterSessionNotification(window);
    });
    (void)future.get(); // Synchronizes the worker's initialization of control_.
}
OverlayEngine::~OverlayEngine() {
    if (control_) PostMessageW(control_, WM_CLOSE, 0, 0);
    if (thread_.joinable()) thread_.join();
}
void OverlayEngine::submit(const Settings& settings, bool persist, bool rebuildRequested) {
    if (!control_) return;
    {
        const std::lock_guard lock(mutex_);
        desired_ = settings;
        persist_ = persist_ || persist;
        rebuild_ = rebuild_ || rebuildRequested;
    }
    if (!pending_.exchange(true)) PostMessageW(control_, updateMessage, 0, 0);
}
void OverlayEngine::reportFailure() {
    // Fail open: remove every dimming surface if the overlay cannot be maintained safely.
    active_.paused = true;
    {
        const std::lock_guard lock(mutex_);
        desired_.paused = true;
    }
    // Display broadcasts must not revive the last active opacity after an error.
    KillTimer(control_, 1);
    for (const auto& surface : surfaces_) DestroyWindow(surface.window);
    surfaces_.clear();
    PostMessageW(owner_, overlayFailure, 0, 0);
}
void OverlayEngine::rebuild() {
    for (const auto& surface : surfaces_) DestroyWindow(surface.window);
    surfaces_.clear();
    for (const auto& monitor : enumerateMonitors()) {
        auto window = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
            surfaceClass, L"Screen dimming overlay", WS_POPUP,
            monitor.bounds.left, monitor.bounds.top,
            monitor.bounds.right - monitor.bounds.left, monitor.bounds.bottom - monitor.bounds.top,
            nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!window) { reportFailure(); return; }
        surfaces_.push_back({window, monitor.device});
        SetWindowLongPtrW(window, GWLP_USERDATA, active_.warm ? 1 : 0);
        if (!SetLayeredWindowAttributes(window, 0, 0, LWA_ALPHA) ||
            !SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE)) {
            reportFailure(); return;
        }
        // Kept hidden at zero opacity. Visible surfaces appear only for an active fade.
    }
}
void OverlayEngine::consume() {
    bool persist, refresh;
    {
        const std::lock_guard lock(mutex_);
        active_ = desired_;
        persist = persist_;
        refresh = rebuild_;
        persist_ = rebuild_ = false;
        pending_ = false;
    }
    if (refresh || surfaces_.empty()) rebuild();
    for (const auto& surface : surfaces_) {
        SetWindowLongPtrW(surface.window, GWLP_USERDATA, active_.warm ? 1 : 0);
        InvalidateRect(surface.window, nullptr, FALSE);
    }
    transition();
    if (persist && !saveSettings(active_)) PostMessageW(owner_, settingsFailure, 0, 0);
}
void OverlayEngine::transition() {
    start_ = std::chrono::steady_clock::now();
    for (auto& surface : surfaces_) {
        surface.from = surface.current;
        surface.target = opacity(active_.forMonitor(surface.device), active_.paused || sessionLocked_);
        if (surface.target > 0 || surface.current > 0) ShowWindow(surface.window, SW_SHOWNOACTIVATE);
    }
    tick();
}
void OverlayEngine::tick() {
    const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count();
    const bool finished = !active_.smooth || elapsed >= transitionSeconds;
    bool animating = false;
    for (auto& surface : surfaces_) {
        surface.current = finished ? surface.target : interpolate(surface.from, surface.target, elapsed);
        if (!SetLayeredWindowAttributes(surface.window, 0, static_cast<BYTE>(std::lround(surface.current)), LWA_ALPHA)) {
            reportFailure(); return;
        }
        if (finished && surface.target == 0) ShowWindow(surface.window, SW_HIDE);
        animating = animating || (!finished && surface.from != surface.target);
    }
    if (animating) {
        if (!SetTimer(control_, 1, 15, nullptr)) reportFailure();
    } else KillTimer(control_, 1);
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
    case WM_TIMER: self->tick(); return 0;
    case WM_DISPLAYCHANGE: self->rebuild(); self->transition(); return 0;
    case WM_WTSSESSION_CHANGE:
        if (wParam == WTS_SESSION_LOCK) {
            self->sessionLocked_ = true;
            // Lock transitions must restore immediately, regardless of animation preference.
            for (auto& surface : self->surfaces_) {
                surface.current = surface.from = surface.target = 0;
                ShowWindow(surface.window, SW_HIDE);
            }
            KillTimer(window, 1);
        } else if (wParam == WTS_SESSION_UNLOCK) {
            self->sessionLocked_ = false;
            self->transition();
        }
        return 0;
    case WM_CLOSE:
        KillTimer(window, 1);
        for (const auto& surface : self->surfaces_) DestroyWindow(surface.window);
        self->surfaces_.clear();
        DestroyWindow(window);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
}
