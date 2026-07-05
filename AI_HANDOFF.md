# AI Handoff — AeroSniffer V2.8

## Current State

**V2.8 Sprint 1 — Presentation Intelligence is CERTIFIED.** Tag: `v2.8-sprint1`.

**V2.8 Sprint 2 — Animation Intelligence is CERTIFIED.** Tag: `v2.8-sprint2`.

PresentationProfile/PresentationState structs, 30 profile variants across 10 emotions (PROGMEM), event-driven integer Q8 scoring, deterministic xorshift32 PRNG, transition interpolation (geometry snap, color lerp), animation profile modulation (blink/gaze/bounce/speed), gaze range modulation, mood affinity scoring, engagement band threshold crossing, context bucket mapping (CALM/ACTIVE/FOCUSED/SOCIAL). Hardware-validated — all emotion variants fire correctly, transitions smooth, zero crashes. Test log: `docs/v2.8 tests/sprint1_v2.8.txt`.

**V2.7.3 — Calibration Release is COMPLETE.** Tag: `v2.7.3`.

Freeze → Measure → Calibrate → Release. Three Measure items all closed as No Change:
- **B1** (kEngagementDecayPerSec = 0.333f): 3-run soak, range/mean = 0.27%, deterministic decay.
- **M8** (PLAYFUL decay 36 s/unit): Evidence Sufficient — 3+ observed decays, no oscillation, no stuck transitions.
- **M9** (ANXIOUS decay 24 s/unit): Engineering Equivalence — identical decay code path.

Calibration infrastructure delivered: `Calibration.h` with 4 macros, `availableForWrite()` guard, auto-resetting drop counter, `tools/serial_capture.py` with millisecond timestamps. Full artifact structure at `docs/CALIBRATION/`. Macro guard verified via smoke test: 55 drops reported, zero firmware hangs, all stamps flowing. USB CDC stall root cause identified (80–85% confidence: spin loop in `USBCDC::write()`).

No behavioral constants were changed. All 65 constants retain their V2.7.2 values. The calibration freeze is lifted for V2.8 development.

**V2.7.2 — Behavioral Consolidation is COMPLETE.** BehaviorEngine is now sole canonical producer of `engagement_drive`. Full lifecycle (decay, mood multiplier, memory modulation, touch/attention reset, security floor override) migrated from FaceEngine. FaceEngine is presentation-only — eyelids, blink, gaze, bounce, pulse — all read-only consumers of `CreatureState`. Behavior-preserving ownership migration: zero functional changes. Tags: `v2.7-sprint1`, `v2.7.2`.

**V2.7 Sprint 1 — BehaviorEngine V1 is CERTIFIED.** Behavioral Layer reads MemoryEngine recall summaries and influences CreatureState through 3 deterministic V1 transforms. 125 lines across 2 new files (Companion/BehaviorEngine.h, BehaviorEngine.cpp). Additive `delta += N` evaluation, persistent `_mood_modifier` with linear -1/tick decay, edge-triggered debug. O(1), allocation-free. Certified on hardware via 60-min soak across all 3 modes (Companion, Security, Aviation). Zero WDT, zero panics, zero asserts, zero heap growth. Tag: `v2.7-sprint1`.

**Release Engineering — CI Pipeline and Evidence Framework is CERTIFIED.**
GitHub Actions pipeline validated across 5 runs (cold/warm/failure/recovery). `releng.py ci-build` is the single CI entry point. Evidence pack generation with 5 providers. 26/26 tests passing. 95/100 certification score.

**V2.6 Sprint 3 — Memory Domain Expansion is IMPLEMENTED.** MemoryEngine now supports 4 domains (TOUCH, SECURITY, AVIATION, MOOD) with 15 subtypes total. All additive. NOT CERTIFIED — hardware validation pending (30-min multi-domain soak).

**V2.6 Sprint 2 — Memory Formation Expansion is CERTIFIED.** Memory now distinguishes 5 touch subtypes (TAP, HOLD, LONG_HOLD, DOUBLE, BURST). All 5 subtypes verified on hardware.

**V2.6 Sprint 1 — Memory Layer Foundation is CERTIFIED and FROZEN.** Ring buffer (64 records), LittleFS persistence, accumulator decay model — validated via 60-min P3 soak.

