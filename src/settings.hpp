#pragma once
#include "core.hpp"
#include <filesystem>
#include <iosfwd>

namespace screen {
std::filesystem::path settingsPath();
Settings loadSettings();
Settings parseSettings(std::wistream& input);
void writeSettings(std::wostream& output, const Settings& settings);
bool saveSettings(const Settings& settings) noexcept;
}
