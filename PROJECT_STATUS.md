# Project Status — AeroSniffer V2.7

## Phase: Memory & Behavior

### Completed

| Sprint | Description | Status |
|--------|-------------|--------|
| **Sprint 1** | Companion Intelligence Foundation | ✅ Committed, hardware-validated |
| **Sprint 2A** | FaceEngine Attention Migration | ✅ Committed, hardware-validated |
| **Sprint 2B** | Compatibility Shim Removal | ✅ Committed |
| **Sprint 3A** | Mood Infrastructure | ✅ Committed, hardware-validated |
| **Sprint 3B** | Mood Consumption | ✅ Committed, compile-validated |
| **Sprint 3C** | Stabilization & Retrospective | ✅ Retrospective created, tagged, debt documented |
| **Sprint 4A** | Creature Persistence | ✅ Implemented: PersistenceService, 5-min periodic save, rising-edge counters, schema migration |
| **Sprint 4B P6** | Mode Transition Save | ✅ Certified: Runtime, Touch, Mood, and Cross-Mode persistence survive power cycle |
| **Sprint 4B P7** | Schema Recovery | ✅ Certified: old schema, future schema, corrupted blob all recover cleanly; recovery profile survives power cycle |
| **Sprint 5A** | Autonomous Presence Layer | ✅ Certified: engagement_drive, mood-modulated decay, 3s→30s saccade range, deep blink suppression, cross-mode carryover, 32-min stable run |
| **V2.6 Sprint 1** | Memory Layer Foundation | ✅ Certified: memory formation, recall, decay, accumulator model, bounded ring buffer, LittleFS persistence. P3 passed (60-min hardware soak). |
| **V2.6 Sprint 2** | Memory Formation Expansion | ✅ Certified: PendingTouch ring buffer, duration-based TAP/HOLD/LONG_HOLD/DOUBLE/BURST classification. 30-min hardware soak. All 5 subtypes verified on hardware. |
| **V2.6 Sprint 3** | Memory Domain Expansion | ✅ Implemented: Security, Aviation, Mood memory domains (15 subtypes). Additive-only — no changes to certified subsystems. Hardware certification pending (30-min multi-domain soak). |
| **Release Engineering** | CI Pipeline + Evidence Framework | ✅ Certified: 5-run validation (cold/warm/failure/recovery). GitHub Actions pipeline with arduino-cli v2, pip caching, evidence pack generation, 26/26 tests passing. 95/100 certification score. |

### Blocked

None.

### Risks

| Risk | Severity | Mitigation |
|------|----------|------------|
| OpenSky HTTPS fetch failing (B007) | Low | Non-blocking for development. HTTP fallback or cert fix deferred. |
| lwIP assertion on HTTP fetch (B004) | Low | Weather fetch disabled. Fix via hostname resolution before HTTP. |
| `netstack` error on mode transition (B006) | Low | Non-blocking. Mode switch succeeds despite error. |

### Bugs

See [BUG_LOG.md](BUG_LOG.md) for full details.

### Tags

| Tag | Description |
|-----|-------------|
| `v2.5-attention-complete` | Sprint 2B — attention layer stable |
| `v2.5-mood-foundation` | Sprint 3A — mood infrastructure stable |
| `v2.5-creature-brain-complete` | Sprint 3C — full pipeline frozen, retrospective archived |
| `v2.5-persistence` | Sprint 4A — Creature Persistence |
| `v2.6-memory-expansion` | V2.6 Sprint 1 — Memory Layer Foundation |
| `v2.6-memory-expansion-certified` | V2.6 Sprint 2 — Memory Formation Expansion |
| `v2.6-releng` | V2.6 Release Engineering — CI pipeline + evidence framework |
| `v2.7-sprint1` | V2.7 Sprint 1 — BehaviorEngine V1 certified |
| `v2.7.2` | V2.7.2 — Behavioral Consolidation (engagement lifecycle ownership, build-verified, hardware soak pending) |
| `v2.7.3` | V2.7.3 — Calibration Release (B1/M8/M9 closed as No Change, calibration infrastructure, macro guard, drop counter, all artifacts) |

### Recent Commits

| Date | Commit | Description |
|------|--------|-------------|
| 2026-07-01 | `v2.7.3` | V2.7.3 — Calibration Release (B1/M8/M9 closed as No Change, calibration infrastructure, macro guard, drop counter, runtime validation, all artifacts) |
| 2026-07-01 | `2cb9c37` | docs: finalize calibration artifact structure and .gitignore exception |
| 2026-07-01 | `ca9bc0e` | Phase 1: add Calibration.h infrastructure to remaining engine headers |
| 2026-07-01 | `1791f7a` | fix: sync MoodEngine.h declarations with committed .cpp implementation |
| 2026-07-01 | `5a7551f` | Phase 0: binary size is traceability-only, not a gate; remove stale comparison reference |
| 2026-06-30 | `v2.7.2` | V2.7.2 — Behavioral Consolidation (engagement lifecycle, canonical ownership, build-verified, hardware soak pending) |
| 2026-06-29 | `v2.7-sprint1` | V2.7 Sprint 1 — BehaviorEngine V1 certified, hardware-validated |
| 2026-06-28 | `db34556` | docs: redesign README into production-grade GitHub landing page |
| 2026-06-28 | `9cf1869` | Release Engineering: CI pipeline, evidence framework, build caching, failure-path handling (merged) |
| 2026-06-25 | `3023b35` | V2.6 Sprint 3 — Memory Domain Expansion implemented |

