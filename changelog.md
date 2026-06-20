# Changelog

## [v2.5-sprint2a] — 2026-06-20

### Added
- **V2.5 Sprint 2B — Compatibility Shim Removal**: Removed `attention_state` field from `CreatureState`, shim write from `AeroSniffer.ino` and `AttentionEngine::publish()`, and `getState()` method (zero remaining callers). Zero behavioral change — FaceEngine already consumed structured fields since Sprint 2A.

### Added
- **V2.5 Sprint 1 — Companion Intelligence Foundation**: Structured attention model with `AttentionTarget`/`AttentionSource` enums, `attention.strength` (0-100), fixed-point 3-zone decay, priority preemption, event-to-attention mapping, queue integration, pause/resume lifecycle, V2.4 `attention_state` rollback shim (removed in Sprint 2B)
- **V2.5 Sprint 2A — FaceEngine Attention Migration**: FaceEngine gaze now driven by `g_creature.attention.{target,strength}` instead of `attention_state`. TARGET_THREAT locks center, TARGET_USER scales upward gaze with strength (-1 to -3), TARGET_FLIGHT scales skyward gaze (-1 to -4), TARGET_NONE + strength≥25 gives mild curiosity (-1)

### Fixed
- **MAX_SUBSCRIBERS 24→32**: EventBus subscriber exhaustion — EmotionEngine used 22 slots, leaving only 2 for AttentionEngine. All events now route correctly.
- **portMUX spinlock init**: `portMUX_TYPE _mux = {}` → `portMUX_INITIALIZER_UNLOCKED`. Root cause of touch-triggered IWDT reboots.
- **Serial removed from critical sections**: `Serial.println()` inside `portENTER_CRITICAL()` removed from `AttentionEngine::queueEvent()`.

### Hardware Validated
- All three modes (Pet, Security, Aviation) stable with touch, WiFi, mode transitions
- `[ATTN] SOFT_FOCUS -> WATCHING (TOUCH_SHORT)` and `[ATTN] WATCHING -> SOFT_FOCUS (decay)` confirmed
- Sprint 2A gaze behavior confirmed: correct eye movement on tap, decay, threat, flight, mode switches
- No IWDT, no Guru Meditation, no panics

### Remaining
- B004: `udp_new_ip_type` assertion during HTTP fetch (lwIP lock issue)
- B006: `netstack cb reg failed with 12308` during Security→Aviation transition
- B007: Aviation OpenSky HTTPS fetch returning `connection refused`
- B008: Touch degradation after WiFi activity (`g_block_touch`)

### Metadata
- Branch: `feature/v2.5-creature-brain`
- Tag: (next release)
- Requires: ESP-IDF v5.x, Arduino ESP32 core 3.x

## [v2.4-transition-stable] — 2026-06-19

### Added
- **AttentionEngine Phase 1**: MPSC ring buffer with `portMUX_TYPE` spinlock, `queueEvent()`, `tick()`, `pause()`, `resume()`, overflow stats
- **AttentionEngine Phase 2**: Three-state machine (SOFT_FOCUS, WATCHING, THREAT_LOCK), priority-driven preemption (P1–P4), millis()-based decay deadlines, Serial diagnostics
- **AttentionEngine Phase 3A**: OS wiring — forward decl + extern in `AeroSnifferOS.h`, global instance in `AeroSnifferOS.cpp`, `#include`, `ae_event_callback`, `AttentionEngine.begin()` + 7 EventBus subscriptions, `tick()` hook, `pause()`/`resume()` in mode switch path
- Breadcrumb instrumentation throughout mode transition path (BEFORE teardown, AFTER teardown, AFTER pause, AFTER splash, BEFORE setup, AFTER setup, AFTER resume)

### Changed
- `AttentionEngine.cpp` moved from `Companion/` to sketch root for Arduino IDE compatibility
- `AeroSniffer.ino`: added AttentionEngine include, event callback, begin + subscriptions in setup, tick in task_core1, pause/resume in mode transition
- `AeroSnifferOS.h`: forward declaration and extern for `AttentionEngineClass`
- `AeroSnifferOS.cpp`: global `AttentionEngine` instance

### Fixed
- **B001**: TFT_eSPI display init — `HW_DEVKITC` → `HW_DESKBUDDY_2` in `User_Setup.h`
- **B002**: WiFi PMF timeout — added `esp_wifi_set_pmf_config(PMF_OPTIONAL)` to `connectSTA()`
- **B003**: Linker error — moved `AttentionEngine.cpp` to sketch root
- **B005**: Interrupt WDT during mode transition — removed spinlock from `AttentionEngine.pause()`

### Disabled (isolation testing)
- `pet_fetch_standalone_weather()` — isolated `udp_new_ip_type` assertion
- `AttentionEngine.tick()` — pending serial validation of state transitions

### Remaining
- **B004**: `udp_new_ip_type` assertion during HTTP fetch (lwIP lock issue)
- **B006**: `netstack cb reg failed with 12308` during Security→Aviation transition
- **B007**: Aviation OpenSky HTTPS fetch returning `connection refused`
- **B008**: Touch degradation after WiFi activity (`g_block_touch`)

### Metadata
- Branch: `feature/v2.4-companion-intelligence`
- Requires: ESP-IDF v5.x, Arduino ESP32 core 3.x
