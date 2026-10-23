#pragma once
#include "Curve.h"
#include <string>
namespace Curve {
inline constexpr auto PreferencesKey=L"Software\\TitanCSA\\CurveController";
Settings LoadPreferences(std::wstring& notice,const wchar_t* key=PreferencesKey);
void SavePreferences(const Settings&,const wchar_t* key=PreferencesKey);
std::string ReadJson(const std::wstring& path);
void WriteJson(const std::wstring& path,const Settings&);
}