### Branch

- Active: `main`
- Upstream: `origin/main`
- Tags: `v2.5-attention-complete`, `v2.5-mood-foundation`, `v2.5-creature-brain-complete`, `v2.5-persistence`, `v2.6-memory-expansion`, `v2.6-memory-expansion-certified`, `v2.6-releng`, `v2.7-sprint1`
- Merged: All V2.6 branches merged — `feature/v2.6-releng-validation` → `main` (`9cf1869`)

### Current Architecture

```
Observe → Attention → Emotion → Mood → Memory → Behavior → Face
```

BehaviorEngine is the canonical producer of `engagement_drive` and transforms MoodEngine's baseline `mood_strength` into the canonical value. FaceEngine is presentation-only — reads CreatureState, never writes behavioral fields.

### V2.7.2 — Behavioral Consolidation (Implementation Complete)

**Objective:** Make BehaviorEngine the canonical behavioral authority. Eliminate duplicate `MemoryEngine.recall()` calls. Strip behavioral logic from FaceEngine.

**Status:**
- ✅ Implementation complete
- ✅ Build verified (93% flash, 21% RAM)
- ✅ Architecture verified (ownership, pipeline order, no dual-writers)
- ⏳ Hardware certification pending (30–60 min soak required)

| Commit | Description |
|--------|-------------|
| Commit 1 | Move engagement lifecycle (decay, memory modulation, touch reset, write-back) from FaceEngine to BehaviorEngine. Old path preserved under `#if 0`. |
| Commit 2 | FaceEngine cleanup: remove dead engagement code, duplicate `MemoryEngine.recall()`, unused cache logic. FaceEngine becomes presentation-only. |
| Commit 3 | Documentation: Canonical State Ownership table, Stage Ownership Rule, architectural principle. |
| Commit 4 | Validation and tagging (`v2.7.2`). |

**Architecture after V2.7.2:**

```
Observe → Attention → Emotion → Mood → Memory → Behavior → Face
```

- **BehaviorEngine** is the canonical producer of `engagement_drive` (full lifecycle: decay → memory mod → reset → write-back → security floor)
- **MoodEngine** produces baseline `mood_strength`; **BehaviorEngine** transforms it into canonical value
- **FaceEngine** is presentation-only: eyelids, blink, gaze, bounce, pulse — all read-only consumers of `CreatureState`

**Architectural principle adopted:**

> Behavior engines transform state. Presentation engines interpret state. Rendering code must never define creature behavior.

### V2.7.3 — Calibration Release (Complete)

**Objective:** Freeze behavior, measure, calibrate, certify. No new features.

**Status:** ✅ **COMPLETE** — Tag `v2.7.3`

**Measure items — all closed as No Change:**

| ID | Constant | Value | Decision | Rationale |
|----|----------|-------|----------|-----------|
| B1 | `kEngagementDecayPerSec` | `0.333f` | No Change | 3-run soak: range/mean = 0.27%, decay deterministic. Adjusting would mask uint16_t saturation bug. |
| M8 | PLAYFUL decay interval | `36000` ms/unit | No Change | Evidence Sufficient: 3+ observed decays Happy→Relaxed, no oscillation, no stuck transitions. |
| M9 | ANXIOUS decay interval | `24000` ms/unit | No Change | Engineering Equivalence: identical decay code path as M8. Threat-generation testing would not reduce uncertainty. |

**Calibration infrastructure delivered:**
- ✅ Calibration.h — 4 macros (`CALIB`, `CALIB_STR`, `CALIB_RATE`, `CALIB_RATE_STR`), 2-stage glue, format constants, `availableForWrite()` guard, drop counter with auto-reset
- ✅ 15 stamp tags across 5 files (all 4 engines + main loop)
- ✅ Macro guard verified: USB CDC stall prevention without data loss — zero firmware hangs across all runs
- ✅ `tools/serial_capture.py` — millisecond timestamps, interactive filename prompt, append support
- ✅ Artifact structure: `raw/`, `processed/`, `reports/`, `runtime/`, `metadata.yaml`, `CALIBRATION_REGISTRY.md`, `metadata.template.yaml`
- ✅ Phase 0 (6 gates), Phase 1 (5 includes), Phase 2 (15 stamps) — all complete
- ✅ Phase 3 closure: B1 complete (3 runs), M8 complete (Evidence Sufficient), M9 complete (Engineering Equivalence)
- ✅ Runtime Validation: 47-min soak, macro guard verification, two M8 attempts (stable, no hangs)

