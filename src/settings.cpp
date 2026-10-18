#include "settings.hpp"
#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>

namespace screen {
std::filesystem::path settingsPath() {
    PWSTR folder = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &folder))) return {};
    std::filesystem::path path(folder);
    CoTaskMemFree(folder);
    return path / L"Shardapps" / L"Screen" / L"settings.ini";
}
Settings loadSettings() {
    Settings result;
    const auto path = settingsPath();
    if (path.empty()) return result;
    std::wifstream input(path);
    std::wstring line;
    while (std::getline(input, line)) {
        const auto split = line.find(L'=');
        if (split == std::wstring::npos) continue;
        const auto key = line.substr(0, split);
        const auto text = line.substr(split + 1);
        int value = 0;
        std::wistringstream parser(text);
        if (!(parser >> value)) continue;
        if (key == L"brightness") result.brightness = clampBrightness(value);
        // A fresh process always starts paused: restoration cannot unexpectedly darken a display.
        else if (key == L"warm") result.warm = value == 1;
        else if (key == L"smooth") result.smooth = value != 0;
        else if (key.starts_with(L"monitor:")) result.monitors[key.substr(8)] = clampBrightness(value);
    }
    return result;
}
bool saveSettings(const Settings& settings) noexcept {
    try {
        const auto path = settingsPath();
        if (path.empty()) return false;
        std::filesystem::create_directories(path.parent_path());
        auto temporary = path;
        temporary += L".tmp";
        {
            std::wofstream output(temporary, std::ios::trunc);
            output << L"brightness=" << settings.brightness << L'\n'
                   << L"warm=" << settings.warm << L'\n'
                   << L"smooth=" << settings.smooth << L'\n';
            for (const auto& [device, value] : settings.monitors)
                output << L"monitor:" << device << L'=' << value << L'\n';
            output.flush();
            if (!output) return false;
        }
        return MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    } catch (...) { return false; }
}
}
