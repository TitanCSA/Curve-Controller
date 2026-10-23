#include "Transform.h"
#include <hidsdi.h>
#include <hidpi.h>
#include <algorithm>
#include <cmath>

namespace Curve {
void HidDescriptor::Load(const std::vector<unsigned char>& data){
    fields.clear();descriptorValid=false;if(data.empty())return;
    auto parsed=reinterpret_cast<PHIDP_PREPARSED_DATA>(const_cast<unsigned char*>(data.data()));HIDP_CAPS caps{};
    if(HidP_GetCaps(parsed,&caps)!=HIDP_STATUS_SUCCESS||caps.NumberInputValueCaps>1024)return;
    USHORT count=caps.NumberInputValueCaps;std::vector<HIDP_VALUE_CAPS> values(count);
    if(count && HidP_GetValueCaps(HidP_Input,values.data(),&count,parsed)!=HIDP_STATUS_SUCCESS)return;
    values.resize(count);
    for(const auto& c:values){
        if(c.IsAlias||!c.IsAbsolute||c.UsagePage!=1)continue;
        const unsigned low=c.IsRange?c.Range.UsageMin:c.NotRange.Usage,high=c.IsRange?c.Range.UsageMax:low;
        if(high<low||high-low>255)return;
        for(unsigned usage=low;usage<=high;++usage)fields.push_back({c.UsagePage,usage,c.LinkCollection,c.ReportID});
    }
    descriptorValid=true;
}
bool HidDescriptor::Has(const Interceptor::Hid_Value& a,unsigned usage) const noexcept {
    for(const auto& f:fields)if(f.page==1 && f.usage==usage && f.collection==a.link_collection)return true;
    return false;
}
AxisSample Transform(IC_NATIVE_GAMEPAD_STATE& s,const Response& r) noexcept {
    AxisSample sample;sample.valid=sample.updated=15;sample.paired=3;short* axes[]{&s.left_x,&s.left_y,&s.right_x,&s.right_y};
    for(unsigned stick=0;stick<2;++stick){
        const unsigned i=stick*2;
        for(unsigned a=i;a<i+2;++a)sample.in[a]=*axes[a]<0?*axes[a]/32768.0:*axes[a]/32767.0;
        const auto output=(stick?r.settings.right:r.settings.left)?r.MapStick(sample.in[i],sample.in[i+1]):Point{sample.in[i],sample.in[i+1]};
        sample.out[i]=output.x;sample.out[i+1]=output.y;
        for(unsigned a=i;a<i+2;++a){
            if(sample.out[a]!=sample.in[a])*axes[a]=static_cast<short>(std::lround(std::clamp(sample.out[a],-1.0,1.0)*(sample.out[a]<0?32768.0:32767.0)));
            sample.out[a]=*axes[a]<0?*axes[a]/32768.0:*axes[a]/32767.0;
        }
    }
    return sample;
}
namespace {
using Value=Interceptor::Hid_Value;
bool Eligible(const Value& a) noexcept {return a.usage_page==1&&a.normalized_valid&&!a.relative&&!a.is_null&&a.maximum>a.minimum&&std::isfinite(a.normalized);}
bool Identity(const Value& a,const Value& b) noexcept {return a.usage_page==b.usage_page&&a.usage==b.usage&&a.link_collection==b.link_collection&&a.element==b.element&&a.report_id==b.report_id&&a.data_index==b.data_index;}
const Value* Unique(const HidReportSet& reports,const Value& scope,unsigned usage) noexcept {
    const Value* found=nullptr;
    for(unsigned i=0;i<reports.count;++i)for(const auto& a:reports.reports[i]->axes)
        if(a.usage_page==1&&a.usage==usage&&a.link_collection==scope.link_collection&&a.element==scope.element){if(found)return nullptr;found=&a;}
    return found;
}
Value* Local(Interceptor::Hid_State& state,const Value& raw) noexcept {for(auto& a:state.axes)if(Identity(a,raw))return &a;return nullptr;}
double Representable(const Value& a,double normalized) noexcept {
    const double low=static_cast<double>(a.minimum),high=static_cast<double>(a.maximum);
    const double raw=std::clamp(std::round(low+(normalized+1)*0.5*(high-low)),low,high);
    return 2*(raw-low)/(high-low)-1;
}
}
AxisSample Transform(Interceptor::Hid_State& s,const Response& r,const HidDescriptor& layout,const HidReportSet& physical) noexcept {
    AxisSample sample;
    auto reports=physical;
    bool included=false;
    if(!s.axes.empty())for(unsigned i=0;i<reports.count;++i)if(!reports.reports[i]->axes.empty()&&reports.reports[i]->axes.front().report_id==s.axes.front().report_id){included=true;break;}
    if(!included)reports.Add(s);
    for(auto& a:s.axes){
        if(a.usage_page!=1||a.relative)continue;
        int axis=-1;
        auto has=[&](unsigned usage){if(layout.Has(a,usage))return true;for(unsigned i=0;i<reports.count;++i)for(const auto& b:reports.reports[i]->axes)if(b.usage_page==1&&b.usage==usage&&b.link_collection==a.link_collection&&b.element==a.element)return true;return false;};
        const bool zr=r.settings.layout==HidLayout::ZRz || (r.settings.layout==HidLayout::Auto&&has(0x32)&&has(0x35));
        if(a.usage==0x30)axis=0;else if(a.usage==0x31)axis=1;
        else if(a.usage==(zr?0x32u:0x33u))axis=2;else if(a.usage==(zr?0x35u:0x34u))axis=3;
        if(axis<0)continue;
        const unsigned i=static_cast<unsigned>(axis),stick=i/2;
        sample.updated|=1u<<i;
        if(!Eligible(a))continue;
        const bool enabled=stick?r.settings.right:r.settings.left;
        if(r.settings.mode==ResponseMode::Axes){
            sample.in[i]=a.normalized;
            if(enabled)a.normalized=r.Map(a.normalized);
            sample.out[i]=Representable(a,a.normalized);sample.valid|=1u<<i;
            continue;
        }
        const unsigned ux=stick?(zr?0x32u:0x33u):0x30u,uy=stick?(zr?0x35u:0x34u):0x31u;
        const auto* x=Unique(reports,a,ux);const auto* y=Unique(reports,a,uy);
        if(!x||!y||!Eligible(*x)||!Eligible(*y)){
            // A missing or ambiguous component must not be guessed as zero.
            sample.in[i]=a.normalized;sample.out[i]=Representable(a,a.normalized);sample.valid|=1u<<i;continue;
        }
        auto* localX=Local(s,*x);auto* localY=Local(s,*y);
        if((i&1u)&&localX)continue; // The X iteration transforms both local fields.
        const Point input{x->normalized,y->normalized};
        const auto output=enabled?r.MapStick(input.x,input.y):input;
        const unsigned first=stick*2;
        sample.in[first]=input.x;sample.in[first+1]=input.y;
        sample.out[first]=Representable(*x,output.x);sample.out[first+1]=Representable(*y,output.y);
        sample.valid|=3u<<first;sample.updated|=3u<<first;sample.paired|=1u<<stick;
        if(enabled&&r.settings.preset!=Preset::Linear&&x->report_id!=y->report_id)sample.split|=1u<<stick;
        if(localX)localX->normalized=output.x;
        if(localY)localY->normalized=output.y;
    }
    return sample;
}
void NeutralRelative(Interceptor::Hid_State& s) noexcept {
    for(auto* list:{&s.axes,&s.triggers,&s.hats,&s.values})for(auto& value:*list)if(value.relative){value.value=0;value.is_null=false;}
}
}
