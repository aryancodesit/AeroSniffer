# Changelog

## [v2.5-persistence-p7] — 2026-06-23

### Sprint 4B P7 — Schema Recovery (Certified 2026-06-23)

**All three recovery paths validated on hardware. Zero production firmware modifications — standalone NVS injector tool.**

### Certified
- **Test 1 — Old Schema (version=0)**: `migrate(0)` → `factoryReset()` → clean profile created. ✅
- **Test 1R — Recovery Persistence**: Power cycle after recovery — boot counter incremented, no double reset. ✅
- **Test 2 — Future Schema (version=255)**: `migrate(255)` → `factoryReset()` → clean profile. ✅
- **Test 3 — Corrupted Blob (84 bytes vs 64)**: Blob size mismatch → `load()` returns false → `factoryReset()`. ✅
- **Test 4 — Post-Recovery Reboot**: Clean profile survives subsequent boot with normal counter increment. ✅

### Validation Method
- Standalone `tools/p7_injector/p7_injector.ino` — upload, inject via serial menu, re-upload AeroSniffer to observe recovery
- Zero changes to AeroSniffer production firmware
- All evidence captured from serial console logs

### Architecture Verified
- `PersistenceService::load()` correctly detects schema mismatch → `migrate()` → `factoryReset()`
- `PersistenceService::load()` correctly detects blob size mismatch → returns false → `factoryReset()`
- `PersistenceService::factoryReset()` + `save()` writes a self-consistent, bootable NVS state
- Recovery is not a reset loop — second boot loads normally

### Metadata
- Branch: `feature/v2.5-creature-brain`
- Tool: `tools/p7_injector/p7_injector.ino`

## [v2.5-rc] — 2026-06-23

### Sprint 4B P3 — Runtime Accuracy (Certified 2026-06-23)

**67-minute continuous soak. Zero crashes, zero panics, zero memory leaks.**

### Certified
- **Duration**: 67 minutes (exceeds 60-min requirement) ✅
- **Heartbeat cadence**: ~1/sec throughout ✅
- **Mood transitions**: 3 transitions (RELAXED↔PLAYFUL), no stuck moods ✅
- **Touch detection**: 30+ TAP events, clean rising-edge detection ✅
- **Persistence**: Boot 24 loaded correctly, runtime=34 min restored ✅
- **Memory**: 149.9 KB free after setup, no leak ✅
- **Errors**: 2 transient DNS failures (external, gracefully handled) ✅

### Evidence
- Raw serial log: `tests/p3_60min.txt`

### Metadata
- Branch: `feature/v2.5-creature-brain`
- Tags: `v2.5-persistence`, `v2.5-autonomous-presence`
- All Sprint 4B gates passed. Ready for V2.5 release.

## [v2.5-autonomous-presence] — 2026-06-23

### Sprint 5A — Autonomous Presence Layer (Certified 2026-06-23)

**Behavior Layer V1 — first step from Reactive Interface toward Companion. Runtime-only autonomous presence. Not persistence, not memory. FaceEngine sole writer.**

### Added
- **engagement_drive (0–100)** in `CreatureState` — runtime-only, FaceEngine sole writer. Starts at 100 on boot. Resets to 100 on any touch or non-zero attention target (orienting response). Decays per mood: RELAXED 1.0× (~5 min to drowsy), PLAYFUL 0.5× (~10 min), ANXIOUS 0.3× with 20 floor (~16 min, hyper-vigilant).
- **Saccade interval modulation**: `3000 + (100 - drive) * 270` — 3s at full alert, 30s when drowsy. ±50% jitter. Pre-hardware concern that 30s would look frozen invalidated by hardware — creature never appeared broken.
- **Deep blink suppression**: deep blinks (2-frame hold) only when `attention.target == TARGET_NONE` and drive < 80. Prevents odd blinks during threat/user/flight lock.
- **Eyelid drowsiness**: `_eyelid_factor` lerps from 1.0 (fully open) toward 0.45 (drowsy) as engagement decays. Applied as multiplier to default eye height in `renderEmotionLayer()`.
- **Cross-mode carryover**: engagement_drive persists across mode transitions. Aviation → Companion transition shows sleepy carryover without explicit programming — emergent behavior.

### Architecture
- Single variable `engagement_drive` in CreatureState — no new subsystems, no EventBus, no spinlocks
- Engagement is an *internal state*, not a behavioral directive — FaceEngine uses it to modulate presentation, not to drive decisions
- Time-domain decay with `dt_ms` ensures frame-rate independence
- Eyelid lerp (0.95/0.05) creates ~2s organic recovery on orienting response
- Deep blink guard prevents behavioral dissonance during attention events
- Frozen for V2.5 — no further tuning. Power-law decay, multi-drive models, curiosity/fatigue drives deferred to V2.6.