V2.5 Sprint 1–3C complete. Sprint 4A (Creature Persistence) certified. **Sprint 5A (Autonomous Presence Layer / Behavior Layer V1) certified** — engagement_drive (0–100), mood-modulated decay, 3s→30s saccade, deep blink suppression, cross-mode carryover. No further tuning planned. A [Creature Brain Retrospective](docs/V2.5_CREATURE_BRAIN_RETROSPECTIVE.md) archives goals, bugs, architecture decisions (12 preserved), technical debt (3 items), and lessons learned.

## V2.8 Sprint 1 — Presentation Intelligence (Certified)

### Status
- **CERTIFIED** (hardware-validated — all emotion variants fire correctly, transitions smooth, zero crashes)
- **FROZEN** — no further edits except bug fixes

### Features
- Presentation Intelligence architecture doc (`docs/V2.8_PRESENTATION_INTELLIGENCE.md`) created
- `PresentationProfile`/`PresentationState` structs implemented
- 30 profile variants across 10 emotions in PROGMEM
- Event-driven selection with integer Q8 scoring
- Deterministic xorshift32 PRNG
- Transition interpolation (geometry snap, color lerp)
- Animation profile modulation (blink, gaze, bounce, speed)
- Gaze range modulation
- Mood affinity scoring
- Engagement band threshold crossing
- Context bucket mapping (CALM/ACTIVE/FOCUSED/SOCIAL)
- Hardware validation PASS — all emotion variants fire correctly, transitions smooth, zero crashes

### Test Log
`docs/v2.8 tests/sprint1_v2.8.txt`

## V2.8 Sprint 2 — Animation Intelligence (Certified)

### Status
- **CERTIFIED** (build verification, runtime and animation validation via hardware observation)
- **FROZEN** — no further edits except bug fixes

### Objective
Create a dedicated physical and biological animation intelligence layer within FaceEngine. This increases baseline expressiveness and kinetic high-fidelity before introducing complex micro-expressions or external desktop connectivity.

### Delivered
- **Physical Saccades:** Spring-damper physics (Target → Velocity → Ease → Settle) and continuous, organic micro-drift (tremor). Wall-clock dt clamped at 3.0x max.
- **Biological Breathing:** 4-phase waveform (inhale → hold → exhale → rest) with 6-second period, modulating eye vertical scale and position.
- **Focus Lock:** Hysteresis-based (engage ≥25, release <20) suspension of random saccades, locking gaze to attention source coordinate.
- **Emotional Recovery:** Slow 1.5s brow / 2s eyelid decay when reverting from ALERT/ANGRY to CALM.
- **Transition Polish:** 4-phase wall-clock timer-driven state machine (IDLE → Anticipate 50ms → Transition 150ms → Settle 100ms) with squeeze, overshoot, and elastic bounce.
- **PresentationTarget (Additive):** Stage 1 output contract emitted alongside existing PresentationState — Sprint 1 untouched.
- **Integer-Only AnimationPose:** All float→int resolution in Stage 2; Stage 3 receives only integer coordinates.
- **ANIM_DEBUG Overlay:** Build-time toggle showing emotion + touch state on display.
- **Backward Compatibility:** All existing render functions (renderActivityLayer, renderEffectLayer) work unchanged via backward-compat fields.

### Key Architecture Decisions
- **PresentationTarget is additive** — `updatePresentation()` emits `_pres_target` alongside `_pres_state`. Zero changes to Sprint 1 PresentationState.
- **Wall-clock dt** — spring-damper uses actual elapsed wall-clock time, not frame count. Decouples physics from framerate variance.
- **Focus-lock hysteresis** — 5-point band prevents lock/unlock oscillation.
- **TransitionPhase & transition_progress coexist** — TransitionPhase governs physical effects via wall-clock timer; transition_progress (0–255) governs Sprint 1 profile blending.
- **Integer-only Stage 3** — `produceAnimationPose()` converts all floats to ints. `drawEye()` and `drawBrows()` receive only integer values.
- **No heap allocation** — all state statically declared in FaceEngineClass.
- **No behavioral changes** — zero modifications to BehaviorEngine, MoodEngine, MemoryEngine, AttentionEngine, CreatureState.

