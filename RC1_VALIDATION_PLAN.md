# AeroSniffer V2.3-RC1 Validation Plan

Last updated: 2026-06-16

## Assessment

### Release Scope
V2.3-RC1 covers three operational modes (Companion, Security, Aviation) with Settings & Control Layer, Threat Intelligence, and Portal v2 web dashboard. This is the first release candidate after the stabilization sprint.

### Known Open Items
| ID | Issue | Severity | Impact |
|----|-------|----------|--------|
| BUG-007 | Typing Emotion Stuck (THINKING/TYPING may persist after PC Agent stops) | Medium | Cosmetic; does not affect Security, Aviation, or Portal operation |
| DASH-001 | Dashboard connection diagnostics validation pending | Low | No functional regression; mode updates and telemetry verified |

### Risk Assessment
| Area | Risk | Mitigation |
|------|------|------------|
| Hardware variance | DeskBuddy 2.0 (XIAO ESP32S3 + ST7789) only validated target | Release targets this single SKU |
| Runtime stability | 3-mode FreeRTOS with shared services | Long-runtime checklist below |
| Security false positives | Deauth/channel flood thresholds are user-configurable | Sensitivity setting verified in Portal |
| Wi-Fi credential change | AP restart requires full reboot (documented) | `reboot_required` flag returned by API |

### Exit Criteria
1. All checklist items marked PASS or WAIVED with documented rationale
2. BUG-007 reproduction steps and impact clearly stated for downstream
3. Portal screenshots captured for all 4 tabs
4. Serial boot logs captured for all 3 modes

---

## Validation Matrix

### 1. Hardware Validation

| # | Test | Procedure | Pass Criteria | Pass/Fail | Notes |
|---|------|-----------|---------------|-----------|-------|
| H1 | Power-on boot | Connect USB power, observe splash screen | AeroSniffer splash displays within 3s | □ | |
| H2 | Display init | Observe TFT after power-on | Full 240×240 ST7789 rendering, no artifacts, no flicker | □ | |
| H3 | Touch sensor | Short-tap (20ms–1.5s) capacitive pad | Serial logs `[TOUCH] TAP` within 30ms debounce window | □ | |
| H4 | Long-press mode switch | Hold touch >1.5s | Device cycles mode; transition splash shows COMPANION → SECURITY → AVIATION | □ | |
| H5 | Mode 0 boot default | Fresh boot without saved mode | Boots directly to Companion mode (g_mode=0) | □ | |
| H6 | Touch debounce | Rapid tap 3x in under 500ms | Each tap produces exactly one `[TOUCH] TAP` event; no chatter | □ | |
| H7 | Backlight (if applicable) | DeskBuddy 2.0 only (always-on) | Display remains lit through all mode transitions | □ | |
| H8 | I2C bus (MPU-6050) | Present on I2C, `Wire.begin()` succeeds | No I2C timeout logs; device enumerated if HAS_IMU=1 | □ | N/A on DeskBuddy |

#### Required Logs (H1–H8)
```
[AeroSniffer] Booting...
[HW] DeskBuddy 2.0 — XIAO ESP32S3 + ST7789 240x240
[TOUCH] boot raw=0 pressed=0 active=0 ms=...
[TOUCH] START raw=1 ms=...
[TOUCH] RELEASE raw=0 duration=... ms=...
[TOUCH] LONG_PRESS (or TAP)
[MODE] Transitioning: X -> Y
```

#### Required Screenshots
- Splash screen (should show "AeroSniffer", "Multi-Boot Desk Gadget", and 3 mode icons)
- Companion mode idle face
- Security mode TFT (shows PORTAL v2 header, radar sweep, PPS, total, deauths, SSID, clients, heap)

---

### 2. Security Mode Validation

