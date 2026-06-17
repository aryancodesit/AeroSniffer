# AeroSniffer Bug Audit

Last updated: 2026-06-14

Source of truth: `docs/system_state.md`

## Scope

This audit is documentation-only. It reflects current implementation status and current blockers. `bugs.md` is the active bug ledger.

## Current Blockers

1. BUG-007: Typing Emotion Stuck.
2. PC Agent / firmware emotion ownership boundary.
3. Dashboard connection diagnostics validation.

## BUG-007: Typing Emotion Stuck

Status:
Open

Priority:
Medium

Component:
PC Agent / Emotion Engine ownership boundary

Reproduction:
1. Launch PC Agent.
2. Type continuously.
3. Stop typing.
4. Observe emotion remains active.

Observed behavior:
- Face remains in THINKING/TYPING state.
- Animation eventually freezes.
- Face does not return to IDLE.

Expected behavior:
- Return to IDLE after inactivity timeout.

Potential causes:
- Missing typing stop event.
- Missing inactivity timeout.
- Event ownership conflict.
- State lock.

## Resolved Runtime Issues

### Companion Mode

Status:
Stable

Verified:
- Companion mode boots by default.
- Emotion state machine active.
- Random emotions working.
- Idle timeout working.

### Security Mode

Status:
Stable

Resolved:
- Security Mode startup crash.
- Guru Meditation after AP startup.
- Crash `EXCVADDR 0x44`.

Verified:
- AP startup stable.
- BLE startup stable.
- WebServer startup stable.

### Mode Manager

Status:
Stable

Verified:
- Mode transitions stable.

## Dashboard Diagnostics

Status:
Pending validation

Priority:
Low

Verified:
- Dashboard mode updates are visible.
- Dashboard telemetry updates are visible.

Pending:
- Connection diagnostics validation.

## Aviation Mode

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
- 10 aircraft parsed and rendered.

## Current Priority

The immediate priority is PC Agent Emotion Ownership. PC Agent typing state should not be able to lock FaceEngine in THINKING/TYPING or prevent return to IDLE.
