# AeroSniffer Roadmap

Last updated: 2026-06-14

Source of truth: `docs/system_state.md`

## Current State

Phase:
Stabilization Sprint

Current priority:
PC Agent Emotion Ownership

## Implementation Status

- Companion Mode is STABLE and boots by default.
- Emotion Engine is PARTIALLY STABLE.
- PC Agent is under INVESTIGATION.
- Security Mode is STABLE.
- AP startup is stable.
- BLE startup is stable.
- WebServer startup is stable.
- Mode Manager is STABLE.
- Mode transitions are stable.
- Aviation Mode is WORKING, with OpenSky integration previously verified.
- Dashboard is WORKING for mode updates and telemetry updates.
- Dashboard connection diagnostics remain pending.

## Current Priority

### PC Agent Emotion Ownership

Open issue:
- PC Agent typing emotion can become stuck.
- Face remains in THINKING/TYPING state.
- Animation eventually freezes.
- Face does not return to IDLE after typing stops.

Potential causes:
- Missing typing stop event.
- Missing inactivity timeout.
- Event ownership conflict.
- State lock.

Goal:
- Ensure PC Agent-driven THINKING/TYPING states return to IDLE after inactivity.

## Active Issues

- BUG-007: Typing Emotion Stuck.
- Dashboard connection diagnostics validation.
- Emotion ownership boundary between PC Agent and firmware.

## Resolved Issues

- Security Crash Investigation moved from active issues to resolved.
- Security Mode Guru Meditation after AP startup resolved.
- Crash `EXCVADDR 0x44` resolved.
- AP startup stable.
- BLE startup stable.
- WebServer startup stable.
- Mode transitions stable.
- Emotion Engine compile recovery resolved for runtime testing.

## Recommendation

PC Agent should not own emotions directly. It should emit events such as typing started, typing stopped, active app changed, CPU high, and idle detected. Firmware EmotionEngine and FaceEngine should own state transitions, priority arbitration, decay, animation lifetime, and return-to-IDLE behavior.

Reasoning:
- The device needs a single authority for emotional state and animation lifecycle.
- Firmware is closest to face rendering, idle timeout, and state decay.
- PC Agent can disconnect, miss host events, or hold stale state.
- Event-based ownership prevents stuck THINKING/TYPING states by allowing firmware timeouts to recover.

## Next Validation Queue

- Reproduce BUG-007 with PC Agent.
- Confirm whether PC Agent sends an explicit typing stop/idle event.
- Confirm firmware inactivity timeout can override stale PC Agent state.
- Validate that animation unfreezes and returns to IDLE after typing stops.
- Validate dashboard connection diagnostics.

## Guardrails

- Do not create new feature plans until BUG-007 is understood.
- Keep PC Agent changes focused on event emission and activity ownership boundaries.
- Keep firmware responsible for state transitions unless a documented exception is needed.
