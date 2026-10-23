#pragma once
#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <string_view>

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
inline constexpr size_t maxDesktopProfiles = 64;
inline bool validDesktopId(std::wstring_view id) noexcept {
    if (id.size() != 38 || id.front() != L'{' || id.back() != L'}') return false;
    for (size_t i = 1; i < 37; ++i) {
        if (i == 9 || i == 14 || i == 19 || i == 24) { if (id[i] != L'-') return false; }
        else if (!((id[i] >= L'0' && id[i] <= L'9') || (id[i] >= L'A' && id[i] <= L'F'))) return false;
    }
    return id != L"{00000000-0000-0000-0000-000000000000}";
}
struct BrightnessProfile {
    int brightness = 70;
    std::map<std::wstring, int> monitors;
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
    bool brightnessKeys = false; // Optional consumer-HID brightness input; never captures typing.
    bool brightnessKeyFeedback = true; // Only used after the brightness-key opt-in.
    bool separateDesktops = false;
    std::wstring activeDesktop; // Runtime context; the shared baseline is never replaced on a switch.
    std::map<std::wstring, BrightnessProfile> desktops;
    std::wstring selectedDisplay;
    std::map<std::wstring, int> monitors;
    const BrightnessProfile* currentProfile() const {
        const auto it = desktops.find(activeDesktop);
        return separateDesktops && it != desktops.end() ? &it->second : nullptr;
    }
    int currentBrightness() const { const auto* p = currentProfile(); return clampBrightness(p ? p->brightness : brightness); }
    int desktopBrightness(const std::wstring& id) const {
        const auto it = desktops.find(id);
        return clampBrightness(it == desktops.end() ? brightness : it->second.brightness);
    }
    BrightnessProfile* profile(const std::wstring& id) {
        if (!validDesktopId(id)) return nullptr;
        const auto it = desktops.find(id);
        if (it != desktops.end()) return &it->second;
        if (desktops.size() >= maxDesktopProfiles) return nullptr;
        return &desktops.emplace(id, BrightnessProfile{brightness, monitors}).first->second;
    }
    int forMonitor(const std::wstring& device) const {
        const auto* p = currentProfile(); const auto& levels = p ? p->monitors : monitors;
        const auto it = levels.find(device);
        return clampBrightness(it == levels.end() ? currentBrightness() : it->second);
    }
    void setMonitor(const std::wstring& device, int value) {
        if (separateDesktops && validDesktopId(activeDesktop)) {
            if (auto* p = profile(activeDesktop)) p->monitors[device] = clampBrightness(value);
        } else monitors[device] = clampBrightness(value);
    }
    bool setDesktopBrightness(const std::wstring& id, int value) {
        auto* p = profile(id); if (!p) return false;
        p->brightness = clampBrightness(value); p->monitors.clear(); return true;
    }
    void setAll(int value) {
        if (separateDesktops && validDesktopId(activeDesktop)) setDesktopBrightness(activeDesktop, value);
        else { brightness = clampBrightness(value); monitors.clear(); }
    }
    Settings renderingSnapshot() const {
        Settings result;
        result.brightness=currentBrightness(); result.paused=paused;
        result.warm=warm; result.smooth=smooth; result.activeDesktop=activeDesktop;
        const auto* p=currentProfile(); result.monitors=p?p->monitors:monitors;
        return result; // No profile-map copies on high-frequency slider input.
    }
};
}
