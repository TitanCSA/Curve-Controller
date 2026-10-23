#include "Capture.h"
#include <algorithm>
#include <variant>
#include <system_error>
#include <cmath>

namespace Curve {
namespace {
using namespace Interceptor;
using Payload=std::variant<Controller_Hid_Input,Controller_Xinput_Input,Controller_XinputHid_Input,Controller_GIP_Input,Controller_GIP_Raw_Input>;
struct Cached {unsigned report=0;Payload physical;};
struct DeviceCache {DeviceState state;HidDescriptor layout;std::vector<Cached> reports;};
std::wstring Wide(const char* s){if(!s||!*s)return {};const int n=MultiByteToWideChar(CP_UTF8,0,s,-1,nullptr,0);if(n<1)return L"Unknown error";std::wstring v(n,0);MultiByteToWideChar(CP_UTF8,0,s,-1,v.data(),n);v.resize(n-1);return v;}
void Require(const Send_Result& sent,size_t expected){if(sent.error||sent.accepted!=expected)throw std::system_error(sent.error?sent.error:ERROR_WRITE_FAULT,std::system_category(),"Send_Input / incomplete forwarding");}
template<class T> unsigned Report(const T& value){if constexpr(std::is_base_of_v<Hid_Decoded_Input,T>)return value.length?value.report[0]:0;else return 0;}
template<class T> AxisSample Apply(T& input,const Response& r,const HidDescriptor& layout){
    if constexpr(std::is_base_of_v<Hid_Decoded_Input,T>)return Transform(input.state,r,layout);
    else if constexpr(std::is_same_v<T,Controller_GIP_Raw_Input>)return Transform(input.input.state,r);
    else return Transform(input.state,r);
}
std::wstring Family(Device_Type type){switch(type){case Device_Type::Controller_Hid:return L"HID";case Device_Type::Controller_Xinput:return L"XUSB";case Device_Type::Controller_XinputHid:return L"XInputHID";default:return L"GIP";}}
}
Capture::Capture(){wake_=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!wake_)throw std::system_error(GetLastError(),std::system_category(),"Create capture wake event");}
Capture::~Capture(){Stop();CloseHandle(wake_);}
void Capture::Start(HWND window,const Settings& settings){if(thread_.joinable())return;window_=window;stopping_=false;Update(settings);thread_=std::thread([this]{Run();});}
void Capture::Update(const Settings& s){std::atomic_store(&response_,std::shared_ptr<const Response>(std::make_shared<Response>(s)));SetEvent(wake_);}
void Capture::Stop() noexcept{stopping_=true;SetEvent(wake_);if(thread_.joinable())thread_.join();window_=nullptr;}
void Capture::Notify() noexcept {if(window_&&!posted_.exchange(true))if(!PostMessageW(window_,InputMessage,0,0))posted_=false;}
Snapshot Capture::Read(){std::lock_guard<std::mutex> lock(mutex_);return snapshot_;}
void Capture::Run() noexcept {
    std::vector<DeviceCache> devices;devices.reserve(32);
    std::shared_ptr<const Response> applied;
    auto publish=[&](const wchar_t* status,bool active,const std::wstring& error=L"",DWORD code=0){
        {std::lock_guard<std::mutex> lock(mutex_);snapshot_.active=active;snapshot_.status=status;snapshot_.error=error;snapshot_.errorCode=code;snapshot_.devices.clear();for(const auto& d:devices){snapshot_.devices.push_back(d.state);if(!active)snapshot_.devices.back().sample.valid=0;}}Notify();
    };
    while(!stopping_){
    devices.clear();applied.reset();
    try{
        const auto caps=Get_Capabilities();
        if(caps.input_mask!=0 || caps.controller_mask!=15)throw std::system_error(ERROR_REVISION_MISMATCH,std::system_category(),"Install the Gamepad & Controller edition");
        const auto init=Init_AllDevices();
        if(!init.ready)throw std::system_error(init.error?init.error:ERROR_SUCCESS_REBOOT_REQUIRED,std::system_category(),init.error_detail.empty()?"Driver initialization / restart required":init.error_detail);
        // Prime the automatic context without waiting for a physical action.
        // A real batch from this call is processed normally below.
        bool first=true;
        while(!stopping_){
            auto current=std::atomic_load(&response_);
            if(applied && current!=applied){
                for(auto& d:devices)for(auto& saved:d.reports)std::visit([&](const auto& source){
                    auto edited=source;const auto sample=Apply(edited,*current,d.layout);
                    if(sample.valid){Require(Send_Input(edited),1);for(unsigned i=0;i<4;++i)if(sample.valid&(1u<<i)){d.state.sample.in[i]=sample.in[i];d.state.sample.out[i]=sample.out[i];d.state.sample.valid|=1u<<i;}std::lock_guard<std::mutex> lock(mutex_);++snapshot_.configSends;}
                },saved.physical);
                publish(L"Live - settings take effect immediately",true);
            }
            applied=current;
            {
                const bool prime=first;auto batch=Get_Input_With_Timeout(first?0:INFINITE,wake_);first=false;
                for(auto& event:batch){
                    const auto token=event.device_info?event.device_info->token:std::to_string(static_cast<unsigned>(event.type))+":"+std::to_string(event.device);
                    auto position=std::find_if(devices.begin(),devices.end(),[&](const auto& d){return d.state.token==token;});
                    if(position==devices.end()){
                        if(devices.size()>=32)continue; // Still forwarded by Send_Input(batch).
                        devices.emplace_back();position=devices.end()-1;position->state.token=token;position->state.family=Family(event.type);
                        if(event.device_info)position->state.name=reinterpret_cast<const wchar_t*>(event.device_info->name);
                        if(position->state.name.empty())position->state.name=L"Controller "+std::to_wstring(event.device);
                        if(event.type==Device_Type::Controller_Hid||event.type==Device_Type::Controller_XinputHid){
                            const auto descriptor=Get_descriptor(token);if(descriptor)position->layout.Load(descriptor.data);
                        }
                    }
                    auto& device=*position;
                    auto process=[&](auto& input){
                        using T=std::decay_t<decltype(input)>;
                        const auto id=Report(input);
                        auto saved=std::find_if(device.reports.begin(),device.reports.end(),[&](const auto& r){return r.report==id;});
                        if(saved==device.reports.end()&&device.reports.size()<32){device.reports.push_back({id,Payload(input)});saved=device.reports.end()-1;}
                        else if(saved!=device.reports.end())saved->physical=input;
                        if constexpr(std::is_base_of_v<Hid_Decoded_Input,T>)if(saved!=device.reports.end())NeutralRelative(std::get<T>(saved->physical).state);
                        const auto sample=Apply(input,*current,device.layout);
                        for(unsigned i=0;i<4;++i)if(sample.valid&(1u<<i)){device.state.sample.in[i]=sample.in[i];device.state.sample.out[i]=sample.out[i];device.state.sample.valid|=1u<<i;}
                        if constexpr(std::is_base_of_v<Hid_Decoded_Input,T>){if(input.decode_error)device.state.detail=L"Unsupported HID report forwarded unchanged";else if(!sample.valid)device.state.detail=L"No stick axes in this report; check HID layout if needed";else device.state.detail=L"Descriptor-based axes; buttons and triggers preserved";}
                        else device.state.detail=L"Native stick values; buttons, triggers and protocol data preserved";
                        ++device.state.reports;
                    };
                    if(auto hid=event.Get<Controller_Hid_Input>())process(*hid);
                    else if(auto xusb=event.Get<Controller_Xinput_Input>())process(*xusb);
                    else if(auto xih=event.Get<Controller_XinputHid_Input>())process(*xih);
                    else if(auto raw=event.Get<Controller_GIP_Raw_Input>())process(*raw);
                    else if(auto gip=event.Get<Controller_GIP_Input>())process(*gip);
                }
                if(!batch.empty()){
                    Require(Send_Input(batch),batch.size());
                    {std::lock_guard<std::mutex> lock(mutex_);snapshot_.received+=batch.size();snapshot_.forwarded+=batch.size();}
                    publish(L"Live - low-level controller response",true);
                }else if(prime)publish(L"Live - move a stick to inspect its response",true);
            }
        }
        // Restore each cached physical report before releasing capture, even
        // when the stick is held still and no final physical event arrives.
        for(auto& d:devices)for(auto& report:d.reports)std::visit([](const auto& p){(void)Send_Input(p);},report.physical);
        Destroy_Context();publish(L"Closed - physical input passes through",false);
    }catch(const std::system_error& e){
        try{Destroy_Context();}catch(...){}
        std::wstring hint=Wide(e.what());
        if(e.code().value()==ERROR_FILE_NOT_FOUND||e.code().value()==ERROR_SERVICE_DOES_NOT_EXIST)hint+=L". Install Interceptor Gamepad & Controller with Driver setup.";
        if(e.code().value()==ERROR_EVALUATION_EXPIRATION)hint+=L". The driver demo has expired. Restart Windows for a new allowance.";
        if(e.code().value()==ERROR_ACCESS_DENIED)hint+=L". Run this application as administrator.";
        if(e.code().value()==ERROR_SUCCESS_REBOOT_REQUIRED)hint+=L". Restart Windows to finish driver setup, then reopen the app.";
        publish(L"Passthrough - driver connection needs attention",false,hint,static_cast<DWORD>(e.code().value()));
    }catch(const std::exception& e){try{Destroy_Context();}catch(...){}publish(L"Passthrough - capture stopped",false,Wide(e.what()),ERROR_GEN_FAILURE);}
    if(stopping_)break;
    // Healthy capture has no polling timer. Only a failed connection is
    // retried, with a bounded idle wait that Stop/configuration can interrupt.
    const auto error=Read().errorCode;
    WaitForSingleObject(wake_,error==ERROR_EVALUATION_EXPIRATION?INFINITE:3000);
    }
}
}
