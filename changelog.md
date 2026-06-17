# Changelog

## 2026-06-14 — Compile Fix Pass

### Modified Files

| File | Change |
|------|--------|
| `AeroSniffer/AeroSnifferOS.h` | Added `anim_bounce_y`, `anim_pulse_scale` private members; added `setStatusLines()` public method declaration |
| `AeroSniffer/Mode2_Security.h` | Added `#include <FS.h>` and `#include <SPIFFS.h>` before `#include <WebServer.h>` |
| `AeroSniffer/Security/UI.h` | Added `#include <FS.h>` and `#include <SPIFFS.h>` before `#include <WebServer.h>` |
| `docs/system_state.md` | Updated compilation status to "FIXES APPLIED — PENDING VERIFICATION" |
| `docs/ai_handoff.md` | Added OpenCode Update section documenting compile-fix pass |

### Root Cause
`FaceEngineClass` header in `AeroSnifferOS.h` was missing declarations for `anim_bounce_y`, `anim_pulse_scale`, and `setStatusLines()` that were defined in `AeroSnifferOS.cpp`. Additionally, `Mode2_Security.h` and `Security/UI.h` included `<WebServer.h>` without first including `<FS.h>` and `<SPIFFS.h>`, causing `FS not declared` errors.

### Status
Fixes applied, pending compile verification in Arduino IDE.

## 2026-06-14 — Phase 1: Animation Freeze Investigation

### Modified Files

| File | Change |
|------|--------|
| `AeroSniffer.ino` | Added `[FACE] heartbeat` log every 1s in Core 1 task |
| `AeroSniffer.ino` | Replaced `taskYIELD()` with `vTaskDelay(pdMS_TO_TICKS(16))` in Core 1 loop |

### Purpose
Phase 1 of freeze investigation. Heartbeat determines whether Core 1 (UI) stops during freeze. vTaskDelay ensures Core 1 yields to the scheduler instead of spinning with no-op `taskYIELD()`.

### Test Procedure
Run Companion Mode + PC Agent for 10-15min. Collect heartbeat logs and freeze timestamps. If heartbeat stops during freeze → Core 1 is blocked. If heartbeat continues but display stalls → SPI flash contention or renderer issue.

## 2026-06-14 — Touch State Reset After Mode Transitions

### Modified Files

| File | Change |
|------|--------|
| `AeroSniffer.ino` | Inserted `_touch_start = 0; _touch_active = false; _touch_last_change = millis();` after `setup_mode()` in mode transition block |

### Root Cause
After a long-press mode switch, touch state variables (`_touch_start`, `_touch_active`, `_touch_last_change`) held stale values from the original press/release cycle. `_touch_last_change` was ~700ms old by the time the transition completed, so any noise or mechanical bounce on GPIO43 passed the 30ms debounce gate immediately, producing a false `g_touch_tap = true`.

## 2026-06-14 — Security Mode AP Channel Hopping Fix

### Modified Files

| File | Change |
|------|--------|
| `Mode2_Security.h` | Added `ch_hopping = false` after `sec_scanning = true` in `security_setup()` |

### Root Cause
`ch_hopping = true` was the default (`Security/Statistics.h:19`). After AP + promiscuous mode started, `security_core0_task()` called `sec_hop_channel()` every 180ms, changing `esp_wifi_set_channel()` across all 13 channels. Since ESP32-S3 has a single radio PHY, AP beacon frames and client traffic were disrupted with every hop, breaking client association.

### Fix
Set `ch_hopping = false` after `sec_scanning = true` in `security_setup()`. The AP stays on its default channel. Promiscuous sniffing still works on that channel — probe requests, beacons, deauth frames are all captured. Clients can now associate and the Web UI remains reachable.

### Test Expectations
- Android/phone can connect to AeroSniffer-SEC AP ✅
- Web portal `http://192.168.4.1` loads ✅
- Promiscuous detections (probes, beacons, deauth) still appear on the single channel ✅
- Serial shows `[SEC] Fixed-channel AP mode enabled` on boot ✅

### Fix
Reset all three touch state variables after `setup_mode()` completes and before `g_mode_transitioning = false`. This ensures:
- `_touch_active = false` (no touch in progress)
- `_touch_last_change = millis()` (debounce timer is current — any noise must exceed full 30ms window)
- `_touch_start = 0` (extra safety, though overwritten on next press)

## 2026-06-14 — Sticky PC-Agent Typing Emotion Fix

### Modified Files

| File | Change |
|------|--------|
| `AeroSniffer/AeroSnifferOS.cpp` | Added `case EVENT_PC_TYPING:` handler — sets EMOTION_CURIOUS, ACTVITY_TYPING, 8s transient timeout, `[PC] Typing start` log |
| `AeroSniffer/AeroSnifferOS.cpp` | Added `[PC] Typing stop` log in `forceIdle()` when prior emotion was EMOTION_CURIOUS |
| `AeroSniffer/AeroSniffer.ino` | Line 248: fixed `EVENT_PC_TYPING` → `EVENT_PC_CODING` for `"coding"` activity |
| `docs/system_state.md` | Updated Current Priority, Current Task status, Blockers |
| `docs/ai_handoff.md` | Added OpenCode Update section documenting the fix |

### Root Cause
`EVENT_PC_TYPING` was published by the PC agent handler but had no case in `EmotionEngineClass::handleEvent()` — fell to `default:` no-op. No emotion change or transient timeout was established, so the prior emotion persisted indefinitely.

### Bonus Bug
`"coding"` activity incorrectly published `EVENT_PC_TYPING` instead of `EVENT_PC_CODING`, which would have caused a regression (coding → THINKING face) once the new handler was added.

### Timeout Model
- Typing sets 8s transient via `transient_until`
- Repeated events (active typing) renew the transient
- 8s after last event → `tick()` calls `forceIdle()` → logs `[PC] Typing stop` + `[EMOTION] THINKING -> IDLE (timeout)`
- Fallback: 60s activity timeout catches stale states
