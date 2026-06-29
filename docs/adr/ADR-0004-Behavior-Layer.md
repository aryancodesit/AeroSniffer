# ADR-0004: Behavior Layer

**Status:** Accepted (Sprint 0, V2.7)  
**Date:** 2026-06-28  
**Author:** Sprint 0 Architectural Review  

See also: [DECISIONS.md](../DECISIONS.md) for ADR-001 through ADR-006 (V2.3–V2.5 era).

---

## Motivation

V2.6 Memory Engine records and decays events across 4 domains (Touch, Security, Aviation, Mood) with 15 subtypes. It has no consumer — the creature cannot act on what it remembers. Memory exists as an observer with no downstream effect on behavior.

Closing this gap requires a component that:
- Reads memory output (domain strengths, recency, top records)
- Translates memory state into behavioral influence
- Does not violate existing certified subsystem boundaries

---

## Decision

Create a **Behavior Layer** as a new engine positioned between MemoryEngine and CreatureState. The canonical data flow is:

```
MemoryEngine.recall() → BehaviorEngine → CreatureState → FaceEngine
```

BehaviorEngine is a class (`BehaviorEngineClass`) following the established pattern:
- Declaration: `AeroSniffer/Companion/BehaviorEngine.h`
- Implementation: `AeroSniffer/BehaviorEngine.cpp` (sketch root for Arduino build compatibility)
- Tick: called from Core 1 loop after `MemoryEngine.tick()` and before `FaceEngine.updateAnimations()`

---

## Architectural Boundaries

| Component | Reads | Writes | Must NOT |
|-----------|-------|--------|----------|
| MemoryEngine | Touch/Security/Aviation events | Internal ring buffer, LittleFS | Read CreatureState, write to FaceEngine |
| BehaviorEngine | `MemoryEngine.recall()` via `MemorySummary`, `CreatureState.mood_strength`, `CreatureState.engagement_drive` | `CreatureState.mood_strength` (read-modify-write pipeline stage), `CreatureState.engagement_drive` (read-modify-write pipeline stage) | Write `CreatureState.mood`; read or write FaceEngine, MemoryEngine, MoodEngine, or AttentionEngine state |
| MoodEngine | Touch history, `CreatureState.(attention,emotion)` | `CreatureState.mood`, `CreatureState.mood_strength` (uint8_t baseline) | Read MemoryEngine, write to FaceEngine |
| FaceEngine | `CreatureState.mood`, `CreatureState.mood_strength`, `CreatureState.engagement_drive` | TFT display buffer, `CreatureState.engagement_drive` (natural decay decrement) | Perform any arithmetic, clamp, max, or merge policy; read MemoryEngine or BehaviorEngine directly |
| PersistenceService | `CreatureState.(attention,mood)` | NVS Preferences | Read MemoryEngine, subscribe to EventBus |

**Merge model applied entirely inside BehaviorEngine — canonical state is overwritten:**

```
BehaviorEngine.tick():
  internal_modifier = clamp(compute_from_memory(), -15, 15)      // private member
  internal_floor    = compute_engagement_floor_from_memory()      // private member
  
  // Read MoodEngine baseline (written at position 3), write finalized value
  CreatureState.mood_strength = clamp(
    CreatureState.mood_strength + internal_modifier, 0, 100
  )
  
  // Read FaceEngine engagement (decayed at position 7 of previous tick), apply floor
  CreatureState.engagement_drive = max(
    CreatureState.engagement_drive, internal_floor
  )
```

No modifier fields are exposed in CreatureState — `internal_modifier` and `internal_floor` are private members of BehaviorEngine.

**Backward compatibility:** When BehaviorEngine is not yet implemented or disabled, `mood_strength` and `engagement_drive` carry MoodEngine's baseline and FaceEngine's decayed value respectively — identical to V2.6 behavior. FaceEngine reads the canonical fields and has no awareness of whether BehaviorEngine exists.