| # | Test | Procedure | Pass Criteria | Pass/Fail | Notes |
|---|------|-----------|---------------|-----------|-------|
| S1 | AP startup | Enter Security mode | SoftAP "AeroSniffer-Test" (or custom SSID) visible on phone/PC within 5s | □ | |
| S2 | AP auth | Connect with configured password | Client associates; serial logs `[WIFI] AP client connected` with MAC | □ | |
| S3 | Web portal loads | Browse to `http://192.168.4.1` from connected client | Full SPA loads; status bar shows AeroSniffer logo + IDLE + PPS counter | □ | |
| S4 | Promiscuous scanning | Verify packet counters increment after AP client connects | PPS > 0, Total increments, Beacons/Probes counters increase | □ | |
| S5 | Channel stability | Observe AP connectivity for 5 minutes | No client disconnects; AP IP stays `192.168.4.1`; serial shows no reassociation | □ | |
| S6 | Deauth detection | Send deauth frames to target (airplay/mdk3) | `[EVENT] DeauthAttack` fires; Portal threats tab shows deauth alert with severity badge | □ | |
| S7 | Threat sensitivity wiring | Set sensitivity Low → Medium → High via Portal | `getDeauthThreshold()` returns 20 → 10 → 5; deauth alert threshold changes without reboot | □ | |
| S8 | AP SSID change | Update AP SSID via Settings → Save | `reboot_required` flag returned; device reboot shows new SSID | □ | |
| S9 | AP password change | Update password via Settings → Save | `reboot_required` flag returned; old password no longer connects | □ | |
| S10 | Threat sensitivity persist | Set sensitivity to High, power-cycle | After reboot, API returns `threat_sensitivity: 2` and alert threshold is 5 | □ | |
| S11 | Mode re-entry after AP change | Reboot, re-enter Security mode | New AP SSID/PASS used for SoftAP; Portal accessible at `http://192.168.4.1` | □ | |
| S12 | Concurrent client support | Connect 2+ devices to AP simultaneously | Each client can load Portal independently; AP stations counter increments per client | □ | |

#### Required Logs (S1–S12)
```
[SEC] === security_setup START (PORTAL v2) ===
[SEC] AP config: channel=X auth=Y max_conn=Z
[PORTAL] WebServer on port 80
[PORTAL] Open http://192.168.4.1
[WIFI] AP client connected mac=XX:XX:XX:XX:XX:XX aid=1 total=1
EVT:{"type":"deauth_alert","count":%lu,"threshold":%d}
```

#### Required Screenshots
- Overview tab: shows PPS > 0, Total > 0, AP SSID, AP IP, channel, uptime, heap
- Devices tab: shows at least 1 AP in APs list, client with MAC/type/trusted/presence
- Threats tab: shows severity grid (Crit/Warn/Info/Total), at least one deauth alert with ◆-badge
- Settings tab: shows Device Name, AP SSID, AP Password, Threat Sensitivity dropdown, HomeGuard toggle, Aviation toggle

---

### 3. Home Guard Validation

| # | Test | Procedure | Pass Criteria | Pass/Fail | Notes |
|---|------|-----------|---------------|-----------|-------|
| HG1 | Device enrollment | Walk a known phone with randomized MAC past the sniffer | Device appears in Portal Devices tab with type "NEW" or "FAMILIAR" | □ | |
| HG2 | Trusted device toggle | Click TRUST button on a device | Device type changes to "trusted"; green badge shown | □ | |
| HG3 | HomeGuard disable | Set HomeGuard toggle OFF via Settings → Save | `sys_homeguard_enabled = false`; `enroll_device()` exits early (no enrollment, no tracking) | □ | |
| HG4 | HomeGuard re-enable | Set HomeGuard toggle ON via Settings → Save | `sys_homeguard_enabled = true`; new devices are enrolled | □ | |
| HG5 | HomeGuard persist | Toggle OFF, power-cycle, verify | After reboot, API returns `homeguard_enabled: false` | □ | |
| HG6 | Known device return | After trusting a device, let it go away >120s, then return | Portal shows "Away" → "Present"; trusted return alert generated | □ | |
| HG7 | Randomized MAC detection | Connect Android/iOS device with Private MAC | Device classified as "Randomized MAC"; intruder alert created | □ | |
| HG8 | Probe leak detection | Observe devices sending probe requests with stored SSIDs | Probe leak entries appear in Devices tab; intruder warnings for leaked SSIDs | □ | |

