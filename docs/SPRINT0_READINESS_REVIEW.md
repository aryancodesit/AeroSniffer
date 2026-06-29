# Sprint 0 Implementation Readiness Review
## BehaviorEngine V1 — ADR-0004 Requirement Traceability

**Date:** 2026-06-28
**Status:** ✅ Ready for Sprint 1 (with 1 pre-condition and 1 FaceEngine fix)
**Reviewer:** Architecture audit

---

## 0. Prerequisite: V2.6 Sprint 3 Hardware Certification

| Requirement | Source | Status |
|---|---|---|
| 30-min multi-domain soak (security + aviation + touch) | ADR-0004 Future Considerations | ❌ **BLOCKED** — no hardware test time |
| 15 memory subtypes certified for strength accuracy | ADR-0004 Consequences | ❌ **BLOCKED** — same dependency |

**Risk:** Without certified domain strengths, BehaviorEngine V1 rules (which branch on `security_strength > 30`, `touch_strength > 25`) may fire incorrectly. This is the highest risk for Sprint 1.

---

## 1. ADR Requirements → Implementation Mapping

### 1.1 File Requirements

| # | ADR Requirement | Implementation Step | Verification |
|---|---|---|---|
| R1 | BehaviorEngine.h in `Companion/` | Create `AeroSniffer/Companion/BehaviorEngine.h` | File exists |
| R2 | BehaviorEngine.cpp in sketch root | Create `AeroSniffer/BehaviorEngine.cpp` | File exists |
| R3 | Standard header guard and include pattern | `#pragma once` + `#ifndef BEHAVIOR_ENGINE_H` | Matches MoodEngine.h, AttentionEngine.h convention |

**Must-validate**

### 1.2 Data Flow Requirements

| # | ADR Requirement | Implementation Step | Verification |
|---|---|---|---|
| R4 | BehaviorEngine.tick() called AFTER MemoryEngine.tick() | Insert at `AeroSniffer.ino:622` — after line 621 `MemoryEngine.tick();` | Code review |
| R5 | BehaviorEngine.tick() called BEFORE FaceEngine.render() | Insert BEFORE line 674 mode dispatch — 52 lines before | Order confirmed |
| R6 | Reads MemoryEngine.recall() | `MemorySummary mem = MemoryEngine.recall();` in tick() | Compiles |
| R7 | Reads CreatureState.mood_strength | `g_creature.mood_strength` in security rule comparison | Code review |
| R8 | Reads CreatureState.engagement_drive | `g_creature.engagement_drive` for read-modify-write floor | Code review |
| R9 | Writes CreatureState.mood_strength via read-modify-write | `g_creature.mood_strength = constrain(g_creature.mood_strength + modifier, 0, 100);` | Unit test |
| R10 | Writes CreatureState.engagement_drive via read-modify-write | `g_creature.engagement_drive = max(g_creature.engagement_drive, floor);` | Unit test |
| R11 | Must NOT write CreatureState.mood | No assignment to `g_creature.mood` anywhere in BehaviorEngine | Grep check |
| R12 | Must NOT read FaceEngine state | No reference to `FaceEngine` or `_engagement_level` in BehaviorEngine | Grep check |
| R13 | Include in AeroSniffer.ino | `#include "Companion/BehaviorEngine.h"` after line 28 | Lint |
| R14 | Extern instance | `extern BehaviorEngineClass BehaviorEngine;` in .h, definition in .cpp | Linker |

**Must-validate (R4-R14)**

### 1.3 BehaviorEngine Internal Requirements

| # | ADR Requirement | Implementation Step | Verification |
|---|---|---|---|
| R15 | `_mood_modifier` private int8_t, clamped ±15 | `int8_t _mood_modifier = 0;` in private section | Code review |
| R16 | `_engagement_floor` private uint8_t | `uint8_t _engagement_floor = 0;` in private section | Code review |
| R17 | No modifier fields exposed in CreatureState | Verify no `mood_strength_modifier` or `engagement_drive_floor` in AeroSnifferOS.h | Grep check |
| R18 | Modifier is floor-only for engagement | `engagement_drive` is `max()`, never `constrain()` with lower bound | Code review |
| R19 | V1 rule: `if security_strength > 30 && security_strength > mood_strength → floor = 60` | `if (mem.domain_strength[DOMAIN_SECURITY] > 30 && mem.domain_strength[DOMAIN_SECURITY] > g_creature.mood_strength) { _engagement_floor = 60; }` | Test |
| R20 | V1 rule: `if touch_strength > 25 → modifier = clamp(modifier + 5, -15, 15)` | `if (mem.domain_strength[DOMAIN_TOUCH] > 25) { _mood_modifier = constrain((int16_t)_mood_modifier + 5, -15, 15); }` | Test |
| R21 | V1 rule: no activity > 120s → no override (implicit) | If `mem.ms_since_last_touch > 120000` and no security/touch rule fires, modifier=0, floor=0 | Test |
| R22 | Aviation rule deferred (future field) | No aviation check in V1 — document as TODO | Code review |