### Build Metrics
- Flash: +1,308 bytes (1,256,705 total, 95%)
- RAM: +8 bytes (71,900 total, 21%)
- Compile: 0 errors, 0 warnings

### Validation Artifacts
- `docs/v2.8 tests/sprint2_v2.8.txt` — serial capture log
- `docs/TESTING/V2.8_SPRINT2_RESULTS.md` — certification report
- Animation behaviors validated on physical hardware through visual observation on DeskBuddy 2.0 (spring-damper convergence, breathing cycle, TransitionPhase sequencing, focus-lock hysteresis, recovery curves)

## V2.8 Future Sprints

### V2.8 Sprint 3 — Micro-Expressions (Planned)
**Objective:** Add high-frequency visual overlays to represent fast micro-states:
- Double blinks, one-eye blinks, squints, pupil flicks, eyebrow twitches.
- These fire as transient overlays on top of the stable animation foundation, triggered by sudden external events or engagement shifts.
- Depends on: V2.8 Sprint 2 (Animation Intelligence) — available and certified.

### V2.8 Sprint 4 — Desktop Observer (Planned)
**Objective:** Connect desktop observations (typing activity, code focus, media streams via `aerosniffer.vbat` / PC agent) to feed observations directly into behavioral state, which drives the mature animation system.
- Depends on: V2.8 Sprint 2 (Animation Intelligence) — available and certified.

---

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
- Raw log: `docs/TESTING/v2.6_p3.txt` (archived)

## V2.6 Sprint 2 — Memory Formation Expansion (Certified)

### Status
- **CERTIFIED** (30-min hardware soak completed 2026-06-24)
- **FROZEN** — no further edits except bug fixes

### Features
- PendingTouch ring buffer (`pending_[8]`) — replaces monolithic `touch_events_` counter
- Duration-based classification: TAP (≤300ms), HOLD (301–800ms), LONG_HOLD (801–1499ms)
- Double-tap promotion (gap 50–500ms → TOUCH_DOUBLE)
- Burst detection (5+ in 10s window → additive TOUCH_BURST)
- Salience by subtype: TAP 22, HOLD constrain(d/20,22,35), LONG_HOLD constrain(d/30,30,45), DOUBLE 30, BURST 45
- `touch_duration_ms` stores real duration in every record
- Overflow guard: `dropped_touch_events_` counter in MemorySummary

### Changes
- `MemoryEngine.h`: Added `struct PendingTouch`, ring buffer fields (`pending_[8]`, `pending_head_`, `pending_tail_`, `dropped_touch_events_`). Removed `touch_events_`.
- `MemoryEngine.cpp`: `onTouchEvent()` pushes to ring buffer. `observe()` drains, classifies, deducts salience baseline per subtype, promotes DOUBLE/BURST. Runtime trace: `[MEM] formed TOUCH xxx salience=N strength=100 duration=NNN`. Added `#include "Config.h"`.
- `MemoryTypes.h`: `dropped_touch_events` field in `MemorySummary`.

### Validation
- 30-min hardware soak on DeskBuddy 2.0 (3241 line log)
- All 5 subtypes formed: TAP (88/176/264ms), HOLD (352/440/528/704ms), LONG_HOLD (1232/1496ms), DOUBLE (gap-based), BURST (5+ in 10s)
- Dedup verified for all 5 subtypes — each boosts +10 within 60s window
- Mode transitions clean: Pet→Security→Aviation→Pet
- No WDT, no panics, no heap corruption
- Full report: `docs/V2.6_SPRINT2_RESULTS.md`
- Raw log: `docs/TESTING/30MINSOAKTEST.txt` (archived)

### Next Phase
- **V2.6 Sprint 3**: Security/Aviation/Mood memory domains — expand beyond touch-only. Each domain: minimum subtype set, appropriate salience baselines, domain-specific dedup windows, cross-domain salience comparison for behavior-layer entry.

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

### V2.7 Sprint 0 — Repository Cleanup

Sprint 0 (completed) tasks:

