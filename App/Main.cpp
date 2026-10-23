#include "UI.h"
#include "Storage.h"
#include <shellapi.h>
#include <fstream>
#include <iostream>

int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
    int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);if(!argv)return 1;
    std::wstring mode=argc>1?argv[1]:L"",path=argc>2?argv[2]:L"",preview=argc>3?argv[3]:L"";LocalFree(argv);
    const bool offline=mode==L"--demo"||mode==L"--render"||mode==L"--self-test"||mode==L"--check-json";
    try{
        if(mode==L"--check-json"){Curve::Parse(Curve::ReadJson(path));return 0;}
        if(!mode.empty()&&!offline)throw std::runtime_error("Unknown argument.");
        if(!offline){
            SID_IDENTIFIER_AUTHORITY authority=SECURITY_NT_AUTHORITY;PSID admins=nullptr;BOOL elevated=FALSE;
            if(AllocateAndInitializeSid(&authority,2,SECURITY_BUILTIN_DOMAIN_RID,DOMAIN_ALIAS_RID_ADMINS,0,0,0,0,0,0,&admins)){CheckTokenMembership(nullptr,admins,&elevated);FreeSid(admins);}
            if(!elevated){wchar_t file[32768]{};GetModuleFileNameW(nullptr,file,32768);const auto result=reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"runas",file,nullptr,nullptr,SW_SHOWNORMAL));return result>32?0:static_cast<int>(result);}
        }
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        const auto com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);if(FAILED(com))return 1;
        int result=0;
        {
            Curve::Window window(offline);
            if(mode==L"--render"){if(!preview.empty())window.PreviewSettings(Curve::Parse(Curve::ReadJson(preview)));window.RenderPng(path);}
            else if(mode==L"--self-test"){
                window.Create(false);const auto checks=window.SelfTest();
                std::ofstream report(path);report<<"{\"passed\":true,\"checks\":"<<checks<<",\"driverOpened\":false,\"preferencesWritten\":false}";if(!report)result=1;
            }else{
                window.Create(true);MSG message{};while(GetMessageW(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}
            }
        }
        CoUninitialize();return result;
    }catch(const std::exception& e){
        if(!path.empty()&&mode==L"--self-test"){std::ofstream out(path);out<<"{\"passed\":false}";}
        OutputDebugStringA(e.what());if(!offline)MessageBoxA(nullptr,e.what(),"Curve Controller",MB_OK|MB_ICONERROR);return 1;
    }
}
