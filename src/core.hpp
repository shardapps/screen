#pragma once
#include <algorithm>
#include <cmath>
#include <map>
#include <string>

namespace screen {
inline constexpr int minBrightness = 8;
inline constexpr int maxBrightness = 100;
inline constexpr double transitionSeconds = 0.20;

inline int clampBrightness(int value) noexcept {
    return std::clamp(value, minBrightness, maxBrightness);
}
inline unsigned char opacity(int brightness, bool paused) noexcept {
    if (paused) return 0;
    return static_cast<unsigned char>(std::lround((100 - clampBrightness(brightness)) * 255.0 / 100));
}
inline double ease(double elapsed) noexcept {
    const auto t = std::clamp(elapsed / transitionSeconds, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}
inline double interpolate(double from, double to, double elapsed) noexcept {
    return from + (to - from) * ease(elapsed);
}
// Exact critically damped integration. Retargeting retains velocity rather than
// restarting an ease-in curve on every mouse event (which can stall a drag).
struct SmoothValue {
    double value = 0;
    double velocity = 0;
    double target = 0;
    bool advance(double seconds) noexcept {
        const auto dt = std::clamp(seconds, 0.0, 1.0);
        constexpr double omega = 45.0;
        const auto offset = value - target;
        const auto momentum = velocity + omega * offset;
        const auto decay = std::exp(-omega * dt);
        value = std::clamp(target + (offset + momentum * dt) * decay, 0.0, 235.0);
        velocity = (velocity - omega * momentum * dt) * decay;
        if (std::abs(value - target) < 0.25 && std::abs(velocity) < 3.0) {
            value = target;
            velocity = 0;
            return false;
        }
        return true;
    }
    void snap() noexcept { value = target; velocity = 0; }
};
struct Settings {
    int brightness = 70;
    bool paused = true; // First launch is transparent until the user opts in.
    bool warm = false;
    bool smooth = true;
    int theme = 0; // System, Pearl, Midnight, Graphite, Ocean, Forest, Sunset.
    int adjustmentStep = 5;
    int openingShortcut = 0; // Alt+S, Ctrl+Alt+S, Ctrl+Shift+S.
    bool resumeOnLaunch = false;
    std::wstring selectedDisplay;
    std::map<std::wstring, int> monitors;
    int forMonitor(const std::wstring& device) const {
        const auto it = monitors.find(device);
        return clampBrightness(it == monitors.end() ? brightness : it->second);
    }
    void setAll(int value) {
        brightness = clampBrightness(value);
        monitors.clear();
    }
};
}
