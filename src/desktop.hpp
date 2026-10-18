#pragma once
#include "color.hpp"
#include <windows.h>
#include <magnification.h>

namespace screen {
// Owned by the rendering thread. No magnification/zoom or input transforms.
class DesktopDimmer {
public:
    ~DesktopDimmer() { reset(); }
    bool prepare();
    bool apply(BYTE alpha, bool warm);
    bool intact() const;
    void reset();
private:
    bool initialized_ = false, owned_ = false;
    ColorMatrix baseline_{}, last_{};
};
}
