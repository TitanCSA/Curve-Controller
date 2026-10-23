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
    AxisSample sample;sample.valid=15;short* axes[]{&s.left_x,&s.left_y,&s.right_x,&s.right_y};
    for(unsigned i=0;i<4;++i){const auto original=*axes[i];const double x=original<0?original/32768.0:original/32767.0;
        sample.in[i]=x;sample.out[i]=(i<2?r.settings.left:r.settings.right)?r.Map(x):x;
        if(sample.out[i]!=x)*axes[i]=static_cast<short>(std::lround(std::clamp(sample.out[i],-1.0,1.0)*(sample.out[i]<0?32768.0:32767.0)));
        sample.out[i]=*axes[i]<0?*axes[i]/32768.0:*axes[i]/32767.0;
    }
    return sample;
}
AxisSample Transform(Interceptor::Hid_State& s,const Response& r,const HidDescriptor& layout) noexcept {
    AxisSample sample;
    for(auto& a:s.axes){
        if(a.usage_page!=1||!a.normalized_valid||a.relative||a.is_null||a.maximum<=a.minimum||!std::isfinite(a.normalized))continue;
        int axis=-1;
        auto has=[&](unsigned usage){if(layout.Has(a,usage))return true;for(const auto& b:s.axes)if(b.usage_page==1&&b.usage==usage&&b.link_collection==a.link_collection&&b.element==a.element)return true;return false;};
        const bool zr=r.settings.layout==HidLayout::ZRz || (r.settings.layout==HidLayout::Auto&&has(0x32)&&has(0x35));
        if(a.usage==0x30)axis=0;else if(a.usage==0x31)axis=1;
        else if(a.usage==(zr?0x32u:0x33u))axis=2;else if(a.usage==(zr?0x35u:0x34u))axis=3;
        if(axis<0)continue;
        const unsigned i=static_cast<unsigned>(axis);sample.in[i]=a.normalized;
        if(i<2?r.settings.left:r.settings.right)a.normalized=r.Map(a.normalized);
        // Display the nearest representable HID value, not an interpolated UI
        // animation or a value at an assumed 8/16-bit resolution.
        const double low=static_cast<double>(a.minimum),high=static_cast<double>(a.maximum);
        const double raw=std::clamp(std::round(low+(a.normalized+1)*0.5*(high-low)),low,high);
        sample.out[i]=2*(raw-low)/(high-low)-1;sample.valid|=1u<<i;
    }
    return sample;
}
void NeutralRelative(Interceptor::Hid_State& s) noexcept {
    for(auto* list:{&s.axes,&s.triggers,&s.hats,&s.values})for(auto& value:*list)if(value.relative){value.value=0;value.is_null=false;}
}
}