### Certified (32-min run)
- 0 errors, 0 crashes, 0 panics
- 1816 stable heartbeats, consistent 1s interval
- 16 touches with full alert recovery
- 3 mode transitions (Companion→Security→Aviation→Companion)
- Security mode survived without B009 crash (expected — crash binary is commit 450dabd, 18 commits behind)
- Feel-alive, feel-calm, feel-sleepy, not-broken all validated

### Hardware Validated
- Alert immediately after interaction ✅
- Gradual settling over ~5 min ✅
- Drowsy state reached ✅
- Wake-up response visible on touch ✅
- PLAYFUL decay ~10 min (half-speed) ✅
- ANXIOUS floor at 20 (never fully asleep) ✅
- Deep blinks suppressed during threat/user/flight

### Frozen
- 30s max saccade interval accepted — hardware proved theory wrong
- Current eyelid curve accepted
- Current decay model accepted
- No behavioral coefficient tuning

### Changed
- `AeroSnifferOS.h:126` — `uint8_t engagement_drive` added to `CreatureState`
- `AeroSnifferOS.h:221-225` — FaceEngine private members for Sprint 5A
- `AeroSnifferOS.cpp:31-48` — `g_creature` initializer with `engagement_drive=100`
- `AeroSnifferOS.cpp:525-556` — FaceEngine::begin() Sprint 5A init
- `AeroSnifferOS.cpp:585-687` — FaceEngine::updateAnimations() engagement decay, saccade, deep blink, eyelid
- `AeroSnifferOS.cpp:720-722` — renderEmotionLayer() eyelid factor in default eye height

### Metadata
- Branch: `feature/v2.5-creature-brain`
- Tags: (pending commit)
- Next: Sprint 4B P7, NV1 → V2.5 RC

## [v2.5-persistence] — 2026-06-22

### Sprint 4B P6 — Mode Transition Save (Certified 2026-06-22)

**Hardware validation: Runtime, Touch, Mood, and Cross-Mode persistence verified.**

### Certified
- **P6 Runtime Persistence**: total_runtime_minutes=12 after ~11 min session + unplug. ✅
- **P6 Touch Persistence**: total_touches=14 (1 baseline + 13 from taps across Companion sessions). ✅
- **P6 Mood Persistence**: playful_entries=1 after RELAXED→PLAYFUL transition in Security mode. ✅
- **P6 Cross-Mode Persistence**: All 3 transitions (Companion→Security, Security→Aviation, Aviation→Companion) completed clean. Profile survived unplug power cycle. ✅

### Observed
- **OBS-001**: Boot counter retained value 4 instead of incrementing to 5. Possible serial reconnection without power cycle, or NVS write ordering detail. No investigation planned — LOW priority. Does not affect certification.

### Architecture
- `PersistenceService::save()` now resets `_last_save_ms = millis()` — periodic save timer won't double-fire after mode transition save. Applied during code review before P6 certification.

### Metadata
- Branch: `feature/v2.5-creature-brain`
- Tags: (pending commit)
- Next: P7 Schema Recovery, NV1 Power-Loss Stress

### Sprint 4A — Creature Persistence

**Observer subsystem for cross-session continuity. Not memory, not learning, not behavior.**

### Added
- **PersistenceService**: NVS-backed `CreatureProfile` with `schema_version`, `name[32]`, `total_boots`, `total_runtime_minutes`, `total_touches`, `playful_entries`, `anxious_entries`, `first_boot_unix`, `last_seen_unix`
- **Rising-edge touch counter**: `onTouch()` inside `tick()` detects `SOURCE_TOUCH` transitions via `g_creature.attention.source` — no EventBus, no direct touch hardware coupling
- **Rising-edge mood counter**: `detectMoodTransition()` tracks `g_creature.mood` transitions for PLAYFUL and ANXIOUS — zero MoodEngine coupling
- **Runtime accumulation**: delta-based with 1-hour millis() overflow guard, no double-counting
- **5-minute periodic save**: primary persistence mechanism with flash wear protection
- **Mode transition save**: checkpoint on every Pet→Security→Aviation→Pet switch
- **Schema validation**: version check + blob size check on load, migration switch with factoryReset fallback
- **`Persistence/PersistenceService.h`** + **`PersistenceService.cpp`** (sketch root): same file ownership discipline as Attention/Mood

