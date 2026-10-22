#pragma once
#include "overlay.hpp"
#include "startup.hpp"
#include "ui.hpp"
#include "virtual_desktops.hpp"
#include <functional>

namespace screen {
enum class PreferenceAction { display, brightness, theme, warm, smooth, resume, step, shortcut,
    startup, pause, timedPause, restore, desktopProfiles, desktopBrightness, resetDesktop };
class PreferencesWindow {
public:
    PreferencesWindow(Settings& settings, std::vector<Monitor>& displays, std::wstring& selected,
        std::function<void(PreferenceAction, int)> action);
    ~PreferencesWindow();
    void show(HWND owner);
    void refresh(bool rebuildDisplays = false);
    void startupState(StartupState state, bool busy);
    HWND window() const { return window_; }
    bool message(MSG& message);
    void close();
    void desktops(const std::vector<VirtualDesktop>& items);
    std::wstring profileDesktop() const;
private:
    struct Item { HWND window; int page, x, y, width, height; };
    Settings& settings_;
    std::vector<Monitor>& displays_;
    std::wstring& selected_;
    std::function<void(PreferenceAction, int)> action_;
    HWND window_{};
    HWND owner_{};
    HFONT font_{}, titleFont_{}, smallFont_{};
    HBRUSH background_{};
    ui::Palette colors_{};
    UINT dpi_ = 96;
    int page_ = 0, scroll_ = 0, animationAlpha_ = 255;
    StartupState startup_ = StartupState::unknown;
    bool startupBusy_ = false, refreshing_ = false, layingOut_ = false;
    int appliedTheme_ = -1;
    std::vector<Item> items_;
    std::vector<VirtualDesktop> desktops_;
    std::vector<HWND> identifiers_;
    HFONT identifierFont_{};
    int px(int value) const;
    HWND add(const wchar_t* type, const wchar_t* text, DWORD style, int id,
        int page, int x, int y, int width, int height);
    void createControls();
    void layout();
    void theme();
    void fonts();
    void page(int index);
    void identify();
    void clearIdentifiers();
    void refreshDesktops(bool rebuild);
    void command(int id, int notification);
    void drawButton(const DRAWITEMSTRUCT& draw);
    void paint(HDC dc);
    LRESULT customDraw(NMCUSTOMDRAW& draw);
    static LRESULT CALLBACK procedure(HWND, UINT, WPARAM, LPARAM);
};
}