| Task | Description | Status |
|------|-------------|--------|
| S0-T1 | Update PROJECT_STATUS.md — header, phase, tags, branch, recent commits, Sprint 4 | ✅ Complete |
| S0-T2 | Update BUG_LOG.md — header, reclassify B007/B008, add P5 | ✅ Complete |
| S0-T3 | Update AI_HANDOFF.md — remove stale refs, reconcile issues, add V2.7 section | ✅ Complete |
| S0-T4 | Rewrite ARCHITECTURE.md — actual repo structure, V2.6 subsystems, V2.7 plan | ✅ Complete |
| S0-T5 | Archive obsolete planning docs to `docs/archive/` | ✅ Complete |
| S0-T6 | Remove legacy duplicates (RobotPet_PC_Integration, build/, *.log) | ✅ Complete |
| S0-T7a | Repository governance verification — tags, branches, version, CI workflow config | ✅ Complete |
| S0-T7b | Firmware build validation post-Sprint-0 changes | 🔄 Deferred (no `arduino-cli` available locally; CI will verify post-merge) |
| S0-T8 | Fix TD-011/012/013 technical debt | ✅ Complete |
| S0-T9 | Reconcile bug tracking across BUG_LOG.md, AI_HANDOFF.md, PROJECT_STATUS.md | ✅ Complete |

### V2.7 Sprint 1 — BehaviorEngine V1 (Certified)

**Status: CERTIFIED** (60-min hardware soak across all 3 modes — Companion, Security, Aviation)

**Implementation:** 125 lines across 2 new files:
- `Companion/BehaviorEngine.h` (47 lines) — class declaration, `MemorySummary` include, debug sentinel members
- `BehaviorEngine.cpp` (78 lines) — 3 V1 transforms, additive `delta += N` pattern, persistent `_mood_modifier` with linear `-1/tick` decay, edge-triggered debug logging
- `AeroSniffer.ino` — +5 lines (include, `begin()`, `tick()` at pipeline position 6)

**V1 Rules (certified):**
- Security domain strength > 30 && > mood domain strength → `engagement_drive = max(engagement_drive, 60)` (security floor)
- Touch domain > 25 → `mood_strength` accumulates +5/tick (capped ±15 via `_mood_modifier`), decays -1/tick toward zero when touch absent
- No significant memory activity > 120s → no override; natural decay continues

**Edge-triggered debug:** `Serial.printf()` fires only on state change via sentinel init (-128, 0xFF). Debug output format: `[BEH] MF=0 DS=[0 0 0 0] s=11 md=+0` — MF=mood floor, DS=domain_strength array (4 domains), s=total strength sum, md=mood modifier.

**Pipeline position:** 6 — after MemoryEngine (5), before FaceEngine.render() → updateAnimations() (7).

**Architecture:** Determined by ADR-0004 (7 invariants). Additive pattern ensures future rules compose without restructuring. Single `constrain()` at end of tick. No dynamic allocation — confirmed by grep.

**Excluded from V1:** Learned preferences, personality evolution, predictive modeling.

### Uncommitted Changes
- V2.7 Sprint 1 documentation sync — this file, PROJECT_STATUS.md, CHANGELOG.md
- Sprint 0 documentation edits (this file + PROJECT_STATUS.md + BUG_LOG.md + ARCHITECTURE.md)
- Sprint 3 implementation committed (`3023b35`)

### Sprint 4A — Creature Persistence (Complete)
PersistenceService implemented: `CreatureProfile` struct, `load()/save()/factoryReset()`, rising-edge touch/mood counter hooks (no EventBus, no spinlocks, no upstream writes), 5-min periodic save + mode transition checkpoint, Preferences blob storage with schema version validation. Three critical bugs from spec review fixed (Preferences handle leak, missing schema key, unchecked load failure).

### Sprint 4B P6 — Mode Transition Save (Certified)
P6 Runtime, Touch, Mood, and Cross-Mode persistence verified on hardware:
- **Runtime**: total_runtime_minutes=12 after ~11 min session + unplug
- **Touch**: total_touches=14 (1 baseline + 13 from Companion taps)
- **Mood**: playful_entries=1 (RELAXED→PLAYFUL during Security mode)
- **Cross-Mode**: All 3 transitions (Companion→Security→Aviation→Companion) clean; profile survived unplug

**OBS-001 (LOW)**: Boot counter showed 4 not 5 after power cycle. Possibly serial reconnect or NVS ordering edge case. No investigation planned.

