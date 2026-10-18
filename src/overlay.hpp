#pragma once
#include "core.hpp"
#include <windows.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

namespace screen {
inline constexpr UINT overlayFailure = WM_APP + 10;
inline constexpr UINT settingsFailure = WM_APP + 11;
struct Monitor { std::wstring device; RECT bounds; bool primary; };
std::vector<Monitor> enumerateMonitors();

class OverlayEngine {
public:
    explicit OverlayEngine(HWND owner);
    ~OverlayEngine();
    OverlayEngine(const OverlayEngine&) = delete;
    OverlayEngine& operator=(const OverlayEngine&) = delete;
    void submit(const Settings& settings, bool persist = false, bool rebuild = false);
    bool healthy() const noexcept { return control_ != nullptr; }
private:
    struct Surface {
        HWND window{};
        std::wstring device;
        double current = 0, from = 0, target = 0;
    };
    HWND owner_{}, control_{};
    std::thread thread_;
    std::mutex mutex_;
    std::atomic<bool> pending_{false};
    Settings desired_, active_;
    bool persist_ = false, rebuild_ = false;
    bool sessionLocked_ = false;
    std::vector<Surface> surfaces_;
    std::chrono::steady_clock::time_point start_;
    static LRESULT CALLBACK procedure(HWND, UINT, WPARAM, LPARAM);
    void rebuild();
    void consume();
    void transition();
    void tick();
    void reportFailure();
};
}
