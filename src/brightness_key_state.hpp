#pragma once
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <optional>

namespace screen {
// Notifications describe levels, not key presses. The initial snapshot never dims.
class WindowsBrightnessState {
public:
    std::optional<unsigned> update(unsigned level) {
        if(level>100) return {};
        const auto previous=last_;last_=level;
        return previous && *previous!=level?last_:std::nullopt;
    }
    void clear() { last_.reset(); }
private:
    std::optional<unsigned> last_;
};
inline bool validBrightnessReportBatch(std::size_t bytes,std::uint32_t length,std::uint32_t count) {
    return bytes<=65536 && length && length<=4096 && count && count<=64 && count<=bytes/length;
}
// The only retained input state: brightness up/down. No ordinary key codes.
class BrightnessKeyState {
public:
    static constexpr unsigned up=1, down=2;
    int update(unsigned mask,std::uint64_t now) {
        mask &= up | down;
        if(mask==held_) return 0;
        held_=mask;
        direction_=mask==up?1:mask==down?-1:0;
        due_=direction_?now+delay_:0; expires_=now+3000;
        return direction_;
    }
    int repeat(std::uint64_t now) {
        if(now>=expires_) due_=0; // Lost releases cannot repeat indefinitely.
        if(!due_ || now<due_) return 0;
        due_=now+interval_; // Never replay a backlog after a blocked UI thread.
        return direction_;
    }
    void timing(unsigned delay,unsigned interval) {
        delay_=std::clamp(delay,250u,1000u); interval_=std::clamp(interval,30u,400u);
    }
    bool repeating() const { return due_!=0; }
    void clear() { held_=0; direction_=0; due_=expires_=0; }
private:
    unsigned held_=0,delay_=500,interval_=80;
    int direction_=0;
    std::uint64_t due_=0,expires_=0;
};
}