## Known Issues

**Single source of truth:** [BUG_LOG.md](BUG_LOG.md) contains all bugs, severity, status, and root cause analysis.

### P1 — Fix Aviation HTTPS fetch (B007)
- **Severity**: Low — non-blocking for development
- **Reference**: BUG_LOG.md — B007

### P2 — Re-enable weather fetch with lwIP fix (B004)
- **Severity**: Low — weather fetch disabled, system runs clean
- **Reference**: BUG_LOG.md — B004

### P3 — Investigate `netstack` error (B006)
- **Severity**: Low — mode switch succeeds despite error
- **Reference**: BUG_LOG.md — B006

### P4 — Touch degradation after WiFi (B008)
- **Severity**: Low — workaround exists
- **Reference**: BUG_LOG.md — B008

### P5 — Suppress same-state decay logs (cosmetic)
- **Severity**: Cosmetic — no functional impact
- **Reference**: BUG_LOG.md — P5

### Technical Debt (Resolved in Sprint 0)
- **TD-011** ✅ `_last_mood_change` split into `_last_mood_transition` + `_last_decay_tick` in MoodEngine.h/.cpp
- **TD-012** ✅ Runtime overflow warning added to `EventBusClass::subscribe()` — `static_assert` infeasible (sub_count is runtime), serial warning provides developer visibility
- **TD-013** ✅ `_playful_condition_met_since = 0` added to all mood transitions in MoodEngine.cpp

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
| NV1 Power-Loss Stress | ✅ Certified (10-cycle archived evidence in `docs/TESTING/nv1_10cycles.txt` — file archived) |
| P3 Runtime Accuracy | ✅ Certified (67 min, 2026-06-23) |

**P7 Schema Recovery — Validated:** schema=0 (old), schema=255 (future), corrupted blob (84 bytes vs 64). All three trigger factoryReset + clean profile. Recovery profile survives power cycle with no reset loop. Injection method: standalone NVS injector tool (`tools/p7_injector/p7_injector.ino`) — zero production firmware modifications. Test log archived.

**NV1 Power-Loss Stress — Validated:** 10-cycle hardware log archived at `docs/TESTING/nv1_10cycles.txt` (file no longer in working tree — preserved in git history). Boot sequence observed: 10→11→13→14→15→18→19→20→21→22 (monotonic, no rollback, no factory resets). Gaps (Boot 12, 16, 17) consistent with pre-serial USB contact, not persistence failure. `touches=6`, `runtime=14 min`, `playful=1`, `anxious=0` stable across all cycles. Pass criteria met for release qualification; 25-cycle variant deferred as NV1B if desired.

**OBS-001 (LOW)**: Boot counter retained value 4 instead of incrementing to 5 after power cycle. Possibly serial reconnect (device stayed powered) or subtle NVS ordering detail. No investigation or code change planned — does not affect persistence correctness.

**Pass criteria:** Remaining hardware test (P3) passes, no regression in existing behavior (mood cycle, touch response, engagement drive, mode transitions).

### Release Engineering Merge

Release Engineering CI pipeline, evidence framework, and signature matcher certified. Branch `feature/v2.6-releng-validation` **merged to `main`** (`9cf1869`). Future development benefits from automated build + evidence on every push to `main` and every PR.

### Current Architecture (V2.7.2)

```
Observe → Attention → Emotion → Mood → Persistence → Memory → Behavior → Face
             ↑                          ↑           ↑          ↑
         EventBus                   Observer     Observer   Sole canonical
                                                             producer of
                                                             engagement_drive
```

Memory Layer is an observer subsystem — reads touch events and domain events, writes to ring buffer and LittleFS. BehaviorEngine is the sole canonical producer of `engagement_drive` (full lifecycle: decay → memory mod → touch reset → write-back → security floor override). It transforms MoodEngine's baseline `mood_strength` into the final canonical value. FaceEngine is a pure presentation engine — no behavioral decisions, no memory queries, no CreatureState mutations.