**Should-validate (R19-R21), Must-validate (R15-R18, R22)**

### 1.4 V1 Rule Pseudocode (from ADR-0004)

```cpp
void BehaviorEngineClass::tick() {
    MemorySummary mem = MemoryEngine.recall();

    int8_t modifier = 0;
    uint8_t floor = 0;

    // Security > 30 && security > mood_strength → floor engagement at 60
    if (mem.domain_strength[DOMAIN_SECURITY] > 30 &&
        mem.domain_strength[DOMAIN_SECURITY] > g_creature.mood_strength) {
        floor = 60;
    }

    // Touch > 25 → mood modifier +5
    if (mem.domain_strength[DOMAIN_TOUCH] > 25) {
        modifier = constrain((int16_t)_mood_modifier + 5, -15, 15);
    }

    // Apply transforms (read-modify-write)
    g_creature.mood_strength = constrain(g_creature.mood_strength + modifier, 0, 100);
    g_creature.engagement_drive = max(g_creature.engagement_drive, floor);

    // Persist internal state for debug
    _mood_modifier = modifier;
    _engagement_floor = floor;
}
```

---

## 2. 🚨 Critical Finding: FaceEngine Private Float Bypass

### Problem

FaceEngine maintains a **private `_engagement_level` float** (`AeroSnifferOS.h:227`) that is the *real* source of engagement state. The canonical `g_creature.engagement_drive` is merely a **write-through cache** updated at `AeroSnifferOS.cpp:619`:

```cpp
// Line 612 — operates on private float, NOT canonical field
_engagement_level = max(floor_val, _engagement_level - decay);
// ...
// Line 619 — then copies to canonical (overwriting any upstream modifications)
g_creature.engagement_drive = (uint8_t)(_engagement_level + 0.5f);
```

### Consequence for BehaviorEngine

When BehaviorEngine floors `g_creature.engagement_drive` at pipeline position 6, the value is **immediately overwritten** by FaceEngine at position 7 (`updateAnimations()` line 619). The BehaviorEngine floor has **zero effect** on the rendered output.

### Trace Without Fix

```
Tick N:
  Start: _engagement_level=59.3, g_creature.engagement_drive=59
  BehaviorEngine floors: g_creature.engagement_drive = max(59, 60) = 60  ← RAISED
  FaceEngine.updateAnimations():
    _engagement_level = max(floor_val, 59.3 - decay) = 59.29  ← private float IGNORES canonical!
    g_creature.engagement_drive = (uint8_t)(59.29 + 0.5) = 59  ← OVERWRITES BehaviorEngine's 60!
  Result: BehaviorEngine floor is LOST. Engagement continues to decay below floor.
```

### Fix: Import Canonical Value at Top of updateAnimations()

Add one line at `AeroSnifferOS.cpp:595` (start of engagement section):

```cpp
// Import any upstream pipeline transforms (BehaviorEngine floor)
_engagement_level = max(_engagement_level, (float)g_creature.engagement_drive);
```

### Trace With Fix

```
Tick N:
  Start: _engagement_level=59.3, g_creature.engagement_drive=59
  BehaviorEngine floors: g_creature.engagement_drive = max(59, 60) = 60
  FaceEngine.updateAnimations():
    FIX: _engagement_level = max(59.3, 60.0) = 60.0  ← imported!
    _engagement_level = max(floor_val, 60.0 - decay) = 59.995
    g_creature.engagement_drive = (uint8_t)(59.995 + 0.5) = 60
  Result: BehaviorEngine floor IS preserved (oscillates around 60).
```

### Backward Compatibility

When BehaviorEngine is absent, `g_creature.engagement_drive` carries the same V2.6 FaceEngine-decayed value. The fix `_engagement_level = max(_engagement_level, (float)g_creature.engagement_drive)` is a no-op in V2.6 because `_engagement_level` is always ≥ `g_creature.engagement_drive` (the private float is the source of truth). **Zero behavioral change for V2.6.**

**Must-validate — pre-condition for Sprint 1**

---

## 3. Verification Methods

### 3.1 Static Verification (No Hardware)

| Check | Command | Expected |
|---|---|---|
| No `g_creature.mood` write in BehaviorEngine | `grep -rn "g_creature.mood\s*=" AeroSniffer/Companion/BehaviorEngine.* AeroSniffer/BehaviorEngine.*` | No matches |
| No FaceEngine reference | `grep -rn "FaceEngine" AeroSniffer/Companion/BehaviorEngine.* AeroSniffer/BehaviorEngine.*` | No matches |
| No `_final` fields in CreatureState | `grep -n "_final" AeroSniffer/AeroSnifferOS.h` | No matches |
| Read-modify-write on both fields | Code review of `.cpp` | Both use `g_creature.field = f(g_creature.field, ...)` |
| Pipeline order in task_core1 | Code review of `AeroSniffer.ino:622` | After MemoryEngine.tick(), before mode dispatch |

