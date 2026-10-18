#include "core.hpp"
#include "color.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>

int main() {
    int failures = 0;
    auto check = [&](bool ok, const char* message) {
        if (!ok) { std::cerr << message << '\n'; ++failures; }
    };
    using namespace screen;
    const auto identity = identityColor();
    check(sameColor(dimColor(identity, 0, false), identity), "Zero opacity must preserve the existing color matrix");
    const auto half = dimColor(identity, 128, false);
    check(std::abs(half[0][0] - 127 / 255.0f) < 0.00001f && half[3][3] == 1 && half[4][4] == 1,
          "Desktop dimming must scale RGB while preserving alpha and coordinates");
    const auto floor = dimColor(identity, 255, false);
    check(std::abs(floor[0][0] - 20 / 255.0f) < 0.00001f, "Desktop matrix must retain the safe dimming floor");
    auto inverted = identity;
    for (int channel = 0; channel < 3; ++channel) { inverted[channel][channel] = -1; inverted[4][channel] = 1; }
    const auto filtered = dimColor(inverted, 128, false);
    check(std::abs(filtered[0][0] + 127 / 255.0f) < 0.00001f &&
          std::abs(filtered[4][0] - 127 / 255.0f) < 0.00001f,
          "Dimming must compose after an existing accessibility color filter");
    const auto warm = dimColor(identity, 128, true);
    check(warm[4][0] > warm[4][1] && warm[4][1] > warm[4][2] && warm[0][0] == half[0][0],
          "Warm tint must use additive RGB offsets, not change the gain");
    auto invalid = identity; invalid[0][0] = std::numeric_limits<float>::quiet_NaN();
    check(!sameColor(invalid, identity), "Non-finite matrices must never compare equal");
    check(opacity(100, false) == 0, "Full brightness must remove the overlay");
    check(opacity(8, false) == 235, "Deeper dimming must retain the new brightness floor");
    check(opacity(-100, false) == 235, "Corrupt values must not black out the display");
    check(opacity(50, true) == 0, "Pause must always be transparent");
    check(opacity(500, false) == 0, "Brightness must clamp at 100");
    check(ease(-1) == 0 && ease(1) == 1, "Animation must clamp outside its duration");
    check(std::abs(interpolate(0, 200, 0.1) - 100) < 0.001, "Animation midpoint");
    for (int i = 1; i <= 200; ++i)
        check(ease(i / 1000.0) >= ease((i - 1) / 1000.0), "Animation must be monotonic");
    check(interpolate(40, 40, 0.1) == 40, "No movement for equal targets");
    SmoothValue fade;
    fade.target = 235;
    for (int frame = 0; frame < 30; ++frame) {
        const auto before = fade.value;
        fade.advance(1.0 / 60.0);
        check(fade.value >= before && fade.value <= 235, "Single-step fade must be monotonic and bounded");
    }
    check(fade.value == 235 && fade.velocity == 0, "Fade must settle and stop");
    fade = {};
    for (int input = 1; input <= 100; ++input) {
        fade.target = input * 2.0;
        fade.advance(0.002); // Input arrives faster than a frame; motion must not restart.
    }
    check(fade.value > 140 && fade.velocity > 0, "Rapid retargeting must keep moving instead of starving animation");
    fade.target = 0;
    for (int frame = 0; frame < 120; ++frame) {
        fade.advance(frame % 2 == 0 ? 0.005 : 0.025);
        check(fade.value >= 0 && fade.value <= 235, "Reversals must remain safe at uneven frame times");
    }
    check(fade.value == 0 && fade.velocity == 0, "Reversed animation must settle exactly");
    fade.target = 120;
    fade.snap();
    check(fade.value == 120 && fade.velocity == 0, "Emergency / reduced-motion changes must snap immediately");
    Settings settings;
    check(settings.paused, "First run must be transparent");
    settings.monitors[L"DISPLAY-A"] = 30;
    check(settings.forMonitor(L"DISPLAY-A") == 30 && settings.forMonitor(L"DISPLAY-B") == 70,
          "Overrides must isolate monitors");
    settings.setAll(80);
    check(settings.monitors.empty() && settings.forMonitor(L"DISPLAY-A") == 80, "All displays must clear overrides");
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
