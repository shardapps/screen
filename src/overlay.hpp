#pragma once
#include "core.hpp"
#include "desktop.hpp"
#include "virtual_desktops.hpp"
#include <windows.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

namespace screen {
inline constexpr UINT overlayFailure = WM_APP + 10;
inline constexpr UINT settingsFailure = WM_APP + 11;
inline constexpr UINT coverageChanged = WM_APP + 12;
struct Monitor { std::wstring device; RECT bounds; bool primary; };
std::vector<Monitor> enumerateMonitors();

class OverlayEngine {
public:
    explicit OverlayEngine(HWND owner, bool overlayOnly = false);
    ~OverlayEngine();
    OverlayEngine(const OverlayEngine&) = delete;
    OverlayEngine& operator=(const OverlayEngine&) = delete;
    void submit(const Settings& settings, bool persist = false, bool rebuild = false, bool immediate = false);
    bool healthy() const noexcept { return control_ != nullptr; }
private:
    struct Surface {
        HWND window{};
        std::wstring device;
        SmoothValue alpha;
        BYTE applied = 0;
    };
    HWND owner_{}, control_{};
    std::thread thread_;
    std::mutex mutex_;
    std::atomic<bool> pending_{false};
    Settings desired_, active_, saved_;
    bool persist_ = false, rebuild_ = false, immediate_ = false;
    bool sessionLocked_ = false, motionAllowed_ = true;
    bool fading_ = false, coveragePending_ = false, auditing_ = false;
    bool overlayOnly_ = false, desktopMode_ = false, desktopBlocked_ = false;
    int reportedMode_ = -1;
    DesktopDimmer desktop_;
    IVirtualDesktopManager* virtualManager_{};
    bool virtualApartment_ = false;
    SmoothValue desktopAlpha_;
    HWINEVENTHOOK foregroundHook_{}, windowHook_{}, desktopHook_{}, locationHook_{};
    static thread_local OverlayEngine* observer_;
    std::vector<Surface> surfaces_;
    std::vector<HWND> higherBandWindows_;
    std::chrono::steady_clock::time_point start_;
    static LRESULT CALLBACK procedure(HWND, UINT, WPARAM, LPARAM);
    static void CALLBACK windowEvent(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD);
    void queueCoverage();
    void ensureCoverage();
    void rebuild();
    void consume();
    void transition();
    void tick();
    void reportFailure();
};
}
