#include "virtual_desktops.hpp"
#include <array>

namespace screen {
namespace {
std::wstring idText(const GUID& id) {
    wchar_t text[40]{};
    if (id == GUID_NULL || !StringFromGUID2(id,text,40)) return {};
    return text;
}
std::wstring currentId(HKEY key) {
    GUID id{}; DWORD bytes=sizeof(id);
    return key && RegGetValueW(key,nullptr,L"CurrentVirtualDesktop",RRF_RT_REG_BINARY,nullptr,&id,&bytes)==ERROR_SUCCESS && bytes==sizeof(id)
        ? idText(id) : L"";
}
}
VirtualDesktops::VirtualDesktops(HWND owner,const std::wstring& registryPath) : owner_(owner), path_(registryPath) {
    native_=path_==L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VirtualDesktops";
    RegOpenKeyExW(HKEY_CURRENT_USER,path_.c_str(),0,KEY_READ,&root_);
    watched_=root_;
    if(native_ && currentId(root_).empty()) {
        DWORD session=0;
        if(ProcessIdToSessionId(GetCurrentProcessId(),&session)) {
            const auto sessionPath=L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\SessionInfo\\"+
                std::to_wstring(session)+L"\\VirtualDesktops";
            HKEY key{};
            if(RegOpenKeyExW(HKEY_CURRENT_USER,sessionPath.c_str(),0,KEY_READ,&key)==ERROR_SUCCESS) watched_=key;
        }
    }
    event_=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    arm(); refresh();
}
VirtualDesktops::~VirtualDesktops() {
    closing_=true; stopWait();
    if(watched_ && watched_!=root_) RegCloseKey(watched_);
    if(root_) RegCloseKey(root_);
    if(event_) CloseHandle(event_);
    if(manager_) manager_->Release();
    if(uninitialize_) CoUninitialize();
}
void VirtualDesktops::stopWait() {
    if(wait_) { UnregisterWaitEx(wait_,INVALID_HANDLE_VALUE); wait_=nullptr; }
}
void CALLBACK VirtualDesktops::changed(void* context,BOOLEAN) {
    auto* self=static_cast<VirtualDesktops*>(context);
    if(!self->closing_ && !self->pending_.exchange(true))
        if(!PostMessageW(self->owner_,virtualDesktopsChanged,1,0)) self->pending_=false;
}
void VirtualDesktops::arm() {
    if(!watched_ || !event_) return;
    if(RegNotifyChangeKeyValue(watched_,TRUE,REG_NOTIFY_CHANGE_LAST_SET | REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_THREAD_AGNOSTIC,event_,TRUE)==ERROR_SUCCESS)
        RegisterWaitForSingleObject(&wait_,event_,changed,this,INFINITE,WT_EXECUTEONLYONCE);
}
bool VirtualDesktops::manager() {
    if(manager_) return true;
    if(!native_) return false;
    if(!uninitialize_) uninitialize_=SUCCEEDED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED));
    return SUCCEEDED(CoCreateInstance(CLSID_VirtualDesktopManager,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&manager_)));
}
void VirtualDesktops::refresh(bool notification) {
    if(notification) { stopWait(); pending_=false; arm(); }
    current_=currentId(watched_);
    if(current_.empty()) current_=currentId(root_);
    if(current_.empty() && manager()) {
        const auto foreground=GetForegroundWindow(); BOOL onCurrent=FALSE; GUID id{};
        if(foreground && SUCCEEDED(manager_->IsWindowOnCurrentVirtualDesktop(foreground,&onCurrent)) && onCurrent &&
            SUCCEEDED(manager_->GetWindowDesktopId(foreground,&id))) current_=idText(id);
    }
    items_.clear();
    std::array<GUID,maxDesktopProfiles> ids{}; DWORD bytes=static_cast<DWORD>(sizeof(ids));
    if(root_ && RegGetValueW(root_,nullptr,L"VirtualDesktopIDs",RRF_RT_REG_BINARY,nullptr,ids.data(),&bytes)==ERROR_SUCCESS && bytes%sizeof(GUID)==0) {
        for(size_t i=0;i<bytes/sizeof(GUID);++i) {
            auto id=idText(ids[i]); if(id.empty()) continue;
            wchar_t label[160]{}; DWORD labelBytes=sizeof(label);
            const auto key=L"Desktops\\"+id;
            if(RegGetValueW(root_,key.c_str(),L"Name",RRF_RT_REG_SZ,nullptr,label,&labelBytes)!=ERROR_SUCCESS) label[0]=0;
            label[159]=0;
            items_.push_back({id,*label?std::wstring(label):L"Desktop "+std::to_wstring(i+1)});
        }
    }
    if(!current_.empty() && std::none_of(items_.begin(),items_.end(),[&](const auto& item){return item.id==current_;}))
        items_.push_back({current_,L"Current desktop"});
}
std::wstring VirtualDesktops::name() const {
    for(const auto& item:items_) if(item.id==current_) return item.name;
    return L"Desktop detection unavailable";
}
void moveToDesktop(HWND window,const std::wstring& id,IVirtualDesktopManager* manager) {
    if(!window || !manager || !validDesktopId(id)) return;
    GUID value{};
    if(SUCCEEDED(CLSIDFromString(id.c_str(),&value))) manager->MoveWindowToDesktop(window,value);
}
void VirtualDesktops::moveWindow(HWND window) {
    if(window) {
        const auto style=GetWindowLongPtrW(window,GWL_EXSTYLE);
        if((style & WS_EX_TOOLWINDOW) && !(style & WS_EX_APPWINDOW) && !GetWindow(window,GW_OWNER)) return;
    }
    if(window && !current_.empty() && manager()) moveToDesktop(window,current_,manager_);
}
}
