#pragma once
#include <windows.h>
#include <thread>
#include <atomic>
#include <optional>
namespace screen {
inline constexpr UINT startupResult = WM_APP + 13;
enum class StartupState { unknown, off, on, userDisabled, blocked, error };
bool packagedApp();
bool removeStartupForCurrentExecutable();
class StartupController {
public:
    bool request(HWND notify, std::optional<bool> enabled = {});
    bool busy() const { return busy_; }
private:
    std::atomic<bool> busy_{false};
    std::jthread worker_;
};
}
