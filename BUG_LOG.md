# Bug Log — AeroSniffer V2.4

## Resolved

### B001 — TFT_eSPI display fails to initialize on DeskBuddy 2.0
- **Status**: Fixed
- **Root cause**: `User_Setup.h` selected `HW_DEVKITC` instead of `HW_DESKBUDDY_2`
- **Fix**: Changed to `HW_DESKBUDDY_2` in `User_Setup.h`
- **Verified**: Display initializes and renders correctly across all modes

### B002 — WiFi connection timeout (reason 210) on phone hotspots
- **Status**: Fixed
- **Root cause**: ESP32-S3 default PMF (Protected Management Frames) is `PMF_REQUIRED`; many phone hotspots don't support it, causing assoc timeout
- **Fix**: Added `esp_wifi_set_pmf_config()` with `PMF_OPTIONAL` in `WiFiService::connectSTA()`
- **Verified**: Connects reliably to phone hotspots

### B003 — Linker error: undefined reference to AttentionEngine functions
- **Status**: Fixed
- **Root cause**: Arduino IDE compiles only `.cpp` files in sketch root; `AttentionEngine.cpp` in `Companion/` subdirectory was not compiled
- **Fix**: Moved `AttentionEngine.cpp` to sketch root; changed `#include` to `"Companion/AttentionEngine.h"`
- **Verified**: Linker error resolved

### B004 — `udp_new_ip_type` assertion crash during HTTP fetch
- **Status**: Isolated (fix pending)
- **Root cause**: `HTTPClient` → `WiFiClient::connect(hostname)` triggers lwIP DNS → `udp_new_ip_type` without TCP/IP core lock in ESP-IDF v5.x SMP
- **Workaround**: Disabled `pet_fetch_standalone_weather()` — system runs clean with weather fetch disabled
- **Fix plan**: Use IP literal with `WiFi.hostByName()` resolution before HTTP fetch, or wrap in `tcpip_adapter` lock

### B005 — Interrupt WDT on CPU1 during mode transition
- **Status**: Fixed (verified on hardware)
- **Root cause**: `AttentionEngine.pause()` called `portENTER_CRITICAL(&_mux)` on Core 1 while Core 0 held the same spinlock in `queueEvent()`. Core 1 spun with interrupts masked, tripping the 300ms Interrupt WDT.
- **Fix**: Removed spinlock from `pause()` — `_paused` flag alone is sufficient; `queueEvent()` discards events while paused, `tick()` returns early. Pre-pause events remain in the queue and drain naturally on the next `tick()` after `resume()`.
- **Verified**: Multiple mode-switch cycles clean — no WDT

### B005b — Interrupt WDT on CPU1 during AttentionEngine tick()
- **Status**: Fixed (verified on hardware)
- **Root cause**: Same spinlock contention as B005. `AttentionEngine.tick()` on Core 1 called `portENTER_CRITICAL` while Core 0 held the same spinlock in `queueEvent()`. Single-threaded consumer (Core 1) does not need a spinlock — contention with Core 0 writer caused the WDT.
- **Fix**: Replaced spinlock in `tick()` with lock-free drain using volatile `_head`/`_tail`. Consumer reads entries without locking; producer writes with spinlock. Safe because producer and consumer never touch the same index.
- **Verified**: 30-minute soak with tick() enabled — zero WDTs

### B006 — `netstack cb reg failed with 12308` during Security→Aviation transition
- **Status**: Deferred to V2.5 (non-blocking)
- **Root cause**: Unknown error code; likely netif initialization race during AP teardown + STA reconnect sequence
- **Workaround**: Not blocking stability — mode transition succeeds despite the error

## Open (deferred to V2.5)

### B007 — Aviation OpenSky HTTPS fetch returns `connection refused`
- **Severity**: Low (V2.5)
- **Root cause**: Unknown. `WiFi.status() == WL_CONNECTED`, but HTTPS GET returns HTTP Code -1 (`connection refused`). May be DNS failure or TLS certificate validation issue.
- **Notes**: HTTP (non-HTTPS) fetch to same host should be tested to isolate TLS vs. DNS vs. routing

### B008 — Touch degradation after WiFi activity
- **Severity**: Low (V2.5)
- **Root cause**: Instances where `g_block_touch` flag blocks touch processing after WiFi operations
- **Notes**: Not currently a functional blocker