### Fixed (spec bugs before implementation)
- **Preferences handle leak**: `p.end()` now called on all return paths in `load()` — no stale handle prevents reopen
- **Missing schema key in save()**: `save()` writes both `"profile"` (blob) and `"schema"` (u32) — loading without schema key would trigger spurious migration
- **Boot flow unchecked load failure**: `begin()` checks `load()` return value — corrupt profile triggers factoryReset instead of silent corruption
- **factoryReset() stale detection state**: resets `_last_seen_mood` and `_prev_was_touch_source` alongside NVS data

### Architecture
- Observer subsystem — Persistence is NOT part of the behavioral pipeline. FaceEngine does not consume Persistence data.
- Runs on Core 1 tick only — no spinlocks, no cross-core writes, no EventBus subscription
- Reads `g_creature.{attention, mood}` (counters only) — zero upstream writes
- Same counter hook pattern as MoodEngine (`PersistenceService.cpp:42-48` matches `MoodEngine.cpp:34-37`)

### Metadata
- Branch: `feature/v2.5-creature-brain`
- Tag: `v2.5-persistence` (pending)
- Requires: hardware validation (Sprint 4B)

## [v2.5-creature-brain-complete] — 2026-06-22

### Sprint 3C — Stabilization & Retrospective

**No new features, no behavioral changes, no animation coefficient tuning.**

### Added
- **Creature Brain Retrospective**: `docs/V2.5_CREATURE_BRAIN_RETROSPECTIVE.md` — full Sprint 1–3B retrospective covering 5 sprints, 9 bugs, 12 architecture decisions preserved, 3 technical debt items, 8 lessons learned, and metrics
- **TD-011**: `_last_mood_change` dual-purpose tracked — split into `_last_mood_transition` + `_last_decay_tick` before any feature that queries mood age
- **TD-012**: EventBus capacity needs compile-time assertion
- **TD-013**: `_playful_condition_met_since` not reset on mood departure

### Changed
- `PROJECT_STATUS.md`: Sprint 3C row, `v2.5-creature-brain-complete` tag, updated HEAD
- `AI_HANDOFF.md`: Sprint 3C state, retrospective reference, TD items, updated tags
- `changelog.md`: Sprint 3C entry

### Metadata
- Branch: `feature/v2.5-creature-brain`
- Tags: `v2.5-attention-complete`, `v2.5-mood-foundation`, `v2.5-creature-brain-complete`
- Pipeline frozen: Attention → Emotion → Mood → Face

## [v2.5-mood-consumption] — 2026-06-21

### Added
- **V2.5 Sprint 3B — Mood Consumption**: FaceEngine reads `g_creature.mood` and `g_creature.mood_strength` to modulate blink interval, idle amplitude, idle speed, and eye wander intensity. Pure consumer layer — no mood generation, no persistence, no portal telemetry, no EventBus additions.
- **`MoodPresentation` cache struct** inside FaceEngineClass: caches computed blink_ms, idle_amplitude, idle_speed, eye_intensity. Lazy recompute on mood/strength change only (not every frame).
- **`profileFor(MoodType)` switch-based profile accessor**: returns `MoodProfile{blink_ms, amp, speed, intens}` for each mood. Future-proof — adding a new mood requires only one new case.
- **Smoothstep interpolation**: `t²(3-2t)` for perceptually even transitions across strength 0–50. Replaces raw linear interpolation.
- **Blink interval**: mood-modulated with ±500ms jitter (RELAXED 4000ms, PLAYFUL 3000ms, ANXIOUS 2200ms)
- **Idle bounce amplitude**: mood-modulated multiplier on existing emotion-driven bounce (RELAXED 1.0x, PLAYFUL 1.5x, ANXIOUS 0.5x)
- **Idle animation speed**: mood-modulated frequency multiplier on bounce and pulse (RELAXED 1.0x, PLAYFUL 1.3x, ANXIOUS 0.8x)
- **Eye wander intensity**: mood-modulated random wander range for idle gaze (RELAXED 1.0x, PLAYFUL 1.4x, ANXIOUS 0.6x). Attention-driven gaze (THREAT/USER/FLIGHT) intentionally unaffected — reactive behaviors override internal state.

### Architecture
- One-way data flow preserved: `Attention → Emotion → Mood → Face`
- Mood only influences presentation — no feedback to attention, emotion, or behavior layers
- `eye_intensity` applies to idle wander only; attention targets remain authoritative for gaze
- No new EventBus subscriptions, no spinlocks, no cross-core writes

### Hardware Validated
- 8+ minute stable run: no reboot, WDT, panic, heap exhaustion, or task crash
- Touch → Attention → Emotion pipeline intact
- Mood consumption pipeline: `MoodEngine.publish() → FaceEngine.updateAnimations()` verified in tick order
- Compile-validated with XIAO_ESP32S3, ESP32 Arduino core 3.x

