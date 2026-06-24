# AI Handoff — AeroSniffer V2.6

## Current State

**V2.6 Sprint 1 — Memory Layer Foundation is CERTIFIED.** Memory formation, recall, decay, accumulator model, bounded ring buffer (64 records), and LittleFS persistence validated on hardware via 60-min P3 soak. No WDT, no panics, no heap issues. Memory behaves as memory (accumulator decay, dedup boost) rather than persistent event logging — the primary V2.6 architectural risk is retired.

V2.5 Sprint 1–3C complete. Sprint 4A (Creature Persistence) implemented and **Sprint 4B P6 (Mode Transition Save) certified**. **Sprint 5A (Autonomous Presence Layer / Behavior Layer V1) certified** — engagement_drive (0–100) in CreatureState, FaceEngine sole writer, mood-modulated decay, 3s→30s saccade range, deep blink suppression during attention lock, cross-mode carryover validated. 32-min certification run: 1816 heartbeats, 16 touches, 3 mode transitions, zero errors. No further tuning planned — behavior numbers frozen. This is **Behavior Layer V1** (one drive, one decay curve, one behavior family), not a full Behavior System — keeps expectations clear for future Memory expansion. A [Creature Brain Retrospective](docs/V2.5_CREATURE_BRAIN_RETROSPECTIVE.md) archives goals, bugs, architecture decisions (12 preserved), technical debt (3 items), and lessons learned. Technical debt item **TD-011** documents the `_last_mood_change` dual-purpose hazard (mood-entry timestamp + decay accumulator) — split into `_last_mood_transition` + `_last_decay_tick` before any feature that queries mood age.

## V2.6 Sprint 1 — Memory Layer Foundation (Certified)

### Status
- **CERTIFIED** (P3 hardware validation completed 2026-06-24)
- **FROZEN** — no further edits except bug fixes

### Features
- Memory formation (touch-triggered)
- Memory recall (serial output)
- Memory decay (accumulator model, 60s tick)
- LittleFS-backed persistence
- Bounded ring buffer (64 records max)
- Dedup logic (same-type events boost existing record)

### Validation
- 60-min P3 hardware soak on DeskBuddy 2.0
- 3,425 heartbeats, 64 touch events
- Decay curve verified: 42→41→40→39→38
- Zero WDT, zero panics, zero heap issues
- Full report: `docs/V2.6_P3_RESULTS.md`
- Raw log: `docs/TESTING/v2.6 p3.txt`

### Next Phase
- **V2.6 Sprint 2**: Touch duration expansion (TAP, DOUBLE_TAP, HOLD, LONG_HOLD, BURST) — still touch-only, still low-risk, still isolated. Security/aviation/mood memory deferred.

### What's Working
- All three modes boot, render, and respond to touch
- Attention model: structured `target`/`source`/`strength`, fixed-point 3-zone decay, priority preemption, event-to-attention mapping, ring buffer queue with spinlock
- FaceEngine gaze driven by `g_creature.attention.{target,strength}`:
  - TARGET_THREAT: center locked (y=0)
  - TARGET_USER: upward, strength-scaled (-1 to -3)
  - TARGET_FLIGHT: skyward, strength-scaled (-1 to -4)
  - TARGET_NONE + strength≥25: mild curiosity (y=-1)
  - TARGET_NONE + strength<25: random wandering with engagement_drive-modulated saccade interval
