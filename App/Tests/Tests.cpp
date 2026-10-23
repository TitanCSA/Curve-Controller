#include "Curve.h"
#include "Transform.h"
#include "Storage.h"
#include "ThirdParty/json.hpp"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>

namespace {
unsigned checks=0;
void Check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
template<class F> void Reject(F f){bool rejected=false;try{f();}catch(...){rejected=true;}Check(rejected,"Malformed JSON must be rejected");}
void Curves(){
    for(const auto& preset:Curve::Presets()){
        Curve::Settings s;s.preset=preset.id;Curve::Response r(s);double before=0;
        for(unsigned i=0;i<=20000;++i){const double x=double(i)/20000,y=r.Map(x);Check(std::isfinite(y)&&y>=0&&y<=1,"Bounded finite response");Check(std::abs(y-Curve::Evaluate(s,x))<1e-10,"Compiled curve matches the drawn mathematical response");Check(std::abs(y+r.Map(-x))<1e-12,"Negative axes preserve symmetric sign");Check(y+1e-12>=before,"Every standard preset is monotone");before=y;}
        Check(r.Map(0)==0&&std::abs(r.Map(1)-1)<1e-12,"Neutral/full deflection preserved");
        const auto restored=Curve::Parse(Curve::Serialize(s));Check(restored.preset==s.preset,"Preset roundtrip");
    }
    Curve::Settings custom;Curve::MakeCustom(custom);Curve::MovePoint(custom,0,{0.1,0.5});Curve::Response response(custom);
    Check(std::abs(response.Map(0.1)-0.5)<1e-12,"10 percent physical maps exactly to 50 percent output");
    for(const auto& p:custom.points)Check(std::abs(response.Map(p.x)-p.y)<1e-10,"Custom interpolation passes through every editable point");
    std::mt19937 random(0xCCA2u);std::uniform_real_distribution<double> unit(0,1);
    for(unsigned run=0;run<500;++run){for(unsigned i=0;i<10;++i)custom.points[i]={double(i+1)/11,unit(random)};Curve::Response curve(custom);
        for(unsigned i=0;i<1000;++i){const double x=unit(random),v=curve.Map(x);Check(v>=0&&v<=1&&std::abs(v-Curve::Evaluate(custom,x))<1e-10,"Random non-monotone custom curve stays bounded and matches graph");}
    }
    auto json=nlohmann::json::parse(Curve::Serialize(custom));
    Reject([&]{Curve::Parse("");});Reject([&]{Curve::Parse(std::string(65537,' '));});
    Reject([&]{auto j=json;j["schema"]=2;Curve::Parse(j.dump());});
    Reject([&]{auto j=json;j["preset"]="not-a-preset";Curve::Parse(j.dump());});
    Reject([&]{auto j=json;j["sticks"]["left"]=1;Curve::Parse(j.dump());});
    Reject([&]{auto j=json;j["points"][1]["x"]=j["points"][0]["x"];Curve::Parse(j.dump());});
    Reject([&]{auto j=json;j["points"][0]["y"]=101;Curve::Parse(j.dump());});
    Reject([&]{auto j=json;j["points"][0]["x"]=0;Curve::Parse(j.dump());});
    Reject([&]{auto j=json;j["points"].erase(0);Curve::Parse(j.dump());});
    Reject([&]{auto j=json;j["points"][0]["y"]=nullptr;Curve::Parse(j.dump());});
    Reject([&]{auto j=json;j["hiddenCommand"]="execute";Curve::Parse(j.dump());});
    Reject([&]{auto j=json;j["mode"]="unknown";Curve::Parse(j.dump());});
    Reject([&]{auto j=json;j["mode"]=true;Curve::Parse(j.dump());});
    Check(Curve::Settings{}.mode==Curve::ResponseMode::Distance,"Distance from center is the default");
    auto legacy=json;legacy.erase("mode");Check(Curve::Parse(legacy.dump()).mode==Curve::ResponseMode::Distance,"Older files without a mode use the requested radial default");
    auto axes=custom;axes.mode=Curve::ResponseMode::Axes;Check(Curve::Parse(Curve::Serialize(axes)).mode==Curve::ResponseMode::Axes,"Explicit X/Y mode survives JSON roundtrip");
    Reject([&]{Curve::Parse(std::string(500,'[')+std::string(500,']'));});
    for(unsigned i=0;i<10000;++i){auto s=custom;Curve::MovePoint(s,i%10,{unit(random)*3-1,unit(random)*3-1});Check(Curve::Valid(s),"Dragging never crosses adjacent points or leaves bounds");}
}
void Native(){
    Curve::Settings settings;Curve::Response linear(settings);
    for(int value=-32768;value<=32767;++value){IC_NATIVE_GAMEPAD_STATE input{};input.packet=0xABCDEF;input.buttons=0xA5C3;input.left_trigger=233;input.right_trigger=1000;input.extra_buttons=0x81;input.left_x=static_cast<short>(value);input.left_y=static_cast<short>(-value-1);input.right_x=input.left_y;input.right_y=input.left_x;const auto before=input;
        Curve::Transform(input,linear);Check(std::memcmp(&input,&before,sizeof(input))==0,"Linear is bit-exact across all 16-bit stick values");
    }
    settings.mode=Curve::ResponseMode::Axes;settings.preset=Curve::Preset::Inverse;settings.left=false;Curve::Response inverse(settings);
    for(int value=-32768;value<=32767;value+=7){IC_NATIVE_GAMEPAD_STATE input{};input.buttons=0x1337;input.left_trigger=174;input.right_trigger=201;input.extra_buttons=1;input.left_x=input.left_y=input.right_x=input.right_y=static_cast<short>(value);const auto before=input;
        auto sample=Curve::Transform(input,inverse);Check(input.left_x==before.left_x&&input.left_y==before.left_y,"Unchecked stick is never transformed");Check(input.buttons==before.buttons&&input.left_trigger==before.left_trigger&&input.right_trigger==before.right_trigger&&input.extra_buttons==before.extra_buttons,"Buttons and triggers are never remapped");Check(sample.valid==15,"Native axis availability is explicit");
        const double x=value<0?value/32768.0:value/32767.0;Check(std::abs(sample.out[2]-std::copysign(std::sqrt(std::abs(x)),x))<1.0/32767,"Inverse near center is not approximated by a low-resolution lookup table");
    }
}
Interceptor::Hid_Value Axis(unsigned usage,unsigned report=1,unsigned collection=0){Interceptor::Hid_Value a;a.usage_page=1;a.usage=static_cast<USHORT>(usage);a.report_id=static_cast<UCHAR>(report);a.link_collection=static_cast<USHORT>(collection);a.kind=Interceptor::Hid_Value_Kind::Axis;a.minimum=0;a.maximum=255;a.normalized_valid=true;a.normalized=0.5;a.value=191;return a;}
void Hid(){
    Curve::Settings s;s.mode=Curve::ResponseMode::Axes;s.preset=Curve::Preset::Exponential;s.left=false;Curve::Response curve(s);Curve::HidDescriptor descriptor;
    descriptor.fields={{1,0x30,0,1},{1,0x31,0,1},{1,0x32,0,2},{1,0x35,0,3}};
    Interceptor::Hid_State state;state.axes={Axis(0x30),Axis(0x31),Axis(0x32,2)};auto trigger=Axis(0x33);trigger.kind=Interceptor::Hid_Value_Kind::Trigger;state.triggers.push_back(trigger);state.buttons.push_back({});state.buttons[0].pressed=true;
    auto sample=Curve::Transform(state,curve,descriptor);Check(sample.valid==7,"Descriptor establishes the right stick across split report IDs");Check(state.axes[0].normalized==0.5&&state.axes[2].normalized==0.25,"HID left/right selection uses descriptor usages");Check(state.triggers[0].normalized==0.5&&state.buttons[0].pressed,"HID buttons and identified triggers preserved");
    state.axes={Axis(0x32)};Curve::HidDescriptor unknown;Curve::Transform(state,curve,unknown);Check(state.axes[0].normalized==0.5,"A lone Z axis is not guessed to be a right stick");
    state.axes={Axis(0x33),Axis(0x34)};Curve::Transform(state,curve,unknown);Check(state.axes[0].normalized==0.25,"Common Rx/Ry layout supported");
    state.axes={Axis(0x33)};state.axes[0].relative=true;const auto original=state.axes[0].normalized;Curve::Transform(state,curve,unknown);Check(state.axes[0].normalized==original,"Relative axes not treated as sticks");Curve::NeutralRelative(state);Check(state.axes[0].value==0,"Cached relative fields are neutralized for settings-only replay");
    state.axes={Axis(0x30)};state.axes[0].normalized=std::numeric_limits<double>::quiet_NaN();s.left=true;Curve::Response all(s);Check(!Curve::Transform(state,all,unknown).valid,"Invalid decoded axes stay explicitly unknown");
}
void Radial(){
    Curve::Settings custom;Curve::MakeCustom(custom);Curve::MovePoint(custom,0,{0.1,0.5});Curve::Response response(custom);
    for(unsigned angle=0;angle<720;++angle){const double a=angle*6.283185307179586/720,x=0.1*std::cos(a),y=0.1*std::sin(a);const auto output=response.MapStick(x,y);
        Check(std::abs(std::hypot(output.x,output.y)-0.5)<1e-12,"Ten percent distance maps to fifty percent at every angle");
        Check(std::abs(output.x*y-output.y*x)<1e-12,"Custom radial curves preserve direction and quadrants");
    }
    std::mt19937 random(0x1234u);std::uniform_real_distribution<double> value(-1,1);
    for(const auto& preset:Curve::Presets()){
        Curve::Settings s;s.preset=preset.id;Curve::Response curve(s);
        for(unsigned i=0;i<8000;++i){const double x=value(random),y=value(random),distance=std::hypot(x,y);const auto output=curve.MapStick(x,y);
            Check(std::isfinite(output.x)&&std::isfinite(output.y)&&std::abs(output.x)<=1&&std::abs(output.y)<=1,"Random stick pairs remain finite and in range");
            const double expected=distance>=1?distance:Curve::Evaluate(s,distance);
            Check(std::abs(std::hypot(output.x,output.y)-expected)<1e-11,"Radial magnitude matches the plotted preset");
            Check(std::abs(output.x*y-output.y*x)<1e-12&&output.x*x+output.y*y>=0,"Direction is preserved in all quadrants");
            if(distance>=1||preset.id==Curve::Preset::Linear)Check(output.x==x&&output.y==y,"Linear and square-gate corners are unchanged");
        }
        const auto neutral=curve.MapStick(0,0);Check(neutral.x==0&&neutral.y==0,"Neutral stays neutral");
        const auto tiny=curve.MapStick(std::numeric_limits<double>::denorm_min(),0);Check(std::isfinite(tiny.x)&&tiny.y==0,"Subnormal input does not overflow radial scaling");
    }
    Curve::Settings s;s.preset=Curve::Preset::Exponential;Curve::Response expo(s);
    IC_NATIVE_GAMEPAD_STATE input{};input.left_x=9830;input.left_y=-13107;input.right_x=6553;input.right_y=0;input.buttons=0x1234;input.left_trigger=999;
    const auto sample=Curve::Transform(input,expo);
    Check(std::abs(int(input.left_x)-4915)<=1&&std::abs(int(input.left_y)+6554)<=1,"Native diagonal uses joint radius, rather than squaring each axis");
    Check(sample.paired==3&&input.buttons==0x1234&&input.left_trigger==999,"Native radial transform preserves other controls and exposes pairs");
    Curve::HidDescriptor descriptor;
    Interceptor::Hid_State hid;hid.axes={Axis(0x31),Axis(0x30)};hid.axes[0].normalized=-0.4;hid.axes[1].normalized=0.3;
    const auto hs=Curve::Transform(hid,expo,descriptor);
    Check(std::abs(hid.axes[1].normalized-0.15)<1e-12&&std::abs(hid.axes[0].normalized+0.2)<1e-12,"HID radial transform is independent of field order and does not transform twice");
    Check(hs.paired==1&&std::abs(hs.in[0]-0.3)<1e-12&&std::abs(hs.in[1]+0.4)<1e-12,"HID preview retains the true physical pair");
    Interceptor::Hid_State rawX,rawY;rawX.axes={Axis(0x30,1,3)};rawY.axes={Axis(0x31,2,3)};rawX.axes[0].normalized=0.3;rawY.axes[0].normalized=0.4;
    Curve::HidReportSet reports;reports.Add(rawX);reports.Add(rawY);
    auto edited=rawY;const auto split=Curve::Transform(edited,expo,descriptor,reports);
    Check(std::abs(edited.axes[0].normalized-0.2)<1e-12&&split.paired==1&&split.split==1,"Axes in separate HID reports use the last observed physical counterpart");
    Check(rawX.axes[0].normalized==0.3&&rawY.axes[0].normalized==0.4,"Radial computation never feeds modified values back into the physical cache");
    rawX.axes[0].normalized=0;edited=rawY;Curve::Transform(edited,expo,descriptor,reports);Check(std::abs(edited.axes[0].normalized-0.16)<1e-12,"A held Y component changes when only physical X changes");
    edited=rawX;Curve::HidReportSet incomplete;incomplete.Add(rawX);const auto waiting=Curve::Transform(edited,expo,descriptor,incomplete);Check(!waiting.paired&&edited.axes[0].normalized==rawX.axes[0].normalized,"Missing counterpart is not guessed as zero");
    auto duplicate=rawY;duplicate.axes[0].report_id=3;reports.Add(duplicate);edited=rawX;Check(!Curve::Transform(edited,expo,descriptor,reports).paired,"Ambiguous repeated usages are not paired across different report identities");
    reports.count=2;rawY.axes[0].is_null=true;edited=rawY;const auto invalid=Curve::Transform(edited,expo,descriptor,reports);Check(!invalid.valid&&invalid.updated==2&&!invalid.paired,"Null axes invalidate their preview instead of leaving a fabricated center value");
    hid.axes={Axis(0x30,1,4),Axis(0x31,1,4),Axis(0x30,1,5),Axis(0x31,1,5)};hid.axes[0].normalized=0.3;hid.axes[1].normalized=0.4;hid.axes[2].normalized=0.1;hid.axes[3].normalized=0;
    Curve::Transform(hid,expo,descriptor);Check(std::abs(hid.axes[0].normalized-0.15)<1e-12&&std::abs(hid.axes[2].normalized-0.01)<1e-12,"Repeated X/Y groups in distinct HID collections remain independent");
}
void Storage(){
    wchar_t folder[MAX_PATH]{},file[MAX_PATH]{};Check(GetTempPathW(MAX_PATH,folder)>0&&GetTempFileNameW(folder,L"ccv",0,file)!=0,"Temporary export path");
    Curve::Settings s;s.preset=Curve::Preset::StrongInverse;s.right=false;s.mode=Curve::ResponseMode::Axes;
    Curve::WriteJson(file,s);const auto read=Curve::Parse(Curve::ReadJson(file));Check(read.preset==s.preset&&read.right==s.right,"Unicode Windows path export/import roundtrip");Check(DeleteFileW(file)!=FALSE,"Owned temporary JSON removed");
    const auto key=std::wstring(L"Software\\TitanCSA\\CurveControllerTests\\")+std::to_wstring(GetCurrentProcessId());
    try{
        std::wstring notice;Curve::SavePreferences(s,key.c_str());const auto restored=Curve::LoadPreferences(notice,key.c_str());Check(restored.preset==s.preset&&!restored.right&&restored.mode==s.mode,"Actual registry autosave retains the response mode in isolated fixture");
        HKEY h=nullptr;Check(RegOpenKeyExW(HKEY_CURRENT_USER,key.c_str(),0,KEY_SET_VALUE,&h)==ERROR_SUCCESS,"Open isolated registry fixture");DWORD invalid=42;const auto code=RegSetValueExW(h,L"Settings",0,REG_DWORD,reinterpret_cast<const BYTE*>(&invalid),sizeof(invalid));RegCloseKey(h);Check(code==ERROR_SUCCESS,"Invalid type fixture");
        const auto fallback=Curve::LoadPreferences(notice,key.c_str());Check(fallback.preset==Curve::Preset::Linear&&notice.find(L"invalid")!=std::wstring::npos,"Invalid saved preference type returns explicit safe defaults");
    }catch(...){RegDeleteTreeW(HKEY_CURRENT_USER,key.c_str());throw;}
    Check(RegDeleteTreeW(HKEY_CURRENT_USER,key.c_str())==ERROR_SUCCESS,"Only isolated test registry key removed");
}
}
int main(int argc,char** argv){try{
    Curves();Native();Hid();Radial();Storage();
    if(argc==2){unsigned examples=0;for(const auto& item:std::filesystem::directory_iterator(argv[1]))if(item.path().extension()==".json"){std::ifstream file(item.path());std::string json((std::istreambuf_iterator<char>(file)),{});Curve::Parse(json);++examples;++checks;}Check(examples>=7,"All shipped JSON examples validated");}
    Curve::Settings s;s.preset=Curve::Preset::Custom;Curve::Response r(s);volatile double sink=0;
    const auto start=std::chrono::steady_clock::now();for(unsigned i=0;i<1000000;++i)sink+=r.Map(double(i%65536)/65535);
    const double ns=std::chrono::duration<double,std::nano>(std::chrono::steady_clock::now()-start).count()/1000000;
    std::cout<<"PASS "<<checks<<" curve, JSON, 16-bit, HID, selection and persistence assertions; no driver opened, live preferences untouched (isolated registry fixtures).\nCustom curve evaluation: "<<ns<<" ns/value (CPU-only microbenchmark, not input latency).\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
