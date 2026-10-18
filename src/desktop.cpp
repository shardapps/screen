#include "desktop.hpp"

namespace screen {
namespace {
ColorMatrix matrix(const MAGCOLOREFFECT& effect) {
    ColorMatrix result{};
    for (int row = 0; row < 5; ++row)
        for (int column = 0; column < 5; ++column) result[row][column] = effect.transform[row][column];
    return result;
}
bool setEffect(const ColorMatrix& value) {
    MAGCOLOREFFECT effect{};
    for (int row = 0; row < 5; ++row)
        for (int column = 0; column < 5; ++column) effect.transform[row][column] = value[row][column];
    return MagSetFullscreenColorEffect(&effect) != FALSE;
}
}
bool DesktopDimmer::prepare() {
    if (initialized_) return intact();
    if (!MagInitialize()) return false;
    initialized_ = true;
    MAGCOLOREFFECT current{};
    if (!MagGetFullscreenColorEffect(&current)) { reset(); return false; }
    baseline_ = matrix(current);
    if (!sameColor(baseline_, baseline_)) { reset(); return false; }
    return true;
}
bool DesktopDimmer::intact() const {
    if (!owned_) return true;
    MAGCOLOREFFECT current{};
    return MagGetFullscreenColorEffect(&current) && sameColor(matrix(current), last_);
}
bool DesktopDimmer::apply(BYTE alpha, bool warm) {
    if (!alpha) { reset(); return true; }
    if (!prepare()) { reset(); return false; }
    const auto next = dimColor(baseline_, alpha, warm);
    if (owned_ && sameColor(next, last_)) return true;
    if (!setEffect(next)) { reset(); return false; }
    last_ = next; owned_ = true;
    return true;
}
void DesktopDimmer::reset() {
    // Do not overwrite a newer transform installed by another accessibility app.
    if (owned_ && intact()) setEffect(baseline_);
    owned_ = false;
    if (initialized_) MagUninitialize();
    initialized_ = false;
}
}
