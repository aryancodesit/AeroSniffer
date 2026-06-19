# Changelog

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
