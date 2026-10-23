#include "UI.h"
#include "Storage.h"
#include <Windowsx.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <wincodec.h>
#include <d2d1helper.h>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <system_error>

namespace Curve {
using Microsoft::WRL::ComPtr;
namespace {
void Check(HRESULT result){if(FAILED(result))throw std::system_error(static_cast<int>(result),std::system_category(),"Windows graphics operation");}
std::wstring Wide(const char* text){const int n=MultiByteToWideChar(CP_UTF8,0,text,-1,nullptr,0);std::wstring s(n,0);MultiByteToWideChar(CP_UTF8,0,text,-1,s.data(),n);if(!s.empty())s.pop_back();return s;}
std::wstring Number(double x,int digits=1){std::wostringstream s;s.imbue(std::locale::classic());s<<std::fixed<<std::setprecision(digits)<<x;return s.str();}
bool Inside(D2D1_RECT_F r,float x,float y){return x>=r.left&&x<=r.right&&y>=r.top&&y<=r.bottom;}
constexpr unsigned Background=0x0B1220,Surface=0x131E31,Elevated=0x1A2942,Border=0x273952,Blue=0x4389FF,White=0xF2F6FF,Muted=0x91A6C4,Teal=0x58DDCC;
}
Window::Window(bool offline):offline_(offline){
    Check(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,factory_.GetAddressOf()));
    Check(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(textFactory_.GetAddressOf())));
    const float sizes[]{12,13,14,16,20,28};
    for(unsigned i=0;i<fonts_.size();++i){Check(textFactory_->CreateTextFormat(i==6?L"Consolas":L"Segoe UI",nullptr,i==4||i==5?DWRITE_FONT_WEIGHT_SEMI_BOLD:DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,i==6?14:sizes[i],L"en-us",&fonts_[i]));fonts_[i]->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);}
    if(offline_){notice_=L"Offline preview - simulated controller values";snapshot_.active=true;snapshot_.status=notice_;DeviceState d;d.name=L"DualSense Wireless Controller";d.family=L"HID";d.detail=L"Simulated preview. No driver or controller is accessed.";d.sample.valid=15;d.sample.in={0.34,-0.68,0.52,-0.21};settings_.preset=Preset::Exponential;Response r(settings_);for(unsigned i=0;i<4;++i)d.sample.out[i]=r.Map(d.sample.in[i]);snapshot_.devices.push_back(d);}
    else settings_=LoadPreferences(notice_);
}
Window::~Window(){capture_.Stop();if(window_&&IsWindow(window_))DestroyWindow(window_);}
D2D1_RECT_F Window::Plot() const noexcept{return D2D1::RectF(370,215,width_-56,height_-350);}
void Window::Draw(ID2D1RenderTarget* rt){
    ComPtr<ID2D1SolidColorBrush> brush;Check(rt->CreateSolidColorBrush(D2D1::ColorF(White),&brush));
    auto color=[&](unsigned c,float alpha=1.f){brush->SetColor(D2D1::ColorF(c,alpha));};
    auto rect=[&](D2D1_RECT_F box,unsigned fill,float radius=12.f){color(fill);rt->FillRoundedRectangle(D2D1::RoundedRect(box,radius,radius),brush.Get());};
    auto line=[&](float x,float y,float a,float b,unsigned c,float stroke=1.f,float alpha=1.f){color(c,alpha);rt->DrawLine({x,y},{a,b},brush.Get(),stroke);};
    auto text=[&](const std::wstring& s,D2D1_RECT_F box,unsigned c=White,unsigned font=2){color(c);rt->DrawTextW(s.c_str(),static_cast<UINT32>(s.size()),fonts_[font].Get(),box,brush.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);};
    auto label=[&](const std::wstring& s,float x,float y,float w,unsigned c=White,unsigned font=2){text(s,D2D1::RectF(x,y,x+w,y+36),c,font);};
    hits_.clear();
    auto button=[&](int id,const std::wstring& caption,D2D1_RECT_F box,bool selected=false){
        const bool hover=id==hover_;rect(box,Surface,8);
        if(selected||hover){const float t=animation_*animation_*(3-2*animation_);color(selected?Blue:Elevated,selected?0.76f+0.24f*t:t);rt->FillRoundedRectangle(D2D1::RoundedRect(box,8,8),brush.Get());}
        color(selected?Blue:Border);rt->DrawRoundedRectangle(D2D1::RoundedRect(box,8,8),brush.Get(),focus_==id?2.f:1.f);
        text(caption,D2D1::RectF(box.left+13,box.top+9,box.right-8,box.bottom+2),selected?White:hover?White:Muted,1);hits_.push_back({id,box});
    };
    rt->Clear(D2D1::ColorF(Background));
    rect(D2D1::RectF(26,24,66,64),Blue,12);label(L"C",36,25,25,White,5);
    label(L"Curve Controller",82,19,350,White,5);label(L"Joystick response, without extra steps.",84,55,490,Muted,1);
    if(offline_)label(L"OFFLINE PREVIEW",width_-670,35,180,Teal,1);
    button(10,L"Import JSON",D2D1::RectF(width_-432,28,width_-308,66));
    button(11,L"Export JSON",D2D1::RectF(width_-296,28,width_-172,66));
    button(12,L"Driver setup",D2D1::RectF(width_-160,28,width_-24,66));
    rect(D2D1::RectF(24,104,280,height_-64),Surface);
    label(L"APPLY TO",44,123,200,Muted,0);
    for(unsigned i=0;i<2;++i){const bool enabled=i?settings_.right:settings_.left;const float y=153.f+i*47;
        button(20+static_cast<int>(i),(enabled?L"[x]  ":L"[  ]  ")+std::wstring(i?L"Right joystick":L"Left joystick"),D2D1::RectF(43,y,261,y+38),enabled);
    }
    label(L"RESPONSE PRESET",44,255,210,Muted,0);
    for(unsigned i=0;i<12;++i){const float y=285.f+(i/2)*43;const float x=43.f+(i%2)*112;
        const wchar_t* captions[]{L"Linear",L"Soft expo",L"Exponential",L"Strong expo",L"Soft inverse",L"Inverse expo",L"Strong inverse",L"Smooth S",L"Smoother S",L"Ease out",L"Precision",L"Early max"};
        button(100+static_cast<int>(i),captions[i],D2D1::RectF(x,y,x+106,y+36),static_cast<unsigned>(settings_.preset)==i);
    }
    button(112,L"Custom curve  /  10 points",D2D1::RectF(43,554,261,594),settings_.preset==Preset::Custom);
    label(L"HID STICK LAYOUT",44,621,215,Muted,0);
    const auto layout=settings_.layout==HidLayout::Auto?L"Automatic  v":settings_.layout==HidLayout::RxRy?L"X/Y + Rx/Ry  v":L"X/Y + Z/Rz  v";
    button(30,layout,D2D1::RectF(43,649,261,686));
    label(L"Applied to every supported controller.",44,704,220,Muted,0);
    label(L"No Apply button. No config file.",44,726,220,Muted,0);
    rect(D2D1::RectF(300,104,width_-24,height_-264),Surface);
    label(Wide(Info(settings_.preset).name),324,122,width_-400,White,4);
    label(Wide(Info(settings_.preset).description),324,155,width_-400,Muted,1);
    const auto plot=Plot();const float pw=plot.right-plot.left,ph=plot.bottom-plot.top;
    label(L"OUTPUT",324,188,90,Muted,0);
    for(unsigned i=0;i<=4;++i){const float amount=i/4.f,x=plot.left+amount*pw,y=plot.bottom-amount*ph;
        line(plot.left,y,plot.right,y,Border,1,0.65f);line(x,plot.top,x,plot.bottom,Border,1,0.65f);
        label(std::to_wstring(i*25)+L"%",plot.left-49,y-8,44,Muted,0);
        label(std::to_wstring(i*25)+L"%",x-12,plot.bottom+13,50,Muted,0);
    }
    line(plot.left,plot.bottom,plot.right,plot.top,Muted,1,0.32f);
    const Response response(settings_);
    D2D1_POINT_2F previous{plot.left,plot.bottom};
    for(unsigned i=1;i<=600;++i){const double x=double(i)/600;const D2D1_POINT_2F next{plot.left+static_cast<float>(x)*pw,plot.bottom-static_cast<float>(response.Map(x))*ph};line(previous.x,previous.y,next.x,next.y,Blue,3);previous=next;}
    if(settings_.preset==Preset::Custom){
        for(unsigned i=0;i<10;++i){const auto& p=settings_.points[i];const D2D1_POINT_2F center{plot.left+static_cast<float>(p.x)*pw,plot.bottom-static_cast<float>(p.y)*ph};color(Background);rt->FillEllipse({center,i==point_?7.f:5.f,i==point_?7.f:5.f},brush.Get());color(i==point_?Teal:White);rt->DrawEllipse({center,i==point_?7.f:5.f,i==point_?7.f:5.f},brush.Get(),2);}
    }
    if(!snapshot_.devices.empty()){
        const auto& sample=snapshot_.devices[std::min<size_t>(device_,snapshot_.devices.size()-1)].sample;
        for(unsigned i=0;i<4;++i)if(sample.valid&(1u<<i)){
            const float x=plot.left+static_cast<float>(std::abs(sample.in[i]))*pw,y=plot.bottom-static_cast<float>(std::abs(sample.out[i]))*ph;
            color(i<2?Teal:White,0.8f);rt->FillEllipse({{x,y},4,4},brush.Get());
        }
    }
    label(L"PHYSICAL INPUT",plot.left+pw/2-52,plot.bottom+30,160,Muted,0);
    const float ey=height_-303;
    if(settings_.preset==Preset::Custom){
        label(L"Point "+std::to_wstring(point_+1)+L" / 10",324,ey+2,132,White,1);
        label(L"Input",460,ey+2,58,Muted,1);label(L"%",583,ey+2,24,Muted,1);
        label(L"Output",626,ey+2,65,Muted,1);label(L"%",762,ey+2,24,Muted,1);
        rect(D2D1::RectF(509,ey-1,578,ey+30),Elevated,5);rect(D2D1::RectF(691,ey-1,759,ey+30),Elevated,5);
        if(offline_){label(Number(settings_.points[point_].x*100),517,ey+3,65,White,1);label(Number(settings_.points[point_].y*100),699,ey+3,65,White,1);}
        label(L"Drag points or type percentages",802,ey+3,width_-820,Muted,0);
    }else label(L"Choose Custom curve to edit ten points. Negative stick values keep their sign.",324,ey+4,width_-360,Muted,1);
    rect(D2D1::RectF(300,height_-244,width_-24,height_-64),Surface);
    label(L"LIVE INPUT / OUTPUT",324,height_-226,250,Muted,0);
    const bool hasDevice=!snapshot_.devices.empty();
    const auto* selected=hasDevice?&snapshot_.devices[std::min<size_t>(device_,snapshot_.devices.size()-1)]:nullptr;
    button(31,selected?selected->name+L"  v":L"Waiting for a controller report...",D2D1::RectF(width_-470,height_-232,width_-44,height_-198));
    const float middle=300+(width_-324)/2;
    line(middle,height_-187,middle,height_-84,Border);
    for(unsigned stick=0;stick<2;++stick){const float left=stick?middle+20:324;const float available=(width_-390)/2;
        label(stick?L"Right joystick":L"Left joystick",left,height_-185,230,White,3);
        label((stick?settings_.right:settings_.left)?L"Curve enabled":L"Unchanged",left+available-135,height_-180,145,(stick?settings_.right:settings_.left)?Teal:Muted,0);
        for(unsigned axis=0;axis<2;++axis){const unsigned i=stick*2+axis;const float y=height_-150.f+axis*29;
            label(axis?L"Y":L"X",left,y,24,Muted,1);
            const bool known=selected&&(selected->sample.valid&(1u<<i));
            label(known?Number(selected->sample.in[i]*100)+L"%":L"--",left+31,y,92,Muted,6);
            label(L"->",left+124,y,34,Muted,1);
            label(known?Number(selected->sample.out[i]*100)+L"%":L"--",left+164,y,93,White,6);
            const float bx=left+262,bw=std::max(20.f,available-272);rect(D2D1::RectF(bx,y+7,bx+bw,y+12),Elevated,2);
            if(known){const float amount=static_cast<float>(std::abs(selected->sample.out[i]));rect(D2D1::RectF(bx,y+7,bx+std::max(1.f,bw*amount),y+12),stick?Blue:Teal,2);}
        }
    }
    const float bottom=height_-44;
    color(snapshot_.active?Teal:Muted);rt->FillEllipse({{31,bottom+8},3,3},brush.Get());
    std::wstring status=snapshot_.error.empty()?snapshot_.status:snapshot_.error;
    if(!notice_.empty()&&notice_.find(L"failed")!=std::wstring::npos)status=notice_;
    label(status,45,bottom,width_-300,Muted,0);
    label(offline_?L"SIMULATED PREVIEW":dirty_?L"Saving preferences...":L"Saved automatically",width_-222,bottom,200,offline_?Teal:Muted,0);
    if(!snapshot_.active&&!offline_)button(32,L"Reconnect",D2D1::RectF(width_-155,118,width_-44,153));
}
HWND Window::Create(bool visible){
    WNDCLASSW cls{};cls.lpfnWndProc=Procedure;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"TitanCSA.CurveController";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.hIcon=LoadIconW(nullptr,IDI_APPLICATION);RegisterClassW(&cls);
    dpi_=static_cast<float>(GetDpiForSystem());const float scale=dpi_/96;
    RECT r{0,0,static_cast<LONG>(width_*scale),static_cast<LONG>(height_*scale)};AdjustWindowRectExForDpi(&r,WS_OVERLAPPEDWINDOW,FALSE,0,static_cast<UINT>(dpi_));
    window_=CreateWindowExW(0,cls.lpszClassName,L"Curve Controller",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,nullptr,nullptr,cls.hInstance,this);
    if(!window_)throw std::system_error(GetLastError(),std::system_category(),"Create main window");
    if(!offline_){editX_=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|ES_AUTOHSCROLL|WS_TABSTOP,0,0,0,0,window_,reinterpret_cast<HMENU>(501),cls.hInstance,nullptr);editY_=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|ES_AUTOHSCROLL|WS_TABSTOP,0,0,0,0,window_,reinterpret_cast<HMENU>(502),cls.hInstance,nullptr);SendMessageW(editX_,EM_LIMITTEXT,8,0);SendMessageW(editY_,EM_LIMITTEXT,8,0);LayoutEdits();Editors();}
    if(visible){ShowWindow(window_,SW_SHOW);UpdateWindow(window_);if(!offline_)capture_.Start(window_,settings_);}
    return window_;
}
void Window::LayoutEdits(){if(!editX_)return;const auto scale=dpi_/96;const int y=static_cast<int>((height_-299)*scale),h=static_cast<int>(25*scale);MoveWindow(editX_,static_cast<int>(517*scale),y,static_cast<int>(58*scale),h,TRUE);MoveWindow(editY_,static_cast<int>(699*scale),y,static_cast<int>(57*scale),h,TRUE);ShowWindow(editX_,settings_.preset==Preset::Custom?SW_SHOW:SW_HIDE);ShowWindow(editY_,settings_.preset==Preset::Custom?SW_SHOW:SW_HIDE);}
void Window::Editors(){if(!editX_)return;editing_=true;SetWindowTextW(editX_,Number(settings_.points[point_].x*100,2).c_str());SetWindowTextW(editY_,Number(settings_.points[point_].y*100,2).c_str());editing_=false;LayoutEdits();}
void Window::Changed(){
    if(!Valid(settings_))return;
    if(!offline_)capture_.Update(settings_);
    dirty_=true;if(!offline_)SetTimer(window_,2,300,nullptr);
    animation_=0;animationStart_=GetTickCount64();SetTimer(window_,1,16,nullptr);InvalidateRect(window_,nullptr,FALSE);
    if(offline_&&!snapshot_.devices.empty()){Response r(settings_);auto& s=snapshot_.devices[0].sample;for(unsigned i=0;i<4;++i)s.out[i]=(i<2?settings_.left:settings_.right)?r.Map(s.in[i]):s.in[i];}
}
int Window::HitTest(float x,float y) const{for(const auto& h:hits_)if(Inside(h.box,x,y))return h.id;return -1;}
int Window::PointAt(float x,float y) const{if(settings_.preset!=Preset::Custom)return -1;const auto p=Plot();for(unsigned i=0;i<10;++i){const float px=p.left+static_cast<float>(settings_.points[i].x)*(p.right-p.left),py=p.bottom-static_cast<float>(settings_.points[i].y)*(p.bottom-p.top);if(std::hypot(px-x,py-y)<=13)return static_cast<int>(i);}return -1;}
void Window::Action(int id){
    if(id==20)settings_.left=!settings_.left;
    else if(id==21)settings_.right=!settings_.right;
    else if(id>=100&&id<112)settings_.preset=static_cast<Preset>(id-100);
    else if(id==112)settings_.preset=Preset::Custom;
    else if(id==10||id==11){ChooseFile(id==11);return;}
    else if(id==12){
        wchar_t module[32768]{};GetModuleFileNameW(nullptr,module,32768);std::wstring path=module;path=path.substr(0,path.find_last_of(L"\\/")+1)+L"InterceptorSetup.exe";
        if(GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES){
            DWORD dummy=0,size=GetFileVersionInfoSizeW(path.c_str(),&dummy);std::vector<BYTE> version(size);LPVOID product=nullptr;UINT length=0;
            if(!size||!GetFileVersionInfoW(path.c_str(),0,size,version.data())||!VerQueryValueW(version.data(),L"\\StringFileInfo\\040904b0\\ProductName",&product,&length)||!product||std::wstring(static_cast<const wchar_t*>(product))!=L"InterceptorGamepadController")throw std::runtime_error("The adjacent installer is not the Gamepad & Controller edition.");
            ShellExecuteW(window_,L"open",path.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
        }else ShellExecuteW(window_,L"open",L"https://github.com/TitanCSA/Interceptor-Gamepad-Controller/releases/latest",nullptr,nullptr,SW_SHOWNORMAL);
        return;
    }else if(id==30){
        HMENU menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING|(settings_.layout==HidLayout::Auto?MF_CHECKED:0),1,L"Automatic (descriptor based)");AppendMenuW(menu,MF_STRING|(settings_.layout==HidLayout::RxRy?MF_CHECKED:0),2,L"Left X/Y - Right Rx/Ry");AppendMenuW(menu,MF_STRING|(settings_.layout==HidLayout::ZRz?MF_CHECKED:0),3,L"Left X/Y - Right Z/Rz");POINT p{43,686};p.x=static_cast<LONG>(p.x*dpi_/96);p.y=static_cast<LONG>(p.y*dpi_/96);ClientToScreen(window_,&p);const auto item=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_LEFTALIGN,p.x,p.y,0,window_,nullptr);DestroyMenu(menu);if(!item)return;settings_.layout=static_cast<HidLayout>(item-1);
    }else if(id==31){
        if(snapshot_.devices.empty())return;HMENU menu=CreatePopupMenu();for(unsigned i=0;i<snapshot_.devices.size();++i)AppendMenuW(menu,MF_STRING|(i==device_?MF_CHECKED:0),i+1,(snapshot_.devices[i].name+L" / "+snapshot_.devices[i].family).c_str());POINT p{static_cast<LONG>((width_-470)*dpi_/96),static_cast<LONG>((height_-197)*dpi_/96)};ClientToScreen(window_,&p);const auto item=TrackPopupMenu(menu,TPM_RETURNCMD,p.x,p.y,0,window_,nullptr);DestroyMenu(menu);if(item)device_=item-1;InvalidateRect(window_,nullptr,FALSE);return;
    }else if(id==32){capture_.Stop();capture_.Start(window_,settings_);return;}
    else return;
    Editors();Changed();
}
void Window::ChooseFile(bool save){
    ComPtr<IFileDialog> dialog;
    Check(CoCreateInstance(save?CLSID_FileSaveDialog:CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)));
    const COMDLG_FILTERSPEC filter{L"Curve Controller settings (*.json)",L"*.json"};Check(dialog->SetFileTypes(1,&filter));dialog->SetDefaultExtension(L"json");dialog->SetTitle(save?L"Export curve settings":L"Import curve settings");if(save)dialog->SetFileName(L"my-curve.json");
    const auto shown=dialog->Show(window_);if(shown==HRESULT_FROM_WIN32(ERROR_CANCELLED))return;Check(shown);
    ComPtr<IShellItem> item;Check(dialog->GetResult(&item));PWSTR path=nullptr;Check(item->GetDisplayName(SIGDN_FILESYSPATH,&path));std::wstring destination=path;CoTaskMemFree(path);
    if(save){WriteJson(destination,settings_);notice_=L"JSON exported.";}else{auto loaded=Parse(ReadJson(destination));settings_=loaded;point_=0;Editors();Changed();notice_=L"JSON imported and applied.";}
}
void Window::Paint(){
    PAINTSTRUCT ps{};BeginPaint(window_,&ps);
    capture_.Acknowledge();if(!offline_)snapshot_=capture_.Read();
    if(!target_){RECT r{};GetClientRect(window_,&r);const auto result=factory_->CreateHwndRenderTarget(D2D1::RenderTargetProperties(),D2D1::HwndRenderTargetProperties(window_,D2D1::SizeU(r.right,r.bottom)),&target_);if(FAILED(result)){EndPaint(window_,&ps);return;}target_->SetDpi(dpi_,dpi_);}
    target_->BeginDraw();Draw(target_.Get());const auto result=target_->EndDraw();if(result==D2DERR_RECREATE_TARGET)target_.Reset();EndPaint(window_,&ps);
}
LRESULT CALLBACK Window::Procedure(HWND h,UINT m,WPARAM w,LPARAM l){
    auto* self=reinterpret_cast<Window*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(m==WM_NCCREATE){self=static_cast<Window*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);self->window_=h;SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
    if(!self)return DefWindowProcW(h,m,w,l);
    try{return self->Message(m,w,l);}catch(const std::exception& e){self->notice_=L"Operation failed: "+Wide(e.what());InvalidateRect(h,nullptr,FALSE);return 0;}
}
LRESULT Window::Message(UINT m,WPARAM w,LPARAM l){
    switch(m){
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:Paint();return 0;
    case WM_SIZE:{RECT r{};GetClientRect(window_,&r);width_=r.right*96.f/dpi_;height_=r.bottom*96.f/dpi_;if(target_)target_->Resize(D2D1::SizeU(r.right,r.bottom));LayoutEdits();InvalidateRect(window_,nullptr,FALSE);return 0;}
    case WM_GETMINMAXINFO:{auto* i=reinterpret_cast<MINMAXINFO*>(l);i->ptMinTrackSize={static_cast<LONG>(1080*dpi_/96),static_cast<LONG>(810*dpi_/96)};return 0;}
    case WM_DPICHANGED:{dpi_=static_cast<float>(HIWORD(w));auto r=reinterpret_cast<RECT*>(l);if(target_)target_->SetDpi(dpi_,dpi_);SetWindowPos(window_,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOACTIVATE|SWP_NOZORDER);LayoutEdits();return 0;}
    case WM_LBUTTONDOWN:{SetFocus(window_);const float x=GET_X_LPARAM(l)*96.f/dpi_,y=GET_Y_LPARAM(l)*96.f/dpi_;const int point=PointAt(x,y);if(point>=0){point_=static_cast<unsigned>(point);dragging_=true;SetCapture(window_);Editors();InvalidateRect(window_,nullptr,FALSE);}else{focus_=HitTest(x,y);Action(focus_);}return 0;}
    case WM_LBUTTONUP:if(dragging_){dragging_=false;ReleaseCapture();}return 0;
    case WM_CAPTURECHANGED:dragging_=false;return 0;
    case WM_MOUSEMOVE:{const float x=GET_X_LPARAM(l)*96.f/dpi_,y=GET_Y_LPARAM(l)*96.f/dpi_;if(dragging_){const auto p=Plot();MovePoint(settings_,point_,{(x-p.left)/(p.right-p.left),(p.bottom-y)/(p.bottom-p.top)});Editors();Changed();}else{const auto old=hover_;hover_=HitTest(x,y);if(old!=hover_){animationStart_=GetTickCount64();SetTimer(window_,1,16,nullptr);InvalidateRect(window_,nullptr,FALSE);}}return 0;}
    case WM_SETCURSOR:if(LOWORD(l)==HTCLIENT){POINT p{};GetCursorPos(&p);ScreenToClient(window_,&p);const float x=p.x*96.f/dpi_,y=p.y*96.f/dpi_;SetCursor(LoadCursorW(nullptr,dragging_?IDC_SIZEALL:HitTest(x,y)>=0||PointAt(x,y)>=0?IDC_HAND:IDC_ARROW));return TRUE;}break;
    case WM_KEYDOWN:
        if(w==VK_TAB&&!hits_.empty()){auto it=std::find_if(hits_.begin(),hits_.end(),[&](const auto& h){return h.id==focus_;});size_t i=it==hits_.end()?0:static_cast<size_t>(it-hits_.begin()+1)%hits_.size();focus_=hits_[i].id;InvalidateRect(window_,nullptr,FALSE);return 0;}
        if(w==VK_SPACE||w==VK_RETURN){Action(focus_);return 0;}
        if(settings_.preset==Preset::Custom&&(w==VK_LEFT||w==VK_RIGHT||w==VK_UP||w==VK_DOWN)){auto p=settings_.points[point_];const double step=(GetKeyState(VK_SHIFT)&0x8000)?0.001:0.01;if(w==VK_LEFT)p.x-=step;if(w==VK_RIGHT)p.x+=step;if(w==VK_UP)p.y+=step;if(w==VK_DOWN)p.y-=step;MovePoint(settings_,point_,p);Editors();Changed();return 0;}break;
    case WM_COMMAND:
        if(HIWORD(w)==EN_KILLFOCUS&&(LOWORD(w)==501||LOWORD(w)==502)){Editors();return 0;}
        if(!editing_&&HIWORD(w)==EN_CHANGE&&(LOWORD(w)==501||LOWORD(w)==502)){
            wchar_t x[32]{},y[32]{};GetWindowTextW(editX_,x,32);GetWindowTextW(editY_,y,32);wchar_t *xe=nullptr,*ye=nullptr;const auto a=wcstod(x,&xe),b=wcstod(y,&ye);if(xe!=x&&ye!=y&&!*xe&&!*ye&&std::isfinite(a)&&std::isfinite(b)){MovePoint(settings_,point_,{a/100,b/100});Changed();}return 0;
        }break;
    case WM_CTLCOLOREDIT:{const HDC dc=reinterpret_cast<HDC>(w);SetTextColor(dc,RGB(242,246,255));SetBkColor(dc,RGB(26,41,66));SetDCBrushColor(dc,RGB(26,41,66));return reinterpret_cast<LRESULT>(GetStockObject(DC_BRUSH));}
    case InputMessage:InvalidateRect(window_,nullptr,FALSE);return 0;
    case WM_TIMER:
        if(w==1){animation_=std::min(1.f,static_cast<float>(GetTickCount64()-animationStart_)/150);if(animation_>=1)KillTimer(window_,1);InvalidateRect(window_,nullptr,FALSE);}
        if(w==2){KillTimer(window_,2);try{SavePreferences(settings_);dirty_=false;notice_=L"Preferences saved.";}catch(const std::exception& e){notice_=L"Autosave failed: "+Wide(e.what());}InvalidateRect(window_,nullptr,FALSE);}return 0;
    case WM_CLOSE:if(!offline_&&dirty_){try{SavePreferences(settings_);}catch(...){}}capture_.Stop();DestroyWindow(window_);return 0;
    case WM_DESTROY:PostQuitMessage(0);return 0;
    case WM_NCDESTROY:{const auto old=window_;SetWindowLongPtrW(old,GWLP_USERDATA,0);window_=nullptr;return DefWindowProcW(old,m,w,l);}
    }
    return DefWindowProcW(window_,m,w,l);
}
void Window::PreviewSettings(const Settings& s){
    if(!offline_||!Valid(s))throw std::logic_error("Valid offline preview settings required");settings_=s;
    if(!snapshot_.devices.empty()){Response r(s);auto& sample=snapshot_.devices[0].sample;for(unsigned i=0;i<4;++i)sample.out[i]=(i<2?s.left:s.right)?r.Map(sample.in[i]):sample.in[i];}
}
void Window::RenderPng(const std::wstring& path){
    ComPtr<IWICImagingFactory> wic;Check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&wic)));
    ComPtr<IWICBitmap> bitmap;Check(wic->CreateBitmap(static_cast<UINT>(width_),static_cast<UINT>(height_),GUID_WICPixelFormat32bppPBGRA,WICBitmapCacheOnLoad,&bitmap));
    ComPtr<ID2D1RenderTarget> rt;Check(factory_->CreateWicBitmapRenderTarget(bitmap.Get(),D2D1::RenderTargetProperties(),&rt));rt->BeginDraw();Draw(rt.Get());Check(rt->EndDraw());
    ComPtr<IWICStream> stream;Check(wic->CreateStream(&stream));Check(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE));
    ComPtr<IWICBitmapEncoder> encoder;Check(wic->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder));Check(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache));
    ComPtr<IWICBitmapFrameEncode> frame;Check(encoder->CreateNewFrame(&frame,nullptr));Check(frame->Initialize(nullptr));Check(frame->SetSize(static_cast<UINT>(width_),static_cast<UINT>(height_)));WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;Check(frame->SetPixelFormat(&format));
    ComPtr<IWICFormatConverter> converter;Check(wic->CreateFormatConverter(&converter));Check(converter->Initialize(bitmap.Get(),GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));Check(frame->WriteSource(converter.Get(),nullptr));Check(frame->Commit());Check(encoder->Commit());
}
unsigned Window::SelfTest(){
    unsigned checks=0;auto expect=[&](bool ok){++checks;if(!ok)throw std::runtime_error("UI state check failed.");};
    Paint();expect(hits_.size()>=19);expect(offline_);
    for(unsigned i=0;i<12;++i){Action(100+static_cast<int>(i));expect(static_cast<unsigned>(settings_.preset)==i);expect(Valid(settings_));}
    Action(112);expect(settings_.preset==Preset::Custom);MovePoint(settings_,0,{0.1,0.5});expect(std::abs(Response(settings_).Map(0.1)-0.5)<1e-12);
    const bool old=settings_.left;Action(20);expect(settings_.left!=old);Action(20);expect(settings_.left==old);Action(21);expect(!settings_.right);Action(21);expect(settings_.right);
    expect(HitTest(60,165)==20);expect(PointAt(Plot().left+static_cast<float>(settings_.points[0].x)*(Plot().right-Plot().left),Plot().bottom-static_cast<float>(settings_.points[0].y)*(Plot().bottom-Plot().top))==0);
    Action(102);Action(112);expect(std::abs(Response(settings_).Map(0.1)-0.5)<1e-12);
    settings_=Parse(Serialize(settings_));expect(Valid(settings_));Paint();return checks;
}
}