**EventBus Capacity — Known Limitation (unchanged)**

The EventBus subscriber array (32 slots) is exceeded at boot when all engine subscriptions are registered. The attention subsystem does not receive six non-security events (TOUCH_SHORT, FLIGHT_DETECTED, FLIGHT_RARE, WIFI_CONNECTING, WIFI_CONNECTED, WIFI_DISCONNECTED). This causes partial degradation of attention-driven companion behaviors (visual orientation toward touch, flight, and WiFi events) while leaving emotional processing, memory formation, and security response fully functional.

The overflow is a longstanding capacity limitation, not a V2.7.3 regression:

| Version | Subscribers | Capacity | Result |
|---------|------------:|--------:|--------|
| V2.4 | 30 | 24 | Overflow |
| V2.5 | 30 | 32 | Healthy |
| V2.6 Memory Expansion | 38 | 32 | Overflow |
| V2.7.x | 38 | 32 | Overflow + warning |

The initial EventBus capacity was undersized for the number of registered subscribers in V2.4. V2.5 temporarily resolved it by expanding to 32 slots. V2.6 Memory Expansion added subscriptions without revisiting capacity, re-introducing the overflow. V2.7.2 added the visible warning. V2.7.3 intentionally leaves this unchanged — altering EventBus capacity would invalidate the behavioral baseline being calibrated.

**Backlog (post-V2.7.x):**
- EventBus Capacity Audit — Determine required subscriber capacity from actual registrations. Deliverables: subscriber inventory, headroom target, memory cost, scalability recommendation.

### V2.8 — Next Capability (Planned)

**Objective:** New feature development. Calibration freeze lifted — all infrastructure remains in place for future campaigns.

**Candidate areas** (not committed — scope TBD in Sprint 0):
- Behavior Layer V2 (memory-informed action selection)
- Serial Bridge Service (TCP + WebSocket COM port sharing)
- EventBus capacity expansion
- `ms_since_last_touch` uint16_t saturation fix (BehaviorEngine.cpp:52)
- MoodEngine PLAYFUL entry tuning (UX polish)
- Any other feature the project owner finds exciting

**Status:** 📋 Planning — no commits yet. Branch from `v2.7.3` tag when ready.

### Previous Sprints

engagement_drive (0–100) in CreatureState, FaceEngine sole writer. Mood-modulated decay: RELAXED 1.0x (~5 min to drowsy), PLAYFUL 0.5x (~10 min), ANXIOUS 0.3x with floor at 20 (~16 min). Saccade interval 3s→30s, deep blink suppressed during TARGET_THREAT/USER/FLIGHT. 32-min certification run: 1816 stable heartbeats, 16 touches, all 3 mode transitions clean, no errors. Cross-mode carryover observed (sleepy state persists across Aviation→Companion).

**V2.5 Sprint 4B — Persistence Validation**

Completed exit criteria:
- [x] P7 — Schema Recovery (old schema, future schema, corrupted blob — all recover cleanly; recovery persists across power cycle ✅ 2026-06-23)
- [x] NV1 — Power-Loss Stress (10-cycle archived evidence — monotonic boot counter, no resets, no data loss)

Remaining exit criteria:
- [x] P3 — Runtime Accuracy (60-min soak) ✅ 2026-06-23
- [ ] Factory reset clears profile
- [ ] No noticeable boot delay (<100ms)
- [ ] No excessive NVS writes (≤1 per 5 min + mode transitions)
- [ ] No EventBus subscription (grep confirm)
- [ ] No spinlocks (grep confirm)
- [ ] No write to upstream CreatureState fields (grep confirm)

### Observations

| ID | Description | Priority | Status |
|----|-------------|----------|--------|
| OBS-001 | Boot counter showed 4 instead of 5 after power cycle. Possibly serial reconnection without true power-off, or NVS write ordering edge case. No persistence data loss. Do not investigate further. | LOW | Open — no action planned |

### V2.6+ Architecture

Current pipeline (certified):

```
Observe → Attention → Emotion → Mood → Persistence → Memory → Behavior → Face
             ↑                          ↑           ↑          ↑
         EventBus                   Observer     Observer   Sole canonical
                                                             producer of
                                                             engagement_drive
```

BehaviorEngine is the canonical producer of `engagement_drive` (decay → memory modulation → touch reset → write-back → security floor override). It transforms MoodEngine's baseline `mood_strength` into the final canonical value. FaceEngine is a pure presentation engine — no behavioral decisions, no memory queries, no CreatureState mutations.

Explicitly deferred from V2.7:
- Learned preferences, memory recall, relationship modeling, personality evolution — all depend on a richer behavior layer in future sprints.