**Key rule:** Data flow is strictly unidirectional within each tick. Fields that undergo multiple writes in a single tick are classified as **pipeline transform fields** — a formal architectural pattern defined in the CreatureState section below. All such writes are read-modify-write transforms in a fixed tick order, never blind assignments.

---

## Data Flow (V1)

### Execution Order (Architectural Contract)

The following tick order on Core 1 is an **architectural invariant**. Reordering requires documented architectural review and ADR amendment.

```
For each Core 1 tick:
  1. AttentionEngine.tick()           — Fast: gaze targets, fixation state
  2. EmotionEngine.tick()             — Reads attention output from same tick
  3. MoodEngine.tick()                — Reads emotion from same tick; MoodEngine must run before PersistenceService that snapshots its output
  4. PersistenceService.tick()        — Snapshots mood/attention; must run after MoodEngine, before Memory (I2C/NVS latency window)
  5. MemoryEngine.tick()              — Records events including mood changes; must run after all state producers
  6. BehaviorEngine.tick()            — Reads MemoryEngine.recall() from this tick; produces modifiers for FaceEngine
  7. FaceEngine.updateAnimations()    — Reads final CreatureState; must run last to consume all upstream engine output
```

**Dependency justification:**

| Position | Engine | Depends on (same tick) | Required by (same tick) | Rationale |
|----------|--------|------------------------|------------------------|-----------|
| 1 | Attention | — | Emotion, Mood | No upstream engine dependency |
| 2 | Emotion | Attention | Mood | Reads fixation state to derive emotion |
| 3 | Mood | Attention, Emotion, Touch | Persistence, Memory | Reads emotion, attention, touch to compute baseline mood |
| 4 | Persistence | Mood | — | Snapshots mood/attention; runs before Memory to leave I2C window |
| 5 | Memory | Mood (via emotion), Attention | Behavior | Records all events including mood transitions; must precede consumption |
| 6 | Behavior | Memory | Face | Reads recall() from this tick; runs after all state is finalized |
| 7 | Face | All upstream engines | — | Pure consumer; must run last |

Ordering constraint: 1–3 are chained (Attention → Emotion → Mood). 4 must follow 3 (Persistence snapshots mood). 5 must follow 4 (Memory records after I2C subsystem window). 6 must follow 5 (Behavior reads Memory). 7 must be last (Face consumes all). Any reordering risks stale reads across engine boundaries.

### BehaviorEngine Tick Logic

Within `BehaviorEngine.tick()`, the engine reads upstream state, applies internal rules, and overwrites canonical CreatureState values:

```
summary = MemoryEngine.recall()

// -- Internal computations (private members, no CreatureState exposure) --
mood_strength_modifier = 0
engagement_drive_floor = 0

if security_strength > 30 AND security > mood_strength:
    engagement_drive_floor = 60

if aviation_strength > 20 AND recent_flight_event (last 60s):
    attention_bias = TARGET_FLIGHT tendency    // future field, not in V1

if touch_strength > 25:
    mood_strength_modifier = clamp(mood_strength_modifier + 5, -15, 15)

if no significant activity > 120s:
    no override (modifiers remain at 0)

// -- Overwrite canonical state with finalized values --
// MoodEngine's baseline is consumed and replaced with the clamped result.
// FaceEngine's engagement_drive has the floor applied before FaceEngine reads it.
// Pattern: read-modify-write. Sequential write guaranteed by tick ordering.
CreatureState.mood_strength = clamp(
    CreatureState.mood_strength + mood_strength_modifier, 0, 100
)
CreatureState.engagement_drive = max(
    CreatureState.engagement_drive, engagement_drive_floor
)
```

These rules are deterministic, additive, and bounded. No rule reduces CreatureState values below their natural decay floor. BehaviorEngine reads `mood_strength` (MoodEngine baseline from position 3) then overwrites it with the finalized value — FaceEngine at position 7 reads the finalized value. FaceEngine never performs arithmetic.

---

## Ownership Rules

### MoodEngine — Baseline Mood Computation