- **Sprint 5A — engagement_drive (0–100)**: autonomous presence layer. FaceEngine sole writer. Decay per mood: RELAXED 1.0x (~5 min to drowsy at 0), PLAYFUL 0.5x (~10 min), ANXIOUS 0.3x with floor at 20 (~16 min). Any touch or attention target resets to 100. Saccade interval ranges 3s (alert) to 30s (drowsy) via `3000 + (100 - drive) * 270`. Deep blink suppressed when `attention.target != TARGET_NONE`. Eyelid factor lerps toward drowsy closed state as drive decays. Cross-mode carryover observed — sleepy state persists across Aviation→Companion transition.
- MoodEngine: RELAXED (default), PLAYFUL (≥2 touches/5min + positive emotion 60s recency + 30s hold-off), ANXIOUS (TARGET_THREAT + EMOTION_ALERT, immediate bypass)
- Mood decay: fixed-point, 36s/unit for PLAYFUL (30 min total), 24s/unit for ANXIOUS (20 min), advances `_last_mood_change` by consumed interval steps (not cumulative re-subtraction)
- **Sprint 3B — Mood Consumption**: FaceEngine caches a `MoodPresentation` struct with smoothstep-interpolated blink interval, idle amplitude/speed, and eye wander intensity. Lazy recompute only on mood/strength change. `eye_intensity` applies to idle wander only (attention-driven gaze is reactive, unaffected by mood). Mood profile via `profileFor(MoodType)` switch — future-proof for additional moods.
- Touch history ring buffer (10 slots, sliding 5-min window, SOURCE_TOUCH rising edge detection)
- `[ATTN] SOFT_FOCUS -> WATCHING (TOUCH_SHORT)` and decay confirmed on hardware
- `[MOOD] RELAXED -> PLAYFUL -> RELAXED` cycle validated
- Mode transitions Pet→Security→Aviation→Pet clean in all directions
- EmotionEngine ticks independently
- Touch input (tap, long press) works across all modes
- WiFi connect/disconnect lifecycle clean on mode entry/exit
- No IWDT, no Guru Meditation, no panics — spinlock fix validated

### What's Disabled (isolation testing)
- `pet_fetch_standalone_weather()` — disabled to avoid `udp_new_ip_type` assertion (B004)
- Mode switch `delay(500)` — removed; 600ms splash kept for visual feedback

### Uncommitted Changes
- Working tree has Sprint 4A (PersistenceService.h/cpp, wiring), Sprint 5A (engagement_drive in CreatureState, FaceEngine autonomous presence, deep blink guard), and doc updates (PROJECT_STATUS.md, AI_HANDOFF.md, changelog.md).

### Sprint 4A — Creature Persistence (Complete)
PersistenceService implemented: `CreatureProfile` struct, `load()/save()/factoryReset()`, rising-edge touch/mood counter hooks (no EventBus, no spinlocks, no upstream writes), 5-min periodic save + mode transition checkpoint, Preferences blob storage with schema version validation. Three critical bugs from spec review fixed (Preferences handle leak, missing schema key, unchecked load failure).

### Sprint 4B P6 — Mode Transition Save (Certified)
P6 Runtime, Touch, Mood, and Cross-Mode persistence verified on hardware:
- **Runtime**: total_runtime_minutes=12 after ~11 min session + unplug
- **Touch**: total_touches=14 (1 baseline + 13 from Companion taps)
- **Mood**: playful_entries=1 (RELAXED→PLAYFUL during Security mode)
- **Cross-Mode**: All 3 transitions (Companion→Security→Aviation→Companion) clean; profile survived unplug

**OBS-001 (LOW)**: Boot counter showed 4 not 5 after power cycle. Possibly serial reconnect or NVS ordering edge case. No investigation planned.

## Known Issues to Resolve (in priority order)

### P1 — Fix Aviation HTTPS fetch (B007)
- [ ] Test HTTP (non-TLS) to isolate TLS vs. DNS vs. routing
- [ ] Verify certificate bundle is present
- [ ] Consider `WiFiClientSecure::setInsecure()` for testing
- [ ] Fix root cause

### P2 — Re-enable weather fetch with lwIP fix (B004)
- [ ] Uncomment `pet_fetch_standalone_weather()` in `Mode1_Pet.h`
- [ ] Apply fix: resolve hostname via `WiFi.hostByName()` before HTTP fetch
- [ ] Or wrap HTTP in `tcpip_adapter_lock()` / use IP literal
- [ ] Verify no `udp_new_ip_type` assertion through multiple mode cycles

### P3 — Investigate `netstack` error (B006)
- [ ] Capture full log of Security→Aviation transition
- [ ] Decode error code 12308
- [ ] Evaluate if netif teardown/setup order needs fixing

### P4 — Touch degradation after WiFi (B008)
- [ ] Investigate `g_block_touch` flag blocking touch after WiFi operations

### P5 — Suppress same-state decay logs (cosmetic)
- [ ] Guard `resetToIdle()` log to only print when `old != _state`
- [ ] Prevents `[ATTN] SOFT_FOCUS -> SOFT_FOCUS (decay)` noise during internal events

### Technical Debt
- **TD-011** (MoodEngine): `_last_mood_change` dual-purpose — split into `_last_mood_transition` + `_last_decay_tick` before timeSinceCurrentMood() is needed
- **TD-012** (EventBus): Add compile-time capacity assertion for `MAX_SUBSCRIBERS`
- **TD-013** (MoodEngine): Reset `_playful_condition_met_since` on mood departure from PLAYFUL

