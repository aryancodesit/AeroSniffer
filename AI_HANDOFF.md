# AI Handoff — AeroSniffer V2.4

## Current State

The AttentionEngine MVP (Phases 1–3A) is implemented and wired into the OS layer. All three modes (Pet, Security, Aviation) cycle cleanly in hardware with weather fetch disabled. The system has crossed the mode-transition stability milestone for serial-only validation.

### What's Working
- All three modes boot, render, and respond to touch
- Mode transitions Pet→Security→Aviation→Pet succeed with weather fetch disabled and pause/resume re-enabled with spinlock fix
- EmotionEngine ticks independently and transitions correctly
- FaceEngine heartbeat renders at 1 Hz
- Touch input (tap, long press) works in all modes
- WiFi connect/disconnect lifecycle clean on mode entry/exit
- Serial breadcrumbs provide ±1-line crash isolation

### What's Disabled (isolation testing)
- `pet_fetch_standalone_weather()` — disabled to avoid `udp_new_ip_type` assertion
- `AttentionEngine.tick()` — disabled pending state-transition validation
- Mode switch `delay(500)` — removed; 600ms splash with `delay(600)` kept for visual feedback

### Uncommitted Changes
- `AeroSniffer/AttentionEngine.cpp` (new — sketch root)
- `AeroSniffer/AeroSniffer.ino` (pause/resume re-enabled, breadcrumbs added, tick commented)
- `AeroSniffer/AeroSnifferOS.h` (forward decl + extern for AttentionEngine)
- `AeroSniffer/AeroSnifferOS.cpp` (global AttentionEngine instance)
- `AeroSniffer/Mode1_Pet.h` (weather fetch commented out)
- `aerosniffer.html` (companion UI updates)
- Deleted: `AeroSniffer/Companion/AttentionEngine.cpp`

## Known Issues to Resolve (in priority order)

### P1 — Validate pause() spinlock fix (B005)
- [ ] Flash current state
- [ ] Run 5+ full mode cycles (0→1→2→0)
- [ ] Confirm `[ATTN] pause` + `[DBG] AFTER pause` appear
- [ ] If clean: scenario A confirmed for WDT root cause

### P2 — Re-enable AttentionEngine.tick() (Phase 3C)
- [ ] Uncomment tick in `AeroSniffer.ino` (lines 574–576)
- [ ] Subscribe to `EVENT_TOUCH_SHORT`, `EVENT_TOUCH_LONG`, `EVENT_FLIGHT_RARE`
- [ ] Validate state transitions via serial: `[ATTN] SOFT_FOCUS -> WATCHING (TOUCH_SHORT)`
- [ ] Validate decay: state returns to SOFT_FOCUS after priority timeout

### P3 — Re-enable weather fetch with lwIP fix (B004)
- [ ] Uncomment `pet_fetch_standalone_weather()` in `Mode1_Pet.h`
- [ ] Apply fix: resolve hostname via `WiFi.hostByName()` before HTTP fetch
- [ ] Or wrap HTTP in `tcpip_adapter_lock()` / use IP literal
- [ ] Verify no `udp_new_ip_type` assertion through multiple mode cycles

### P4 — Fix Aviation HTTPS fetch (B007)
- [ ] Test HTTP (non-TLS) to isolate TLS vs. DNS vs. routing
- [ ] Verify certificate bundle is present
- [ ] Consider `WiFiClientSecure::setInsecure()` for testing
- [ ] Fix root cause

### P5 — Investigate `netstack` error (B006)
- [ ] Capture full log of Security→Aviation transition
- [ ] Decode error code 12308
- [ ] Evaluate if netif teardown/setup order needs fixing

### P6 — Re-enable pause/resume serial flush
- [ ] Re-add `delay(500)` after splash(600) if needed for serial clarity
- [ ] Or defer to V2.5

## Next Phase

### Phase 3B — FaceEngine Gaze Integration
- AttentionEngine provides context via getters
- FaceEngine::setGazeTarget(x, y) driven by attention state
- Threat gaze: centre (0, 0) — no random threat direction
- EmotionEngine remains sole emotion authority

### Phase 3C — Serial Validation
- Enable tick, inject events manually via serial (if supported)
- Log all state transitions, verify decay timings

### Phase 3D — Controlled Event Injection
- 7 EventBus subscriptions active (Phase 3A)
- Inject events from WiFi, security, flight detection
- Validate state machine responds correctly

### Phase 4 — Habituation
- Preserve habituation across pause/resume
- Implement per-priority habituation counters

## File Map
- `AeroSniffer/AttentionEngine.cpp` — queue + state machine implementation (sketch root)
- `AeroSniffer/Companion/AttentionEngine.h` — class declaration
- `AeroSniffer/AeroSnifferOS.h:315-317` — forward decl + extern
- `AeroSniffer/AeroSnifferOS.cpp:34` — global `AttentionEngine` instance
- `AeroSniffer/AeroSniffer.ino` — wiring: begin, subscriptions, tick, pause/resume
- `docs/V2.4_ATTENTION_ENGINE_SPEC.md` — architecture spec
- `docs/V2.4_ATTENTION_ENGINE_IMPLEMENTATION_PLAN.md` — implementation plan
- `docs/ANCHORED_SUMMARY.md` — running engineering log

## Git
- Branch: `feature/v2.4-companion-intelligence`
- Tag: `v2.4-transition-stable`
- 4 commits ahead of origin (unpushed)
