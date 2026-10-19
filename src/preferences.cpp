#include "preferences.hpp"
#include <windowsx.h>
#include <shellapi.h>
#include <algorithm>

namespace screen {
namespace {
constexpr auto preferencesClass = L"Shardapps.Screen.Settings";
constexpr int navBase = 1000, displayId = 1010, levelId = 1011, percentId = 1012,
    pauseId = 1013, timerId = 1014, restoreId = 1015, identifyId = 1016, coverageId = 1018,
    presetBase = 1020, themeBase = 1100, warmId = 1200, smoothId = 1201,
    resumeId = 1202, startupId = 1203, stepId = 1204, shortcutId = 1205,
    systemStartupId = 1206, startupStatusId = 1207;
constexpr int steps[] = {1,2,5,10};
constexpr int presets[] = {25,50,75,100};
void setText(HWND window, const std::wstring& value) {
    std::wstring current(static_cast<size_t>(GetWindowTextLengthW(window))+1, L'\0');
    GetWindowTextW(window, current.data(), static_cast<int>(current.size()));
    current.resize(wcslen(current.c_str()));
    if (current != value) SetWindowTextW(window, value.c_str());
}
}
PreferencesWindow::PreferencesWindow(Settings& settings, std::vector<Monitor>& displays, std::wstring& selected,
    std::function<void(PreferenceAction,int)> action)
    : settings_(settings), displays_(displays), selected_(selected), action_(std::move(action)) {}
PreferencesWindow::~PreferencesWindow() { close(); }
int PreferencesWindow::px(int value) const { return MulDiv(value, static_cast<int>(dpi_), 96); }
void PreferencesWindow::show(HWND owner) {
    if (window_) { refresh(true); ShowWindow(window_, SW_RESTORE); SetForegroundWindow(window_); return; }
    WNDCLASSEXW type{sizeof(type)};
    type.lpfnWndProc = procedure; type.hInstance = GetModuleHandleW(nullptr);
    type.lpszClassName = preferencesClass; type.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    type.hIcon = LoadIconW(type.hInstance, MAKEINTRESOURCEW(1));
    RegisterClassExW(&type);
    dpi_ = GetDpiForWindow(owner);
    MONITORINFO info{sizeof(info)}; GetMonitorInfoW(MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST), &info);
    constexpr DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME | WS_CLIPCHILDREN | WS_VSCROLL;
    RECT rect{0,0,px(520),px(640)}; AdjustWindowRectExForDpi(&rect, style, FALSE, WS_EX_APPWINDOW, dpi_);
    const auto width = std::min<LONG>(rect.right-rect.left, info.rcWork.right-info.rcWork.left);
    const auto height = std::min<LONG>(rect.bottom-rect.top, info.rcWork.bottom-info.rcWork.top);
    window_ = CreateWindowExW(WS_EX_APPWINDOW, preferencesClass, L"Screen settings", style,
        info.rcWork.left+(info.rcWork.right-info.rcWork.left-width)/2,
        info.rcWork.top+(info.rcWork.bottom-info.rcWork.top-height)/2, width, height,
        nullptr, nullptr, type.hInstance, this);
    if (window_) {
        BOOL allowed=TRUE; SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&allowed,0);
        if (!settings_.smooth || !allowed || !AnimateWindow(window_,120,AW_BLEND | AW_ACTIVATE)) ShowWindow(window_,SW_SHOW);
        SetForegroundWindow(window_);
    }
}
void PreferencesWindow::close() { if (window_) DestroyWindow(window_); }
bool PreferencesWindow::message(MSG& message) { return window_ && IsDialogMessageW(window_, &message); }
HWND PreferencesWindow::add(const wchar_t* type, const wchar_t* value, DWORD style, int id,
    int pageIndex, int x, int y, int width, int height) {
    const auto child = CreateWindowExW(0, type, value, WS_CHILD | WS_VISIBLE | style,
        0,0,0,0,window_,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
    items_.push_back({child,pageIndex,x,y,width,height}); return child;
}
void PreferencesWindow::createControls() {
    const auto label = [&](const wchar_t* value, int pageIndex, int x, int y, int width, int height, int id = 0) {
        return add(L"STATIC",value,SS_NOPREFIX,id,pageIndex,x,y,width,height);
    };
    constexpr auto button = WS_TABSTOP | BS_OWNERDRAW;
    constexpr auto combo = WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS;
    add(L"BUTTON",L"Displays",button,navBase,-1,24,94,148,36);
    add(L"BUTTON",L"Appearance",button,navBase+1,-1,186,94,148,36);
    add(L"BUTTON",L"Preferences",button,navBase+2,-1,348,94,148,36);
    label(L"Choose a screen",0,24,158,472,24);
    add(WC_COMBOBOXW,L"Display",combo,displayId,0,24,188,472,230);
    label(L"Brightness",0,24,252,280,26);
    add(L"STATIC",L"70%",SS_RIGHT | SS_NOPREFIX,percentId,0,360,240,136,38);
    const auto slider = add(TRACKBAR_CLASSW,L"Brightness",TBS_HORZ | TBS_NOTICKS | WS_TABSTOP,levelId,0,18,286,484,42);
    SetWindowSubclass(slider,ui::sliderProcedure,1,0);
    SendMessageW(slider,TBM_SETRANGE,TRUE,MAKELPARAM(minBrightness,maxBrightness));
    SendMessageW(slider,TBM_SETPAGESIZE,0,5);
    for (int i=0;i<4;++i) add(L"BUTTON",(std::to_wstring(presets[i])+L"%").c_str(),button,presetBase+i,0,24+i*122,342,106,38);
    label(L"All displays uses desktop dimming for full-screen coverage. Different screen levels use compatibility overlays.",0,24,398,472,48,coverageId);
    add(L"BUTTON",L"Pause dimming",button,pauseId,0,24,466,228,40);
    add(L"BUTTON",L"Pause for 15 minutes",button,timerId,0,268,466,228,40);
    add(L"BUTTON",L"Restore all displays",button,restoreId,0,24,520,228,40);
    add(L"BUTTON",L"Identify screens",button,identifyId,0,268,520,228,40);

    label(L"Make Screen feel like yours",1,24,158,472,24);
    label(L"Choose a palette for the flyout and settings. Windows high contrast always takes priority.",1,24,190,472,40);
    add(L"BUTTON",ui::themeNames[0],button,themeBase,1,24,240,472,58);
    for (int i=1;i<=6;++i) add(L"BUTTON",ui::themeNames[static_cast<size_t>(i)],button,themeBase+i,1,
        24+((i-1)%2)*244,312+((i-1)/2)*96,228,82);

    add(L"BUTTON",L"Start with Windows",BS_AUTOCHECKBOX | WS_TABSTOP,startupId,2,24,160,472,38);
    add(L"BUTTON",L"Resume saved dimming when Screen opens",BS_AUTOCHECKBOX | WS_TABSTOP,resumeId,2,24,214,472,38);
    add(L"BUTTON",L"Smooth transitions",BS_AUTOCHECKBOX | WS_TABSTOP,smoothId,2,24,268,472,38);
    add(L"BUTTON",L"Warm tint",BS_AUTOCHECKBOX | WS_TABSTOP,warmId,2,24,322,472,38);
    label(L"Adjustment step",2,24,386,228,24);
    const auto step = add(WC_COMBOBOXW,L"Adjustment step",combo,stepId,2,24,416,228,150);
    for (const auto amount : steps) {
        const auto value = std::to_wstring(amount)+L"%";
        SendMessageW(step,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(value.c_str()));
    }
    label(L"Open flyout shortcut",2,268,386,228,24);
    const auto shortcut = add(WC_COMBOBOXW,L"Open flyout shortcut",combo,shortcutId,2,268,416,228,150);
    for (const auto value : {L"Alt + S",L"Ctrl + Alt + S",L"Ctrl + Shift + S"})
        SendMessageW(shortcut,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(value));
    label(L"Checking startup registration…",2,24,478,472,48,startupStatusId);
    add(L"BUTTON",L"Windows startup settings",button,systemStartupId,2,24,536,272,36);
    label(L"Emergency restore: Ctrl + Alt + Shift + F12",2,24,590,472,24);
    fonts(); theme(); refresh(true); layout();
}
void PreferencesWindow::fonts() {
    for (auto font : {font_,titleFont_,smallFont_}) if (font) DeleteObject(font);
    const auto create = [&](int size,int weight) { return CreateFontW(-px(size),0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI"); };
    font_=create(13,FW_NORMAL); titleFont_=create(26,FW_SEMIBOLD); smallFont_=create(11,FW_NORMAL);
    for (const auto& item : items_) SendMessageW(item.window,WM_SETFONT,reinterpret_cast<WPARAM>(font_),TRUE);
    SendMessageW(GetDlgItem(window_,percentId),WM_SETFONT,reinterpret_cast<WPARAM>(titleFont_),TRUE);
    for (const auto id : {coverageId,startupStatusId}) SendMessageW(GetDlgItem(window_,id),WM_SETFONT,reinterpret_cast<WPARAM>(smallFont_),TRUE);
    for (const auto id : {displayId,stepId,shortcutId}) {
        SendMessageW(GetDlgItem(window_,id),CB_SETITEMHEIGHT,static_cast<WPARAM>(-1),px(30));
        SendMessageW(GetDlgItem(window_,id),CB_SETITEMHEIGHT,0,px(28));
    }
}
void PreferencesWindow::theme() {
    const auto next = ui::palette(settings_.theme);
    if (background_ && next == colors_ && appliedTheme_ == settings_.theme) return;
    colors_ = next; appliedTheme_ = settings_.theme;
    if (background_) DeleteObject(background_);
    background_ = CreateSolidBrush(colors_.bg); ui::windowTheme(window_,colors_);
    for (int i=0;i<7;++i) {
        auto name = std::wstring(ui::themeNames[static_cast<size_t>(i)]);
        if (i == settings_.theme) name += L" (selected)";
        setText(GetDlgItem(window_,themeBase+i),name);
    }
    RedrawWindow(window_,nullptr,nullptr,RDW_INVALIDATE | RDW_ALLCHILDREN);
}
void PreferencesWindow::layout() {
    if (layingOut_ || !window_) return;
    layingOut_ = true;
    RECT client{}; GetClientRect(window_,&client);
    const auto height = MulDiv(client.bottom,96,static_cast<int>(dpi_));
    const int content = page_ == 2 ? 638 : 604;
    scroll_ = std::clamp(scroll_,0,std::max(0,content-height));
    SCROLLINFO info{sizeof(info),SIF_RANGE | SIF_PAGE | SIF_POS,0,content-1,static_cast<UINT>(std::max(1,height)),scroll_,0};
    SetScrollInfo(window_,SB_VERT,&info,TRUE); GetClientRect(window_,&client);
    const auto width = MulDiv(client.right,96,static_cast<int>(dpi_));
    for (const auto& item : items_) {
        const auto x = 24 + MulDiv(item.x-24,std::max(1,width-48),472);
        const auto itemWidth = MulDiv(item.width,std::max(1,width-48),472);
        SetWindowPos(item.window,nullptr,px(x),px(item.y-scroll_),px(itemWidth),px(item.height),SWP_NOZORDER | SWP_NOACTIVATE);
        ShowWindow(item.window,item.page == -1 || item.page == page_ ? SW_SHOWNA : SW_HIDE);
    }
    InvalidateRect(window_,nullptr,FALSE); layingOut_ = false;
}
void PreferencesWindow::page(int index) {
    page_=std::clamp(index,0,2); scroll_=0;
    BOOL allowed=TRUE; SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&allowed,0);
    animationAlpha_=settings_.smooth && allowed ? 0 : 255;
    layout();
    if (settings_.smooth && allowed && IsWindowVisible(window_)) {
        if (!SetTimer(window_,1,15,nullptr)) animationAlpha_=255;
    }
}
void PreferencesWindow::refresh(bool rebuildDisplays) {
    if (!window_ || refreshing_) return;
    refreshing_=true;
    if (appliedTheme_ != settings_.theme || rebuildDisplays) theme();
    const auto combo=GetDlgItem(window_,displayId);
    if (rebuildDisplays) {
        SendMessageW(combo,CB_RESETCONTENT,0,0);
        SendMessageW(combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"All displays"));
        for (size_t i=0;i<displays_.size();++i) {
            const auto& display=displays_[i];
            const auto name=L"Display "+std::to_wstring(i+1)+(display.primary?L" · Primary":L"")+L" · "+
                std::to_wstring(display.bounds.right-display.bounds.left)+L" × "+std::to_wstring(display.bounds.bottom-display.bounds.top);
            SendMessageW(combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_str()));
        }
    }
    int selection=0;
    for (size_t i=0;i<displays_.size();++i) if (displays_[i].device==selected_) selection=static_cast<int>(i)+1;
    if (SendMessageW(combo,CB_GETCURSEL,0,0)!=selection) SendMessageW(combo,CB_SETCURSEL,selection,0);
    const auto level=selected_.empty()?settings_.brightness:settings_.forMonitor(selected_);
    const auto slider=GetDlgItem(window_,levelId);
    if (GetCapture()!=slider && SendMessageW(slider,TBM_GETPOS,0,0)!=level) SendMessageW(slider,TBM_SETPOS,TRUE,level);
    setText(GetDlgItem(window_,percentId),std::to_wstring(level)+L"%");
    setText(GetDlgItem(window_,pauseId),settings_.paused?L"Resume dimming":L"Pause dimming");
    for (const auto [id,checked] : {std::pair{warmId,settings_.warm}, {smoothId,settings_.smooth}, {resumeId,settings_.resumeOnLaunch}, {startupId,startup_==StartupState::on}}) {
        const auto control=GetDlgItem(window_,id);
        if (SendMessageW(control,BM_GETCHECK,0,0)!=(checked?BST_CHECKED:BST_UNCHECKED)) SendMessageW(control,BM_SETCHECK,checked?BST_CHECKED:BST_UNCHECKED,0);
    }
    int step=0; for(int i=0;i<4;++i) if(settings_.adjustmentStep==steps[i]) step=i;
    if (SendDlgItemMessageW(window_,stepId,CB_GETCURSEL,0,0)!=step) SendDlgItemMessageW(window_,stepId,CB_SETCURSEL,step,0);
    if (SendDlgItemMessageW(window_,shortcutId,CB_GETCURSEL,0,0)!=settings_.openingShortcut)
        SendDlgItemMessageW(window_,shortcutId,CB_SETCURSEL,settings_.openingShortcut,0);
    EnableWindow(GetDlgItem(window_,startupId),!startupBusy_ && startup_!=StartupState::blocked && startup_!=StartupState::unknown);
    std::wstring startupText;
    if(startupBusy_ || startup_==StartupState::unknown) startupText=L"Checking startup registration…";
    else if(startup_==StartupState::userDisabled) startupText=L"Disabled in Windows. Re-enable Screen in Windows startup settings.";
    else if(startup_==StartupState::blocked) startupText=L"Startup is managed by your Windows policy.";
    else if(startup_==StartupState::error) startupText=L"Startup could not be updated. Try again or use Windows startup settings.";
    else startupText=L"Launches quietly in the tray. Starts paused unless saved dimming is set to resume. Windows startup settings can override registration.";
    setText(GetDlgItem(window_,startupStatusId),startupText);
    refreshing_=false;
}
void PreferencesWindow::startupState(StartupState state,bool busy) { startup_=state; startupBusy_=busy; refresh(); }
void PreferencesWindow::identify() {
    clearIdentifiers();
    identifierFont_=CreateFontW(-px(64),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    for(size_t i=0;i<displays_.size();++i) {
        const auto& rect=displays_[i].bounds; const auto size=px(128);
        const auto number=std::to_wstring(i+1);
        const auto window=CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            L"STATIC",number.c_str(),WS_POPUP | SS_CENTER | SS_CENTERIMAGE | WS_BORDER,
            rect.left+(rect.right-rect.left-size)/2,rect.top+(rect.bottom-rect.top-size)/2,size,size,
            window_,nullptr,GetModuleHandleW(nullptr),nullptr);
        if(window) { SendMessageW(window,WM_SETFONT,reinterpret_cast<WPARAM>(identifierFont_),TRUE); ShowWindow(window,SW_SHOWNOACTIVATE); identifiers_.push_back(window); }
    }
    if(!SetTimer(window_,2,2000,nullptr)) clearIdentifiers();
}
void PreferencesWindow::clearIdentifiers() {
    for(const auto window:identifiers_) DestroyWindow(window);
    identifiers_.clear();
    if(identifierFont_) { DeleteObject(identifierFont_); identifierFont_=nullptr; }
    if(window_) KillTimer(window_,2);
}
void PreferencesWindow::command(int id,int notification) {
    if(refreshing_) {
        // Accessibility/automation can send commands while a native control is
        // refreshing. Defer that input instead of silently dropping the action.
        PostMessageW(window_,WM_COMMAND,MAKEWPARAM(id,notification),0);
        return;
    }
    if(id>=navBase && id<navBase+3 && notification==BN_CLICKED) { page(id-navBase); return; }
    if(id==displayId && notification==CBN_SELCHANGE) action_(PreferenceAction::display,static_cast<int>(SendDlgItemMessageW(window_,id,CB_GETCURSEL,0,0)));
    else if(id==stepId && notification==CBN_SELCHANGE) {
        const auto index=static_cast<int>(SendDlgItemMessageW(window_,id,CB_GETCURSEL,0,0));
        if(index>=0 && index<4) action_(PreferenceAction::step,steps[index]);
    } else if(id==shortcutId && notification==CBN_SELCHANGE) action_(PreferenceAction::shortcut,static_cast<int>(SendDlgItemMessageW(window_,id,CB_GETCURSEL,0,0)));
    else if(notification==BN_CLICKED) {
        if(id>=themeBase && id<themeBase+7) action_(PreferenceAction::theme,id-themeBase);
        else if(id>=presetBase && id<presetBase+4) action_(PreferenceAction::brightness,presets[id-presetBase]);
        else if(id==pauseId) action_(PreferenceAction::pause,0);
        else if(id==timerId) action_(PreferenceAction::timedPause,0);
        else if(id==restoreId) action_(PreferenceAction::restore,0);
        else if(id==identifyId) identify();
        else if(id==systemStartupId) ShellExecuteW(window_,L"open",L"ms-settings:startupapps",nullptr,nullptr,SW_SHOWNORMAL);
        else {
            const auto checked=SendDlgItemMessageW(window_,id,BM_GETCHECK,0,0)==BST_CHECKED;
            if(id==warmId) action_(PreferenceAction::warm,checked);
            if(id==smoothId) action_(PreferenceAction::smooth,checked);
            if(id==resumeId) action_(PreferenceAction::resume,checked);
            if(id==startupId) action_(PreferenceAction::startup,checked);
        }
    }
    refresh();
}
void PreferencesWindow::drawButton(const DRAWITEMSTRUCT& draw) {
    const auto dc=draw.hDC; auto rect=draw.rcItem; const int id=static_cast<int>(draw.CtlID);
    const auto font=SelectObject(dc,font_); FillRect(dc,&rect,background_);
    if(draw.CtlType==ODT_COMBOBOX) {
        if(draw.itemState & ODS_SELECTED) ui::roundFill(dc,rect,px(4),colors_.track);
        const auto length=SendMessageW(draw.hwndItem,CB_GETLBTEXTLEN,draw.itemID,0);
        if(length>=0 && length<32768) {
            std::wstring value(static_cast<size_t>(length)+1,L'\0');
            SendMessageW(draw.hwndItem,CB_GETLBTEXT,draw.itemID,reinterpret_cast<LPARAM>(value.data()));
            value.resize(static_cast<size_t>(length)); rect.left+=px(10); ui::text(dc,rect,value,colors_.fg,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        }
    } else if(id>=themeBase && id<themeBase+7) {
        const int index=id-themeBase; const auto preview=ui::palette(index);
        ui::roundFill(dc,rect,px(14),settings_.theme==index?colors_.accent:colors_.border);
        InflateRect(&rect,-px(2),-px(2)); ui::roundFill(dc,rect,px(12),preview.bg);
        auto name=rect; name.left+=px(14);
        if(index!=0) { name.top+=px(8); name.bottom=name.top+px(25); }
        ui::text(dc,name,ui::themeNames[static_cast<size_t>(index)],preview.fg);
        int x=index==0?rect.right-px(150):rect.left+px(14);
        const auto swatchY=index==0?(rect.top+rect.bottom)/2-px(6):rect.bottom-px(24);
        for(const auto color:{preview.accent,preview.fg,preview.muted,preview.track}) {
            ui::roundFill(dc,{x,swatchY,x+px(20),swatchY+px(12)},px(6),color); x+=px(28);
        }
        if(settings_.theme==index) ui::roundFill(dc,{rect.right-px(27),rect.top+px(12),rect.right-px(15),rect.top+px(24)},px(12),preview.accent);
    } else {
        const bool nav=id>=navBase && id<navBase+3;
        const bool active=nav && id-navBase==page_;
        auto fill=colors_.border;
        if(draw.itemState & ODS_SELECTED) fill=colors_.track;
        else if(active) fill=RGB(
            GetRValue(colors_.border)+MulDiv(GetRValue(colors_.track)-GetRValue(colors_.border),animationAlpha_,255),
            GetGValue(colors_.border)+MulDiv(GetGValue(colors_.track)-GetGValue(colors_.border),animationAlpha_,255),
            GetBValue(colors_.border)+MulDiv(GetBValue(colors_.track)-GetBValue(colors_.border),animationAlpha_,255));
        ui::roundFill(dc,rect,px(10),fill);
        auto label=std::wstring(static_cast<size_t>(GetWindowTextLengthW(draw.hwndItem))+1,L'\0');
        GetWindowTextW(draw.hwndItem,label.data(),static_cast<int>(label.size())); label.resize(wcslen(label.c_str()));
        ui::text(dc,rect,label,active?colors_.accent:colors_.fg,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    }
    if((draw.itemState & ODS_FOCUS) && !(draw.itemState & ODS_NOFOCUSRECT)) {
        auto focus=draw.rcItem; InflateRect(&focus,-px(4),-px(4)); DrawFocusRect(dc,&focus);
    }
    SelectObject(dc,font);
}
LRESULT PreferencesWindow::customDraw(NMCUSTOMDRAW& draw) {
    if(draw.dwDrawStage!=CDDS_PREPAINT) return CDRF_DODEFAULT;
    const int id=GetDlgCtrlID(draw.hdr.hwndFrom);
    if(id==levelId) { ui::drawSlider(draw.hdr.hwndFrom,draw.hdc,colors_,dpi_); return CDRF_SKIPDEFAULT; }
    if(id>=warmId && id<=startupId) {
        const auto dc=draw.hdc; auto rect=draw.rc; FillRect(dc,&rect,background_);
        const bool checked=SendMessageW(draw.hdr.hwndFrom,BM_GETCHECK,0,0)==BST_CHECKED;
        const bool enabled=IsWindowEnabled(draw.hdr.hwndFrom)!=FALSE;
        RECT box{rect.left+px(2),rect.top+px(9),rect.left+px(24),rect.top+px(31)};
        ui::roundFill(dc,box,px(7),checked?colors_.accent:colors_.border);
        if(checked) {
            const auto pen=CreatePen(PS_SOLID,px(2),colors_.bg); const auto previous=SelectObject(dc,pen);
            MoveToEx(dc,box.left+px(5),box.top+px(11),nullptr); LineTo(dc,box.left+px(9),box.top+px(15)); LineTo(dc,box.left+px(17),box.top+px(7));
            SelectObject(dc,previous); DeleteObject(pen);
        }
        wchar_t value[128]{}; GetWindowTextW(draw.hdr.hwndFrom,value,128); rect.left+=px(38);
        const auto previous=SelectObject(dc,font_); ui::text(dc,rect,value,enabled?colors_.fg:colors_.muted); SelectObject(dc,previous);
        if(GetFocus()==draw.hdr.hwndFrom && !(SendMessageW(draw.hdr.hwndFrom,WM_QUERYUISTATE,0,0)&UISF_HIDEFOCUS)) DrawFocusRect(dc,&rect);
        return CDRF_SKIPDEFAULT;
    }
    return CDRF_DODEFAULT;
}
void PreferencesWindow::paint(HDC dc) {
    RECT rect{}; GetClientRect(window_,&rect); FillRect(dc,&rect,background_);
    const auto previous=SelectObject(dc,titleFont_);
    ui::text(dc,{px(24),px(20-scroll_),rect.right-px(24),px(56-scroll_)},L"Screen settings",colors_.fg);
    SelectObject(dc,smallFont_);
    ui::text(dc,{px(24),px(60-scroll_),rect.right-px(24),px(82-scroll_)},L"Your screens. Your comfort. Everything stays local.",colors_.muted);
    SelectObject(dc,previous);
}
LRESULT CALLBACK PreferencesWindow::procedure(HWND window,UINT message,WPARAM wParam,LPARAM lParam) {
    auto* self=reinterpret_cast<PreferencesWindow*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(message==WM_NCCREATE) {
        self=static_cast<PreferencesWindow*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        self->window_=window; SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
    }
    if(!self) return DefWindowProcW(window,message,wParam,lParam);
    switch(message) {
    case WM_CREATE: self->dpi_=GetDpiForWindow(window); self->createControls(); return 0;
    case WM_SIZE: self->layout(); return 0;
    case WM_GETMINMAXINFO: {
        auto* limits=reinterpret_cast<MINMAXINFO*>(lParam); limits->ptMinTrackSize={self->px(440),self->px(320)}; return 0;
    }
    case WM_DPICHANGED: {
        self->dpi_=HIWORD(wParam); const auto* rect=reinterpret_cast<RECT*>(lParam);
        SetWindowPos(window,nullptr,rect->left,rect->top,rect->right-rect->left,rect->bottom-rect->top,SWP_NOZORDER|SWP_NOACTIVATE);
        self->fonts(); self->layout(); return 0;
    }
    case WM_VSCROLL: {
        SCROLLINFO info{sizeof(info),SIF_ALL}; GetScrollInfo(window,SB_VERT,&info);
        switch(LOWORD(wParam)) {
        case SB_LINEUP:self->scroll_-=28;break; case SB_LINEDOWN:self->scroll_+=28;break;
        case SB_PAGEUP:self->scroll_-=static_cast<int>(info.nPage);break; case SB_PAGEDOWN:self->scroll_+=static_cast<int>(info.nPage);break;
        case SB_THUMBTRACK:self->scroll_=info.nTrackPos;break;
        }
        self->layout(); return 0;
    }
    case WM_MOUSEWHEEL: self->scroll_-=GET_WHEEL_DELTA_WPARAM(wParam)/WHEEL_DELTA*56; self->layout(); return 0;
    case WM_COMMAND: if(LOWORD(wParam)==IDCANCEL) self->close(); else self->command(LOWORD(wParam),HIWORD(wParam)); return 0;
    case WM_HSCROLL:
        if(reinterpret_cast<HWND>(lParam)==GetDlgItem(window,levelId)) self->action_(PreferenceAction::brightness,
            static_cast<int>(SendMessageW(reinterpret_cast<HWND>(lParam),TBM_GETPOS,0,0)) | (LOWORD(wParam)==TB_THUMBTRACK?0x10000:0));
        return 0;
    case WM_NOTIFY:
        if(reinterpret_cast<NMHDR*>(lParam)->code==NM_CUSTOMDRAW) return self->customDraw(*reinterpret_cast<NMCUSTOMDRAW*>(lParam));
        break;
    case ui::printSlider: ui::drawSlider(reinterpret_cast<HWND>(lParam),reinterpret_cast<HDC>(wParam),self->colors_,self->dpi_); return 0;
    case WM_DRAWITEM: self->drawButton(*reinterpret_cast<DRAWITEMSTRUCT*>(lParam)); return TRUE;
    case WM_MEASUREITEM: reinterpret_cast<MEASUREITEMSTRUCT*>(lParam)->itemHeight=static_cast<UINT>(self->px(28)); return TRUE;
    case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX: case WM_CTLCOLORBTN:
        SetTextColor(reinterpret_cast<HDC>(wParam),self->colors_.fg); SetBkColor(reinterpret_cast<HDC>(wParam),self->colors_.bg);
        return reinterpret_cast<LRESULT>(self->background_);
    case WM_ERASEBKGND: return 1;
    case WM_PRINTCLIENT: self->paint(reinterpret_cast<HDC>(wParam)); return 0;
    case WM_PAINT: {
        PAINTSTRUCT paint{}; const auto dc=BeginPaint(window,&paint); self->paint(dc); EndPaint(window,&paint); return 0;
    }
    case WM_TIMER:
        if(wParam==1) {
            self->animationAlpha_=std::min(255,self->animationAlpha_+36);
            InvalidateRect(GetDlgItem(window,navBase+self->page_),nullptr,FALSE);
            if(self->animationAlpha_==255) KillTimer(window,1);
        } else if(wParam==2) self->clearIdentifiers();
        return 0;
    case WM_SETTINGCHANGE: case WM_THEMECHANGED: self->theme(); return 0;
    case WM_CLOSE: self->close(); return 0;
    case WM_DESTROY:
        KillTimer(window,1); self->clearIdentifiers();
        for(auto font:{self->font_,self->titleFont_,self->smallFont_}) if(font) DeleteObject(font);
        self->font_=self->titleFont_=self->smallFont_=nullptr;
        if(self->background_) DeleteObject(self->background_); self->background_=nullptr;
        self->items_.clear(); self->items_.shrink_to_fit(); self->window_=nullptr; self->appliedTheme_=-1; return 0;
    }
    return DefWindowProcW(window,message,wParam,lParam);
}
}
