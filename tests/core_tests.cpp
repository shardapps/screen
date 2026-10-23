#include "core.hpp"
#include "color.hpp"
#include "brightness_key_state.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>

int main() {
    int failures = 0;
    auto check = [&](bool ok, const char* message) {
        if (!ok) { std::cerr << message << '\n'; ++failures; }
    };
    using namespace screen;
    check(validBrightnessReportBatch(8,2,4) && !validBrightnessReportBatch(7,2,4),"HID batches must fit their packet without truncation");
    check(!validBrightnessReportBatch(8,0,1) && !validBrightnessReportBatch(8,1,0),"Empty HID reports cannot divide by zero or apply an input");
    check(!validBrightnessReportBatch(65537,1,1) && !validBrightnessReportBatch(8192,8192,1) && !validBrightnessReportBatch(65,1,65),"Untrusted report allocations and batch work are bounded");
    BrightnessKeyState keys;
    check(!keys.repeating() && keys.update(0,100)==0,"No key means no repeat timer");
    check(keys.update(BrightnessKeyState::down,100)==-1,"Brightness down reacts immediately");
    check(keys.update(BrightnessKeyState::down,200)==0,"Duplicate HID states must not double-apply a press");
    check(keys.repeat(599)==0 && keys.repeat(600)==-1,"Held keys respect the initial repeat delay");
    check(keys.repeat(601)==0 && keys.repeat(2000)==-1 && keys.repeat(2000)==0,"A delayed UI never replays a repeat backlog");
    check(keys.repeat(3100)==0 && !keys.repeating(),"Missing release stops repeating after three seconds");
    check(keys.update(BrightnessKeyState::down,3200)==0,"A stuck key cannot restart the safety timeout");
    check(keys.update(0,3300)==0 && keys.update(BrightnessKeyState::up,3400)==1,"Release permits the next genuine press");
    check(keys.update(3,3450)==0 && !keys.repeating(),"Opposing brightness keys cannot fight each other");
    keys.clear();keys.timing(0,0);
    check(keys.update(1,4000)==1 && keys.repeat(4249)==0 && keys.repeat(4250)==1 && keys.repeat(4279)==0,"Repeat timing is bounded against invalid system values");
    keys.clear();check(!keys.repeating() && keys.repeat(99999)==0,"Disable, disconnect and session reset cancel held keys");
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
    check(!settings.resumeOnLaunch && settings.theme == 0 && settings.adjustmentStep == 5 && settings.openingShortcut == 0,
          "New preferences must preserve safe startup and familiar defaults");
    settings.monitors[L"DISPLAY-A"] = 30;
    check(settings.forMonitor(L"DISPLAY-A") == 30 && settings.forMonitor(L"DISPLAY-B") == 70,
          "Overrides must isolate monitors");
    settings.setAll(80);
    check(settings.monitors.empty() && settings.forMonitor(L"DISPLAY-A") == 80, "All displays must clear overrides");
    const std::wstring desktopA=L"{11111111-1111-1111-1111-111111111111}", desktopB=L"{22222222-2222-2222-2222-222222222222}";
    check(validDesktopId(desktopA) && !validDesktopId(L"Desktop 1") && !validDesktopId(L"{00000000-0000-0000-0000-000000000000}"),
          "Only real canonical desktop identifiers can create profiles");
    settings.monitors[L"DISPLAY-A"]=65; settings.separateDesktops=true; settings.activeDesktop=desktopA;
    check(settings.currentBrightness()==80 && settings.forMonitor(L"DISPLAY-A")==65,"New desktops inherit the shared baseline");
    settings.setAll(20);
    check(settings.brightness==80 && settings.monitors.at(L"DISPLAY-A")==65 && settings.forMonitor(L"DISPLAY-A")==20,
          "Changing one desktop must not overwrite shared monitor settings");
    settings.activeDesktop=desktopB;
    check(settings.currentBrightness()==80 && settings.forMonitor(L"DISPLAY-A")==65,"First visit must not inherit the previous desktop's dimming");
    settings.setMonitor(L"DISPLAY-A",40);
    settings.activeDesktop=desktopA;
    check(settings.currentBrightness()==20 && settings.forMonitor(L"DISPLAY-A")==20,"Switching back restores the original desktop's levels");
    settings.setDesktopBrightness(desktopB,95);
    check(settings.currentBrightness()==20 && settings.desktopBrightness(desktopB)==95,"Editing an inactive profile must not change the active desktop");
    const auto render=settings.renderingSnapshot();
    check(render.desktops.empty() && render.forMonitor(L"DISPLAY-A")==20 && render.activeDesktop==desktopA,
          "Rendering snapshots resolve active levels without copying all desktop profiles");
    settings.separateDesktops=false;
    check(settings.currentBrightness()==80 && settings.forMonitor(L"DISPLAY-A")==65,"Disabling desktop profiles restores the shared baseline");
    settings.separateDesktops=true; settings.desktops.erase(desktopA);
    check(settings.currentBrightness()==80,"Resetting a profile restores shared brightness without deleting other desktops");
    check(!settings.setDesktopBrightness(L"invalid",8),"Malformed desktop IDs must not create persistent entries");
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