#### Required Logs (HG1–HG8)
```
[HOMEGUARD] enroll mac=XX:XX:XX:XX:XX:XX name=... count=1
[HOMEGUARD] Skipped enrollment (disabled)
[HOMEGUARD] Trust toggled mac=XX:XX:XX:XX:XX:XX trusted=1
EVT:{"type":"welcome_hello","name":"..."}
```

#### Required Screenshots
- Devices tab (Clients sub-tab): device list with MAC, type (NEW/FAMILIAR/TRUSTED), presence status, sightings count, seen age, first-seen age
- Devices tab with HomeGuard OFF: no new devices appearing over 2-minute window

---

### 4. Aviation Mode Validation

| # | Test | Procedure | Pass Criteria | Pass/Fail | Notes |
|---|------|-----------|---------------|-----------|-------|
| A1 | Mode entry | Switch to Aviation mode via long-press | Display shows "Flight Radar" + "Connecting to WiFi..." | □ | |
| A2 | WiFi STA connect | If configured, auto-connect to home WiFi | `[AVI] WiFi OK -- <ssid>` in serial; wifi_ok = true | □ | |
| A3 | OpenSky API fetch | After WiFi connect, Auto-fetch from OpenSky | `[AVI] OK — X aircraft found` in serial within FETCH_INTERVAL_MS | □ | |
| A4 | Flight card rendering | At least 1 aircraft in range | Card shows callsign, altitude, ground speed, heading, compass rose, radar map | □ | |
| A5 | Card auto-scroll | With 2+ aircraft | Display auto-advances every CARD_SCROLL_MS (4s) | □ | |
| A6 | Aviation disable | Set Aviation toggle OFF via Settings → Save | On next Aviation mode entry, display shows "Aviation Disabled"; no WiFi connect, no OpenSky fetch | □ | |
| A7 | Aviation re-enable | Set Aviation toggle ON, power-cycle, enter mode | Existing Aviation behavior restored | □ | |
| A8 | Aviation persist | Toggle OFF, power-cycle, enter mode | Display shows "Aviation Disabled" (config persisted) | □ | |
| A9 | No-WiFi fallback | Enter Aviation mode without credentials (DEFAULT_WIFI_SSID) | Display shows "No WiFi set! Use Setup in app."; no crash, periodic retry with 30s guard | □ | |
| A10 | OpenSky rate-limit | Force rapid re-entry to Aviation mode (3x in 60s) | 30s guard between failed fetches; no API ban | □ | |
| A11 | Bounding box config | Custom bounding box via Settings | Aircraft returned within custom lat/lon range | □ | |

#### Required Logs (A1–A11)
```
[AVI] Box: 19.80 85.00 21.00 86.80
[AVI] Connecting to <ssid> ...
[AVI] WiFi OK -- <ssid>
[AVI] OK — X aircraft found
EVT:{"type":"flight_detected","count":X}
[AVI] WiFi FAILED: ...
```

#### Required Screenshots
- Aviation mode card: callsign, altitude (m), ground speed (kn), heading + compass rose, radar map with aircraft dots
- Aviation mode with no aircraft: empty state message
- Aviation mode disabled: "Aviation Disabled" message on TFT

---

### 5. Portal Validation

