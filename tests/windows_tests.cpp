#include "settings.hpp"
#include "virtual_desktops.hpp"
#include "frame_timer.hpp"
#include "brightness_keys.hpp"
#include <sstream>
#include <iostream>
#include <string_view>
#include <cstring>
#include <array>
#include <wtsapi32.h>

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
int main(int argc,char** argv) {
    if(argc==2 && std::string_view(argv[1])=="--inspect-brightness-keys") {
        screen::BrightnessKeys keys;
        std::cout<<"Consumer HID collections advertising display brightness keys: "<<keys.supportedDevices()<<'\n';
        std::cout<<"Read-only capability check; no input listener, hardware change or key recording. Firmware-only Fn keys may not appear.\n";
        return 0;
    }
    if(argc==2 && std::string_view(argv[1])=="--inspect-desktops") {
        screen::VirtualDesktops desktops(nullptr);
        std::wcout<<L"Detected desktops: "<<desktops.items().size()<<L'\n';
        for(const auto& item:desktops.items()) std::wcout<<(item.id==desktops.current()?L"* ":L"  ")<<item.name<<L'\n';
        return desktops.current().empty()?2:0;
    }
    int failures=0;
    const auto check=[&](bool ok,const char* text){if(!ok){std::cerr<<text<<'\n';++failures;}};
    using namespace screen;
    const std::wstring a=L"{11111111-1111-1111-1111-111111111111}",b=L"{22222222-2222-2222-2222-222222222222}";
    Settings saved; saved.brightness=83; saved.monitors[L"DISPLAY-A"]=47;
    check(!saved.brightnessKeys,"Brightness-key control is off by default");
    std::wistringstream keyOptIn(L"brightnessKeys=2\n");
    check(!parseSettings(keyOptIn).brightnessKeys,"Only explicit value 1 enables the input listener");
    saved.brightnessKeys=true;
    saved.brightnessKeyFeedback=false;
    saved.separateDesktops=true; saved.resumeOnLaunch=true; saved.activeDesktop=a;
    saved.setAll(18); saved.activeDesktop=b; saved.setMonitor(L"DISPLAY-A",55);
    std::wstringstream text; writeSettings(text,saved); auto loaded=parseSettings(text);
    check(loaded.brightnessKeys,"Brightness-key opt-in survives a settings round trip");
    check(!loaded.brightnessKeyFeedback,"The independent mini-panel preference survives restart");
    std::wistringstream compatibility(L"brightnessKeys=1\nwindowsBrightness=1\n");
    auto followSaved=parseSettings(compatibility);
    check(followSaved.windowsBrightness && !followSaved.brightnessKeys,"Compatibility and native input cannot run together after loading");
    std::wstringstream followText;writeSettings(followText,followSaved);
    check(parseSettings(followText).windowsBrightness,"Windows brightness compatibility survives a settings round trip");
    std::wistringstream invalidFollow(L"windowsBrightness=2\n");
    check(!parseSettings(invalidFollow).windowsBrightness,"Only explicit value 1 enables Windows brightness following");
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
    const auto brightnessRegistration=[] {
        UINT count=0;GetRegisteredRawInputDevices(nullptr,&count,sizeof(RAWINPUTDEVICE));
        std::vector<RAWINPUTDEVICE> registered(count);
        if(count && GetRegisteredRawInputDevices(registered.data(),&count,sizeof(RAWINPUTDEVICE))!=UINT(-1))
            for(const auto& value:registered) if(value.usUsagePage==0x0c && value.usUsage==1) return true;
        return false;
    };
    check(!brightnessRegistration(),"No consumer-input registration before explicit opt-in");
    {
        BrightnessKeys keys;
        check(!keys.start(nullptr) && !brightnessRegistration(),"Invalid owner cannot register background input");
        check(keys.start(window) && brightnessRegistration(),"Opt-in registers the consumer collection in this process");
        check(keys.input(nullptr,100).direction==0 && !keys.repeating(),"Invalid raw-input handles cannot change brightness or start repeats");
        keys.clear();keys.removed(reinterpret_cast<HANDLE>(1));
        check(!keys.repeating(),"Unknown unplug and explicit reset are safe");
    }
    check(!brightnessRegistration(),"Disabling removes the consumer-input registration");
    DWORD keyBefore=0,keyAfter=0;GetProcessHandleCount(GetCurrentProcess(),&keyBefore);
    for(int i=0;i<20;++i) { BrightnessKeys keys;check(keys.start(window),"Repeated input enable must succeed"); }
    GetProcessHandleCount(GetCurrentProcess(),&keyAfter);
    check(keyAfter<=keyBefore+2 && !brightnessRegistration(),"Repeated enable/disable releases module, session and registration resources");
    {
        WindowsBrightness follow;
        check(!follow.start(nullptr),"An invalid owner cannot register brightness notifications");
        check(follow.start(window),"Compatibility mode registers Windows brightness notifications");
        alignas(POWERBROADCAST_SETTING) std::array<BYTE,sizeof(POWERBROADCAST_SETTING)+sizeof(DWORD)> bytes{};
        auto& event=*reinterpret_cast<POWERBROADCAST_SETTING*>(bytes.data());
        event.PowerSetting={0x8ffee2c6,0x2d01,0x46be,{0xad,0xb9,0x39,0x8a,0xdd,0xc5,0xb4,0xff}};
        event.DataLength=sizeof(DWORD);
        const auto level=[&](DWORD value){std::memcpy(event.Data,&value,sizeof(value));return follow.input(event);};
        check(!level(40) && !level(40) && level(30)==30u,"Power notifications baseline once, ignore duplicates and deliver changed levels");
        event.DataLength=1;check(!level(20),"Truncated power payloads must be ignored");
        event.DataLength=sizeof(DWORD);event.PowerSetting.Data1=0;
        check(!level(20),"Unrelated power notifications cannot affect app brightness");
        event.PowerSetting.Data1=0x8ffee2c6;
        check(!level(1000) && level(20)==20u,"Malformed percentages must leave the previous baseline intact");
        follow.session(WTS_SESSION_LOCK);check(!level(10),"Lock suspends compatibility notifications");
        follow.session(WTS_SESSION_UNLOCK);check(!level(40) && level(35)==35u,"Unlock registers a new baseline without applying its snapshot");
        follow.power(PBT_APMSUSPEND);check(!level(30),"Suspend removes the brightness notification");
        follow.power(PBT_APMRESUMEAUTOMATIC);check(!level(60) && level(50)==50u,"Resume baselines before a subsequent change");
        follow.session(WTS_SESSION_LOCK);follow.power(PBT_APMRESUMEAUTOMATIC);
        check(!level(40),"Power resume cannot re-enable listening during session lock");
    }
    DWORD followBefore=0,followAfter=0;GetProcessHandleCount(GetCurrentProcess(),&followBefore);
    for(int i=0;i<20;++i) { WindowsBrightness follow;check(follow.start(window),"Repeated compatibility registration must succeed"); }
    GetProcessHandleCount(GetCurrentProcess(),&followAfter);
    check(followAfter<=followBefore+2 && !brightnessRegistration(),"Compatibility teardown must release resources without registering keyboard input");
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
        PostQuitMessage(0);
        check(frame.wait(message)==FrameTimer::Event::quit,"A quit condition must be drained before another input wait");
    }
    return failures?1:0;
}
