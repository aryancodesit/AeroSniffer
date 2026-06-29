# Technical Debt Register

Each debt item is a known suboptimal practice that is intentionally deferred. The register ensures debt is visible, triaged, and not forgotten.

---

## Template

```
### TD-NNN: <Title>

Date: YYYY-MM-DD
Severity: [LOW | MEDIUM | HIGH | CRITICAL]
Area: <subsystem or filename>

Description:
<What is the debt? What is the ideal state?>

Rationale for deferral:
<Why is this acceptable now? Under what condition would it need to be fixed?>

Fix estimate: <engineering hours or "unknown">
Status: [OPEN | IN_PROGRESS | FIXED | WONT_FIX]
```

### Severity Definitions

| Severity | Meaning |
|----------|---------|
| LOW | Cosmetic inconsistency, does not affect correctness or velocity |
| MEDIUM | Causes friction in development, risk of future bug |
| HIGH | Known correctness risk or significant velocity drag |
| CRITICAL | Will cause a bug if not addressed before next release |

---

## Register

### TD-001: EventBus self-subscription in EmotionEngine

Date: 2026-06-25
Severity: LOW
Area: AeroSnifferOS.cpp

Description:
EmotionEngineClass::begin() subscribes to every EventType in a for-loop using a lambda that calls handleEvent(). This bypasses the rule that subscriptions should be wired once in setup() via static callbacks. There is no correctness issue — it works — but it is inconsistent with the architecture and makes the subscription graph harder to trace.

Rationale for deferral:
Changing it requires touching every event type to ensure the EmotionEngine still receives all events. The lambda adds no runtime overhead (it inlines). Fix only when adding new event types that should NOT reach EmotionEngine (currently there are none).

Fix estimate: 1 hour
Status: OPEN

---

### TD-002: Mixed brace style

Date: 2026-06-25
Severity: LOW
Area: All files

Description:
Some functions use Allman braces (`\n{`), others use K&R (`{` at end of line). Classes generally use K&R, methods generally use Allman, but there are exceptions in AeroSniffer.ino.

Rationale for deferral:
A formatting pass would touch nearly every line in every file, creating merge conflicts with active branches. Fix during a sprint with no engine changes.

Fix estimate: 2 hours (clang-format pass + review)
Status: OPEN

---

### TD-003: Dual header guards

Date: 2026-06-25
Severity: LOW
Area: Memory/MemoryEngine.h, Memory/MemoryTypes.h

Description:
These files use both `#pragma once` and `#ifndef MEMORY_ENGINE_H` / `#define MEMORY_ENGINE_H`. The `#ifndef` guard is redundant. All other files use `#pragma once` alone.

Rationale for deferral:
No correctness issue. The dual guard is harmless. Fix when touching these files for other reasons.

Fix estimate: <5 minutes
Status: OPEN

---

### TD-004: Header-only Mode files with large inline functions

Date: 2026-06-25
Severity: MEDIUM
Area: Mode1_Pet.h, Mode2_Security.h, Mode3_Aviation.h

Description:
Mode files are `#include`d exactly once from AeroSniffer.ino, but contain substantial logic as static functions. This works because of the single include, but violates the principle that headers declare and `.cpp` files define. As modes grow beyond 500 lines, compile time and readability suffer.

Rationale for deferral:
Mode3_Aviation.h is the largest at ~400 lines. Refactoring to `.h`/`.cpp` pairs would improve organization but requires touching all modes. Fix when any mode exceeds 500 lines.

Fix estimate: 3 hours
Status: OPEN

---

### TD-005: No log level abstraction

Date: 2026-06-25
Severity: MEDIUM
Area: All files

Description:
Serial output uses raw `Serial.printf` with no log level filtering (DEBUG, INFO, WARN, ERROR). To suppress debug output in production, `[DBG]` lines must be manually removed. A log level system would allow runtime or compile-time filtering.

Rationale for deferral:
Adding a logging wrapper requires touching every `Serial.printf` call (~200+ sites). The current convention (`[TAG]` prefix + `[DBG]` removal discipline) is working. Fix when the number of `[DBG]` lines becomes hard to track, or when flash/memory pressure requires stripping them.

Fix estimate: 4 hours
Status: OPEN

---

### TD-006: DynamicJsonDocument on stack

Date: 2026-06-25
Severity: LOW
Area: AeroSniffer.ino

Description:
The serial command handler uses `DynamicJsonDocument doc(512)` on the stack. This allocates 512 bytes of stack per call. If called recursively or from deep call chains, this risks stack overflow.

Rationale for deferral:
`DynamicJsonDocument` is used in exactly one spot and only from the serial command processing path (not from any engine tick). The call depth is shallow. Fix only if stack overflow is observed in this path.

Fix estimate: 1 hour (switch to `StaticJsonDocument<512>`)
Status: OPEN

---

### TD-007: Fixed delta in MemoryEngine.tick()

Date: 2026-06-25
Severity: LOW
Area: MemoryEngine.cpp

Description:
MemoryEngine.tick() does not accept a `delta_ms` parameter. It runs its decay/prune cycle on a hardcoded 60-second interval using `millis()` internally. This is inconsistent with tick engines that accept a measured delta.

Rationale for deferral:
The 60-second interval is correct for MemoryEngine — it does not need frame-by-frame delta. Changing the signature would break the pattern but provide no benefit. Fix only if MemoryEngine needs sub-second timing.

Fix estimate: 30 minutes
Status: WONT_FIX

---

### TD-008: Inconsistent Class suffix

Date: 2026-06-25
Severity: LOW
Area: All engines

Description:
Some classes use `Class` suffix (`AttentionEngineClass`, `MoodEngineClass`) and some do not (`PersistenceService`, `TimelineClass` — wait, TimelineClass does). The inconsistency adds cognitive load.

Rationale for deferral:
Renaming classes requires touching every file that references them. The `Class` suffix is purely a naming convention with no semantic meaning. Fix during a major version change.

Fix estimate: 2 hours
Status: OPEN
