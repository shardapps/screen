#pragma once
#include "core.hpp"
#include <windows.h>
#include <shobjidl.h>
#include <atomic>
#include <vector>

namespace screen {
inline constexpr UINT virtualDesktopsChanged = WM_APP + 14;
inline constexpr UINT preferencesClosed = WM_APP + 15;
struct VirtualDesktop { std::wstring id, name; };
// Read-only Explorer metadata plus the public desktop manager. No private COM
// interfaces, shell injection, registry writes or periodic discovery timer.
class VirtualDesktops {
public:
    explicit VirtualDesktops(HWND owner, const std::wstring& registryPath =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VirtualDesktops");
    ~VirtualDesktops();
    VirtualDesktops(const VirtualDesktops&) = delete;
    VirtualDesktops& operator=(const VirtualDesktops&) = delete;
    void refresh(bool notification = false);
    void moveWindow(HWND window);
    const std::wstring& current() const { return current_; }
    const std::vector<VirtualDesktop>& items() const { return items_; }
    std::wstring name() const;
private:
    HWND owner_{};
    HKEY root_{}, watched_{};
    HANDLE event_{}, wait_{};
    IVirtualDesktopManager* manager_{};
    bool uninitialize_ = false, native_ = false;
    std::atomic<bool> pending_{false}, closing_{false};
    std::wstring path_, current_;
    std::vector<VirtualDesktop> items_;
    static void CALLBACK changed(void*, BOOLEAN);
    void arm();
    void stopWait();
    bool manager();
};
// The caller owns the desktop manager and its COM apartment.
void moveToDesktop(HWND window, const std::wstring& id, IVirtualDesktopManager* manager);
}
