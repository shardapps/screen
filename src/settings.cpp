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
        if (key == L"selected") { result.selectedDisplay = text.substr(0,128); continue; }
        int value = 0;
        std::wistringstream parser(text);
        if (!(parser >> value)) continue;
        if (key == L"brightness") result.brightness = clampBrightness(value);
        // Resume is opt-in and evaluated after the complete preference file is read.
        else if (key == L"warm") result.warm = value == 1;
        else if (key == L"smooth") result.smooth = value != 0;
        else if (key == L"theme") result.theme = std::clamp(value, 0, 6);
        else if (key == L"step") result.adjustmentStep = value==1 || value==2 || value==5 || value==10 ? value : 5;
        else if (key == L"shortcut") result.openingShortcut = std::clamp(value, 0, 2);
        else if (key == L"resume") result.resumeOnLaunch = value == 1;
        else if (key.starts_with(L"monitor:")) result.monitors[key.substr(8)] = clampBrightness(value);
    }
    result.paused = !result.resumeOnLaunch;
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
                   << L"smooth=" << settings.smooth << L'\n'
                   << L"theme=" << settings.theme << L'\n'
                   << L"step=" << settings.adjustmentStep << L'\n'
                   << L"shortcut=" << settings.openingShortcut << L'\n'
                   << L"resume=" << settings.resumeOnLaunch << L'\n'
                   << L"selected=" << settings.selectedDisplay << L'\n';
            for (const auto& [device, value] : settings.monitors)
                output << L"monitor:" << device << L'=' << value << L'\n';
            output.flush();
            if (!output) return false;
        }
        // Readers and antivirus can briefly hold either file during an atomic
        // replacement. Retry only sharing/access conflicts, on the worker thread.
        for (int attempt = 0; attempt < 6; ++attempt) {
            if (MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
            const auto error = GetLastError();
            if (error != ERROR_SHARING_VIOLATION && error != ERROR_LOCK_VIOLATION && error != ERROR_ACCESS_DENIED) return false;
            if (attempt < 5) Sleep(static_cast<DWORD>(5 << attempt));
        }
        return false;
    } catch (...) { return false; }
}
}