| # | Test | Procedure | Pass Criteria | Pass/Fail | Notes |
|---|------|-----------|---------------|-----------|-------|
| P1 | Overview tab loads | Navigate to Overview tab | Status card: scan state, channel, AP clients, uptime, heap. Stat grid: PPS, Total, Beacon, Deauth | □ | |
| P2 | Devices tab loads | Navigate to Devices tab | APs sub-tab shows detected networks; Clients sub-tab shows known devices + probe leaks | □ | |
| P3 | Threats tab loads | Navigate to Threats tab | Summary grid (Crit/Warn/Info/Total), "By Type" card, filter pills with counts, timeline feed | □ | |
| P4 | Settings tab loads | Navigate to Settings tab | Device Name, AP SSID, AP Password, Threat Sensitivity select, HomeGuard toggle, Aviation toggle, Save button | □ | |
| P5 | Save Settings success | Fill valid values, click Save | Button shows "Saving..." → "Save Settings"; message shows "Settings saved" in green | □ | |
| P6 | Save Settings error | Set AP Password to <8 chars | API returns error; message shows "Error: ..." in red | □ | |
| P7 | Save reboot required | Change AP SSID or Password | Message appends " — reboot required"; `reboot_required: true` in API response | □ | |
| P8 | Save network failure | Save while device AP is unreachable | Message shows "Save failed" in red; button re-enables | □ | |
| P9 | Threat timeline view | Navigate to Threats tab with alerts | Red vertical timeline line; colored dots at each event; filled severity badges (◆ CRIT, ⚠ WARN, ● INFO) | □ | |
| P10 | Threat filtering | Click "Deauth" pill | Only deauth-type threats shown; pill shows count `(N)`; "× Clear" button appears; clicking Clear resets to All | □ | |
| P11 | Threat summary cards | Threats page with mixed severity | Severity grid + "By Type" card with per-type counts + "Latest: X ago" indicator | □ | |
| P12 | Relative timestamps | Observe threat ages in feed | "just now" for <5s, "Xm ago" for minutes, "Xh ago" for hours | □ | |
| P13 | Uptime display | Observe Overview tab uptime | Updates every second; matches `millis()/1000` API value | □ | |
| P14 | GET /api/settings | Fetch via curl/browser | Returns JSON: device_name, ap_ssid, ap_pass, threat_sensitivity, homeguard_enabled, aviation_enabled | □ | |
| P15 | POST /api/settings | Valid JSON payload | Returns `{"ok":true,"reboot_required":false,"changed":true}` | □ | |
| P16 | Tab persistence | Navigate tabs, refresh browser | Hash-based routing preserves active tab | □ | |

#### Required Screenshots
- Overview tab: full status card + stat grid + AP info
- Threats tab (with data): summary grid + "By Type" card + filter pills with counts + timeline feed + "× Clear" button visible
- Settings tab: all fields filled + save button
- Settings save success: green "Settings saved" message
- Settings save reboot: "Settings saved — reboot required" message
- Settings save error: red "Error: ..." message

---

### 6. Long-Runtime Validation

| # | Test | Procedure | Pass Criteria | Pass/Fail | Notes |
|---|------|-----------|---------------|-----------|-------|
| L1 | 2-hour Companion stability | Run Companion mode for 2h with PC Agent connected | No freeze; heartbeats every 1s; emotion state machine responds; face animation smooth | □ | |
| L2 | 1-hour Security stability | Run Security mode for 1h with AP client connected | Web portal remains reachable; threat detection active; no crash; heap stays stable (±10%) | □ | |
| L3 | 30-min Aviation stability | Run Aviation mode for 30min with WiFi | Successful fetch every 15s; no OOM; card rendering consistent; no watchdog reset | □ | |
| L4 | Mode cycle endurance | Cycle Companion → Security → Aviation → Companion × 50 | All transitions complete; no stale state; touch response consistent after each transition | □ | |
| L5 | AP client reconnect | Disconnect AP client for 30s, reconnect | Client reconnects; web portal accessible; counters not reset | □ | |
| L6 | Power cycle persist | Power-cycle 5 times across all 3 modes | All NVS settings persist (WiFi creds, bounding box, colors, device name, AP SSID/PASS, threat sensitivity, HomeGuard, Aviation toggles) | □ | |
| L7 | Heap leak check | Record free heap at t=0 and t=60min in Security mode | Heap delta < 5%; no monotonic decline | □ | |
| L8 | Threat ring buffer wrap | Generate >48 threat events | Ring buffer wraps; oldest threats evicted; newest threats displayed; no crash | □ | |
| L9 | TFT burn-in check | Leave Portal display on for 1h | No pixel burn, ghosting, or display corruption | □ | |
| L10 | Serial buffer overflow | Stream 1000 lines of serial commands at once | No data loss; each command processed; no watchdog reset | □ | |

#### Required Logs (L1–L10)
```
[FACE] heartbeat  (every 1s for L1)
[SEC] AP stations=X  (every 5s for L2)
[AVI] OK — X aircraft found  (every 15s for L3)
[MODE] Transitioning: X -> Y  (for each of 50 cycles for L4)
Heap at t=0: X bytes  (manual log)
Heap at t=3600: X bytes  (manual log)
```

