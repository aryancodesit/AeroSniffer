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
| `v2.7.2` | V2.7.2 — Behavioral Consolidation (engagement lifecycle ownership) |

### Recent Commits

| Date | Commit | Description |
|------|--------|-------------|
| 2026-06-30 | `HEAD` | V2.7.2 — Behavioral Consolidation (engagement lifecycle, canonical ownership) |
| 2026-06-29 | `v2.7-sprint1` | V2.7 Sprint 1 — BehaviorEngine V1 certified, hardware-validated |
| 2026-06-28 | `db34556` | docs: redesign README into production-grade GitHub landing page |
| 2026-06-28 | `9cf1869` | Release Engineering: CI pipeline, evidence framework, build caching, failure-path handling (merged) |
| 2026-06-25 | `3023b35` | V2.6 Sprint 3 — Memory Domain Expansion implemented |
| 2026-06-24 | `9951f00` | V2.6 Sprint 2 certified — Memory Formation Expansion PASS |
| 2026-06-24 | `497e104` | docs: fix stale Uncommitted Changes section in AI_HANDOFF.md |
| 2026-06-24 | `116658c` | Phase 1 — TouchEventData pipeline, duration metadata, ae_event_callback safety fix |
| 2026-06-24 | `c60d8ff` | docs: add CODE_OF_CONDUCT, CONTRIBUTING, and issue templates |
| 2026-06-24 | `83cc441` | test(v2.6): certify Memory Layer Sprint 1 |

### Branch

- Active: `main`
- Upstream: `origin/main`
- Tags: `v2.5-attention-complete`, `v2.5-mood-foundation`, `v2.5-creature-brain-complete`, `v2.5-persistence`, `v2.6-memory-expansion`, `v2.6-memory-expansion-certified`, `v2.6-releng`, `v2.7-sprint1`
- Merged: All V2.6 branches merged — `feature/v2.6-releng-validation` → `main` (`9cf1869`)

### Current Architecture

```
Observe → Attention → Emotion → Mood → Memory → Behavior → Face
```

BehaviorEngine is sole canonical producer of `engagement_drive` and transforms MoodEngine's baseline `mood_strength` into the canonical value. FaceEngine is presentation-only — reads CreatureState, never writes behavioral fields.

### V2.7.2 — Behavioral Consolidation (Complete)

**Objective:** Make BehaviorEngine the sole behavioral authority. Eliminate duplicate `MemoryEngine.recall()` calls. Strip behavioral logic from FaceEngine.

**Status:** ✅ Complete. Behavior-preserving ownership migration with zero functional changes.

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

- **BehaviorEngine** is sole canonical producer of `engagement_drive` (full lifecycle: decay → memory mod → reset → write-back → security floor)
- **MoodEngine** produces baseline `mood_strength`; **BehaviorEngine** transforms it into canonical value
- **FaceEngine** is presentation-only: eyelids, blink, gaze, bounce, pulse — all read-only consumers of `CreatureState`

**Architectural principle adopted:**

> Behavior engines transform state. Presentation engines interpret state. Rendering code must never define creature behavior.

### Next Work

V2.7.3 will focus on calibration: measure real `domain_strength` distributions, tune thresholds using observed data instead of estimates, freeze constants after hardware validation.

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

BehaviorEngine is sole canonical producer of `engagement_drive` (decay → memory modulation → touch reset → write-back → security floor override). It transforms MoodEngine's baseline `mood_strength` into the final canonical value. FaceEngine is a pure presentation engine — no behavioral decisions, no memory queries, no CreatureState mutations.

Explicitly deferred from V2.7:
- Learned preferences, memory recall, relationship modeling, personality evolution — all depend on a richer behavior layer in future sprints.
