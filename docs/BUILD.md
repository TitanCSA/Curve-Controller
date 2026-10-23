# Build and architecture

Use Windows 10/11 x64, Visual Studio 2022 / Build Tools with Desktop C++ development,
a Windows SDK and CMake 3.20+. No WDK, installed driver or .NET runtime is needed
to build the native application.

```powershell
.\Build.ps1
.\Build.ps1 -Render
.\Build.ps1 -AddressSanitizer
.\Build.ps1 -Target Package
```

Output: `App/.build/Release/bin/CurveController.exe`. The ordinary release uses
a static C++ runtime. AddressSanitizer is a separate validation build and is never
included in a portable release.

Build and offline tests do not install a driver, open a physical input capture,
or modify your live preferences. The UI self-test uses an explicitly offline
window and simulated readings. Optional live tests require a dedicated test VM.

```powershell
.\CurveController.exe --demo
.\CurveController.exe --check-json .\Examples\custom.json
.\CurveController.exe --render preview.png
```

Normal interactive startup requests administrator rights, then starts capture.
Offline preview, JSON checks, self-tests and screenshot rendering do not elevate.

## Small source layout

| Path | Role |
| --- | --- |
| `App/Curve.*` | Preset functions and compiled custom cubic response |
| `App/Transform.*` | Precise native/HID stick transformation and descriptor mapping |
| `App/Capture.*` | Automatic SDK context, event-driven receive/forward, live settings replay |
| `App/Storage.*` | Windows registry preferences and explicit JSON import/export |
| `App/UI.*` | Direct2D/DirectWrite UI, graph, points and live values |
| `App/Tests/` | Curve, JSON, full signed-axis range, HID and storage regressions |
| `Interceptor/` | Two-file Gamepad & Controller C++ SDK |
| `Examples/` | Optional, importable JSON examples |

The worker uses `Get_Input_With_Timeout()` and `Send_Input()`. Configuration changes
publish an immutable compiled response and wake the same worker. Cached physical
reports allow a held stick to update without waiting for another physical movement;
relative scalar fields are neutralized before reuse. Input is forwarded before
snapshot publication. The UI has at most one pending input notification and reads
the latest snapshot before painting, without interpolating measured values.

No model-specific VID/PID table or virtual-controller dependency is used. HID
mapping is based on standard usages and descriptor metadata, with an explicit
layout override where semantics are ambiguous.
