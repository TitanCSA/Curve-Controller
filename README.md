# Curve Controller - Real-Time Gamepad Joystick Response Curves

Change how your **left stick, right stick, or both** respond on Windows, with
instant presets and a **ten-point custom curve editor**. Native C++17, a compact
interface, and low-level remapping through the
[Interceptor Gamepad & Controller kernel filter driver](https://github.com/TitanCSA/Interceptor-Gamepad-Controller).

**[Download](https://github.com/TitanCSA/Curve-Controller/releases/latest)** |
[Driver setup](https://github.com/TitanCSA/Interceptor-Gamepad-Controller/releases/latest) |
[Build from source](docs/BUILD.md) | [JSON examples](Examples)

![Curve Controller with simulated stick readings](docs/images/curve-controller.png)

The screenshot is an explicitly simulated preview, not a physical-device test.

## Start in three steps

1. Install the **Gamepad & Controller edition** of Interceptor using its
   `InterceptorSetup.exe`. Restart Windows when requested and finish setup.
2. Extract Curve Controller and open **CurveController.exe**. Accept elevation;
   capture starts automatically. Close other controller capture/remapping apps.
3. Check **Left joystick**, **Right joystick**, or both, and choose a preset.
   Changes are immediate, including when a previously observed stick is held still.

There is no Apply button. Unchecked sticks, buttons, triggers, hats and other
unedited report fields retain their physical values. Settings affect all
compatible captured controllers; the device menu selects only the live preview.
Closing the app releases capture and restores physical passthrough. A failed
connection is retried automatically; **Reconnect** is also available.

The driver is downloaded separately so this project never silently installs a
driver or bundles an older installer. **Driver setup** opens the driver's latest
release, or a verified Gamepad & Controller installer placed beside the app.

## Find your response

Choose **Linear**, three exponential curves, three inverse curves, **Smooth S**,
**Smoother S**, **Ease out**, **Precision**, or **Early max**. The graph plots the
actual response function, from **physical input on X** to **transformed output
on Y**, both from 0 to 100%.

- Linear preserves the original stick values exactly.
- Exponential gives finer control near the center; inverse exponential boosts
  small movements. The quadratic/square-root and cubic/cube-root pairs are inverses.
- Precision includes a 10% inner deadzone. Early max reaches 100% at 80% input.
- Custom provides **ten movable points**, plus fixed 0/0 and 100/100 anchors.
  Click a point and drag it, type its Input/Output percentages, or use arrow keys
  (Shift for smaller steps). Points cannot cross horizontally.

For **10% physical input -> 50% output**, select Custom, choose the first point,
and enter **Input 10**, **Output 50**. The smooth, shape-preserving custom curve
passes through the editable points and stays within the output range. Custom
curves can deliberately be non-monotone.

Response is applied independently to the horizontal and vertical component of
each selected stick. Negative inputs retain their sign; the graph shows their
magnitudes. This is an axis response curve, not a radial deadzone. Inverse curves
can compensate a matching power curve in a game; other game processing may still
affect the final feel.

![Ten-point custom response, simulated preview](docs/images/custom-curve.png)

## Save, export and share

Preferences save automatically in **your Windows registry** at
`HKCU\Software\TitanCSA\CurveController`. No settings file is created beside the
application or in your documents. The last custom points are retained when you
try other presets. Uninstalling the driver does not remove these app preferences.

**Export JSON** saves an explicit, portable settings file; **Import JSON**
validates and immediately loads one. Nothing is executed from JSON. The
[Examples](Examples) folder contains linear, exponential, inverse, left-only,
right-only and custom 10% -> 50% examples. [JSON format](docs/JSON.md).

## Compatibility and live values

Works with supported **HID, XInput/XUSB, XInputHID and USB GIP** controller paths,
including compatible DS4/DualShock 4, DualSense, wired Xbox 360 and Xbox One/Series
profiles supported by the driver. It edits the existing physical input path;
it does not create a virtual gamepad or send keyboard/mouse events.

HID axes are decoded from their descriptor. Automatic mapping uses X/Y and a
common Z/Rz or Rx/Ry right-stick pair; use **HID stick layout** for ambiguous
descriptors. Unknown/proprietary controls are not guessed. Multiple report IDs
and repeated matching stick usages are retained; the four preview values show
the most recently observed matching component.

The live panel shows received physical values and the representable transformed
values accepted for forwarding by the driver. It is not an independent measurement
that a particular game consumed the report. No input yet is shown as `--`.
Windows, device firmware, connection type and game processing still affect delivery.

The capture loop is event driven: no FPS cap, fixed sleep or UI animation delay
is inserted in report processing. UI highlights animate separately; measured
stick values are displayed directly. This is not a claim of zero latency or
compatibility with every device. [Validation scope](docs/VALIDATION.md).

## Driver demo and project status

This is an **example of using Interceptor Gamepad & Controller and its C++ SDK**.
The app source is MIT licensed. Driver source is not included; its binaries have
[separate terms](DRIVER-BINARY-LICENSE.txt).

The driver's demo allowance is shared across applications of that edition and
starts when capture or controller monitoring begins. Closing or reinstalling
does not renew it. At expiry, remapping stops and physical input passes through;
use **Windows Restart** for a new allowance. Install the current Microsoft-signed **one-hour** driver package (kernel **1.0.12.0**). Restart Windows after upgrading from an older demo clock. Curve Controller uses the installed driver's actual policy. For the unrestricted driver, DM
**`titancsa`** on Discord.

Inspired by [GamePad Remapper](https://github.com/TitanCSA/GamePad-Remapper), a more
complete scripting/macro example for the same driver. Interceptor independently
evolves the workflow popularized by
[oblitum/Interception](https://github.com/oblitum/Interception); these projects are
not affiliated with oblitum and are not ABI-compatible with that original API.

## Intended use and responsibility

Created for legitimate customization, accessibility, development and testing.
You are responsible for your use of the application, driver and SDK, including
compliance with applicable law and software/service rules. The author does not
authorize unlawful use and disclaims warranties and liability to the extent
permitted by applicable law. See [LICENSE](LICENSE); nothing here excludes rights
or liability that cannot legally be excluded.
