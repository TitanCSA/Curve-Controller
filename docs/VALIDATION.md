# Validation scope

## Distance-from-center response (1.1.0)

Distance is the default for new settings and older files without a mode field.
The separate-X/Y mode remains selectable and is saved in registry/JSON settings.
The graph and live preview show actual representable output, with a Center
marker for each complete physical stick pair in distance mode.

The expanded Release and AddressSanitizer suites cover custom 10% -> 50% radius
at 720 angles, all preset magnitudes and quadrants, neutral/subnormal values,
square-gate corners, exact linear native input, HID field order, independent
collections, missing/null/ambiguous components, split-report counterparts, JSON
migration and isolated registry roundtrips. The UI suite checks the default and
both mode buttons. All seven shipped JSON examples are parsed.

In an isolated Windows 11 VM with Microsoft-signed Gamepad & Controller 1.0.12.0:

- Windows HID and XInput/GIP consumers observed the custom 10% -> 50% radius.
- A held stick updated when switching between distance and separate-X/Y modes,
  without another physical source submission. Selection, buttons/triggers and
  close-to-passthrough restoration passed.
- 2,048 HID reports in bounded bursts passed; 2,052 received reports were
  forwarded. Process handles remained 187 before and after that stress run.
- A separate owned virtual HID controller sent X and Y under different report
  IDs. Windows received the coupled output, including an updated held Y after
  changing only X; six counterpart replays were observed.

These are bounded software-fixture tests, not exhaustive physical-controller
coverage, a physical latency measurement or a zero-latency guarantee. Incomplete
or ambiguous HID pairs pass through; an unknown counterpart is not guessed.

## Initial release campaign

Version 1.0.0 was validated on 23 October 2026. These results describe the tested
paths; they do not certify every physical controller, Windows build or game.

## Offline checks

- The ordinary native x64 Release build and a separate AddressSanitizer build
  each passed **1,653,112 assertions**.
- Checks cover preset functions, 500 randomized custom curves, all 65,536 native
  signed 16-bit input values for exact linear passthrough, stick selection,
  button/trigger preservation, descriptor-based HID layouts and split reports.
- JSON validation rejects incorrect types, oversized/deep documents and invalid
  points. Import/export and isolated registry persistence fixtures passed.
- The offline UI self-test passed **36 checks**, including custom-point history,
  control interaction and graph state. It does not open a driver or save live
  preferences. README screenshots are rendered, simulated previews.

## Live Windows input path

The actual capture/transform/forward worker was exercised in an isolated Hyper-V
Windows test VM with owned, persistent HID and USB GIP test fixtures.

- Windows HID reads and the Windows XInput consumer received the transformed
  values. Buttons and triggers retained their original values.
- Changing the response or clearing a stick checkbox updated an already held
  stick without another physical-source submission.
- Stopping capture restored the original source values.
- A bounded stress test forwarded **5,120 HID reports in 80 bursts**. The complete
  session reported 5,124 received and 5,124 forwarded; process handle count stayed
  at 182 before and after the stress section.

Newly created transient virtual devices correctly requested a Windows restart
for filter association. Their direct XUSB/XInputHID cases were not completed as
live coverage in this run. Native transformations have offline coverage; real
hardware and additional transport/descriptor combinations still need testing.

## One-hour driver policy

Isolated test-signed copies of the four production candidates reported exactly
**3,600 seconds** for Interceptor, Keyboard & Mouse, Gamepad & Controller and
Vdevice. Querying the policy did not start its allowance. Separate 45-second lab
builds exercised expiry, pending-receive cancellation and refusal of new sessions.
The production submission binaries are unchanged unsigned candidates awaiting
Microsoft signing; lab certificates are not distributed.

## Performance interpretation

The capture worker waits on driver input or a settings/stop event. It has no
healthy-path polling delay or input FPS limit. Curve coefficients are compiled
when settings change, and UI notifications are coalesced independently.

CPU-only curve microbenchmarks do **not** measure USB-to-game or input-to-display
latency. No claim of zero latency, universal compatibility or exhaustive crash
coverage follows from these tests. Driver processing, scheduling, device firmware
and consumers still contribute to the complete input path.
