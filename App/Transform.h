#pragma once
#include "Curve.h"
#include "Interceptor.h"
#include <vector>
namespace Curve {
struct AxisSample {std::array<double,4> in{},out{};unsigned valid=0,paired=0,split=0,updated=0;};
// Views into unchanged cached physical reports; no copy or allocation.
struct HidReportSet {
    std::array<const Interceptor::Hid_State*,33> reports{};
    unsigned count=0;
    void Add(const Interceptor::Hid_State& state) noexcept {if(count<reports.size())reports[count++]=&state;}
};
struct HidField {unsigned page=0,usage=0,collection=0,report=0;};
struct HidDescriptor {
    std::vector<HidField> fields;
    bool descriptorValid=false;
    void Load(const std::vector<unsigned char>&);
    bool Has(const Interceptor::Hid_Value&,unsigned usage) const noexcept;
};
AxisSample Transform(IC_NATIVE_GAMEPAD_STATE&,const Response&) noexcept;
AxisSample Transform(Interceptor::Hid_State&,const Response&,const HidDescriptor&,const HidReportSet& physical={}) noexcept;
void NeutralRelative(Interceptor::Hid_State&) noexcept;
}