- **Owns:** Primary mood arbitration (RELAXED, PLAYFUL, ANXIOUS)
- **Input:** Touch history ring buffer, `CreatureState.(attention,emotion)`
- **Output:** `CreatureState.mood` (enum), `CreatureState.mood_strength` (uint8_t, 0–100 baseline)
- **Authority:** Sole determiner of mood type. BehaviorEngine never writes `mood`. MoodEngine writes `mood_strength` baseline; BehaviorEngine overwrites it with the finalized value at position 6. MoodEngine does not read `mood_strength` again until the next tick.
- **Decay:** Fixed-point, mood-specific intervals (PLAYFUL: 36s/unit, ANXIOUS: 24s/unit)
- **No dependency on:** MemoryEngine, BehaviorEngine, FaceEngine

### BehaviorEngine — Bounded Behavioral Influence

- **Owns:** Memory-informed behavioral modulation. Internal merge of MoodEngine baseline with memory-derived modifier, applied as overwrite of canonical CreatureState fields.
- **Input:** `MemoryEngine.recall()` via `MemorySummary`, `CreatureState.mood_strength` (MoodEngine baseline), `CreatureState.engagement_drive` (FaceEngine natural decay from previous tick)
- **Output:** `CreatureState.mood_strength` (uint8_t, overwritten with finalized 0–100), `CreatureState.engagement_drive` (uint8_t, overwritten with floored 0–100)
- **Internal state (private members, NOT exposed in CreatureState):**
  - `_mood_modifier` (int8_t, clamped ±15 internally) — memory-derived adjustment to baseline
  - `_engagement_floor` (uint8_t, 0–100) — minimum engagement threshold from memory
- **Write pattern:** read-modify-write pipeline transform. BehaviorEngine reads the current canonical value, applies a deterministic transform, and writes the result back to the same field. This overwriting is a pipeline stage, not a blind assignment — it always reads before writing and its modification is bounded by the pipeline transform invariants.
- **Constraints:**
  - Must NOT write `CreatureState.mood` — mood type is MoodEngine's exclusive domain
  - Must NOT write to MemoryEngine, MoodEngine, FaceEngine, or AttentionEngine
  - Must NOT read FaceEngine state
  - All internal modifiers are bounded: `_mood_modifier` clamped to ±15; `_engagement_floor` is floor-only (BehaviorEngine never reduces engagement_drive)
  - All writes are read-modify-write — BehaviorEngine reads the current value before writing

### FaceEngine — Pure Renderer

