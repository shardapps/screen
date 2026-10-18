#pragma once
#include <array>
#include <cmath>
#include <algorithm>

namespace screen {
using ColorMatrix = std::array<std::array<float, 5>, 5>;
inline ColorMatrix identityColor() noexcept {
    ColorMatrix result{};
    for (int i = 0; i < 5; ++i) result[i][i] = 1;
    return result;
}
inline bool sameColor(const ColorMatrix& a, const ColorMatrix& b) noexcept {
    for (int row = 0; row < 5; ++row)
        for (int column = 0; column < 5; ++column)
            if (!std::isfinite(a[row][column]) || !std::isfinite(b[row][column]) ||
                std::abs(a[row][column] - b[row][column]) > 0.00001f) return false;
    return true;
}
// Row-vector convention used by MAGCOLOREFFECT: apply dimming after the existing
// color filter, retaining its alpha/coordinate components and color mixing.
inline ColorMatrix dimColor(const ColorMatrix& base, unsigned char alpha, bool warm) noexcept {
    auto result = base;
    const float amount = std::min(alpha, static_cast<unsigned char>(235)) / 255.0f;
    constexpr float tint[] = {66 / 255.0f, 30 / 255.0f, 8 / 255.0f};
    for (int row = 0; row < 5; ++row)
        for (int column = 0; column < 3; ++column)
            result[row][column] = base[row][column] * (1 - amount) +
                base[row][4] * (warm ? tint[column] * amount : 0);
    return result;
}
}