### Future Sprints
- **V2.6 Sprint 3** (complete, not certified): Security/Aviation/Mood memory domains. **Hardware certification pending** — 30-min multi-domain soak.
- **Release Engineering** (complete, certified): CI pipeline, evidence framework, signature matcher. Merged to `main` (`9cf1869`).
- **V2.7 Sprint 1** (complete, certified): BehaviorEngine V1 — 3 transforms, additive, O(1), 125 lines. Tag: `v2.7-sprint1`.
- **V2.7.2** (complete): Behavioral Consolidation — engagement lifecycle migrated to BehaviorEngine, FaceEngine stripped of behavioral logic, canonical ownership documented. Tag: `v2.7.2`.
- **V2.7.3** (complete): Calibration — measure real `domain_strength` distributions, tune thresholds using observed data instead of estimates, freeze constants after hardware validation.
- **V2.8 Sprint 1** (complete, certified): Presentation Intelligence — PresentationProfile/PresentationState, 30 profile variants, Q8 scoring, xorshift32 PRNG, transition interpolation, animation/gaze modulation, mood affinity scoring, engagement band thresholds, context bucket mapping. Hardware-validated.
- **V2.8 Sprint 2** (complete, certified): Animation Intelligence — physical model eye movement (spring-damper, micro-drift), biological breathing cycle, focus locking, emotional recovery (ALERT/ANGRY decay to CALM), transition phase subdivision (IDLE/Anticipate/Transition/Settle). Tag: `v2.8-sprint2`.
- **V2.8 Sprint 3** (planned): Micro-Expressions — Double blinks, squints, pupil flicks, eyebrow twitches as event feedback overlays.
- **V2.8 Sprint 4** (planned): Desktop Observer — `aerosniffer.vbat` / PC agent observer integration to inject observations into behavioral layers.

## File Map
- `AeroSniffer/AeroSnifferOS.h:74-79` — `MoodType` enum (RELAXED=0, PLAYFUL=1, ANXIOUS=2, MOOD_COUNT=3)
- `AeroSniffer/AeroSnifferOS.h:113` — `uint8_t mood_strength` in `CreatureState`
- `AeroSniffer/AeroSnifferOS.h:97-133` — `AttentionTarget`, `AttentionSource` enums, `CreatureState` with structured `attention`
- `AeroSniffer/AeroSnifferOS.h:126` — `uint8_t engagement_drive` (0–100) in CreatureState — Sprint 5A, now owned by BehaviorEngine
- `AeroSniffer/AeroSnifferOS.h:226-227` — FaceEngine private members: `_eyelid_factor`, `_deep_blink_hold` — presentation only
- `AeroSniffer/AeroSnifferOS.cpp:31-48` — `g_creature` aggregate initializer with `engagement_drive=100`
- `AeroSniffer/AeroSnifferOS.cpp:530-538` — FaceEngine::begin() with presentation init only
- `AeroSniffer/AeroSnifferOS.cpp:575-705` — FaceEngine::updateAnimations() with eyelid, blink, gaze, bounce, pulse (presentation only — no engagement decay)
- `AeroSniffer/AeroSnifferOS.cpp:705-707` — renderEmotionLayer() with `_eyelid_factor` in default eye height
- `AeroSniffer/AeroSnifferOS.cpp:575-605` — FaceEngine gaze driven by `target`/`strength`
- `AeroSniffer/AeroSnifferOS.cpp:13-29` — `g_creature` aggregate initializer with `mood_strength=0`
- `AeroSniffer/AttentionEngine.cpp` — queue, state machine, decay, publish, event mapping (sketch root)
- `AeroSniffer/Companion/AttentionEngine.h` — class declaration, spinlock, structured attention fields
- `AeroSniffer/Companion/MoodEngine.h` — class declaration, touch history ring buffer, arbitration
- `AeroSniffer/MoodEngine.cpp` — implementation: fixed-priority arbitration, fixed-point decay, touch tracking
- `AeroSniffer/AeroSniffer.ino:623-627` — tick order: Attention→Emotion→Mood→Persistence→Memory→Behavior→Face
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
- `AeroSniffer/Companion/BehaviorEngine.h` — BehaviorEngine class declaration, MemorySummary include, debug sentinel members (`_prev_mood_floor`, `_prev_modifier`), engagement lifecycle members (`_engagement_level`, `_last_tick_ms`)
- `AeroSniffer/BehaviorEngine.cpp` — implementation: engagement drive lifecycle (decay, memory modulation, touch reset, canonical write-back) + 3 V1 transforms, additive evaluation, _mood_modifier persistence with linear decay, edge-triggered debug, `BehaviorEngineClass BehaviorEngine;` extern
- `AeroSniffer/AeroSniffer.ino:29` — `#include "BehaviorEngine.h"` (Companion/ path via IDE)
- `AeroSniffer/AeroSniffer.ino:716` — `BehaviorEngine.begin()` in setup
- `AeroSniffer/AeroSniffer.ino:625` — `BehaviorEngine.tick()` at pipeline position 6
- `docs/TESTING/v2.7 sprint1.txt` — 169KB hardware soak log, 60-min run across all 3 modes