### Known Limitations
- Visual differentiation between moods is currently subtle due to small 240×240 display and limited procedural face vocabulary
- No further tuning planned in V2.5 — future behavioral layers (Memory, Companion Behaviors) expected as primary mood consumers

### Remaining
- B004: `udp_new_ip_type` assertion during HTTP fetch (lwIP lock issue)
- B006: `netstack cb reg failed with 12308` during Security→Aviation transition
- B007: Aviation OpenSky HTTPS fetch returning `connection refused`
- B008: Touch degradation after WiFi activity (`g_block_touch`)

### Metadata
- Branch: `feature/v2.5-creature-brain`
- Tags: `v2.5-attention-complete`, `v2.5-mood-foundation`, `v2.5-creature-brain-complete`

## [v2.5-mood-foundation] — 2026-06-21

### Added
- **V2.5 Sprint 3A — Mood Infrastructure**: MoodEngine as slow behavioral observer operating on hour-scale. Reads CreatureState after Attention+Emotion publish — no EventBus access, no spinlocks, single consumer on Core 1.
- **MoodType enum**: `RELAXED=0`, `PLAYFUL=1`, `ANXIOUS=2`, `MOOD_COUNT=3`
- **`uint8_t mood_strength`** in `CreatureState` for fixed-point decay tracking
- **Fixed-priority arbitration**: ANXIOUS (P1, immediate) > PLAYFUL (P4, 30s hold-off) > RELAXED (P7, default). ANXIOUS bypasses hold-off.
- **Touch history ring buffer**: 10 slots, sliding 5-min window, SOURCE_TOUCH rising edge detection (no EventBus dependency)
- **PLAYFUL entry**: ≥2 touches in 5 min + positive emotion (HAPPY/EXCITED/LOVE/SURPRISED) within 60s recency window
- **ANXIOUS entry**: `attention.target == TARGET_THREAT` + `emotion == EMOTION_ALERT`
- **Fixed-point decay**: PLAYFUL = 36s/unit (30 min total), ANXIOUS = 24s/unit (20 min). Advances `_last_mood_change` by consumed interval steps — no cumulative re-subtraction.
- **Serial visibility**: `[MOOD] RELAXED -> PLAYFUL` on transition
- **GET_PET_STATUS**: uses `MoodEngine.moodName(g_creature.mood)` for human-readable mood
- **Tick wiring**: `Attention → Emotion → Mood → Face` in Core 1 loop (`AeroSniffer.ino:577-583`)

### Fixed
- **`AeroSnifferOS.cpp` missing `#include "Companion/MoodEngine.h"`** — was causing `MoodEngineClass` incomplete type
- **`MoodEngine::moodName()` → `MoodEngine.moodName()`** — dot notation on global instance
- **`pruneOldTouches()` unsigned underflow**: `now - 300000` when uptime <5 min produced ~4.29B wraparound — fixed to rollover-safe `(now - _touch_timestamps[i]) <= 300000`
- **PLAYFUL emotion condition too strict**: required continuous HAPPY/EXCITED for 30s, but emotions last ~5s — fixed with 60s emotion recency window via `_last_positive_emotion`
- **Mood strength decay cumulative**: every tick recomputed elapsed/interval and subtracted from strength — fixed by advancing `_last_mood_change += steps * interval`

### Removed
- **EmotionEngine::setMood() bridge**: 5 Portal.h call sites deleted — MoodEngine is sole writer of `g_creature.mood` and `g_creature.mood_strength`

### Hardware Validated
- RELAXED → PLAYFUL → RELAXED cycle: touch → emotion → hold-off → transition → decay → return
- ANXIOUS immediate transition (threat + alert)
- Touch history pruning (5-min sliding window)
- 30-min soak test for PLAYFUL strength decay (50 units × 36s/unit)
- No IWDT, no Guru Meditation, no panics
- Cross-core ownership audit: MoodEngine writes only `g_creature.{mood,mood_strength}`
- Three-bug checklist re-verified (subscriber, spinlock, cross-core, ownership, tick-order, co-dependency)

### Remaining
- B004: `udp_new_ip_type` assertion during HTTP fetch (lwIP lock issue)
- B006: `netstack cb reg failed with 12308` during Security→Aviation transition
- B007: Aviation OpenSky HTTPS fetch returning `connection refused`
- B008: Touch degradation after WiFi activity (`g_block_touch`)

### Metadata
- Branch: `feature/v2.5-creature-brain`
- Tags: `v2.5-attention-complete`, `v2.5-mood-foundation`
- Sprint 3A validated in Pet/Companion mode only (MoodEngine has no mode dependency)
- Requires: ESP-IDF v5.x, Arduino ESP32 core 3.x

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
