#include "ui.hpp"
#include "core.hpp"
#include <dwmapi.h>
#include <algorithm>
#include <string>
#include <new>
namespace screen::ui {
namespace {
constexpr UINT_PTR hoverTimer=0x5343;
struct Interaction { screen::InteractionMotion motion; ULONGLONG last=0; bool hot=false,focus=false; };
bool interaction(HWND window,UINT message,WPARAM wp,LPARAM lp,SUBCLASSPROC procedure,DWORD_PTR raw) {
    auto* state=reinterpret_cast<Interaction*>(raw);
    if(message==WM_NCDESTROY) { KillTimer(window,hoverTimer);RemoveWindowSubclass(window,procedure,1);delete state;return false; }
    if(message==WM_TIMER && wp==hoverTimer) {
        if(!state) { KillTimer(window,hoverTimer);return true; }
        const auto now=GetTickCount64();
        bool moving=false;
        if(!IsWindowVisible(window) || !SendMessageW(GetParent(window),motionAllowed,0,0)) state->motion.snap();
        else moving=state->motion.advance(static_cast<double>(now-state->last)/1000.0);
        state->last=now;InvalidateRect(window,nullptr,FALSE);
        if(!moving) KillTimer(window,hoverTimer);
        return true;
    }
    if(message!=WM_MOUSEMOVE && message!=WM_MOUSELEAVE && message!=WM_SETFOCUS && message!=WM_KILLFOCUS &&
        message!=WM_ENABLE && message!=WM_SHOWWINDOW && message!=WM_CANCELMODE) return false;
    if(!state && (message==WM_MOUSEMOVE || message==WM_SETFOCUS)) {
        state=new(std::nothrow) Interaction;
        if(state && !SetWindowSubclass(window,procedure,1,reinterpret_cast<DWORD_PTR>(state))) {delete state;state=nullptr;}
    }
    if(!state) return false;
    if(message==WM_MOUSEMOVE && !state->hot) {
        state->hot=true;TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,window,0};TrackMouseEvent(&track);
    }
    if(message==WM_MOUSELEAVE) state->hot=false;
    if(message==WM_SETFOCUS) state->focus=true;
    if(message==WM_KILLFOCUS) state->focus=false;
    const bool hidden=message==WM_SHOWWINDOW && !wp;
    if(hidden || message==WM_CANCELMODE) state->hot=state->focus=false;
    const auto target=IsWindowEnabled(window) && !hidden?(state->hot?1.0:state->focus?0.55:0.0):0.0;
    if(target==state->motion.target && !hidden && message!=WM_CANCELMODE) return false;
    state->motion.target=target;
    BOOL allowed=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&allowed,0);
    if(!allowed || !SendMessageW(GetParent(window),motionAllowed,0,0) || hidden || message==WM_CANCELMODE) {
        state->motion.snap();KillTimer(window,hoverTimer);
    } else {
        state->last=GetTickCount64();
        if(!SetTimer(window,hoverTimer,15,nullptr)) state->motion.snap();
    }
    InvalidateRect(window,nullptr,FALSE);(void)lp;return false;
}
}
double interactionAmount(HWND window) {
    DWORD_PTR value=0;
    for(auto procedure:{buttonProcedure,sliderProcedure,comboProcedure})
        if(GetWindowSubclass(window,procedure,1,&value) && value) return reinterpret_cast<Interaction*>(value)->motion.value;
    return 0;
}
Palette palette(int theme) {
    HIGHCONTRASTW contrast{sizeof(contrast)};
    SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0);
    if (contrast.dwFlags & HCF_HIGHCONTRASTON)
        return {GetSysColor(COLOR_WINDOW), GetSysColor(COLOR_WINDOWTEXT), GetSysColor(COLOR_WINDOWTEXT),
            GetSysColor(COLOR_HIGHLIGHT), GetSysColor(COLOR_GRAYTEXT), GetSysColor(COLOR_WINDOWTEXT), false, true};
    if(!theme) {
        DWORD light=1,size=sizeof(light);
        RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&light,&size);
        theme=light?1:2;
    }
    switch (theme) {
    case 1: return {RGB(251,252,255),RGB(30,36,52),RGB(94,102,120),RGB(100,76,209),RGB(222,225,236),RGB(229,232,241),false};
    case 3: return {RGB(29,31,35),RGB(244,246,250),RGB(177,185,200),RGB(202,218,241),RGB(65,71,84),RGB(53,58,69),true};
    case 4: return {RGB(13,27,41),RGB(237,248,255),RGB(163,195,216),RGB(112,211,247),RGB(39,65,87),RGB(35,53,73),true};
    case 5: return {RGB(16,32,28),RGB(241,250,245),RGB(164,199,181),RGB(142,223,180),RGB(44,73,61),RGB(37,58,49),true};
    case 6: return {RGB(39,26,33),RGB(255,246,239),RGB(216,183,178),RGB(251,179,151),RGB(80,53,66),RGB(64,43,54),true};
    default: return {RGB(21,23,31),RGB(245,245,252),RGB(174,180,200),RGB(188,171,255),RGB(53,54,73),RGB(44,46,62),true};
    }
}
COLORREF blend(COLORREF from,COLORREF to,double amount) {
    const auto mix=[&](BYTE a,BYTE b){return static_cast<BYTE>(a+(b-a)*std::clamp(amount,0.0,1.0)+0.5);};
    return RGB(mix(GetRValue(from),GetRValue(to)),mix(GetGValue(from),GetGValue(to)),mix(GetBValue(from),GetBValue(to)));
}
Palette blend(const Palette& from,const Palette& to,double amount) {
    return {blend(from.bg,to.bg,amount),blend(from.fg,to.fg,amount),blend(from.muted,to.muted,amount),
        blend(from.accent,to.accent,amount),blend(from.track,to.track,amount),blend(from.border,to.border,amount),to.dark,to.highContrast};
}
void windowTheme(HWND window, const Palette& colors) {
    const BOOL dark = colors.dark;
    DwmSetWindowAttribute(window, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    DwmSetWindowAttribute(window, DWMWA_BORDER_COLOR, &colors.border, sizeof(colors.border));
    const COLORREF caption=colors.highContrast?DWMWA_COLOR_DEFAULT:colors.bg;
    const COLORREF textColor=colors.highContrast?DWMWA_COLOR_DEFAULT:colors.fg;
    DwmSetWindowAttribute(window,DWMWA_CAPTION_COLOR,&caption,sizeof(caption));
    DwmSetWindowAttribute(window,DWMWA_TEXT_COLOR,&textColor,sizeof(textColor));
}
void roundFill(HDC dc, RECT rect, int radius, COLORREF color) {
    const auto brush = CreateSolidBrush(color);
    const auto previousBrush = SelectObject(dc, brush), previousPen = SelectObject(dc, GetStockObject(NULL_PEN));
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(dc, previousPen); SelectObject(dc, previousBrush); DeleteObject(brush);
}
void text(HDC dc, RECT rect, std::wstring_view value, COLORREF color, UINT flags) {
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, color);
    DrawTextW(dc, value.data(), static_cast<int>(value.size()), &rect, flags | DT_NOPREFIX);
}
void drawSlider(HWND slider, HDC target, const Palette& sourceColors, UINT dpi) {
    auto colors=sourceColors;
    if(!IsWindowEnabled(slider)) colors.accent=blend(colors.bg,colors.muted,0.5);
    RECT client{}, channel{}, thumb{}; GetClientRect(slider, &client);
    if (!client.right || !client.bottom) return;
    SendMessageW(slider, TBM_GETCHANNELRECT, 0, reinterpret_cast<LPARAM>(&channel));
    SendMessageW(slider, TBM_GETTHUMBRECT, 0, reinterpret_cast<LPARAM>(&thumb));
    const auto buffer = CreateCompatibleDC(target);
    const auto bitmap = CreateCompatibleBitmap(target, client.right, client.bottom);
    const auto dc = buffer && bitmap ? buffer : target;
    const auto previous = buffer && bitmap ? SelectObject(dc, bitmap) : nullptr;
    const auto px = [&](int value) { return MulDiv(value, static_cast<int>(dpi), 96); };
    const auto brush = CreateSolidBrush(colors.bg); FillRect(dc, &client, brush); DeleteObject(brush);
    const auto center = (thumb.left + thumb.right) / 2, y = client.bottom / 2;
    roundFill(dc, {channel.left, y-px(2), channel.right, y+px(3)}, px(5), colors.track);
    if (center > channel.left) roundFill(dc, {channel.left, y-px(2), center, y+px(3)}, px(5), colors.accent);
    const auto radius = px(8)+static_cast<int>(std::lround(px(2)*interactionAmount(slider)));
    const auto thumbBrush = CreateSolidBrush(colors.accent);
    const auto oldBrush = SelectObject(dc, thumbBrush), oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
    Ellipse(dc, center-radius, y-radius, center+radius+1, y+radius+1);
    SelectObject(dc, oldPen); SelectObject(dc, oldBrush); DeleteObject(thumbBrush);
    if(!colors.highContrast) {
        const auto centerBrush=CreateSolidBrush(colors.bg); const auto oldCenter=SelectObject(dc,centerBrush);
        const auto r=px(GetCapture()==slider?4:3); const auto noPen=SelectObject(dc,GetStockObject(NULL_PEN));
        Ellipse(dc,center-r,y-r,center+r+1,y+r+1);
        SelectObject(dc,noPen);SelectObject(dc,oldCenter);DeleteObject(centerBrush);
    }
    if (GetFocus() == slider && !(SendMessageW(slider, WM_QUERYUISTATE, 0, 0) & UISF_HIDEFOCUS)) {
        const auto pen = CreatePen(PS_SOLID, 1, colors.muted);
        const auto focusPen = SelectObject(dc, pen), focusBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
        const auto r = radius+px(3); Ellipse(dc, center-r, y-r, center+r+1, y+r+1);
        SelectObject(dc, focusBrush); SelectObject(dc, focusPen); DeleteObject(pen);
    }
    if (buffer && bitmap) { BitBlt(target, 0, 0, client.right, client.bottom, buffer, 0, 0, SRCCOPY); SelectObject(buffer, previous); }
    if (bitmap) DeleteObject(bitmap);
    if (buffer) DeleteDC(buffer);
}
LRESULT CALLBACK sliderProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR state) {
    if(interaction(window,message,wParam,lParam,sliderProcedure,state)) return 0;
    if (message == WM_ERASEBKGND) return 1;
    if (message == WM_PAINT) InvalidateRect(window, nullptr, FALSE);
    if (message == WM_PRINTCLIENT) return SendMessageW(GetParent(window), printSlider, wParam, reinterpret_cast<LPARAM>(window));
    if (message == WM_KEYDOWN && wParam == VK_ESCAPE) { PostMessageW(GetParent(window), WM_CLOSE, 0, 0); return 0; }
    if (message == WM_CONTEXTMENU) return SendMessageW(GetParent(window), message, wParam, lParam);
    if (message == WM_LBUTTONDOWN) SendMessageW(window, WM_UPDATEUISTATE, MAKEWPARAM(UIS_SET, UISF_HIDEFOCUS), 0);
    if (message == WM_KEYDOWN) SendMessageW(window, WM_UPDATEUISTATE, MAKEWPARAM(UIS_CLEAR, UISF_HIDEFOCUS), 0);
    const auto result = DefSubclassProc(window, message, wParam, lParam);
    switch (message) {
    case WM_MOUSEMOVE:
        if (wParam & MK_LBUTTON) InvalidateRect(window, nullptr, FALSE);
        break;
    case TBM_SETPOS: case TBM_SETRANGE: case TBM_SETRANGEMIN: case TBM_SETRANGEMAX:
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_CAPTURECHANGED:
    case WM_KEYDOWN: case WM_KEYUP: case WM_MOUSEWHEEL: case WM_SETFOCUS: case WM_KILLFOCUS: case WM_UPDATEUISTATE:
        InvalidateRect(window, nullptr, FALSE); break;
    }
    return result;
}
void drawCombo(HWND combo,HDC target,const Palette& colors,UINT dpi) {
    RECT rect{};GetClientRect(combo,&rect);if(!rect.right || !rect.bottom) return;
    const auto px=[&](int value){return MulDiv(value,static_cast<int>(dpi),96);};
    const auto buffer=CreateCompatibleDC(target);const auto bitmap=CreateCompatibleBitmap(target,rect.right,rect.bottom);
    const auto dc=buffer && bitmap?buffer:target;const auto oldBitmap=buffer && bitmap?SelectObject(dc,bitmap):nullptr;
    const auto background=CreateSolidBrush(colors.bg);FillRect(dc,&rect,background);DeleteObject(background);
    const bool enabled=IsWindowEnabled(combo)!=FALSE,focused=GetFocus()==combo;
    roundFill(dc,rect,px(14),focused?colors.accent:blend(colors.border,colors.accent,0.45*interactionAmount(combo)));
    InflateRect(&rect,-px(1),-px(1));
    roundFill(dc,rect,px(12),colors.highContrast?colors.bg:blend(colors.bg,colors.fg,colors.dark?0.035:0.018));
    const auto index=SendMessageW(combo,CB_GETCURSEL,0,0);
    const auto length=index>=0?SendMessageW(combo,CB_GETLBTEXTLEN,index,0):0;
    std::wstring label;
    if(length>0 && length<32768) {
        label.resize(static_cast<size_t>(length)+1);
        SendMessageW(combo,CB_GETLBTEXT,index,reinterpret_cast<LPARAM>(label.data()));label.resize(static_cast<size_t>(length));
    }
    auto textRect=rect;textRect.left+=px(12);textRect.right-=px(34);
    const auto oldFont=SelectObject(dc,reinterpret_cast<HFONT>(SendMessageW(combo,WM_GETFONT,0,0)));
    text(dc,textRect,label,enabled?colors.fg:colors.muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    SelectObject(dc,oldFont);
    const auto pen=CreatePen(PS_SOLID,std::max(1,px(1)),colors.muted);const auto oldPen=SelectObject(dc,pen);
    const auto x=rect.right-px(18),y=(rect.top+rect.bottom)/2;
    MoveToEx(dc,x-px(4),y-px(2),nullptr);LineTo(dc,x,y+px(2));LineTo(dc,x+px(4),y-px(2));
    SelectObject(dc,oldPen);DeleteObject(pen);
    if(focused && !(SendMessageW(combo,WM_QUERYUISTATE,0,0)&UISF_HIDEFOCUS)) { InflateRect(&textRect,-px(2),-px(3));DrawFocusRect(dc,&textRect); }
    if(buffer && bitmap) {BitBlt(target,0,0,rect.right+px(1),rect.bottom+px(1),buffer,0,0,SRCCOPY);SelectObject(buffer,oldBitmap);}
    if(bitmap)DeleteObject(bitmap);if(buffer)DeleteDC(buffer);
}
LRESULT CALLBACK comboProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR state) {
    if(interaction(window,message,wp,lp,comboProcedure,state)) return 0;
    if(message==WM_ERASEBKGND)return 1;
    if(message==WM_PAINT) {
        PAINTSTRUCT paint{};const auto dc=BeginPaint(window,&paint);
        SendMessageW(GetParent(window),printCombo,reinterpret_cast<WPARAM>(dc),reinterpret_cast<LPARAM>(window));
        EndPaint(window,&paint);return 0;
    }
    if(message==WM_PRINTCLIENT)return SendMessageW(GetParent(window),printCombo,wp,reinterpret_cast<LPARAM>(window));
    const auto result=DefSubclassProc(window,message,wp,lp);
    switch(message) {
    case CB_SETCURSEL:case CB_RESETCONTENT:case WM_SETFONT:case WM_ENABLE:case WM_SETFOCUS:case WM_KILLFOCUS:
    case WM_KEYDOWN:case WM_KEYUP:case WM_LBUTTONDOWN:case WM_LBUTTONUP:case WM_UPDATEUISTATE:
        InvalidateRect(window,nullptr,FALSE);break;
    }
    return result;
}
LRESULT CALLBACK buttonProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR state) {
    if(interaction(window,message,wp,lp,buttonProcedure,state)) return 0;
    return DefSubclassProc(window,message,wp,lp);
}
}
