#include "Storage.h"
#include <Windows.h>
#include <vector>
#include <system_error>
#include <stdexcept>

namespace Curve {
namespace {
std::wstring Wide(const std::string& s){const int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0);if(!n)throw std::runtime_error("Invalid UTF-8.");std::wstring r(n,0);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),r.data(),n);return r;}
std::string Utf8(const std::wstring& s){const int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0,nullptr,nullptr);if(!n)throw std::runtime_error("Invalid Unicode preferences.");std::string r(n,0);WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),r.data(),n,nullptr,nullptr);return r;}
struct Handle {HANDLE h=INVALID_HANDLE_VALUE;~Handle(){if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);}};
}
Settings LoadPreferences(std::wstring& notice,const wchar_t* key){
    DWORD bytes=0;auto code=RegGetValueW(HKEY_CURRENT_USER,key,L"Settings",RRF_RT_REG_SZ,nullptr,nullptr,&bytes);
    if(code==ERROR_FILE_NOT_FOUND){notice=L"Preferences save automatically on this PC.";return {};}
    try{
        if(code!=ERROR_SUCCESS||bytes<2||bytes>131074||bytes%2)throw std::runtime_error("Invalid preference storage.");
        std::vector<wchar_t> value(bytes/2);code=RegGetValueW(HKEY_CURRENT_USER,key,L"Settings",RRF_RT_REG_SZ,nullptr,value.data(),&bytes);
        if(code!=ERROR_SUCCESS||value.back()!=0)throw std::runtime_error("Cannot read preferences.");
        notice=L"Your saved settings are ready.";return Parse(Utf8(std::wstring(value.data(),value.size()-1)));
    }catch(...){notice=L"Saved settings were invalid. Linear defaults restored.";return {};}
}
void SavePreferences(const Settings& s,const wchar_t* path){
    const auto value=Wide(Serialize(s));HKEY key=nullptr;
    auto code=RegCreateKeyExW(HKEY_CURRENT_USER,path,0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr);
    if(code!=ERROR_SUCCESS)throw std::system_error(code,std::system_category(),"Save preferences");
    code=RegSetValueExW(key,L"Settings",0,REG_SZ,reinterpret_cast<const BYTE*>(value.c_str()),static_cast<DWORD>((value.size()+1)*sizeof(wchar_t)));RegCloseKey(key);
    if(code!=ERROR_SUCCESS)throw std::system_error(code,std::system_category(),"Save preferences");
}
std::string ReadJson(const std::wstring& path){
    Handle file;file.h=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,nullptr);
    if(file.h==INVALID_HANDLE_VALUE)throw std::system_error(GetLastError(),std::system_category(),"Open JSON");
    LARGE_INTEGER size{};if(!GetFileSizeEx(file.h,&size)||size.QuadPart<1||size.QuadPart>65536)throw std::runtime_error("JSON file must be between 1 and 65,536 bytes.");
    std::string data(static_cast<size_t>(size.QuadPart),0);DWORD got=0;
    if(!ReadFile(file.h,data.data(),static_cast<DWORD>(data.size()),&got,nullptr)||got!=data.size())throw std::runtime_error("Could not read the complete JSON file.");
    return data;
}
void WriteJson(const std::wstring& path,const Settings& s){
    const auto data=Serialize(s);const auto temporary=path+L".curve-controller-"+std::to_wstring(GetCurrentProcessId())+L".tmp";
    Handle file;file.h=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file.h==INVALID_HANDLE_VALUE)throw std::system_error(GetLastError(),std::system_category(),"Create JSON export");
    DWORD wrote=0;const bool good=WriteFile(file.h,data.data(),static_cast<DWORD>(data.size()),&wrote,nullptr)&&wrote==data.size()&&FlushFileBuffers(file.h);
    CloseHandle(file.h);file.h=INVALID_HANDLE_VALUE;
    if(!good){DeleteFileW(temporary.c_str());throw std::runtime_error("Could not save JSON export.");}
    if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){const auto error=GetLastError();DeleteFileW(temporary.c_str());throw std::system_error(error,std::system_category(),"Finish JSON export");}
}
}
