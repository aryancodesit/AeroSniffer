# AeroSniffer Bugs

Last updated: 2026-06-14

Source of truth: `docs/system_state.md`

## Active Bugs

### BUG-007: Typing Emotion Stuck

Severity:
Medium

Status:
Open

Component:
PC Agent / Emotion Engine ownership boundary

Reproduction:
1. Launch PC Agent.
2. Type continuously.
3. Stop typing.
4. Observe emotion remains active.

Observed:
- Face remains in THINKING/TYPING state.
- Animation eventually freezes.
- Face does not return to IDLE.

Expected:
- Return to IDLE after inactivity timeout.

Potential causes:
- Missing typing stop event.
- Missing inactivity timeout.
- Event ownership conflict.
- State lock.

Recommendation:
- PC Agent should emit activity events only.
- Firmware EmotionEngine and FaceEngine should own state transitions, priority, timeout, decay, and return-to-IDLE behavior.

## Pending Validation

### Dashboard Connection Diagnostics

Severity:
Low

Status:
Pending validation

Verified:
- Mode updates visible.
- Telemetry updates visible.

Pending:
- Connection diagnostics validation.

## Resolved Bugs

### Security Mode Startup Crash

Status:
Resolved

Previously observed:
- Guru Meditation after AP startup.
- Crash address: `EXCVADDR 0x44`.

Latest result:
- Security mode stable.
- AP startup stable.
- BLE startup stable.
- WebServer startup stable.

### Emotion Engine Compile Recovery

Status:
Resolved for runtime testing

Previously observed:
- FaceEngineClass header/implementation mismatch.
- Missing `setStatusLines()` declaration.
- Missing `anim_bounce_y` declaration.
- Missing `anim_pulse_scale` declaration.
- FS/WebServer include issue.

Latest result:
- Runtime testing has proceeded.

## Working / Not A Current Bug

### Aviation Mode

Status:
Working

Verified:
- OpenSky integration previously verified.
- Wi-Fi connect.
- OpenSky login.
- Aircraft retrieval.
- Aircraft parsing.
- Aircraft rendering.

Last successful test:
10 aircraft parsed and rendered.
