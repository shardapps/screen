#include "settings.hpp"
#include "virtual_desktops.hpp"
#include "frame_timer.hpp"
#include <sstream>
#include <iostream>

namespace {
int notifications=0;
LRESULT CALLBACK procedure(HWND window,UINT message,WPARAM wp,LPARAM lp) {
    if(message==screen::virtualDesktopsChanged) { ++notifications; return 0; }
    return DefWindowProcW(window,message,wp,lp);
}
void pump() {
    MSG message{};
    while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
}
}
int main() {
    int failures=0;
    const auto check=[&](bool ok,const char* text){if(!ok){std::cerr<<text<<'\n';++failures;}};
    using namespace screen;
    const std::wstring a=L"{11111111-1111-1111-1111-111111111111}",b=L"{22222222-2222-2222-2222-222222222222}";
    Settings saved; saved.brightness=83; saved.monitors[L"DISPLAY-A"]=47;
    saved.separateDesktops=true; saved.resumeOnLaunch=true; saved.activeDesktop=a;
    saved.setAll(18); saved.activeDesktop=b; saved.setMonitor(L"DISPLAY-A",55);
    std::wstringstream text; writeSettings(text,saved); auto loaded=parseSettings(text);
    check(!loaded.paused && loaded.separateDesktops && loaded.activeDesktop.empty(),"Persist options but never persist stale active desktop context");
    loaded.activeDesktop=a;
    check(loaded.currentBrightness()==18 && loaded.forMonitor(L"DISPLAY-A")==18,"Round-trip must preserve a cleared profile override map");
    loaded.activeDesktop=b;
    check(loaded.currentBrightness()==83 && loaded.forMonitor(L"DISPLAY-A")==55,"Round-trip must preserve independent per-monitor profiles");
    std::wstringstream old(L"brightness=39\nresume=1\nmonitor:DISPLAY-A=42\n");
    loaded=parseSettings(old);
    check(loaded.currentBrightness()==39 && !loaded.paused && !loaded.separateDesktops && loaded.forMonitor(L"DISPLAY-A")==42,"Existing preferences remain compatible");
    std::wstringstream invalid(L"desktop:bad:brightness=8\nseparateDesktops=1\nresume=0\n");
    loaded=parseSettings(invalid); check(loaded.desktops.empty() && loaded.paused,"Malformed desktop entries cannot create profiles or activate dimming");
    std::wstringstream oversized;
    for(unsigned i=1;i<=80;++i) {
        wchar_t id[40]{}; swprintf_s(id,L"{%08X-0000-0000-0000-000000000001}",i);
        oversized<<L"desktop:"<<id<<L":brightness=-100\n";
    }
    loaded=parseSettings(oversized);
    check(loaded.desktops.size()==maxDesktopProfiles && loaded.desktops.begin()->second.brightness==minBrightness,"Corrupt or oversized profile files remain bounded");

    // Test only a process-specific private fixture; never alter Explorer settings.
    const auto path=L"Software\\Shardapps\\Screen\\Tests\\VirtualDesktops-"+std::to_wstring(GetCurrentProcessId());
    HKEY root{};
    if(RegCreateKeyExW(HKEY_CURRENT_USER,path.c_str(),0,nullptr,0,KEY_ALL_ACCESS,nullptr,&root,nullptr)!=ERROR_SUCCESS) return 1;
    GUID ids[2]{}; CLSIDFromString(a.c_str(),&ids[0]); CLSIDFromString(b.c_str(),&ids[1]);
    RegSetValueExW(root,L"VirtualDesktopIDs",0,REG_BINARY,reinterpret_cast<const BYTE*>(ids),sizeof(ids));
    RegSetValueExW(root,L"CurrentVirtualDesktop",0,REG_BINARY,reinterpret_cast<const BYTE*>(&ids[0]),sizeof(GUID));
    HKEY name{}; const auto namePath=L"Desktops\\"+a;
    RegCreateKeyExW(root,namePath.c_str(),0,nullptr,0,KEY_SET_VALUE,nullptr,&name,nullptr);
    constexpr wchar_t label[]=L"Work & study";
    RegSetValueExW(name,L"Name",0,REG_SZ,reinterpret_cast<const BYTE*>(label),sizeof(label)); RegCloseKey(name);
    WNDCLASSW cls{};cls.lpfnWndProc=procedure;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"Screen.Workspace.Test";
    RegisterClassW(&cls);
    const auto window=CreateWindowW(cls.lpszClassName,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,cls.hInstance,nullptr);
    {
        VirtualDesktops desktops(window,path);
        check(desktops.current()==a && desktops.items().size()==2 && desktops.name()==label,"Read real GUID metadata, ordering and custom desktop names");
        for(int cycle=1;cycle<=3;++cycle) {
            const int target=cycle%2;
            RegSetValueExW(root,L"CurrentVirtualDesktop",0,REG_BINARY,reinterpret_cast<const BYTE*>(&ids[target]),sizeof(GUID));
            for(int attempt=0;attempt<100 && notifications<cycle;++attempt) { Sleep(5);pump(); }
            check(notifications==cycle,"Registry changes must deliver one coalesced event without polling");
            desktops.refresh(true);
            check(desktops.current()==(target?b:a),"Repeated desktop changes must re-arm the watch");
        }
        Sleep(100);pump();check(notifications==3,"No idle registry callback loop");
        RegDeleteValueW(root,L"CurrentVirtualDesktop");
        desktops.refresh();check(desktops.current().empty(),"Unavailable identity must use the shared baseline");
    }
    for(int i=0;i<3;++i) { VirtualDesktops warmup(window,path); }
    DWORD before=0,after=0;GetProcessHandleCount(GetCurrentProcess(),&before);
    for(int i=0;i<20;++i) { VirtualDesktops repeated(window,path); }
    GetProcessHandleCount(GetCurrentProcess(),&after);
    check(after<=before+2,"Repeated desktop observer creation must not leak registry or wait handles");
    DestroyWindow(window);RegCloseKey(root);RegDeleteTreeW(HKEY_CURRENT_USER,path.c_str());
    pump();
    {
        FrameTimer frame;
        check(!frame.handle(),"A fresh timer must allocate no idle wait handle");
        PostThreadMessageW(GetCurrentThreadId(),WM_APP+100,0,0);
        MSG message{};
        check(frame.wait(message)==FrameTimer::Event::message && message.message==WM_APP+100,"The message pump must handle normal input with no animation timer");
        frame.begin();
        if(frame.arm()) {
            const auto handle=frame.handle();
            check(frame.arm() && frame.handle()==handle,"Retargeting keeps the same scheduled timer");
            FrameTimer::Event event;
            do { event=frame.wait(message);if(event==FrameTimer::Event::message) DispatchMessageW(&message); } while(event==FrameTimer::Event::message);
            check(event==FrameTimer::Event::frame && !frame.handle(),"A frame wakes the existing thread once and disarms itself");
            check(frame.arm(),"The next frame can be scheduled");frame.cancel();
            check(!frame.handle(),"Settled or reduced-motion transitions leave no armed timer");
        } else std::cout<<"SKIP: high-resolution timer unavailable; app uses native window-timer fallback\n";
    }
    return failures?1:0;
}
