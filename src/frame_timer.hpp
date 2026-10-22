#pragma once
#include <windows.h>
#include <dwmapi.h>
#include <algorithm>

namespace screen {
// One high-resolution, one-shot timer per animating thread. No timer-resolution
// changes, spin wait, additional thread or steady-state wake-up.
class FrameTimer {
public:
    enum class Event { message, frame, quit, error };
    ~FrameTimer() { cancel(); if(timer_) CloseHandle(timer_); }
    FrameTimer() = default;
    FrameTimer(const FrameTimer&) = delete;
    FrameTimer& operator=(const FrameTimer&) = delete;
    void begin() {
        DWM_TIMING_INFO timing{sizeof(timing)};
        double rate=60;
        if(SUCCEEDED(DwmGetCompositionTimingInfo(nullptr,&timing)) && timing.rateRefresh.uiNumerator && timing.rateRefresh.uiDenominator)
            rate=static_cast<double>(timing.rateRefresh.uiNumerator)/timing.rateRefresh.uiDenominator;
        interval_=static_cast<LONGLONG>(10000000.0/std::clamp(rate,30.0,144.0));
    }
    bool arm() {
        if(armed_) return true; // Retargeting cannot postpone an existing frame.
        if(!available_) return false;
        if(!timer_) timer_=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_MODIFY_STATE | SYNCHRONIZE);
        LARGE_INTEGER due{};due.QuadPart=-interval_;
        if(!timer_ || !SetWaitableTimerEx(timer_,&due,0,nullptr,nullptr,nullptr,0)) { available_=false;return false; }
        armed_=true;return true;
    }
    void cancel() { if(timer_) CancelWaitableTimer(timer_);armed_=false; }
    HANDLE handle() const { return armed_?timer_:nullptr; }
    Event wait(MSG& message) {
        for(;;) {
            // WM_QUIT is a queue condition rather than a normal posted input.
            // Drain it (and sent messages) before sleeping again after dispatch.
            if(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) return message.message==WM_QUIT?Event::quit:Event::message;
            const auto timer=handle();
            const auto result=MsgWaitForMultipleObjectsEx(timer?1:0,timer?&timer:nullptr,INFINITE,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
            if(result==WAIT_FAILED) return Event::error;
            if(timer && result==WAIT_OBJECT_0) { armed_=false;return Event::frame; }
        }
    }
private:
    HANDLE timer_{};
    LONGLONG interval_=166667;
    bool armed_=false,available_=true;
};
}