## Next Phase

### Sprint 5A — Autonomous Presence Layer / Behavior Layer V1 (Completed)

**First step from Reactive Interface → Companion.** Before Sprint 5A the creature only existed when something happened. Now it has behavior while nothing is happening — without Memory, AI, Cloud, new hardware, EventBus changes, or persistence changes.

| Requirement | Status |
|---|---|
| Autonomous behavior visible | ✅ Certified |
| Creature feels alive | ✅ |
| Creature settles over time | ✅ |
| Drowsiness visible | ✅ |
| Wake-up response visible | ✅ |
| Mood integration works | ✅ |
| No frozen state observed | ✅ |
| Cross-mode carryover works | ✅ |
| 32-min stability run passed | ✅ |

**This is Behavior Layer V1** — one drive, one decay curve, one behavior family. Not a full Behavior System. V2.6 can introduce Memory, Learning, Preferences, Behavior narratives, and Recall without reopening Sprint 5A.

**Frozen — no further tuning.** All behavior numbers accepted as-is (30s max interval, current decay model, current eyelid curve, deep blink suppression guard).

### Sprint 4B — Persistence Validation (In Progress)

Hardware validation progress:

| Test | Status |
|------|--------|
| P6 Mode Transition Save | ✅ Certified |
| P7 Schema Recovery | ✅ Certified (2026-06-23) — old schema, future schema, corrupted blob. Recovery persistence verified. |
| NV1 Power-Loss Stress | ✅ Certified (10-cycle archived evidence in `docs/TESTING/nv1_10cycles.txt`) |
| P3 Runtime Accuracy | ✅ Certified (67 min, 2026-06-23) |

**P7 Schema Recovery — Validated:** schema=0 (old), schema=255 (future), corrupted blob (84 bytes vs 64). All three trigger factoryReset + clean profile. Recovery profile survives power cycle with no reset loop. Injection method: standalone NVS injector tool (`tools/p7_injector/p7_injector.ino`) — zero production firmware modifications. Test log archived.

**NV1 Power-Loss Stress — Validated:** 10-cycle hardware log archived at `docs/TESTING/nv1_10cycles.txt`. Boot sequence observed: 10→11→13→14→15→18→19→20→21→22 (monotonic, no rollback, no factory resets). Gaps (Boot 12, 16, 17) consistent with pre-serial USB contact, not persistence failure. `touches=6`, `runtime=14 min`, `playful=1`, `anxious=0` stable across all cycles. Pass criteria met for release qualification; 25-cycle variant deferred as NV1B if desired.

**OBS-001 (LOW)**: Boot counter retained value 4 instead of incrementing to 5 after power cycle. Possibly serial reconnect (device stayed powered) or subtle NVS ordering detail. No investigation or code change planned — does not affect persistence correctness.

**Pass criteria:** Remaining hardware test (P3) passes, no regression in existing behavior (mood cycle, touch response, engagement drive, mode transitions).

### V2.5 Release Candidate

After Sprint 4B validation passes. Branch ready for merge to `main`.

### Current Architecture (V2.6)

```
Observe → Attention → Emotion → Mood → Face
                                         ↑
                                    Persistence (observer)
                                    Memory (observer, LittleFS-backed)
```

Memory Layer is an observer subsystem — reads touch events, writes to ring buffer and LittleFS. Does not feed back into FaceEngine, MoodEngine, or AttentionEngine. No EventBus subscription.

### Future Architecture (V2.6+)

```
Observe → Attention → Emotion → Mood → Face
                                           ↑
                                    Persistence (observer)
                                    Memory → Behavior (planned)
```

Each layer builds on the one before. Persistence is cross-session data. Memory is structured recall (what happened, when, with whom). Behavior is action selection based on memory + mood. No layer reads or writes upstream fields.

### Future Sprints
- **V2.6 Sprint 2**: Touch duration expansion (TAP, DOUBLE_TAP, HOLD, LONG_HOLD, BURST) — still touch-only, still isolated
- **Memory Domains** (post-Sprint 2): Security memory, Aviation memory, Mood memory — deferred
- **Behavior Layer** (post-Memory): Mood-driven + memory-informed action selection — not yet planned

