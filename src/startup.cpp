#include "startup.hpp"
#include <appmodel.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Foundation.h>
#include <string>
#include <chrono>

namespace screen {
namespace {
constexpr auto runKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr auto runName = L"ScreenByShardapps";
std::wstring startupCommand() {
    std::wstring path(32768, L'\0');
    const auto length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size()) return {};
    path.resize(length); return L"\"" + path + L"\" --background --startup";
}
StartupState unpackagedStartup(std::optional<bool> enabled) {
    const auto command = startupCommand();
    if (command.empty()) return StartupState::error;
    if (enabled) {
        if (*enabled) {
            HKEY key{};
            if (RegCreateKeyExW(HKEY_CURRENT_USER, runKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
                return StartupState::error;
            const auto result = RegSetValueExW(key, runName, 0, REG_SZ, reinterpret_cast<const BYTE*>(command.c_str()),
                static_cast<DWORD>((command.size()+1)*sizeof(wchar_t)));
            RegCloseKey(key); if (result != ERROR_SUCCESS) return StartupState::error;
        } else {
            const auto result = RegDeleteKeyValueW(HKEY_CURRENT_USER, runKey, runName);
            if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) return StartupState::error;
        }
    }
    std::wstring value(32768, L'\0'); DWORD bytes = static_cast<DWORD>(value.size()*sizeof(wchar_t));
    const auto result = RegGetValueW(HKEY_CURRENT_USER, runKey, runName, RRF_RT_REG_SZ, nullptr, value.data(), &bytes);
    if (result == ERROR_FILE_NOT_FOUND) return StartupState::off;
    if (result != ERROR_SUCCESS) return StartupState::error;
    value.resize(wcsnlen_s(value.c_str(), value.size()));
    return value == command ? StartupState::on : StartupState::off;
}
template<typename Operation> auto awaitStartup(Operation operation) {
    if (operation.wait_for(std::chrono::seconds(3)) != winrt::Windows::Foundation::AsyncStatus::Completed) {
        operation.Cancel(); throw winrt::hresult_error(HRESULT_FROM_WIN32(ERROR_TIMEOUT));
    }
    return operation.GetResults();
}
StartupState packagedStartup(std::optional<bool> enabled) {
    using namespace winrt::Windows::ApplicationModel;
    const auto task = awaitStartup(StartupTask::GetAsync(L"ScreenStartup"));
    auto state = task.State();
    if (enabled) {
        if (*enabled && state == StartupTaskState::Disabled) state = awaitStartup(task.RequestEnableAsync());
        else if (!*enabled) { task.Disable(); state = task.State(); }
    }
    switch (state) {
    case StartupTaskState::Enabled: case StartupTaskState::EnabledByPolicy: return StartupState::on;
    case StartupTaskState::DisabledByUser: return StartupState::userDisabled;
    case StartupTaskState::DisabledByPolicy: return StartupState::blocked;
    default: return StartupState::off;
    }
}
}
bool packagedApp() {
    UINT32 size = 0; return GetCurrentPackageFullName(&size, nullptr) != APPMODEL_ERROR_NO_PACKAGE;
}
bool removeStartupForCurrentExecutable() {
    // An uninstall must not remove a registration belonging to a different copy.
    if (packagedApp() || unpackagedStartup({}) != StartupState::on) return true;
    return unpackagedStartup(false) == StartupState::off;
}
bool StartupController::request(HWND notify, std::optional<bool> enabled) {
    if (busy_.exchange(true)) return false;
    if (worker_.joinable()) worker_.join();
    worker_ = std::jthread([this, notify, enabled] {
        auto state = StartupState::error;
        try {
            if (packagedApp()) {
                winrt::init_apartment(winrt::apartment_type::multi_threaded);
                try { state = packagedStartup(enabled); } catch (...) { state = StartupState::error; }
                winrt::uninit_apartment();
            } else state = unpackagedStartup(enabled);
        } catch (...) { state = StartupState::error; }
        busy_ = false; PostMessageW(notify, startupResult, static_cast<WPARAM>(state), 0);
    });
    return true;
}
}
