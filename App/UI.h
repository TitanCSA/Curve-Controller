#pragma once
#include "Capture.h"
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <vector>
namespace Curve {
class Window {
    template<class T> using ComPtr=Microsoft::WRL::ComPtr<T>;
    HWND window_=nullptr,editX_=nullptr,editY_=nullptr;
    ComPtr<ID2D1Factory> factory_;
    ComPtr<IDWriteFactory> textFactory_;
    ComPtr<ID2D1HwndRenderTarget> target_;
    std::array<ComPtr<IDWriteTextFormat>,7> fonts_;
    Settings settings_;
    Snapshot snapshot_;
    Capture capture_;
    std::wstring notice_,driverProblem_;
    bool offline_=false,editing_=false,dirty_=false,dragging_=false;
    unsigned point_=0,device_=0;
    int hover_=-1,focus_=-1;
    float width_=1200,height_=860,dpi_=96,animation_=1;
    ULONGLONG animationStart_=0;
    struct Hit {int id;D2D1_RECT_F box;};
    std::vector<Hit> hits_;
    D2D1_RECT_F Plot() const noexcept;
    void Draw(ID2D1RenderTarget*);
    void Changed();
    void Editors();
    void LayoutEdits();
    void Action(int);
    bool CheckDriver();
    int HitTest(float,float) const;
    int PointAt(float,float) const;
    void Paint();
    void ChooseFile(bool save);
    LRESULT Message(UINT,WPARAM,LPARAM);
    static LRESULT CALLBACK Procedure(HWND,UINT,WPARAM,LPARAM);
public:
    explicit Window(bool offline);
    ~Window();
    HWND Create(bool visible);
    void RenderPng(const std::wstring&);
    void PreviewSettings(const Settings&);
    unsigned SelfTest();
};
}
