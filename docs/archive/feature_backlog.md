# AeroSniffer Feature Backlog

Last updated: 2026-06-14

Source of truth: `docs/system_state.md`

## Critical

- Resolve PC Agent emotion ownership boundary so host-driven typing states cannot permanently lock FaceEngine state.
- Verify BUG-007 recovery path: typing stops, inactivity timeout fires, and face returns to IDLE.

## High Priority

- Add or verify PC Agent typing stop/inactivity event emission.
- Add or verify firmware-side timeout recovery for stale PC Agent typing/thinking state.
- Validate dashboard connection diagnostics.
- Document event ownership rules between PC Agent, EmotionEngine, and FaceEngine.

## Medium Priority

- Add repeatable PC Agent interaction tests: typing burst, stop typing, app switch, idle, reconnect.
- Confirm animation lifecycle remains active after long THINKING/TYPING periods.
- Update release validation checklist with BUG-007 regression test.

## Resolved / Verified

- Companion mode boots by default.
- Companion Mode is stable.
- Emotion state machine is active.
- Random emotions are working.
- Idle timeout is working.
- Security Mode is stable.
- AP startup is stable.
- BLE startup is stable.
- WebServer startup is stable.
- Mode transitions are stable.
- OpenSky integration was previously verified.
- PC Agent can control emotions.

## Future Ideas

Feature expansion remains deferred until BUG-007 and connection diagnostics are closed.