- **Owns:** Frame buffer, animation timing, TFT display, `CreatureState.engagement_drive` natural decay
- **Input:** `CreatureState.mood`, `CreatureState.mood_strength`, `CreatureState.engagement_drive` — all values are already finalized before FaceEngine reads them (BehaviorEngine ran at position 6, FaceEngine runs at position 7)
- **Output:** TFT display buffer, `CreatureState.engagement_drive` (per-tick natural decay decrement — this is the FaceEngine's sole write, performed after reading the floored value)
- **Constraints:**
  - Must NOT perform any arithmetic, clamp, max, merge, or conflict resolution on mood or engagement values — these arrive from BehaviorEngine already finalized
  - Must NOT read MemoryEngine, BehaviorEngine, or any engine state directly
  - Must NOT write `CreatureState.mood` or `CreatureState.mood_strength`
  - Must NOT have any awareness that MemoryEngine or BehaviorEngine exist
  - All behavioral state arrives through canonical CreatureState fields — no `_final` suffix convention needed
  - `engagement_drive` decay is the sole output; this is inherited V2.5 Sprint 5A behavior and classified as rendering maintenance (ticking down a counter), not behavioral logic

### CreatureState — The Contract (Single Canonical State)

`CreatureState` is the sole communication channel between engines. Every field stores the current canonical value. Fields fall into two classes:

- **Single-owner fields:** Written by exactly one engine. Most fields (`mood`, `emotion`, `attention.*`).
- **Pipeline transform fields:** Written by a sequence of engines in a fixed tick order, each applying a bounded read-modify-write transform. Used when multiple engines contribute to a single canonical value.

| Field | Written By | Read By | Field Class |
|-------|-----------|---------|-------------|
| `attention.{target,source,strength}` | AttentionEngine | EmotionEngine, MoodEngine, FaceEngine | Single-owner |
| `emotion` | EmotionEngine | MoodEngine, FaceEngine | Single-owner |
| `mood` | MoodEngine | FaceEngine | Single-owner |
| `mood_strength` | MoodEngine (pos 3), BehaviorEngine (pos 6) | FaceEngine (pos 7) | **Pipeline transform** |
| `engagement_drive` | BehaviorEngine (pos 6, floor pipe stage), FaceEngine (pos 7, decay maintenance) | FaceEngine (pos 7, reads floored value before decay) | **Pipeline transform** |

#### Pipeline Transform (CreatureState mutation model)

A **pipeline transform field** is a CreatureState field whose value is produced by a sequence of engine transforms (stages) in a fixed tick order. This mutation model is specific to CreatureState — it is not a general architectural pattern. The seven invariants below govern all pipeline transform fields and must be satisfied for any new pipeline field added in the future.

**Invariant 1 — Deterministic execution order:** The pipeline sequence is fixed, documented in the Execution Order contract, and protected by requiring architectural review before any reordering. Each stage reads an input that was produced by the previous stage.

**Invariant 2 — Read-modify-write transforms:** Every pipeline stage reads the current field value, applies a deterministic function, and writes the result. No stage performs blind assignment (`field = constant`) except during initialization. Read-modify-write ensures that each stage builds on the previous stage's output rather than discarding it.

**Invariant 3 — Bounded modification:** Each pipeline stage's transform is bounded by documented limits. Bounds prevent any single stage from producing out-of-range values or dominating the pipeline:
   - MoodEngine (`mood_strength` stage 1): writes baseline within `[0, 100]` (by definition)
   - BehaviorEngine (`mood_strength` stage 2): `clamp(baseline + modifier, 0, 100)` where modifier is clamped to `[-15, 15]`
   - BehaviorEngine (`engagement_drive` stage 1): `max(current, floor)` where floor is `[0, 100]`

   FaceEngine's `engagement_drive` decay (`max(current - decay_rate, 0)`) is NOT a pipeline stage — it is a cross-tick maintenance operation that prepares the value for the next tick's pipeline. This distinction is explained below.

**Invariant 4 — Temporal linearity:** The pipeline is strictly linear within a single tick. No stage reads a value that was written by a later stage in the same tick. No stage reads a value that it wrote in the same stage. This is guaranteed by the Execution Order contract: each stage runs in a fixed position, and positions are ordered 1–7. For `mood_strength`: MoodEngine (pos 3) writes baseline → BehaviorEngine (pos 6) reads, transforms, writes → FaceEngine (pos 7) reads finalized value. For `engagement_drive`: BehaviorEngine (pos 6) floors → FaceEngine (pos 7) reads floored value.

**Invariant 5 — Consumer isolation:** The engine that reads the final pipeline output for rendering or decision-making must not perform arithmetic on the value — it reads it as-is. For both pipeline fields, FaceEngine is the Consumer and satisfies this: it reads `mood_strength` and `engagement_drive` at position 7 and renders what it reads, without clamp, max, merge, or any transform. Any writes to the same field by the Consumer are cross-tick maintenance operations, not pipeline transforms. FaceEngine's `engagement_drive` decay falls into this category: it reads the floored value, renders, then decays — the decay is a maintenance write that propagates to the next tick's pipeline.

**Invariant 6 — Exclusive transformation ownership:** Each pipeline stage is owned by exactly one engine. No two engines share the same stage. An engine may own multiple operations (e.g., FaceEngine owns the consumer read of `engagement_drive` and the cross-tick decay maintenance write on the same field), but each operation is a distinct, documented responsibility. This prevents ambiguity about which engine is responsible for which transform.

**Invariant 7 — Fallback validity:** With any subset of pipeline stages disabled, the field retains a valid default produced by the remaining stages. For `mood_strength`: if BehaviorEngine is absent, the field carries MoodEngine's baseline — identical to V2.6. For `engagement_drive`: if BehaviorEngine is absent, the field carries FaceEngine's natural decayed value — identical to V2.6.

**Canonical rendering state:** FaceEngine reads `mood`, `mood_strength`, and `engagement_drive` at position 7. By this point in the tick:
- `mood_strength` has been finalized by BehaviorEngine (clamped, with memory-derived modifier applied)
- `engagement_drive` has been floored by BehaviorEngine (no value below the memory-derived threshold)

FaceEngine renders using these values without performing any arithmetic. It then applies natural decay to `engagement_drive` as a cross-tick maintenance operation (described below).

**Cross-tick maintenance: `engagement_drive` decay**

A **cross-tick maintenance operation** is a write to a pipeline transform field that occurs outside the pipeline sequence — it prepares the field value for the next tick rather than contributing to the current tick's rendered state. FaceEngine's `engagement_drive` natural decay (`max(current - decay_rate, 0)`) is the first example. It is NOT a pipeline stage. It is a separate maintenance operation that performs two roles:

1. **Tick N, end of FaceEngine:** Writes the decayed `engagement_drive` value. This value persists across ticks.
2. **Tick N+1, BehaviorEngine (pos 6):** Reads the decayed value (which may have dropped below the floor threshold), applies the floor again. The floor is re-established before FaceEngine reads at position 7.

This design means the floor is applied every tick — it is not a one-time clamp. The decay does not affect FaceEngine's rendering because FaceEngine reads `engagement_drive` at the START of its tick (after BehaviorEngine has floored it) and only decays at the END. The decayed value is what BehaviorEngine will read in the next tick.

The separation between pipeline transform and cross-tick maintenance is:
- **Pipeline** (intra-tick): BehaviorEngine floors at pos 6 → FaceEngine reads floored value at pos 7 start
- **Maintenance** (cross-tick): FaceEngine decays at pos 7 end → BehaviorEngine re-floors in next tick at pos 6

**BehaviorPipelineSnapshot (optional debug structure, specific to BehaviorEngine stage):**

When BehaviorEngine is enabled and debugging is desired, a snapshot captures intermediate pipeline values before the BehaviorEngine stage overwrites them:

```
struct BehaviorPipelineSnapshot {
    uint8_t mood_strength_baseline;   // MoodEngine's baseline before BehaviorEngine overwrote it
    int8_t  mood_modifier_applied;    // The ±15 bounded modifier BehaviorEngine applied
    uint8_t engagement_raw;           // FaceEngine's decayed value before BehaviorEngine floored it
    uint8_t engagement_floor_applied; // The floor threshold BehaviorEngine used
};
```

The `BehaviorPipelineSnapshot` is populated by BehaviorEngine.tick() (at the start of its pipeline stage, before it writes) and queried via serial debug commands. It is **not part of CreatureState** — it is owned by BehaviorEngine and has zero production overhead when debug is compiled out (wrapped in `#ifdef BEHAVIOR_DEBUG`). Future engines that add new pipeline transform fields or pipeline stages should define their own `{Engine}PipelineSnapshot` struct following this naming convention.

**Implication:** Future engines can add new fields to `CreatureState` without modifying existing engines. Fields may be single-owner (most cases) or pipeline transform (when multiple engines must contribute to a single canonical value). New pipeline transforms must satisfy all seven invariants and be documented in this ADR. Cross-tick maintenance writes (like FaceEngine's decay) are not pipeline stages but must be documented separately to prevent confusion about field ownership. This is the extension point for V2.7+ features (curiosity drive, fatigue, learned preferences).

---

## Alternatives Rejected

| Alternative | Reason for Rejection |
|-------------|---------------------|
| MemoryEngine writes CreatureState directly | Violates separation of concerns — couples memory format to creature state. MemoryEngine would need to understand mood semantics and rendering implications. |
| FaceEngine reads MemoryEngine directly | Violates single responsibility — FaceEngine would need to understand memory domains, salience, recency, and dedup. Adds two reasons to change to a certified renderer. |
| Behavior Layer as part of MoodEngine | MoodEngine already has fixed-priority arbitration. Adding memory-aware rules would make it responsible for both mood and behavior, violating single responsibility. Would also break the V2.5 Sprint 3C freeze. |
| Rule-based system in a separate task on Core 0 | Behavior Layer must run after Memory tick and before Face render — this ordering is only guaranteed in the Core 1 loop. Running as a separate task would require synchronization. |
| BehaviorEngine writes modifier fields; FaceEngine performs merge arithmetic | Violates FaceEngine's single responsibility as a pure renderer. FaceEngine would need to understand behavioral merge policy (clamp boundaries, floor semantics, modifier meaning), creating two reasons to change a certified subsystem. BehaviorEngine should own the full merge pipeline and output finalized values. |
| Duplicate `_final` fields in CreatureState (separate finalized fields alongside raw fields) | Adds 2 bytes of permanent memory overhead, creates ambiguity about which field is the rendering source of truth, and requires a `_final` naming convention that all future contributors must learn. The intermediate values are only needed for debugging — the diagnostics struct serves this at zero production cost. Sequential writes to canonical fields are the simpler contract. |
| ML-based or predictive behavior | Over-engineered for V1. Would introduce non-determinism, hardware requirements, and testability problems. Excluded explicitly until V3. |

---

## Consequences

### Positive
1. **No certified subsystem modified** — MemoryEngine, MoodEngine, AttentionEngine, EmotionEngine, FaceEngine, and PersistenceService remain untouched
2. **Testable in isolation** — `BehaviorEngine.tick()` takes a `MemorySummary`, produces state diffs. Unit-testable with synthetic memory data
3. **Additive deployment** — Sprint 1 = 2 new files + 2 lines added to existing files
4. **Rollback** = delete 2 files, revert 2 lines
5. **Future-proof** — New behavior rules are additive within `BehaviorEngine.tick()`. New CreatureState fields are additive to the struct

### Negative
1. **Pipeline transform fields add conceptual complexity** — `mood_strength` and `engagement_drive` are no longer single-owner fields. Transformers must be aware of the pipeline contract (read-modify-write, bounded transforms, fixed ordering).
2. **Tick order is now an architectural contract** — Behavior must run after Memory (to read fresh recall) and MoodEngine (to read baseline mood), before Face (to supply finalized values). Reordering requires documented architectural review and ADR amendment per the execution order contract.
3. **V2.6 Sprint 3 domains are uncertified** — Behavior Layer consumes potentially unvalidated memory data until Sprint 3 hardware certification is complete.
4. **MoodEngine baseline is ephemeral** — The baseline is overwritten by BehaviorEngine and is only available for debugging if `BehaviorPipelineSnapshot` is populated. This is acceptable since no engine needs the baseline after position 6.

### Risks
- **Scope creep:** V1 rules are deterministic and bounded. New rules require documented rationale.
- **Pipeline transform discipline:** All pipeline stages must use read-modify-write. A future change that uses blind assignment (`field = constant` without reading) would destroy the transform sequence. Enforce in code review.
- **Memory→Behavior→Face is not the complete picture:** MoodEngine still computes baseline mood from touch/emotion. BehaviorEngine augments with memory-derived modifiers and overwrites the canonical state. The system has two independent mood influences (MoodEngine from interaction, BehaviorEngine from memory) that merge inside BehaviorEngine, not in CreatureState or FaceEngine.

---

## Future Considerations

| Item | When | Approach |
|------|------|----------|
| Learned preferences | V3 | New field in CreatureState, new engine consuming BehaviorEngine output |
| Curiosity / fatigue drives | V3+ | New CreatureState fields, new engine or extended BehaviorEngine |
| Multi-modal behavior (combine memory + mood + attention) | V2.7 Sprint 2+ | Extended rules in BehaviorEngine |
| Hardware certification of Sprint 3 domains | Sprint 1 | 30-min multi-domain soak before or concurrent with Behavior V1 |
