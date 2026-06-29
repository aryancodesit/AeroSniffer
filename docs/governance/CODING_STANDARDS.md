# Coding Standards — AeroSniffer

Rules are graded **MUST** (enforced in review), **SHOULD** (strongly recommended), or **MAY** (local preference).

---

## 1. Engine Pattern

MUST: Every engine class follows the lifecycle:
- `void begin()` — one-time initialization
- `void tick()` or `void tick(uint32_t delta_ms)` — per-frame update
- `void publish()` (optional) — writes owned slice of `g_creature` at end of tick

MUST: Engines are declared as `extern` singletons in `AeroSnifferOS.h` and defined as plain global objects in their `.cpp` file. No `getInstance()` static method.

MUST: Engines do not call each other's methods directly. Cross-engine communication goes through EventBus or `g_creature`.

## 2. State (`g_creature`) Ownership

MUST: Each engine writes only its owned slice of `g_creature`:

| Slice | Owner |
|-------|-------|
| `g_creature.emotion`, `activity` | EmotionEngine |
| `g_creature.mood`, `mood_strength` | MoodEngine |
| `g_creature.attention.*` | AttentionEngine |
| `g_creature.engagement_drive` | FaceEngine |

SHOULD: An engine that reads another engine's slice from `g_creature` does so only during its own `tick()`, not during an EventBus callback.

## 3. EventBus

MUST: All EventBus subscriptions are wired once in `setup()` (AeroSniffer.ino) via static C-callback functions. Engines do not self-subscribe in their constructors or `begin()`.

(EmotionEngine's lambda subscription in `begin()` is legacy — do not replicate.)

MUST: EventBus callbacks are minimal dispatchers. They queue data or set flags; they do not run engine logic.

## 4. Memory Allocation

MUST: No dynamic allocation (`new`, `delete`, `malloc`, `free`, `std::vector`, `std::map`, etc.) in engine code.

MUST: All arrays are fixed-size at compile time. Ring buffers and queues use circular indices, not linked lists.

SHOULD: Prefer `snprintf()` with stack buffer over `String` concatenation.

MAY: `DynamicJsonDocument` on stack (max 512 bytes) is acceptable in serial command handlers.

## 5. Serial Diagnostics

MUST: Every `Serial.printf`/`println` includes an engine tag in square brackets:

```
[TAG] message
```

SHOULD: Use existing tags from the convention table below. Add new tags when introducing a new subsystem.

| Tag | Subsystem |
|-----|-----------|
| `[ATTN]` | AttentionEngine |
| `[EMOTION]` | EmotionEngine |
| `[MOOD]` | MoodEngine |
| `[MEM]` | MemoryEngine |
| `[PERSIST]` | PersistenceService |
| `[FACE]` | FaceEngine |
| `[TOUCH]` | Touch input |
| `[WIFI]` | WiFi events |
| `[MODE]` | Mode transitions |
| `[SERIAL]` | Serial command parser |
| `[SEC]` | Security mode |
| `[AVI]` | Aviation mode |
| `[PC]` | PC companion |
| `[DBG]` | Debug-only (removed before release) |

MUST: `[DBG]` lines are temporary and must be removed before the commit that closes a sprint. All other tags are permanent diagnostic output.

SHOULD: One `printf` per logical event. Do not split a single event's data across multiple `printf` calls.

## 6. Naming

MUST:

| What | Convention | Example |
|------|-----------|---------|
| Classes | `PascalCase`, optionally `Class` suffix | `AttentionEngineClass`, `PersistenceService` |
| Global instances | `PascalCase` | `EventBus`, `MemoryEngine` |
| Methods | `camelCase()` | `queueEvent()`, `resetToIdle()` |
| Member variables | `snake_case`, optional `_` prefix | `_queue`, `active_particles` |
| Local variables | `snake_case` | `now`, `duration`, `buf` |
| Constants (`constexpr`) | `UPPER_SNAKE_CASE` | `QUEUE_SIZE`, `SAVE_INTERVAL_MS` |
| Macros | `UPPER_SNAKE_CASE` | `HW_DESKBUDDY_2`, `DEBOUNCE_MS` |
| Enums and values | `PascalCase` type, `UPPER_SNAKE_CASE` values | `enum EventType { EVENT_TOUCH_SHORT }` |
| File names | `PascalCase` matching primary class | `AttentionEngine.h`, `MemoryTypes.h` |
| Header guards | `#pragma once` | |

SHOULD: File-local `static` helpers use `local_` prefix: `local_draw_heart()`.

## 7. File Organization

MUST: One `.h` per class, plus `.cpp` if the class has implementation logic.

MUST: Header include order:
1. `#pragma once` (first non-comment line)
2. System headers (`<Arduino.h>`, `<stdint.h>`)
3. Library headers (`<TFT_eSPI.h>`, `<WiFi.h>`)
4. Project headers (`"AeroSnifferOS.h"`, `"Config.h"`)
5. Class declaration

SHOULD: Mode files are header-only (`Mode1_Pet.h`, etc.) with all logic as static functions. Do not add `.cpp` for a mode file unless it exceeds 500 lines.

## 8. Tick Timing

MUST: Every `tick()` that accepts `uint32_t delta_ms` passes the measured delta, not a fixed constant. The only exception is `MemoryEngine.tick()` which runs on its own 60-second interval.

MUST: A `tick()` call must complete within 50 ms on Core 1 to avoid starving `loop()`.

## 9. Thread Safety

MUST: Core 0 tasks read `g_creature` only. They never write to it.

SHOULD: If Core 0 must signal Core 1, use a lock-free atomic or EventBus publish (not a shared variable write).

## 10. Configuration

MUST: Hardware variant selection, pin mappings, and feature flags live in `Config.h`.

SHOULD: Runtime-configurable values (WiFi credentials, thresholds) are loaded from NVS Preferences into `extern` globals in `setup()`, not hardcoded.

## 11. Non-Negotiable (Review Killers)

Any commit that violates one of these MUST be rejected in review regardless of correctness:

1. Dynamic allocation in engine code
2. Direct engine-to-engine method call (must use EventBus or `g_creature`)
3. EventBus self-subscription in `begin()` or constructor
4. EventBus callback running engine logic instead of dispatching
5. Core 0 writing to `g_creature`
6. `tick()` using a fixed delta instead of measured (MemoryEngine's 60-second interval is the documented exception)
