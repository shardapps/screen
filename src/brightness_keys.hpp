#pragma once
#include "brightness_key_state.hpp"
#include <windows.h>
#include <memory>

namespace screen {
struct BrightnessKeyEvent { int direction=0; unsigned observed=0; };
class BrightnessKeys {
public:
    BrightnessKeys();
    ~BrightnessKeys();
    BrightnessKeys(const BrightnessKeys&)=delete;
    BrightnessKeys& operator=(const BrightnessKeys&)=delete;
    bool start(HWND owner);
    BrightnessKeyEvent input(HRAWINPUT input,std::uint64_t now);
    int repeat(std::uint64_t now);
    bool repeating() const;
    void clear();
    void removed(HANDLE device);
    unsigned supportedDevices(); // Explicit, read-only diagnostic; never polled.
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
