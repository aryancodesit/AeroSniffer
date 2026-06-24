## V2.6 Sprint 2 — Memory Formation Expansion (2026-06-24)

**CERTIFIED — 30-min hardware soak on DeskBuddy 2.0.**

Duration-based touch classification: TAP (≤300ms), HOLD (301–800ms), LONG_HOLD (801–1499ms), DOUBLE (gap 50–500ms), BURST (5+ in 10s window). All 5 subtypes formed and verified on real hardware.

### Changed
- `MemoryEngine.h/cpp`: PendingTouch ring buffer (replaced monolithic counter), duration-based classification in `observe()`, DOUBLE/BURST promotion, real `touch_duration_ms` instead of 0
- `MemoryTypes.h`: `dropped_touch_events` added to `MemorySummary` for diagnostics
- `MemoryEngine.cpp`: `#include "Config.h"` for `TAP_MAX_MS`/`HOLD_MAX_MS` constants
- `docs/V2.6_SPRINT2_RESULTS.md`: Full certification report

### Added
- Home Guard MVP
- Trusted Device Presence Tracking
- Device Classification (Trusted/Familiar/New)
- Settings Persistence
- Threat Sensitivity Control
- Aviation Enable/Disable Control
- Threat Timeline UI
- Threat Severity Visualization
- Threat Summary Dashboard

### Improved

- Devices Page
- Threats Page
- Portal UX
- Settings UX
- Home Guard Visibility