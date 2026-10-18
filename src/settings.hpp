#pragma once
#include "core.hpp"
#include <filesystem>

namespace screen {
std::filesystem::path settingsPath();
Settings loadSettings();
bool saveSettings(const Settings& settings) noexcept;
}
