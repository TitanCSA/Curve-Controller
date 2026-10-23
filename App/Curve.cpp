#include "Curve.h"
#include "ThirdParty/json.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace Curve {
const std::array<PresetInfo,13>& Presets() noexcept {
    static constexpr std::array<PresetInfo,13> list{{
        {Preset::Linear,"Linear","linear","One-to-one response. Your original stick values."},
        {Preset::SoftExpo,"Soft exponential","soft-exponential","A little more precision around the center."},
        {Preset::Exponential,"Exponential","exponential","Quadratic response: finer small movements."},
        {Preset::StrongExpo,"Strong exponential","strong-exponential","Cubic response: maximum center precision."},
        {Preset::SoftInverse,"Soft inverse","soft-inverse","A gentle boost to small stick movements."},
        {Preset::Inverse,"Inverse exponential","inverse-exponential","Square-root response: the inverse of quadratic."},
        {Preset::StrongInverse,"Strong inverse","strong-inverse","Cube-root response: the inverse of cubic."},
        {Preset::Smooth,"Smooth S-curve","smooth","Soft center and full deflection, faster in between."},
        {Preset::Smoother,"Smoother S-curve","smoother","A stronger S-curve with smooth end transitions."},
        {Preset::EaseOut,"Ease out","ease-out","Quick initial response, more control near the edge."},
        {Preset::Precision,"Precision","precision","10% inner deadzone, then a soft exponential ramp."},
        {Preset::Fast,"Early saturation","early-saturation","Reach full output at 80% physical deflection."},
        {Preset::Custom,"Custom curve","custom","Drag ten points. Changes take effect immediately."}
    }};
    return list;
}
const PresetInfo& Info(Preset p) noexcept {
    const auto i=static_cast<unsigned>(p);
    return Presets()[i<Presets().size()?i:0];
}
Settings::Settings() noexcept { for(unsigned i=0;i<points.size();++i)points[i]={double(i+1)/11,double(i+1)/11}; }
bool Valid(const Settings& s) noexcept {
    if(static_cast<unsigned>(s.preset)>=Presets().size() || static_cast<unsigned>(s.layout)>2 || static_cast<unsigned>(s.mode)>1)return false;
    double previous=0;
    for(const auto& p:s.points){
        if(!std::isfinite(p.x)||!std::isfinite(p.y)||p.x-previous<0.0001 || p.x>0.9999 || p.y<0 || p.y>1)return false;
        previous=p.x;
    }
    return true;
}
static double Custom(const Settings& s,double x) noexcept {
    std::array<Point,12> p{};p[0]={0,0};p[11]={1,1};
    std::copy(s.points.begin(),s.points.end(),p.begin()+1);
    std::array<double,11> width{},slope{};
    std::array<double,12> tangent{};
    for(unsigned i=0;i<11;++i){width[i]=p[i+1].x-p[i].x;slope[i]=(p[i+1].y-p[i].y)/width[i];}
    // Shape-preserving cubic Hermite interpolation. Each segment remains
    // between its endpoints, including deliberately non-monotone user curves.
    for(unsigned i=1;i<11;++i){
        if(slope[i-1]*slope[i]<=0)tangent[i]=0;
        else {const double a=2*width[i]+width[i-1],b=width[i]+2*width[i-1];tangent[i]=(a+b)/(a/slope[i-1]+b/slope[i]);}
    }
    auto endpoint=[](double h0,double h1,double d0,double d1){
        double m=((2*h0+h1)*d0-h0*d1)/(h0+h1);
        if(m*d0<=0)return 0.0;
        if(d0*d1<0 && std::abs(m)>std::abs(3*d0))m=3*d0;
        return m;
    };
    tangent[0]=endpoint(width[0],width[1],slope[0],slope[1]);
    tangent[11]=endpoint(width[10],width[9],slope[10],slope[9]);
    unsigned i=0;while(i<10 && x>p[i+1].x)++i;
    const double t=std::clamp((x-p[i].x)/width[i],0.0,1.0),t2=t*t,t3=t2*t;
    return std::clamp((2*t3-3*t2+1)*p[i].y+(t3-2*t2+t)*width[i]*tangent[i]
        +(-2*t3+3*t2)*p[i+1].y+(t3-t2)*width[i]*tangent[i+1],0.0,1.0);
}
double Evaluate(const Settings& s,double x) noexcept {
    if(!std::isfinite(x))return 0;
    x=std::clamp(x,0.0,1.0);if(x==0 || x==1)return x;
    switch(s.preset){
    case Preset::SoftExpo:return std::pow(x,1.5);
    case Preset::Exponential:return x*x;
    case Preset::StrongExpo:return x*x*x;
    case Preset::SoftInverse:return std::pow(x,2.0/3);
    case Preset::Inverse:return std::sqrt(x);
    case Preset::StrongInverse:return std::cbrt(x);
    case Preset::Smooth:return x*x*(3-2*x);
    case Preset::Smoother:return x*x*x*(x*(6*x-15)+10);
    case Preset::EaseOut:return 1-(1-x)*(1-x);
    case Preset::Precision:return std::pow(std::max(0.0,(x-0.1)/0.9),1.5);
    case Preset::Fast:return std::min(1.0,x/0.8);
    case Preset::Custom:return Custom(s,x);
    default:return x;
    }
}
void MakeCustom(Settings& s) noexcept {
    if(s.preset==Preset::Custom)return;
    for(unsigned i=0;i<10;++i){const double x=double(i+1)/11;s.points[i]={x,Evaluate(s,x)};}
    s.preset=Preset::Custom;
}
void MovePoint(Settings& s,unsigned index,Point p) noexcept {
    if(index>=10 || !std::isfinite(p.x)||!std::isfinite(p.y))return;
    const double lower=index?s.points[index-1].x+0.00011:0.00011;
    const double upper=index<9?s.points[index+1].x-0.00011:0.99989;
    s.points[index]={std::clamp(p.x,lower,upper),std::clamp(p.y,0.0,1.0)};
    s.preset=Preset::Custom;
}
Settings Parse(std::string_view text){
    using Json=nlohmann::json;
    if(text.empty()||text.size()>65536)throw std::invalid_argument("JSON must contain 1 to 65,536 bytes.");
    const auto j=Json::parse(text,[](int depth,Json::parse_event_t,const Json&){if(depth>12)throw std::invalid_argument("JSON nesting exceeds the settings schema.");return true;},true,false);
    if(!j.is_object()||(j.size()!=5&&j.size()!=6)||(j.size()==6&&!j.contains("mode"))||!j.contains("schema")||j["schema"]!=1||!j.contains("preset")||!j.contains("sticks")||!j.contains("hid_layout")||!j.contains("points"))throw std::invalid_argument("Expected a Curve Controller schema 1 settings object.");
    Settings s;const auto name=j.at("preset").get<std::string>();bool found=false;
    if(j.contains("mode")){
        const auto mode=j.at("mode").get<std::string>();
        if(mode=="distance")s.mode=ResponseMode::Distance;else if(mode=="axes")s.mode=ResponseMode::Axes;else throw std::invalid_argument("mode must be distance or axes.");
    }
    for(const auto& p:Presets())if(name==p.key){s.preset=p.id;found=true;break;}
    if(!found)throw std::invalid_argument("Unknown curve preset.");
    const auto& sticks=j.at("sticks");
    if(!sticks.is_object()||sticks.size()!=2||!sticks.contains("left")||!sticks.contains("right")||!sticks["left"].is_boolean()||!sticks["right"].is_boolean())throw std::invalid_argument("sticks requires left and right booleans.");
    s.left=sticks["left"].get<bool>();s.right=sticks["right"].get<bool>();
    const auto layout=j.at("hid_layout").get<std::string>();
    if(layout=="auto")s.layout=HidLayout::Auto;else if(layout=="xy-rxry")s.layout=HidLayout::RxRy;else if(layout=="xy-zrz")s.layout=HidLayout::ZRz;else throw std::invalid_argument("Unknown HID layout.");
    const auto& points=j.at("points");if(!points.is_array()||points.size()!=10)throw std::invalid_argument("Exactly ten editable curve points are required.");
    for(unsigned i=0;i<10;++i){const auto& p=points[i];if(!p.is_object()||p.size()!=2||!p.contains("x")||!p.contains("y")||!p["x"].is_number()||!p["y"].is_number())throw std::invalid_argument("Each point needs numeric x and y percentages.");s.points[i]={p["x"].get<double>()/100,p["y"].get<double>()/100};}
    if(!Valid(s))throw std::invalid_argument("Point X values must increase strictly between 0 and 100%; Y values must be between 0 and 100%.");
    return s;
}
std::string Serialize(const Settings& s){
    if(!Valid(s))throw std::invalid_argument("Invalid curve settings.");
    nlohmann::json j={{"schema",1},{"preset",Info(s.preset).key},{"mode",s.mode==ResponseMode::Distance?"distance":"axes"},{"sticks",{{"left",s.left},{"right",s.right}}},{"hid_layout",s.layout==HidLayout::Auto?"auto":s.layout==HidLayout::RxRy?"xy-rxry":"xy-zrz"},{"points",nlohmann::json::array()}};
    for(const auto& p:s.points)j["points"].push_back({{"x",p.x*100},{"y",p.y*100}});
    return j.dump(2)+"\n";
}
Response::Response(const Settings& s):settings(s){
    if(!Valid(s))throw std::invalid_argument("Invalid response.");
    std::array<double,12> y{},m{};std::array<double,11> h{},d{};
    knots[0]=y[0]=0;knots[11]=y[11]=1;
    for(unsigned i=0;i<10;++i){knots[i+1]=s.points[i].x;y[i+1]=s.points[i].y;}
    for(unsigned i=0;i<11;++i){h[i]=knots[i+1]-knots[i];d[i]=(y[i+1]-y[i])/h[i];}
    for(unsigned i=1;i<11;++i)if(d[i-1]*d[i]>0){const auto a=2*h[i]+h[i-1],b=h[i]+2*h[i-1];m[i]=(a+b)/(a/d[i-1]+b/d[i]);}
    auto end=[](double a,double b,double da,double db){double v=((2*a+b)*da-a*db)/(a+b);if(v*da<=0)return 0.0;if(da*db<0&&std::abs(v)>std::abs(3*da))v=3*da;return v;};
    m[0]=end(h[0],h[1],d[0],d[1]);m[11]=end(h[10],h[9],d[10],d[9]);
    for(unsigned i=0;i<11;++i)coefficients[i]={y[i],h[i]*m[i],3*(y[i+1]-y[i])-h[i]*(2*m[i]+m[i+1]),2*(y[i]-y[i+1])+h[i]*(m[i]+m[i+1])};
}
double Response::Map(double x) const noexcept {
    if(!std::isfinite(x))return 0;
    const double magnitude=std::min(std::abs(x),1.0);
    if(settings.preset!=Preset::Custom)return std::copysign(Evaluate(settings,magnitude),x);
    const auto next=std::upper_bound(knots.begin(),knots.end(),magnitude);
    const auto i=static_cast<unsigned>(std::min<std::ptrdiff_t>(10,std::max<std::ptrdiff_t>(0,next-knots.begin()-1)));
    const auto t=(magnitude-knots[i])/(knots[i+1]-knots[i]);const auto& c=coefficients[i];
    return std::copysign(std::clamp(((c[3]*t+c[2])*t+c[1])*t+c[0],0.0,1.0),x);
}
Point Response::MapStick(double x,double y) const noexcept {
    if(settings.mode==ResponseMode::Axes)return {Map(x),Map(y)};
    // Keep neutral, linear and square-gate outer corners exactly unchanged.
    // Inside the unit circle, change length only; never change stick direction.
    if(settings.preset==Preset::Linear||!std::isfinite(x)||!std::isfinite(y))return {x,y};
    const double distance=std::hypot(x,y);
    if(distance==0||distance>=1)return {x,y};
    const double mapped=Map(distance);
    return {x/distance*mapped,y/distance*mapped};
}
} // namespace Curve
