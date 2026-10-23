#pragma once
#include <array>
#include <string>
#include <string_view>

namespace Curve {
enum class Preset { Linear, SoftExpo, Exponential, StrongExpo, SoftInverse,
    Inverse, StrongInverse, Smooth, Smoother, EaseOut, Precision, Fast, Custom };
enum class HidLayout { Auto, RxRy, ZRz };
enum class ResponseMode { Distance, Axes };
struct Point { double x=0, y=0; };
struct PresetInfo { Preset id; const char* name; const char* key; const char* description; };
const std::array<PresetInfo,13>& Presets() noexcept;
const PresetInfo& Info(Preset) noexcept;
struct Settings {
    Preset preset=Preset::Linear;
    bool left=true, right=true;
    HidLayout layout=HidLayout::Auto;
    ResponseMode mode=ResponseMode::Distance;
    std::array<Point,10> points{}; // Ten editable points, plus implicit (0,0)/(1,1).
    Settings() noexcept;
};
bool Valid(const Settings&) noexcept;
double Evaluate(const Settings&,double magnitude) noexcept;
void MakeCustom(Settings&) noexcept;
void MovePoint(Settings&,unsigned index,Point) noexcept;
Settings Parse(std::string_view json);
std::string Serialize(const Settings&);
struct Response {
    Settings settings;
    std::array<double,12> knots{};
    std::array<std::array<double,4>,11> coefficients{};
    explicit Response(const Settings&);
    double Map(double signedInput) const noexcept;
    Point MapStick(double x,double y) const noexcept;
};
} // namespace Curve
