#pragma once
#include "Transform.h"
#include <Windows.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <string>
namespace Curve {
constexpr UINT InputMessage=WM_APP+41;
struct DeviceState {std::string token;std::wstring name,family,detail;AxisSample sample;unsigned long long reports=0;};
struct Snapshot {bool active=false;std::wstring status=L"Connecting to the driver...",error;std::vector<DeviceState> devices;unsigned long long received=0,forwarded=0,configSends=0;DWORD errorCode=0;};
class Capture {
    HWND window_=nullptr;
    HANDLE wake_=nullptr;
    std::atomic<bool> stopping_{false},posted_{false};
    std::shared_ptr<const Response> response_;
    std::mutex mutex_;
    Snapshot snapshot_;
    std::thread thread_;
    void Run() noexcept;
    void Notify() noexcept;
public:
    Capture();~Capture();
    void Start(HWND,const Settings&);
    void Update(const Settings&);
    void Stop() noexcept;
    void Acknowledge() noexcept{posted_=false;}
    Snapshot Read();
};
}
