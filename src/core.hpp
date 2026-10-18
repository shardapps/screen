#pragma once
#include <algorithm>
#include <cmath>
#include <map>
#include <string>

namespace screen {
inline constexpr int minBrightness = 15;
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
struct Settings {
    int brightness = 70;
    bool paused = true; // First launch is transparent until the user opts in.
    bool warm = false;
    bool smooth = true;
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