#### Required Screenshots
- Companion mode face after 2h (no artifacts)
- Security mode Portal after 1h (counters non-zero, no stale display)
- Aviation mode card after 30min (aircraft data current)

---

## Pass/Fail Criteria

### Global Pass Conditions
- **All critical (C) tests** must pass — no exceptions
- **All high (H) tests** must pass — waiver requires documented rationale and tracker issue
- **Medium (M) tests** — up to 3 failures acceptable with documented follow-up items

### Severity Mapping

| Checklist | Critical Tests | High Tests | Medium Tests |
|-----------|---------------|------------|--------------|
| Hardware | H1, H2, H4, H5 | H3, H6 | H7, H8 |
| Security | S1, S3, S4, S5, S6, S7, S8 | S2, S9, S10, S11 | S12 |
| Home Guard | HG1, HG3, HG4, HG5 | HG2, HG6, HG7 | HG8 |
| Aviation | A1, A2, A6, A7, A8 | A3, A4, A5, A9 | A10, A11 |
| Portal | P1, P2, P3, P4, P5, P6, P7, P8, P14, P15 | P9, P10, P11, P12, P13, P16 | — |
| Long-Runtime | L1, L2, L4, L6, L7 | L3, L5 | L8, L9, L10 |

### Waiver Process
1. Document the failing test ID, observed behavior, and expected behavior
2. Assess whether the failure is a regression from V2.2 behavior
3. If not a regression, file a tracker issue and waive for RC1
4. If a regression, block release until resolved

---

## Required Artifacts Summary

### Screenshots (minimum 12)
| # | View | Condition |
|---|------|-----------|
| 1 | Splash | Power-on, boot screen |
| 2 | Companion face | Idle state |
| 3 | Security TFT | Radar sweep with counters |
| 4 | Overview tab | PPS > 0, AP info |
| 5 | Devices tab (APs) | At least 1 AP detected |
| 6 | Devices tab (Clients) | At least 1 client enrolled |
| 7 | Threats tab | Summary + timeline + filters |
| 8 | Settings tab | All fields populated |
| 9 | Settings save success | Green confirmation |
| 10 | Aviation card | Aircraft data visible |
| 11 | Aviation disabled | "Aviation Disabled" screen |
| 12 | Long-runtime Security | After 1h, heap stable |

### Logs (minimum 4 captures)
| # | Capture | Content |
|---|---------|---------|
| 1 | Full boot log | From `[AeroSniffer] Booting...` through mode entry |
| 2 | Security mode session | 5 minutes including AP client connection + deauth event |
| 3 | Aviation mode session | 1 successful fetch cycle |
| 4 | Long-runtime tail | Heap comparison at t=0 and t=60min |

---

## Companion Mode Validation (Supplemental)

| # | Test | Procedure | Pass Criteria | Pass/Fail | Notes |
|---|------|-----------|---------------|-----------|-------|
| C1 | Default boot | Power on, no mode switch | Companion face visible within 5s | □ | |
| C2 | Emotion state machine | Observe face over 2–3 min | Random emotions trigger; face transitions between expressions | □ | |
| C3 | PC Agent typing | Type on host PC with PC Agent running | Face transitions to THINKING/CURIOUS; returns to IDLE after timeout | □ | Known BUG-007 limitation |
| C4 | PC Agent app change | Switch active window | Face reacts to activity change; `[PC] Activity: <app>` logged | □ | |
| C5 | Companion teardown | Switch to Security mode | `pet_teardown()` called; no crash; mode transition completes | □ | |
| C6 | Companion re-entry | Switch back from Security | `pet_setup()` called; face resumes; no stale state | □ | |

### BUG-007 Special Notes
- Typing emotion may persist after PC Agent stops typing — this is a known open issue (Medium severity)
- The firmware will self-recover within the activity timeout period
- Regression check: animation must NOT freeze permanently; the face must eventually return to IDLE
- If permanent freeze is observed, block release and escalate