### 3.2 Dynamic Verification (Desk-Test with Serial)

Wire `BEHAVIOR_DEBUG` and `BehaviorPipelineSnapshot` into a Serial print:

| Test | Injection | Expected Observation |
|---|---|---|
| Security rule | Trigger deauth event (raises security_strength > 30) | `engagement_drive` floor rises to 60 |
| Touch rule | Tap repeatedly (>25 touch_strength) | `mood_strength` slowly climbs (modifier accumulates) |
| Both rules | Security + touch simultaneously | Both effects visible independently |
| No activity > 120s | Wait 2+ minutes | modifier = 0 (no spontaneous change) |
| Decay below floor | After security floor triggers, wait | engagement_drive oscillations around 60 |
| Backward compatibility | Remove BehaviorEngine include/tick | V2.6 behavior preserved |

### 3.3 Pipeline Order Verification

Print timestamps at each pipeline position in task_core1:

```
[TICK] Start: mood_strength=45, engagement_drive=72
[TICK] After MemoryEngine: memory domain=[T:30 S:35 A:12 M:40 AT:20]
[TICK] After BehaviorEngine: mood_strength=45, engagement_drive=72 (floor=0)
[TICK] After FaceEngine pre-decay: engagement_drive=72
[TICK] After FaceEngine post-decay: engagement_drive=71.8
```

---

## 4. Pitfalls

| # | Pitfall | Mitigation |
|---|---|---|
| P1 | **FaceEngine private float bypass** (described above) | Add `_engagement_level = max(_engagement_level, (float)g_creature.engagement_drive)` import |
| P2 | **MoodEngine baseline overwrite**: BehaviorEngine's mood_strength output is read-modify-write from MoodEngine's baseline. If BehaviorEngine is disabled, `mood_strength` carries MoodEngine baseline unchanged. | Invariant 7 guarantees this; verify with `#ifdef` toggle |
| P3 | **engagement_drive decay timing**: FaceEngine decays at START of updateAnimations, not after rendering. The "cross-tick maintenance" model requires decay to prepare the value for the *next* tick's pipeline. | Confirm decay modifies `_engagement_level` which becomes next tick's start value; rendered value from CURRENT tick was read before decay |
| P4 | **Touch reset in FaceEngine** (line 615-617): `if (touch || attention) { _engagement_level = 100; }` — this OVERRIDES BehaviorEngine's floor on touch events. This is correct behavior (touch resets engagement to full). | Document as intentional: touch is a stronger signal than security floor |
| P5 | **Modifier accumulation**: `_mood_modifier` persists across ticks (+5 per tick when touch > 25). If touch stays above threshold for 3+ ticks, modifier hits +15 cap. The modifier is NOT reset when touch drops below 25. | WAI per ADR: modifier decays slowly. Document that Sprint 2 should add decay. |
| P6 | **Anxious mood floor in FaceEngine** (line 597): `floor_val = (mood == MOOD_ANXIOUS) ? 20 : 0` — this is FaceEngine's OWN floor, applied BEFORE BehaviorEngine's floor. These are additive in effect: FaceEngine floor (20) + BehaviorEngine floor (60) = effective floor of 60 (max). | Verify with mood=ANXIOUS: both floors apply, BehaviorEngine's is higher |
| P7 | **MemoryEngine.recall() cost**: Called every 16ms tick. If memory scan is expensive, may impact frame timing. | Acceptable for V1; optimize in Sprint 3 if profiling shows >1ms |
| P8 | **Thread safety**: BehaviorEngine runs on Core 1. No concurrent access to g_creature, but EventBus callbacks run on Core 0. | g_creature is modified only on Core 1 (pipeline). Core 0 writes to MemoryEngine (its own state). No shared mutable state. |

---

## 5. Test Cases for Sprint 1

### TC-01: Security Floor — Active
- Pre: `security_strength = 35`, `mood_strength = 20`, `engagement_drive = 45`
- Stimulus: `BehaviorEngine.tick()`
- Expected: `engagement_drive = max(45, 60) = 60`
- Category: Unit test

### TC-02: Security Floor — Below Threshold
- Pre: `security_strength = 25`, `mood_strength = 20`, `engagement_drive = 45`
- Stimulus: `BehaviorEngine.tick()`
- Expected: `engagement_drive = 45` (no change)
- Category: Unit test

