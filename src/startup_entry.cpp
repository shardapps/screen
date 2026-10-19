#include <windows.h>
#include <string>

// MSIX startup tasks launch an executable without custom command-line arguments.
// This short-lived entry point starts the real app quietly, using its own folder.
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    std::wstring executable(32768,L'\0');
    const auto length=GetModuleFileNameW(nullptr,executable.data(),static_cast<DWORD>(executable.size()));
    if(!length || length>=executable.size()) return 1;
    executable.resize(length);
    const auto slash=executable.find_last_of(L"\\/");
    if(slash==std::wstring::npos) return 1;
    executable.resize(slash+1); executable+=L"Screen.exe";
    auto command=L"\""+executable+L"\" --background --startup";
    STARTUPINFOW startup{sizeof(startup)}; PROCESS_INFORMATION process{};
    if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process)) return 1;
    CloseHandle(process.hThread); CloseHandle(process.hProcess); return 0;
}