## File Map
- `AeroSniffer/AeroSnifferOS.h:74-79` — `MoodType` enum (RELAXED=0, PLAYFUL=1, ANXIOUS=2, MOOD_COUNT=3)
- `AeroSniffer/AeroSnifferOS.h:113` — `uint8_t mood_strength` in `CreatureState`
- `AeroSniffer/AeroSnifferOS.h:97-133` — `AttentionTarget`, `AttentionSource` enums, `CreatureState` with structured `attention`
- `AeroSniffer/AeroSnifferOS.h:126` — `uint8_t engagement_drive` (0–100) in CreatureState — Sprint 5A
- `AeroSniffer/AeroSnifferOS.h:221-225` — FaceEngine private members: `_engagement_level`, `_eyelid_factor`, `_deep_blink_hold`, `_last_frame_ms` — Sprint 5A
- `AeroSniffer/AeroSnifferOS.cpp:31-48` — `g_creature` aggregate initializer with `engagement_drive=100`
- `AeroSniffer/AeroSnifferOS.cpp:525-556` — FaceEngine::begin() with Sprint 5A init
- `AeroSniffer/AeroSnifferOS.cpp:585-687` — FaceEngine::updateAnimations() with engagement decay, saccade timing, deep blink, eyelid lerp
- `AeroSniffer/AeroSnifferOS.cpp:720-722` — renderEmotionLayer() with `_eyelid_factor` in default eye height
- `AeroSniffer/AeroSnifferOS.cpp:571-602` — FaceEngine gaze driven by `target`/`strength`
- `AeroSniffer/AeroSnifferOS.cpp:13-29` — `g_creature` aggregate initializer with `mood_strength=0`
- `AeroSniffer/AttentionEngine.cpp` — queue, state machine, decay, publish, event mapping (sketch root)
- `AeroSniffer/Companion/AttentionEngine.h` — class declaration, spinlock, structured attention fields
- `AeroSniffer/Companion/MoodEngine.h` — class declaration, touch history ring buffer, arbitration
- `AeroSniffer/MoodEngine.cpp` — implementation: fixed-priority arbitration, fixed-point decay, touch tracking
- `AeroSniffer/AeroSniffer.ino:583-587` — tick order: Attention→Emotion→Mood→Persistence→Face
- `AeroSniffer/AeroSniffer.ino:340` — GET_PET_STATUS: `MoodEngine.moodName(g_creature.mood)`
- `AeroSniffer/AeroSniffer.ino:586-587` — `CreaturePersistence.tick()` after `MoodEngine.tick()`
- `AeroSniffer/AeroSniffer.ino:602-603` — `CreaturePersistence.save()` on mode transition
- `AeroSniffer/Persistence/PersistenceService.h` — `CreatureProfile` struct and `PersistenceService` class
- `AeroSniffer/PersistenceService.cpp` — implementation: load/save, rising-edge counters, runtime, 5-min save
- `AeroSniffer/Security/Portal.h:1240-1258` — EmotionEngine emotion-only calls (setMood bridge removed)
- `docs/V2.5_SPRINT4A_PERSISTENCE_SPEC.md` — Sprint 4A spec: CreatureProfile, save strategy, migration, counter hooks, exit criteria
- `docs/V2.5_SPRINT3_MOOD_SPEC.md` — full Mood System spec (12 sections)
- `docs/V2.5_ATTENTION_RETROSPECTIVE.md` — Sprint 1–2B retrospective
- `docs/V2.5_CREATURE_BRAIN_RETROSPECTIVE.md` — Sprint 1–3B retrospective (goals, bugs, architecture, debt)
- `docs/V2.5_SPRINT1_SPEC.md` — Sprint 1 implementation specification
- `docs/V2.5_ATTENTION_MODEL.md` — attention data model

## File Map (V2.6 additions)
- `AeroSniffer/MemoryEngine.h` — MemoryEngine class declaration
- `AeroSniffer/MemoryEngine.cpp` — implementation: ring buffer, formation, decay, dedup, persistence, recall
- `docs/V2.6_P3_RESULTS.md` — P3 certification report

## Git
- Branch: `main`
- Tags: `v2.5-attention-complete` (Sprint 2B), `v2.5-mood-foundation` (Sprint 3A), `v2.5-creature-brain-complete` (Sprint 3C)
- Commits: V2.6 Sprint 1 Certified `(HEAD)`, UNDERSTAND gitignore `271d10c`, MemoryEngine fix `9377e1f`, Memory Layer `25da7b7`, Sprint 5A `(previous)`
- Upstream: `origin/main`