### TC-03: Security Floor — High Mood (No Override)
- Pre: `security_strength = 35`, `mood_strength = 40`, `engagement_drive = 45`
- Stimulus: `BehaviorEngine.tick()`
- Expected: `engagement_drive = 45` (security_strength NOT > mood_strength)
- Category: Unit test

### TC-04: Touch Modifier — Accumulation
- Pre: `touch_strength = 30`, `mood_strength = 50`
- Stimulus: First `BehaviorEngine.tick()`
- Expected: `mood_strength = min(50 + 5, 100) = 55`, `_mood_modifier = 5`
- Category: Unit test

### TC-05: Touch Modifier — Cap at +15
- Pre: `touch_strength = 30`, `_mood_modifier = 13`, `mood_strength = 50`
- Stimulus: `BehaviorEngine.tick()`
- Expected: `_mood_modifier = 15` (capped), `mood_strength = 65`
- Follow-up tick: `_mood_modifier` stays at 15, `mood_strength = 80`
- Category: Unit test

### TC-06: Touch Below Threshold
- Pre: `touch_strength = 20`, `mood_strength = 50`
- Stimulus: `BehaviorEngine.tick()`
- Expected: `_mood_modifier = 0`, `mood_strength = 50` (no change)
- Category: Unit test

### TC-07: No Activity > 120s
- Pre: `ms_since_last_touch = 180000`, `touch_strength = 30`, `security_strength = 15`
- Stimulus: `BehaviorEngine.tick()`
- Expected: `_mood_modifier = 0`, `_engagement_floor = 0` (no overrides)
- Note: Touch rule fires on strength, not recency. In V1, touch_strength persists from memory even if no recent touch. This is WAI per ADR.
- Category: Integration test

### TC-08: FaceEngine Bypass Fix
- Pre: `_engagement_level = 55.0`, `g_creature.engagement_drive = 55`, security sets floor=60
- Stimulus: BehaviorEngine sets `g_creature.engagement_drive = 60`
- FaceEngine `updateAnimations()`: `_engagement_level = max(55.0, 60.0) = 60.0`
- After decay (0.005): `_engagement_level = 59.995`
- `g_creature.engagement_drive = 60`
- Expected: engagement_drive oscillates around 60, never drops below
- Category: Pipeline integration test

### TC-09: Backward Compatibility (BehaviorEngine Absent)
- Pre: `#ifdef BEHAVIOR_ENGINE_ENABLED` set to 0
- Stimulus: Full Core 1 tick cycle
- Expected: `mood_strength` = MoodEngine baseline (unchanged by modifier), `engagement_drive` = FaceEngine decayed value (no floor)
- Category: Regression test

### TC-10: Aviation Rule — No V1 Action
- Pre: `aviation_strength = 40`, recent flight event
- Stimulus: `BehaviorEngine.tick()`
- Expected: No change to mood_strength or engagement_drive from aviation
- Category: Documentation verification (code should have TODO comment)

---

## 6. Sprint 1 Implementation Checklist

### New Files
- [ ] `AeroSniffer/Companion/BehaviorEngine.h` — class declaration
- [ ] `AeroSniffer/BehaviorEngine.cpp` — V1 rule implementation, `BehaviorPipelineSnapshot` behind `#ifdef BEHAVIOR_DEBUG`

### Modified Files
- [ ] `AeroSniffer/AeroSniffer.ino`:
  - [ ] Add `#include "Companion/BehaviorEngine.h"` at line 29
  - [ ] Add `BehaviorEngine.tick();` at line 622 (after MemoryEngine.tick())
  - [ ] Add `BehaviorEngine.begin();` in setup() at ~line 712
- [ ] `AeroSniffer/AeroSnifferOS.cpp` (FaceEngine):
  - [ ] Add `_engagement_level = max(_engagement_level, (float)g_creature.engagement_drive);` at line 595

### Extern Declaration
- [ ] `extern BehaviorEngineClass BehaviorEngine;` in BehaviorEngine.h

### Debug Support
- [ ] `BehaviorPipelineSnapshot` struct behind `#ifdef BEHAVIOR_DEBUG`
- [ ] Serial print of snapshot after tick

---

## 7. Sprint Boundary Items (Do NOT Implement in Sprint 1)

| Item | Reason | Future Sprint |
|---|---|---|
| Aviation recent-flight signal | No API surface in MemorySummary; needs new field | Sprint 3+ or V2.8 |
| Modifier decay over time | Not specified in ADR V1 rules; V1 rules are one-shot | Sprint 2 |
| FaceEngine engagement modulation migration | Breaking change; keep V2.6 behavior through fix | Sprint 3 |
| MemoryEngine.recall() optimization | Not profiled; accept 16ms cost | Sprint 3 |
| Ultimate mood_strength/engagement override | Reserved for future subagent/modifier pattern | V2.9+ |
| CI build validation (S0-T7b) | No `arduino-cli` locally; defer to post-merge | Post-merge |
