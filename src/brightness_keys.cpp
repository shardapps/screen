#include "brightness_keys.hpp"
#include <hidsdi.h>
#include <wtsapi32.h>
#include <array>
#include <cstring>
#include <map>
#include <vector>

namespace screen {
namespace {
constexpr USHORT consumerPage=0x0c,consumerControl=1,brightnessUp=0x6f,brightnessDown=0x70;
constexpr UINT maxPacket=65536,maxPreparsed=65536,maxDevices=16,maxCaps=512,maxUsages=512;
}
struct BrightnessKeys::Impl {
    struct Device {
        std::vector<BYTE> preparsed;
        std::map<UCHAR,unsigned> reports;
        std::vector<USAGE> usages;
        USHORT reportBytes=0;
        bool supported=false;
        PHIDP_PREPARSED_DATA data() { return reinterpret_cast<PHIDP_PREPARSED_DATA>(preparsed.data()); }
    };
    HMODULE hid{};
    HWND owner{};
    bool registered=false,session=false;
    decltype(&HidP_GetCaps) getCaps{};
    decltype(&HidP_GetButtonCaps) getButtons{};
    decltype(&HidP_GetUsages) getUsages{};
    decltype(&HidP_MaxUsageListLength) maxUsageLength{};
    std::map<HANDLE,Device> devices;
    std::vector<BYTE> packet;
    BrightnessKeyState state;
    ~Impl() {
        if(registered) {
            RAWINPUTDEVICE device{consumerPage,consumerControl,RIDEV_REMOVE,nullptr};
            RegisterRawInputDevices(&device,1,sizeof(device));
        }
        if(session) WTSUnRegisterSessionNotification(owner);
        if(hid) FreeLibrary(hid);
    }
    template<class T> bool proc(T& target,const char* name) {
        const auto address=GetProcAddress(hid,name);
        static_assert(sizeof(target)==sizeof(address));
        std::memcpy(&target,&address,sizeof(target));
        return target!=nullptr;
    }
    bool load() {
        if(hid) return getCaps && getButtons && getUsages && maxUsageLength;
        hid=LoadLibraryExW(L"hid.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        return hid && proc(getCaps,"HidP_GetCaps") && proc(getButtons,"HidP_GetButtonCaps") &&
            proc(getUsages,"HidP_GetUsages") && proc(maxUsageLength,"HidP_MaxUsageListLength");
    }
    Device* device(HANDLE handle) {
        if(auto it=devices.find(handle);it!=devices.end()) return &it->second;
        if(!handle || devices.size()>=maxDevices) return nullptr;
        auto& result=devices[handle]; // Negative results are cached too.
        RID_DEVICE_INFO info{}; info.cbSize=sizeof(info); UINT infoBytes=sizeof(info);
        if(GetRawInputDeviceInfoW(handle,RIDI_DEVICEINFO,&info,&infoBytes)==UINT(-1) ||
            info.dwType!=RIM_TYPEHID || info.hid.usUsagePage!=consumerPage || info.hid.usUsage!=consumerControl) return &result;
        UINT bytes=0;
        if(GetRawInputDeviceInfoW(handle,RIDI_PREPARSEDDATA,nullptr,&bytes)==UINT(-1) || !bytes || bytes>maxPreparsed) return &result;
        result.preparsed.resize(bytes);
        if(GetRawInputDeviceInfoW(handle,RIDI_PREPARSEDDATA,result.preparsed.data(),&bytes)==UINT(-1)) return &result;
        HIDP_CAPS caps{};
        if(getCaps(result.data(),&caps)!=HIDP_STATUS_SUCCESS || !caps.InputReportByteLength || caps.InputReportByteLength>4096 ||
            !caps.NumberInputButtonCaps || caps.NumberInputButtonCaps>maxCaps) return &result;
        std::vector<HIDP_BUTTON_CAPS> buttons(caps.NumberInputButtonCaps); auto count=caps.NumberInputButtonCaps;
        if(getButtons(HidP_Input,buttons.data(),&count,result.data())!=HIDP_STATUS_SUCCESS) return &result;
        for(USHORT i=0;i<count;++i) {
            const auto& button=buttons[i];
            if(button.UsagePage!=consumerPage) continue;
            const auto includes=[&](USHORT usage) { return button.IsRange?
                button.Range.UsageMin<=usage && usage<=button.Range.UsageMax:button.NotRange.Usage==usage; };
            if(includes(brightnessUp) || includes(brightnessDown)) result.reports.emplace(button.ReportID,0);
        }
        const auto usageCount=maxUsageLength(HidP_Input,consumerPage,result.data());
        if(result.reports.empty() || !usageCount || usageCount>maxUsages) return &result;
        result.usages.resize(usageCount); result.reportBytes=caps.InputReportByteLength; result.supported=true;
        return &result;
    }
    unsigned held() const {
        unsigned mask=0;
        for(const auto& [handle,device]:devices) { (void)handle;for(const auto& [id,value]:device.reports) { (void)id;mask|=value; } }
        return mask;
    }
    void clear() {
        state.clear();
        for(auto& [handle,device]:devices) { (void)handle;for(auto& [id,value]:device.reports) { (void)id;value=0; } }
    }
};
BrightnessKeys::BrightnessKeys() : impl_(std::make_unique<Impl>()) {}
BrightnessKeys::~BrightnessKeys()=default;
bool BrightnessKeys::start(HWND owner) {
    if(impl_->registered) return impl_->owner==owner;
    if(!IsWindow(owner) || !impl_->load()) return false;
    RAWINPUTDEVICE device{consumerPage,consumerControl,RIDEV_INPUTSINK | RIDEV_DEVNOTIFY,owner};
    if(!RegisterRawInputDevices(&device,1,sizeof(device))) return false;
    impl_->owner=owner; impl_->registered=true;
    impl_->session=WTSRegisterSessionNotification(owner,NOTIFY_FOR_THIS_SESSION)!=FALSE;
    unsigned delay=1,speed=15;
    SystemParametersInfoW(SPI_GETKEYBOARDDELAY,0,&delay,0); SystemParametersInfoW(SPI_GETKEYBOARDSPEED,0,&speed,0);
    impl_->state.timing(250+250*std::min(delay,3u),static_cast<unsigned>(1000.0/(2.5+27.5*std::min(speed,31u)/31.0)));
    return true;
}
BrightnessKeyEvent BrightnessKeys::input(HRAWINPUT input,std::uint64_t now) {
    BrightnessKeyEvent result;
    if(!impl_->registered) return result;
    UINT bytes=0;
    if(GetRawInputData(input,RID_INPUT,nullptr,&bytes,sizeof(RAWINPUTHEADER))==UINT(-1) || bytes<sizeof(RAWINPUTHEADER)+8 || bytes>maxPacket) return result;
    impl_->packet.resize(bytes);
    if(GetRawInputData(input,RID_INPUT,impl_->packet.data(),&bytes,sizeof(RAWINPUTHEADER))!=impl_->packet.size()) return result;
    const auto& raw=*reinterpret_cast<const RAWINPUT*>(impl_->packet.data());
    constexpr auto offset=offsetof(RAWINPUT,data)+offsetof(RAWHID,bRawData);
    if(raw.header.dwType!=RIM_TYPEHID || raw.header.dwSize!=bytes || bytes<offset ||
        !validBrightnessReportBatch(bytes-offset,raw.data.hid.dwSizeHid,raw.data.hid.dwCount)) return result;
    auto* device=impl_->device(raw.header.hDevice);
    if(!device || !device->supported || raw.data.hid.dwSizeHid!=device->reportBytes) return result;
    for(DWORD i=0;i<raw.data.hid.dwCount;++i) {
        auto* report=reinterpret_cast<PCHAR>(impl_->packet.data()+offset+static_cast<size_t>(i)*raw.data.hid.dwSizeHid);
        auto held=device->reports.find(static_cast<UCHAR>(report[0]));
        if(held==device->reports.end()) continue; // Another report ID cannot release a held brightness key.
        auto count=static_cast<ULONG>(device->usages.size());
        if(impl_->getUsages(HidP_Input,consumerPage,0,device->usages.data(),&count,device->data(),report,raw.data.hid.dwSizeHid)!=HIDP_STATUS_SUCCESS) continue;
        unsigned mask=0;
        for(ULONG u=0;u<count;++u) {
            if(device->usages[u]==brightnessUp) mask|=BrightnessKeyState::up;
            else if(device->usages[u]==brightnessDown) mask|=BrightnessKeyState::down;
        }
        held->second=mask; result.observed|=mask;
        result.direction+=impl_->state.update(impl_->held(),now);
    }
    return result;
}
int BrightnessKeys::repeat(std::uint64_t now) { return impl_->session?impl_->state.repeat(now):0; }
bool BrightnessKeys::repeating() const { return impl_->session && impl_->state.repeating(); }
void BrightnessKeys::clear() { impl_->clear(); }
void BrightnessKeys::removed(HANDLE device) { impl_->devices.erase(device);impl_->clear(); }
unsigned BrightnessKeys::supportedDevices() {
    if(!impl_->load()) return 0;
    UINT count=0;
    if(GetRawInputDeviceList(nullptr,&count,sizeof(RAWINPUTDEVICELIST))==UINT(-1) || count>256) return 0;
    std::vector<RAWINPUTDEVICELIST> devices(count);
    const auto actual=GetRawInputDeviceList(devices.data(),&count,sizeof(RAWINPUTDEVICELIST));
    if(actual==UINT(-1) || actual>devices.size()) return 0;
    unsigned supported=0;
    for(UINT i=0;i<actual;++i) if(devices[i].dwType==RIM_TYPEHID) {
        RID_DEVICE_INFO info{};info.cbSize=sizeof(info);UINT bytes=sizeof(info);
        if(GetRawInputDeviceInfoW(devices[i].hDevice,RIDI_DEVICEINFO,&info,&bytes)!=UINT(-1) &&
            info.hid.usUsagePage==consumerPage && info.hid.usUsage==consumerControl)
            if(auto* value=impl_->device(devices[i].hDevice);value && value->supported) ++supported;
    }
    return supported;
}
}