## Release Engineering Subsystem

### CI Pipeline
- `.github/workflows/firmware-build.yml` — 9-step GitHub Actions workflow (77 lines)
- `tools/releng.py` — single CI entry point (`ci-build`), 454 lines
- `tools/evidence.py` — evidence pack generator, schema v1, 165 lines

### Evidence Providers
- `tools/providers/__init__.py` — provider framework (ABC, auto-discovery, isolation)
- `tools/providers/arduino.py` — Arduino core, libs, boards.txt, variants, SDK config
- `tools/providers/compiler.py` — compiler defines, flags, version
- `tools/providers/git.py` — git describe, diff, status, log
- `tools/providers/linker.py` — nm symbols, sections, map, largest symbols
- `tools/providers/tft.py` — TFT backend, DMA context

### Signature Matcher
- `tools/signatures/matcher.py` — lifecycle-aware match engine
- `tools/signatures/*.yaml` — 3 active + 3 lifecycle-example signatures

### Tests
- `tools/tests/test_signatures.py` — 11 signature tests
- `tools/tests/ci/` — 15 CI pipeline tests (synthetic evidence packs)

### Dependencies
- `tools/requirements.txt` — `pyyaml>=6.0` (only non-stdlib dep)
- `tools/deps.toml` — Arduino board, core, library version pins

### Validation
- `docs/TESTING/RELEASE_ENGINEERING_CERTIFICATION.md` — full 5-run certification report
- `docs/TESTING/PRE_MERGE_RELEASE_ENGINEERING_CHECKLIST.md` — pre-merge audit

## File Map (V2.6 additions)
- `AeroSniffer/Memory/MemoryEngine.h` — MemoryEngine class declaration (Sprint 3: domain methods + state)
- `AeroSniffer/Memory/MemoryTypes.h` — all domain enums, MemoryRecord with source_id + expanded union, MemorySummary
- `AeroSniffer/MemoryEngine.cpp` — implementation: ring buffer, formation, decay, dedup, persistence, recall (sketch root)
- `AeroSniffer/Config.h` — `TAP_MAX_MS=300`, `HOLD_MAX_MS=800`
- `docs/V2.6_P3_RESULTS.md` — Sprint 1 P3 certification report
- `docs/V2.6_SPRINT2_RESULTS.md` — Sprint 2 certification report (30-min hardware soak)
- `docs/V2.6_SPRINT3_SPEC.md` — Sprint 3 Memory Domain Expansion specification (439 lines)
- `docs/V2.6_MEMORY_DOMAINS.md` — master domain taxonomy (260 lines)
- `docs/V2.6_SECURITY_MEMORY.md` — security domain design (179 lines)
- `docs/V2.6_AVIATION_MEMORY.md` — aviation domain design (182 lines)
- `docs/V2.6_MOOD_MEMORY.md` — mood domain design (189 lines)

## Git
- Branch: `main`
- Tags: `v2.5-attention-complete`, `v2.5-mood-foundation`, `v2.5-creature-brain-complete`, `v2.5-persistence`, `v2.6-memory-expansion`, `v2.6-memory-expansion-certified`, `v2.6-releng`, `v2.7-sprint1`, `v2.7.2`, `v2.7.3`, `v2.8-sprint1`, `v2.8-sprint2`
- Commits: `v2.8-sprint2` (HEAD — Animation Intelligence certified), `v2.7.3` (Calibration Release), `2cb9c37` (artifact structure), `v2.7.2` (Behavioral Consolidation)
- Upstream: `origin/main`
