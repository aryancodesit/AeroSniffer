# AeroSniffer Stabilization Sprint

Last updated: 2026-06-14

Source of truth: `docs/system_state.md`

## Goal

Achieve stable operation before any new features are added.

Current priority:
PC Agent Emotion Ownership.

## Phase 1: BUG-007 Reproduction

Objective:
Reproduce Typing Emotion Stuck with the PC Agent.

Steps:
1. Launch PC Agent.
2. Type continuously.
3. Stop typing.
4. Observe whether THINKING/TYPING remains active.
5. Observe whether animation eventually freezes.

Expected:
- Face returns to IDLE after inactivity timeout.

## Phase 2: Emotion Ownership Audit

Objective:
Determine whether PC Agent directly owns emotions or only emits events.

Potential causes:
- Missing typing stop event.
- Missing inactivity timeout.
- Event ownership conflict.
- State lock.

Recommended target:
- PC Agent emits events.
- Firmware EmotionEngine and FaceEngine own state transitions, timeout, decay, animation lifecycle, and return-to-IDLE behavior.

## Phase 3: Fix Validation

Objective:
Verify BUG-007 recovery.

Validation cases:
- Typing burst, then stop.
- Long typing session, then stop.
- App switch while typing.
- PC Agent disconnect during typing state.
- PC Agent reconnect after stale THINKING/TYPING state.

Exit criteria:
- THINKING/TYPING returns to IDLE after inactivity.
- Animation does not freeze.
- Firmware recovers even if PC Agent misses a stop event.

## Phase 4: Release Candidate Validation

Objective:
Validate the stabilized system as a release candidate.

Already verified:
- Companion mode boots by default.
- Emotion state machine active.
- Random emotions working.
- Local idle timeout working.
- Security Mode stable.
- AP startup stable.
- BLE startup stable.
- WebServer startup stable.
- Mode transitions stable.
- OpenSky integration previously verified.
- PC Agent can control emotions.

Remaining:
- BUG-007 closed.
- Dashboard connection diagnostics validated.
