#include "core.hpp"
#include <cstdlib>
#include <iostream>

int main() {
    int failures = 0;
    auto check = [&](bool ok, const char* message) {
        if (!ok) { std::cerr << message << '\n'; ++failures; }
    };
    using namespace screen;
    check(opacity(100, false) == 0, "Full brightness must remove the overlay");
    check(opacity(15, false) == 217, "Maximum dimming must retain 15 percent brightness");
    check(opacity(-100, false) == 217, "Corrupt values must not black out the display");
    check(opacity(50, true) == 0, "Pause must always be transparent");
    check(opacity(500, false) == 0, "Brightness must clamp at 100");
    check(ease(-1) == 0 && ease(1) == 1, "Animation must clamp outside its duration");
    check(std::abs(interpolate(0, 200, 0.1) - 100) < 0.001, "Animation midpoint");
    for (int i = 1; i <= 200; ++i)
        check(ease(i / 1000.0) >= ease((i - 1) / 1000.0), "Animation must be monotonic");
    check(interpolate(40, 40, 0.1) == 40, "No movement for equal targets");
    Settings settings;
    check(settings.paused, "First run must be transparent");
    settings.monitors[L"DISPLAY-A"] = 30;
    check(settings.forMonitor(L"DISPLAY-A") == 30 && settings.forMonitor(L"DISPLAY-B") == 70,
          "Overrides must isolate monitors");
    settings.setAll(80);
    check(settings.monitors.empty() && settings.forMonitor(L"DISPLAY-A") == 80, "All displays must clear overrides");
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
